#pragma once

#include "../game/vehicle/VehicleModels.hpp"

#include <Windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

#include <algorithm>
#include <atomic>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace Tutones::UI
{
    struct VehicleThumbnailSyncSnapshot final
    {
        bool running{};
        bool completed{};
        std::size_t total{};
        std::size_t existing{};
        std::size_t downloaded{};
        std::size_t missing{};
        std::size_t failed{};
        std::uint64_t generation{};
        std::string currentModel{};
        std::string message{"Not started"};
    };

    class VehicleThumbnailDownloader final
    {
    public:
        static VehicleThumbnailDownloader& Get() noexcept
        {
            static VehicleThumbnailDownloader instance;
            return instance;
        }

        ~VehicleThumbnailDownloader()
        {
            Stop();
        }

        VehicleThumbnailDownloader(const VehicleThumbnailDownloader&) = delete;
        VehicleThumbnailDownloader& operator=(const VehicleThumbnailDownloader&) = delete;

        void EnsureStarted()
        {
            EnsureStarted(std::vector<int>{});
        }

        void EnsureStarted(const std::vector<int>& classes)
        {
            bool expected = false;
            if (!m_Started.compare_exchange_strong(expected, true))
                return;
            Start(classes);
        }

        void Restart()
        {
            Restart(std::vector<int>{});
        }

        void Restart(const std::vector<int>& classes)
        {
            m_Started.store(true);
            Start(classes);
        }

        void Stop() noexcept
        {
            if (m_Worker.joinable())
            {
                m_Worker.request_stop();
                m_Worker.join();
            }

            std::scoped_lock lock(m_Mutex);
            m_State.running = false;
            if (!m_State.completed && m_State.message != "Not started")
                m_State.message = "Vehicle artwork sync stopped";
        }

        [[nodiscard]] VehicleThumbnailSyncSnapshot Snapshot() const
        {
            std::scoped_lock lock(m_Mutex);
            return m_State;
        }

        [[nodiscard]] std::filesystem::path ThumbnailFolder() const
        {
            return ResolveRootFolder();
        }

    private:
        struct WorkItem final
        {
            std::string model{};
            int classIndex{-1};
        };

        enum class FetchResult : unsigned char
        {
            Downloaded,
            NotFound,
            Failed,
        };

        VehicleThumbnailDownloader() = default;

        static constexpr std::uint64_t MaxImageBytes = 8ull * 1024ull * 1024ull;

        [[nodiscard]] static std::filesystem::path ResolveRootFolder()
        {
            wchar_t localAppData[MAX_PATH]{};
            const DWORD length = ::GetEnvironmentVariableW(L"LOCALAPPDATA", localAppData, MAX_PATH);
            std::filesystem::path root;
            if (length > 0 && length < MAX_PATH)
                root = std::filesystem::path(localAppData);
            else
                root = std::filesystem::path(L".");
            return root / L"TutonesMenu" / L"vehicle_thumbnails";
        }

        [[nodiscard]] static const wchar_t* ClassFolder(int classIndex) noexcept
        {
            static constexpr const wchar_t* Folders[] = {
                L"Compacts",
                L"Sedans",
                L"SUVs",
                L"Coupes",
                L"Muscles",
                L"Sports%20Classics",
                L"Sports",
                L"Super",
                L"Motorcycles",
                L"Off-Road",
                L"Industrial",
                L"Utility",
                L"Vans",
                L"Cycles",
                L"Boats",
                L"Helicopters",
                L"Planes",
                L"Service",
                L"Emergency",
                L"Military",
                L"Commercial",
                L"Trains",
                L"Open%20Wheels",
            };

            if (classIndex < 0 || classIndex >= static_cast<int>(std::size(Folders)))
                return nullptr;
            return Folders[classIndex];
        }

        [[nodiscard]] static bool ExistingImageIsUsable(const std::filesystem::path& path) noexcept
        {
            std::error_code error;
            if (!std::filesystem::is_regular_file(path, error) || error)
                return false;
            const auto size = std::filesystem::file_size(path, error);
            return !error && size >= 1024u;
        }

        [[nodiscard]] static std::wstring ModelToWide(const std::string& model)
        {
            return std::wstring(model.begin(), model.end());
        }

        [[nodiscard]] static FetchResult DownloadPath(
            HINTERNET connection,
            const std::wstring& remotePath,
            const std::filesystem::path& destination) noexcept
        {
            if (!connection || remotePath.empty())
                return FetchResult::Failed;

            HINTERNET request = ::WinHttpOpenRequest(
                connection,
                L"GET",
                remotePath.c_str(),
                nullptr,
                WINHTTP_NO_REFERER,
                WINHTTP_DEFAULT_ACCEPT_TYPES,
                WINHTTP_FLAG_SECURE);
            if (!request)
                return FetchResult::Failed;

            const auto closeRequest = [&]() noexcept { ::WinHttpCloseHandle(request); };
            if (!::WinHttpSendRequest(
                    request,
                    WINHTTP_NO_ADDITIONAL_HEADERS,
                    0,
                    WINHTTP_NO_REQUEST_DATA,
                    0,
                    0,
                    0)
                || !::WinHttpReceiveResponse(request, nullptr))
            {
                closeRequest();
                return FetchResult::Failed;
            }

            DWORD status{};
            DWORD statusSize = sizeof(status);
            if (!::WinHttpQueryHeaders(
                    request,
                    WINHTTP_QUERY_STATUS_CODE | WINHTTP_QUERY_FLAG_NUMBER,
                    WINHTTP_HEADER_NAME_BY_INDEX,
                    &status,
                    &statusSize,
                    WINHTTP_NO_HEADER_INDEX))
            {
                closeRequest();
                return FetchResult::Failed;
            }

            if (status == 404)
            {
                closeRequest();
                return FetchResult::NotFound;
            }
            if (status != 200)
            {
                closeRequest();
                return FetchResult::Failed;
            }

            const auto temporary = destination.wstring() + L".download";
            std::ofstream output(std::filesystem::path(temporary), std::ios::binary | std::ios::trunc);
            if (!output)
            {
                closeRequest();
                return FetchResult::Failed;
            }

            std::uint64_t totalBytes{};
            bool ok = true;
            for (;;)
            {
                DWORD available{};
                if (!::WinHttpQueryDataAvailable(request, &available))
                {
                    ok = false;
                    break;
                }
                if (available == 0)
                    break;

                const DWORD chunkSize = std::min<DWORD>(available, 64u * 1024u);
                std::vector<char> buffer(chunkSize);
                DWORD bytesRead{};
                if (!::WinHttpReadData(request, buffer.data(), chunkSize, &bytesRead))
                {
                    ok = false;
                    break;
                }
                if (bytesRead == 0)
                    break;

                output.write(buffer.data(), static_cast<std::streamsize>(bytesRead));
                if (!output)
                {
                    ok = false;
                    break;
                }

                totalBytes += bytesRead;
                if (totalBytes > MaxImageBytes)
                {
                    ok = false;
                    break;
                }
            }

            output.close();
            closeRequest();

            std::error_code error;
            if (!ok || totalBytes < 1024u)
            {
                std::filesystem::remove(std::filesystem::path(temporary), error);
                return FetchResult::Failed;
            }

            std::filesystem::remove(destination, error);
            error.clear();
            std::filesystem::rename(std::filesystem::path(temporary), destination, error);
            if (error)
            {
                std::filesystem::remove(std::filesystem::path(temporary), error);
                return FetchResult::Failed;
            }

            return FetchResult::Downloaded;
        }

        [[nodiscard]] static FetchResult DownloadFallback(
            HINTERNET connection,
            const std::wstring& model,
            int classIndex,
            const std::filesystem::path& destination,
            std::stop_token stopToken) noexcept
        {
            if (const wchar_t* folder = ClassFolder(classIndex))
            {
                const std::wstring path =
                    L"/atoshit/cfx-img-pack/main/Vehicles/" + std::wstring(folder) + L"/" + model + L".png";
                return DownloadPath(connection, path, destination);
            }

            FetchResult aggregate = FetchResult::NotFound;
            for (int candidate = 0; candidate < 23 && !stopToken.stop_requested(); ++candidate)
            {
                const wchar_t* folder = ClassFolder(candidate);
                if (!folder)
                    continue;
                const std::wstring path =
                    L"/atoshit/cfx-img-pack/main/Vehicles/" + std::wstring(folder) + L"/" + model + L".png";
                const FetchResult result = DownloadPath(connection, path, destination);
                if (result == FetchResult::Downloaded)
                    return result;
                if (result == FetchResult::Failed)
                    aggregate = FetchResult::Failed;
            }
            return aggregate;
        }

        void Start(const std::vector<int>& classes)
        {
            Stop();

            std::vector<WorkItem> work;
            work.reserve(Game::VehicleCatalogs::VehicleModels.size());
            for (std::size_t i = 0; i < Game::VehicleCatalogs::VehicleModels.size(); ++i)
            {
                WorkItem item;
                item.model = Game::VehicleCatalogs::VehicleModels[i];
                item.classIndex = i < classes.size() ? classes[i] : -1;
                work.push_back(std::move(item));
            }

            {
                std::scoped_lock lock(m_Mutex);
                m_State = {};
                m_State.running = true;
                m_State.total = work.size();
                m_State.message = "Syncing real GTA vehicle pictures";
            }

            m_Worker = std::jthread([this, work = std::move(work)](std::stop_token stopToken) {
                Run(stopToken, work);
            });
        }

        void Run(std::stop_token stopToken, const std::vector<WorkItem>& work) noexcept
        {
            const auto root = ResolveRootFolder();
            std::error_code error;
            std::filesystem::create_directories(root, error);
            if (error)
            {
                FinishWithFailure("Could not create vehicle thumbnail cache folder");
                return;
            }

            HINTERNET session = ::WinHttpOpen(
                L"TutonesMenu/2.0 vehicle-art-sync",
                WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                WINHTTP_NO_PROXY_NAME,
                WINHTTP_NO_PROXY_BYPASS,
                0);
            if (!session)
            {
                FinishWithFailure("WinHTTP could not initialize vehicle artwork sync");
                return;
            }

            ::WinHttpSetTimeouts(session, 3000, 3000, 5000, 5000);
            HINTERNET connection = ::WinHttpConnect(
                session,
                L"raw.githubusercontent.com",
                INTERNET_DEFAULT_HTTPS_PORT,
                0);
            if (!connection)
            {
                ::WinHttpCloseHandle(session);
                FinishWithFailure("Could not connect to the vehicle artwork source");
                return;
            }

            for (const auto& item : work)
            {
                if (stopToken.stop_requested())
                    break;

                {
                    std::scoped_lock lock(m_Mutex);
                    m_State.currentModel = item.model;
                }

                const auto destination = root / (item.model + ".png");
                if (ExistingImageIsUsable(destination))
                {
                    std::scoped_lock lock(m_Mutex);
                    ++m_State.existing;
                    continue;
                }

                const std::wstring model = ModelToWide(item.model);
                const std::wstring primaryPath =
                    L"/matthias18771/v-vehicle-images/main/images/" + model + L".png";
                FetchResult result = DownloadPath(connection, primaryPath, destination);

                if (result != FetchResult::Downloaded && !stopToken.stop_requested())
                {
                    const FetchResult fallback = DownloadFallback(
                        connection,
                        model,
                        item.classIndex,
                        destination,
                        stopToken);
                    if (fallback == FetchResult::Downloaded)
                        result = fallback;
                    else if (result == FetchResult::NotFound)
                        result = fallback;
                }

                std::scoped_lock lock(m_Mutex);
                if (result == FetchResult::Downloaded)
                {
                    ++m_State.downloaded;
                    ++m_State.generation;
                }
                else if (result == FetchResult::NotFound)
                {
                    ++m_State.missing;
                }
                else
                {
                    ++m_State.failed;
                }
            }

            ::WinHttpCloseHandle(connection);
            ::WinHttpCloseHandle(session);

            std::scoped_lock lock(m_Mutex);
            m_State.running = false;
            m_State.currentModel.clear();
            if (stopToken.stop_requested())
            {
                m_State.message = "Vehicle artwork sync stopped";
                return;
            }

            m_State.completed = true;
            ++m_State.generation;
            const std::size_t ready = m_State.existing + m_State.downloaded;
            m_State.message = "Real vehicle pictures ready: " + std::to_string(ready)
                + "/" + std::to_string(m_State.total);
        }

        void FinishWithFailure(const char* message) noexcept
        {
            std::scoped_lock lock(m_Mutex);
            m_State.running = false;
            m_State.completed = true;
            ++m_State.failed;
            ++m_State.generation;
            m_State.currentModel.clear();
            m_State.message = message ? message : "Vehicle artwork sync failed";
        }

        mutable std::mutex m_Mutex;
        VehicleThumbnailSyncSnapshot m_State{};
        std::jthread m_Worker{};
        std::atomic_bool m_Started{};
    };
}
