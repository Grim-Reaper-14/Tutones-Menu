#pragma once

#include "../game/vehicle/VehicleModels.hpp"

#include <Windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_set>
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

        // Put the vehicle the user is actually looking at ahead of the bulk sync. Requests
        // are de-duplicated for the lifetime of the current sync, so asking every UI frame is
        // cheap and does not hammer the network.
        void Request(std::string_view model, int classIndex)
        {
            if (model.empty())
                return;

            std::string name(model);
            std::scoped_lock lock(m_Mutex);
            if (m_State.completed)
                return;

            if (m_PriorityModels.insert(name).second)
            {
                m_PriorityWork.push_front(WorkItem{std::move(name), classIndex});
                return;
            }

            if (classIndex < 0)
                return;

            for (auto& queued : m_PriorityWork)
            {
                if (queued.model == name && queued.classIndex < 0)
                {
                    queued.classIndex = classIndex;
                    break;
                }
            }
        }

        void Restart()
        {
            Restart(std::vector<int>{});
        }

        void Restart(const std::vector<int>& classes)
        {
            m_Started.store(true);
            Stop();
            {
                std::scoped_lock lock(m_Mutex);
                m_PriorityWork.clear();
                m_PriorityModels.clear();
            }
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

        enum class ImageKind : unsigned char
        {
            Png,
            Webp,
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
                L"Sports%20Classic",
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

        [[nodiscard]] static bool SignatureMatches(
            const std::filesystem::path& path,
            ImageKind kind) noexcept
        {
            std::ifstream input(path, std::ios::binary);
            if (!input)
                return false;

            std::array<unsigned char, 12> signature{};
            input.read(reinterpret_cast<char*>(signature.data()), static_cast<std::streamsize>(signature.size()));
            const auto read = static_cast<std::size_t>(input.gcount());

            if (kind == ImageKind::Png)
            {
                static constexpr std::array<unsigned char, 8> Png{{
                    0x89u, 0x50u, 0x4Eu, 0x47u, 0x0Du, 0x0Au, 0x1Au, 0x0Au,
                }};
                return read >= Png.size()
                    && std::equal(Png.begin(), Png.end(), signature.begin());
            }

            return read >= 12
                && signature[0] == 'R'
                && signature[1] == 'I'
                && signature[2] == 'F'
                && signature[3] == 'F'
                && signature[8] == 'W'
                && signature[9] == 'E'
                && signature[10] == 'B'
                && signature[11] == 'P';
        }

        [[nodiscard]] static bool ExistingImageIsUsable(
            const std::filesystem::path& path,
            ImageKind kind) noexcept
        {
            std::error_code error;
            if (!std::filesystem::is_regular_file(path, error) || error)
                return false;
            const auto size = std::filesystem::file_size(path, error);
            return !error && size >= 1024u && size <= MaxImageBytes && SignatureMatches(path, kind);
        }

        [[nodiscard]] static bool ExistingModelImageIsUsable(
            const std::filesystem::path& root,
            const std::string& model) noexcept
        {
            return ExistingImageIsUsable(root / (model + ".png"), ImageKind::Png)
                || ExistingImageIsUsable(root / (model + ".webp"), ImageKind::Webp);
        }

        static void RemoveInvalidCachedImage(
            const std::filesystem::path& path,
            ImageKind kind) noexcept
        {
            std::error_code error;
            if (!std::filesystem::exists(path, error) || error)
                return;
            if (ExistingImageIsUsable(path, kind))
                return;
            std::filesystem::remove(path, error);
        }

        [[nodiscard]] static std::wstring ModelToWide(const std::string& model)
        {
            return std::wstring(model.begin(), model.end());
        }

        [[nodiscard]] static FetchResult DownloadPath(
            HINTERNET connection,
            const std::wstring& remotePath,
            const std::filesystem::path& destination,
            ImageKind kind) noexcept
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
            std::error_code cleanupError;
            std::filesystem::remove(std::filesystem::path(temporary), cleanupError);

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

            const auto temporaryPath = std::filesystem::path(temporary);
            std::error_code error;
            if (!ok
                || totalBytes < 1024u
                || !ExistingImageIsUsable(temporaryPath, kind))
            {
                std::filesystem::remove(temporaryPath, error);
                return FetchResult::Failed;
            }

            std::filesystem::remove(destination, error);
            error.clear();
            std::filesystem::rename(temporaryPath, destination, error);
            if (error)
            {
                std::filesystem::remove(temporaryPath, error);
                return FetchResult::Failed;
            }

            return FetchResult::Downloaded;
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
                m_State.message = "Syncing GTA vehicle pictures";
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
                L"TutonesMenu/2.1 vehicle-art-sync",
                WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                WINHTTP_NO_PROXY_NAME,
                WINHTTP_NO_PROXY_BYPASS,
                0);
            if (!session)
            {
                FinishWithFailure("WinHTTP could not initialize vehicle artwork sync");
                return;
            }

            ::WinHttpSetTimeouts(session, 2500, 2500, 4000, 4000);

            HINTERNET github = ::WinHttpConnect(
                session,
                L"raw.githubusercontent.com",
                INTERNET_DEFAULT_HTTPS_PORT,
                0);
            HINTERNET cfx = ::WinHttpConnect(
                session,
                L"docs-backend.fivem.net",
                INTERNET_DEFAULT_HTTPS_PORT,
                0);

            if (!github && !cfx)
            {
                ::WinHttpCloseHandle(session);
                FinishWithFailure("Could not connect to any vehicle artwork source");
                return;
            }

            std::size_t bulkIndex{};
            std::unordered_set<std::string> processed;
            processed.reserve(work.size());

            while (!stopToken.stop_requested())
            {
                WorkItem item;
                bool haveItem{};

                {
                    std::scoped_lock lock(m_Mutex);
                    if (!m_PriorityWork.empty())
                    {
                        item = std::move(m_PriorityWork.front());
                        m_PriorityWork.pop_front();
                        haveItem = true;
                    }
                }

                if (!haveItem)
                {
                    while (bulkIndex < work.size() && processed.contains(work[bulkIndex].model))
                        ++bulkIndex;
                    if (bulkIndex >= work.size())
                        break;
                    item = work[bulkIndex++];
                    haveItem = true;
                }

                if (!haveItem || !processed.insert(item.model).second)
                    continue;

                if (item.classIndex < 0)
                {
                    const auto matching = std::find_if(work.begin(), work.end(), [&](const WorkItem& candidate) {
                        return candidate.model == item.model;
                    });
                    if (matching != work.end())
                        item.classIndex = matching->classIndex;
                }

                {
                    std::scoped_lock lock(m_Mutex);
                    m_State.currentModel = item.model;
                }

                const auto pngDestination = root / (item.model + ".png");
                const auto webpDestination = root / (item.model + ".webp");
                RemoveInvalidCachedImage(pngDestination, ImageKind::Png);
                RemoveInvalidCachedImage(webpDestination, ImageKind::Webp);

                if (ExistingModelImageIsUsable(root, item.model))
                {
                    std::scoped_lock lock(m_Mutex);
                    ++m_State.existing;
                    continue;
                }

                const std::wstring model = ModelToWide(item.model);
                FetchResult aggregate = FetchResult::NotFound;
                bool downloaded = false;

                const auto attempt = [&](HINTERNET connection,
                                         const std::wstring& remotePath,
                                         const std::filesystem::path& destination,
                                         ImageKind kind) noexcept {
                    if (!connection || downloaded || stopToken.stop_requested())
                        return;
                    const FetchResult result = DownloadPath(connection, remotePath, destination, kind);
                    if (result == FetchResult::Downloaded)
                    {
                        aggregate = result;
                        downloaded = true;
                    }
                    else if (result == FetchResult::Failed)
                    {
                        aggregate = FetchResult::Failed;
                    }
                };

                if (const wchar_t* folder = ClassFolder(item.classIndex))
                {
                    attempt(
                        github,
                        L"/atoshit/cfx-img-pack/main/Vehicles/" + std::wstring(folder) + L"/" + model + L".png",
                        pngDestination,
                        ImageKind::Png);
                }

                attempt(
                    github,
                    L"/matthias18771/v-vehicle-images/main/images/" + model + L".png",
                    pngDestination,
                    ImageKind::Png);

                attempt(
                    cfx,
                    L"/vehicles/" + model + L".webp",
                    webpDestination,
                    ImageKind::Webp);

                std::scoped_lock lock(m_Mutex);
                if (downloaded)
                {
                    ++m_State.downloaded;
                    ++m_State.generation;
                }
                else if (aggregate == FetchResult::NotFound)
                {
                    ++m_State.missing;
                }
                else
                {
                    ++m_State.failed;
                }
            }

            if (github)
                ::WinHttpCloseHandle(github);
            if (cfx)
                ::WinHttpCloseHandle(cfx);
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
            m_State.message = "Vehicle pictures ready: " + std::to_string(ready)
                + "/" + std::to_string(m_State.total)
                + " | missing " + std::to_string(m_State.missing)
                + " | failed " + std::to_string(m_State.failed);
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
        std::deque<WorkItem> m_PriorityWork{};
        std::unordered_set<std::string> m_PriorityModels{};
        std::jthread m_Worker{};
        std::atomic_bool m_Started{};
    };
}
