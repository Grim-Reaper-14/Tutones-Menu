#pragma once

#include "../game/vehicle/VehicleModels.hpp"

#include <Windows.h>
#include <winhttp.h>
#pragma comment(lib, "winhttp.lib")

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <cstdint>
#include <deque>
#include <filesystem>
#include <fstream>
#include <mutex>
#include <string>
#include <string_view>
#include <thread>
#include <unordered_map>
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

        // Selected vehicles always pre-empt the bulk catalog pass. Unlike the previous
        // implementation, requests remain available after the first catalog pass finishes,
        // so a temporary 404/network failure can never permanently strand a preview until
        // the user restarts the game.
        void Request(std::string_view model, int classIndex)
        {
            if (model.empty())
                return;

            const std::string name(model);
            const auto now = Clock::now();

            {
                std::scoped_lock lock(m_Mutex);

                if (const auto status = m_ModelStatus.find(name); status != m_ModelStatus.end())
                {
                    if (status->second == ModelStatus::Existing || status->second == ModelStatus::Downloaded)
                        return;
                }

                if (const auto retry = m_NextRetry.find(name);
                    retry != m_NextRetry.end() && now < retry->second)
                {
                    return;
                }

                if (m_State.currentModel == name || m_PriorityModels.contains(name))
                    return;

                m_PriorityModels.insert(name);
                m_PriorityWork.push_front(WorkItem{name, classIndex});
                m_State.running = true;
                m_State.message = "Fetching exact vehicle picture: " + name;
            }

            m_WorkAvailable.notify_one();
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
                m_ModelStatus.clear();
                m_NextRetry.clear();
            }
            Start(classes);
        }

        void Stop() noexcept
        {
            if (m_Worker.joinable())
            {
                m_Worker.request_stop();
                m_WorkAvailable.notify_all();
                m_Worker.join();
            }

            std::scoped_lock lock(m_Mutex);
            m_State.running = false;
            m_State.currentModel.clear();
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
        using Clock = std::chrono::steady_clock;

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

        enum class ModelStatus : unsigned char
        {
            Unknown,
            Existing,
            Downloaded,
            Missing,
            Failed,
        };

        VehicleThumbnailDownloader() = default;

        static constexpr std::uint64_t MaxImageBytes = 8ull * 1024ull * 1024ull;
        static constexpr auto RetryDelay = std::chrono::seconds{15};

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
                m_State.message = "Syncing exact GTA vehicle pictures";
                m_ModelStatus.clear();
                m_NextRetry.clear();
            }

            m_Worker = std::jthread([this, work = std::move(work)](std::stop_token stopToken) {
                Run(stopToken, work);
            });
        }

        void AdjustStatusCountLocked(ModelStatus status, int delta) noexcept
        {
            auto adjust = [delta](std::size_t& value) noexcept {
                if (delta > 0)
                    value += static_cast<std::size_t>(delta);
                else if (value > 0)
                    --value;
            };

            switch (status)
            {
            case ModelStatus::Existing:   adjust(m_State.existing); break;
            case ModelStatus::Downloaded: adjust(m_State.downloaded); break;
            case ModelStatus::Missing:    adjust(m_State.missing); break;
            case ModelStatus::Failed:     adjust(m_State.failed); break;
            default: break;
            }
        }

        void SetModelStatusLocked(const std::string& model, ModelStatus status) noexcept
        {
            const auto found = m_ModelStatus.find(model);
            const ModelStatus previous = found == m_ModelStatus.end()
                ? ModelStatus::Unknown
                : found->second;

            if (previous == status)
                return;

            AdjustStatusCountLocked(previous, -1);
            AdjustStatusCountLocked(status, 1);
            m_ModelStatus[model] = status;
        }

        void UpdateSummaryLocked()
        {
            const std::size_t ready = m_State.existing + m_State.downloaded;
            m_State.message = "Exact vehicle pictures ready: " + std::to_string(ready)
                + "/" + std::to_string(m_State.total)
                + " | missing " + std::to_string(m_State.missing)
                + " | failed " + std::to_string(m_State.failed);
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
                L"TutonesMenu/2.2 exact-vehicle-art-sync",
                WINHTTP_ACCESS_TYPE_AUTOMATIC_PROXY,
                WINHTTP_NO_PROXY_NAME,
                WINHTTP_NO_PROXY_BYPASS,
                0);
            if (!session)
            {
                FinishWithFailure("WinHTTP could not initialize vehicle artwork sync");
                return;
            }

            ::WinHttpSetTimeouts(session, 2000, 2000, 3500, 3500);

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
            bool initialPassComplete{};

            while (!stopToken.stop_requested())
            {
                WorkItem item;
                bool fromPriority{};
                bool haveItem{};

                {
                    std::unique_lock lock(m_Mutex);

                    if (!m_PriorityWork.empty())
                    {
                        item = std::move(m_PriorityWork.front());
                        m_PriorityWork.pop_front();
                        fromPriority = true;
                        haveItem = true;
                    }
                    else if (bulkIndex < work.size())
                    {
                        item = work[bulkIndex++];
                        haveItem = true;
                    }
                    else
                    {
                        if (!initialPassComplete)
                        {
                            initialPassComplete = true;
                            m_State.completed = true;
                            ++m_State.generation;
                        }

                        m_State.running = false;
                        m_State.currentModel.clear();
                        UpdateSummaryLocked();

                        m_WorkAvailable.wait(lock, stopToken, [this] {
                            return !m_PriorityWork.empty();
                        });

                        if (stopToken.stop_requested())
                            break;

                        continue;
                    }

                    if (haveItem)
                    {
                        m_State.running = true;
                        m_State.currentModel = item.model;
                    }
                }

                if (!haveItem)
                    continue;

                if (item.classIndex < 0)
                {
                    const auto matching = std::find_if(work.begin(), work.end(), [&](const WorkItem& candidate) {
                        return candidate.model == item.model;
                    });
                    if (matching != work.end())
                        item.classIndex = matching->classIndex;
                }

                const auto pngDestination = root / (item.model + ".png");
                const auto webpDestination = root / (item.model + ".webp");
                RemoveInvalidCachedImage(pngDestination, ImageKind::Png);
                RemoveInvalidCachedImage(webpDestination, ImageKind::Webp);

                ModelStatus finalStatus = ModelStatus::Unknown;
                bool downloaded{};

                if (ExistingModelImageIsUsable(root, item.model))
                {
                    finalStatus = ModelStatus::Existing;
                }
                else
                {
                    const std::wstring model = ModelToWide(item.model);
                    FetchResult aggregate = FetchResult::NotFound;

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

                    // Current Cfx/FiveM reference is first because it is a flat exact-model
                    // source and tracks current GTA vehicle additions. Do not waste time on
                    // stale repositories before trying the source that already knows the new
                    // Enhanced/DLC model name.
                    attempt(
                        cfx,
                        L"/vehicles/" + model + L".webp",
                        webpDestination,
                        ImageKind::Webp);

                    // Large transparent exact-model archive. This fills many utility,
                    // trailer and duplicate-numbered variants that older packs omit.
                    attempt(
                        github,
                        L"/aymannajim/gta-v-vehicle-images/main/images/" + model + L".png",
                        pngDestination,
                        ImageKind::Png);

                    // Current Enhanced Native Trainer ships a curated exact-model preview
                    // folder for vehicles where game/shop artwork is awkward or absent.
                    attempt(
                        github,
                        L"/FIying-Scotsman/GTAV-EnhancedNativeTrainer/master/EnhancedNativeTrainer/Documents/previews/" + model + L".png",
                        pngDestination,
                        ImageKind::Png);

                    // Another flat WebP archive gives us an independent exact-model fallback.
                    attempt(
                        github,
                        L"/Stuyk/gtav-image-archive/main/vehicles/" + model + L".webp",
                        webpDestination,
                        ImageKind::Webp);

                    // Class-organized vanilla pack remains useful, but only after the flat
                    // current sources so a wrong/stale class can never hide an available image.
                    if (const wchar_t* folder = ClassFolder(item.classIndex))
                    {
                        attempt(
                            github,
                            L"/atoshit/cfx-img-pack/main/Vehicles/" + std::wstring(folder) + L"/" + model + L".png",
                            pngDestination,
                            ImageKind::Png);
                    }

                    // Archived source is last-resort only.
                    attempt(
                        github,
                        L"/matthias18771/v-vehicle-images/main/images/" + model + L".png",
                        pngDestination,
                        ImageKind::Png);

                    if (downloaded)
                        finalStatus = ModelStatus::Downloaded;
                    else if (aggregate == FetchResult::NotFound)
                        finalStatus = ModelStatus::Missing;
                    else
                        finalStatus = ModelStatus::Failed;
                }

                {
                    std::scoped_lock lock(m_Mutex);
                    SetModelStatusLocked(item.model, finalStatus);

                    if (finalStatus == ModelStatus::Downloaded || finalStatus == ModelStatus::Existing)
                    {
                        m_NextRetry.erase(item.model);
                        ++m_State.generation;
                    }
                    else
                    {
                        m_NextRetry[item.model] = Clock::now() + RetryDelay;
                    }

                    if (fromPriority)
                        m_PriorityModels.erase(item.model);

                    m_State.currentModel.clear();

                    if (initialPassComplete && m_PriorityWork.empty())
                    {
                        m_State.running = false;
                        UpdateSummaryLocked();
                    }
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
            if (stopToken.stop_requested() && !m_State.completed)
                m_State.message = "Vehicle artwork sync stopped";
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
        std::condition_variable_any m_WorkAvailable;
        VehicleThumbnailSyncSnapshot m_State{};
        std::deque<WorkItem> m_PriorityWork{};
        std::unordered_set<std::string> m_PriorityModels{};
        std::unordered_map<std::string, ModelStatus> m_ModelStatus{};
        std::unordered_map<std::string, Clock::time_point> m_NextRetry{};
        std::jthread m_Worker{};
        std::atomic_bool m_Started{};
    };
}
