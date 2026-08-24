#pragma once

#include "../../core/logging/Logger.hpp"
#include "../../runtime/GameRuntime.hpp"
#include "../GamePointers.hpp"
#include "../native/NativeCallContext.hpp"
#include "../native/NativeRegistry.hpp"
#include "../script/ScriptGlobal.hpp"
#include "../script/ScriptRuntime.hpp"

#include <Windows.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <memory>
#include <mutex>
#include <optional>
#include <string>
#include <string_view>
#include <thread>
#include <type_traits>
#include <unordered_map>
#include <utility>
#include <vector>

namespace Tutones::Game::Tunables
{
    [[nodiscard]] constexpr std::uint32_t Joaat(std::string_view text) noexcept
    {
        std::uint32_t hash{};
        for (char c : text)
        {
            if (c >= 'A' && c <= 'Z')
                c = static_cast<char>(c - 'A' + 'a');
            hash += static_cast<std::uint8_t>(c);
            hash += hash << 10;
            hash ^= hash >> 6;
        }
        hash += hash << 3;
        hash ^= hash >> 11;
        hash += hash << 15;
        return hash;
    }

    enum class TunableValueType : std::uint8_t
    {
        Unknown,
        Int,
        Bool,
        Float,
    };

    [[nodiscard]] constexpr const char* TunableValueTypeName(TunableValueType type) noexcept
    {
        switch (type)
        {
        case TunableValueType::Int: return "INT";
        case TunableValueType::Bool: return "BOOL";
        case TunableValueType::Float: return "FLOAT";
        case TunableValueType::Unknown: break;
        }
        return "RAW";
    }

    struct TunableEntrySnapshot final
    {
        std::uint32_t hash{};
        std::size_t globalIndex{};
        std::size_t offset{};
        TunableValueType type{TunableValueType::Unknown};
        std::int64_t currentRawValue{};
        std::int64_t originalRawValue{};
        bool readable{};
        std::string name;
    };

    struct TunableRegistrySnapshot final
    {
        bool running{};
        bool initialized{};
        bool globalsReady{};
        bool caching{};
        std::size_t registeredCount{};
        std::size_t tunableSlots{};
        std::uint64_t revision{};
        std::string message{"Stopped"};
    };

    class TunableRegistry final
    {
    public:
        static constexpr std::size_t BaseGlobal = 0x40001;

        static TunableRegistry& Get() noexcept
        {
            static TunableRegistry instance;
            return instance;
        }

        bool Start()
        {
            bool expected = false;
            if (!m_Running.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return true;

            m_Initialized.store(false, std::memory_order_release);
            m_GlobalsReady.store(false, std::memory_order_release);
            m_Caching.store(false, std::memory_order_release);
            m_TickQueued.store(false, std::memory_order_release);
            m_NativesReady.store(false, std::memory_order_release);
            m_Revision.store(0, std::memory_order_release);
            m_Phase.store(static_cast<std::uint8_t>(Phase::Waiting), std::memory_order_release);
            ResetTransientState();

            {
                std::scoped_lock lock(m_Mutex);
                m_Entries.clear();
                m_Message = "Waiting to cache Rockstar tunables";
            }

            if (!QueueNextTick())
            {
                m_Running.store(false, std::memory_order_release);
                SetMessage("Tunable registry could not queue its discovery tick");
                TUTONES_LOG_ERROR("game.tunables", "Central tunable registry failed to queue its first GTA script-thread tick");
                return false;
            }

            TUTONES_LOG_INFO("game.tunables", "Central tunable registry started; preparing tuneables_processing registration pass");
            return true;
        }

        void Stop() noexcept
        {
            if (!m_Running.exchange(false, std::memory_order_acq_rel))
                return;

            m_TickQueued.store(false, std::memory_order_release);

            const auto cleanup = [this] {
                CleanupRegistrationPass(true);
            };

            auto& runtime = Runtime::GameRuntime::Get();
            if (runtime.IsOnGameThread())
            {
                cleanup();
            }
            else if (runtime.IsInitialized())
            {
                const auto cleaned = std::make_shared<std::atomic<bool>>(false);
                if (runtime.Enqueue([cleanup, cleaned] {
                        cleanup();
                        cleaned->store(true, std::memory_order_release);
                    }))
                {
                    const auto deadline = std::chrono::steady_clock::now() + std::chrono::milliseconds(250);
                    while (!cleaned->load(std::memory_order_acquire)
                        && std::chrono::steady_clock::now() < deadline)
                    {
                        std::this_thread::sleep_for(std::chrono::milliseconds(1));
                    }
                }
            }

            m_Initialized.store(false, std::memory_order_release);
            m_GlobalsReady.store(false, std::memory_order_release);
            m_Caching.store(false, std::memory_order_release);
            m_NativesReady.store(false, std::memory_order_release);
            m_Phase.store(static_cast<std::uint8_t>(Phase::Waiting), std::memory_order_release);

            {
                std::scoped_lock lock(m_Mutex);
                m_Entries.clear();
                m_Message = "Stopped";
            }
            m_Revision.fetch_add(1, std::memory_order_acq_rel);
            TUTONES_LOG_INFO("game.tunables", "Central tunable registry stopped");
        }

        [[nodiscard]] bool IsRunning() const noexcept
        {
            return m_Running.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool Initialized() const noexcept
        {
            return m_Initialized.load(std::memory_order_acquire);
        }

        [[nodiscard]] bool RegisterGlobal(
            std::uint32_t hash,
            std::size_t globalIndex,
            std::string_view name = {},
            TunableValueType type = TunableValueType::Unknown) noexcept
        {
            if (hash == 0 || globalIndex < BaseGlobal)
                return false;

            std::int64_t original{};
            if (auto** globals = Script::ScriptRuntime::Get().Globals())
            {
                if (const auto* value = Script::ScriptGlobal(globalIndex).As<std::int64_t>(globals))
                    original = *value;
            }

            std::scoped_lock lock(m_Mutex);
            auto found = m_Entries.find(hash);
            if (found == m_Entries.end())
            {
                Entry entry{};
                entry.globalIndex = globalIndex;
                entry.name = name.empty() ? std::string{} : std::string{name};
                entry.type = type;
                entry.originalRaw = original;
                m_Entries.emplace(hash, std::move(entry));
            }
            else
            {
                found->second.globalIndex = globalIndex;
                if (!name.empty())
                    found->second.name = std::string{name};
                if (type != TunableValueType::Unknown)
                    found->second.type = type;
            }

            m_Revision.fetch_add(1, std::memory_order_acq_rel);
            return true;
        }

        [[nodiscard]] std::optional<Script::ScriptGlobal> Resolve(std::uint32_t hash) const noexcept
        {
            std::scoped_lock lock(m_Mutex);
            const auto found = m_Entries.find(hash);
            if (found == m_Entries.end())
                return std::nullopt;
            return Script::ScriptGlobal(found->second.globalIndex);
        }

        [[nodiscard]] std::optional<Script::ScriptGlobal> Resolve(std::string_view name) const noexcept
        {
            return Resolve(Joaat(name));
        }

        [[nodiscard]] std::vector<TunableEntrySnapshot> EntriesSnapshot() const
        {
            std::vector<std::pair<std::uint32_t, Entry>> entries;
            {
                std::scoped_lock lock(m_Mutex);
                entries.reserve(m_Entries.size());
                for (const auto& [hash, entry] : m_Entries)
                    entries.emplace_back(hash, entry);
            }

            auto** globals = Script::ScriptRuntime::Get().Globals();
            std::vector<TunableEntrySnapshot> result;
            result.reserve(entries.size());
            for (const auto& [hash, entry] : entries)
            {
                TunableEntrySnapshot snapshot{};
                snapshot.hash = hash;
                snapshot.globalIndex = entry.globalIndex;
                snapshot.offset = entry.globalIndex >= BaseGlobal ? entry.globalIndex - BaseGlobal : 0;
                snapshot.type = entry.type;
                snapshot.originalRawValue = entry.originalRaw;
                snapshot.name = entry.name;
                if (globals)
                {
                    if (const auto* value = Script::ScriptGlobal(entry.globalIndex).As<std::int64_t>(globals))
                    {
                        snapshot.currentRawValue = *value;
                        snapshot.readable = true;
                    }
                }
                result.push_back(std::move(snapshot));
            }

            std::sort(result.begin(), result.end(), [](const auto& left, const auto& right) {
                return left.globalIndex < right.globalIndex;
            });
            return result;
        }

        bool QueueSetInt(std::uint32_t hash, std::int32_t value) noexcept
        {
            return QueueWriteTyped(hash, value, "INT");
        }

        bool QueueSetBool(std::uint32_t hash, bool value) noexcept
        {
            return QueueWriteTyped(hash, static_cast<std::int32_t>(value ? 1 : 0), "BOOL");
        }

        bool QueueSetFloat(std::uint32_t hash, float value) noexcept
        {
            return QueueWriteTyped(hash, value, "FLOAT");
        }

        bool QueueSetRaw(std::uint32_t hash, std::int64_t value) noexcept
        {
            return QueueWriteTyped(hash, value, "RAW");
        }

        bool QueueRestore(std::uint32_t hash) noexcept
        {
            std::size_t globalIndex{};
            std::int64_t original{};
            {
                std::scoped_lock lock(m_Mutex);
                const auto found = m_Entries.find(hash);
                if (found == m_Entries.end())
                    return false;
                globalIndex = found->second.globalIndex;
                original = found->second.originalRaw;
            }

            return Runtime::GameRuntime::Get().Enqueue([this, hash, globalIndex, original] {
                auto** globals = Script::ScriptRuntime::Get().Globals();
                auto* target = globals ? Script::ScriptGlobal(globalIndex).As<std::int64_t>(globals) : nullptr;
                const bool success = target != nullptr;
                if (target)
                    *target = original;

                SetMessage(success
                    ? std::string("Restored tunable 0x") + HexHash(hash) + " to its registration-time value"
                    : std::string("Could not restore tunable 0x") + HexHash(hash));
                m_Revision.fetch_add(1, std::memory_order_acq_rel);
            });
        }

        [[nodiscard]] TunableRegistrySnapshot Snapshot() const
        {
            TunableRegistrySnapshot snapshot;
            snapshot.running = IsRunning();
            snapshot.initialized = Initialized();
            snapshot.globalsReady = m_GlobalsReady.load(std::memory_order_acquire);
            snapshot.caching = m_Caching.load(std::memory_order_acquire);
            snapshot.tunableSlots = m_NumTunables.load(std::memory_order_acquire);
            snapshot.revision = m_Revision.load(std::memory_order_acquire);
            std::scoped_lock lock(m_Mutex);
            snapshot.registeredCount = m_Entries.size();
            snapshot.message = m_Message;
            return snapshot;
        }

    private:
        enum class Phase : std::uint8_t
        {
            Waiting,
            Running,
            Complete,
        };

        enum class RegistrarNative : std::size_t
        {
            Wait,
            RequestScriptWithNameHash,
            HasScriptWithNameHashLoaded,
            StartNewScriptWithNameHashAndArgs,
            SetScriptWithNameHashAsNoLongerNeeded,
            GetNumberOfThreadsRunningScriptWithHash,
            RegistrationInt,
            RegistrationBool,
            RegistrationFloat,
            Count,
        };

        struct NativeDescriptor final
        {
            std::uint64_t hash{};
            const char* name{};
        };

        struct NativeProgram final
        {
            std::byte pad00[0x2C]{};
            std::uint32_t nativeCount{};
            std::byte pad30[0x10]{};
            Native::NativeHandler* nativeEntrypoints{};
            std::byte pad48[0x38]{};
        };

        static_assert(offsetof(NativeProgram, nativeCount) == 0x2C);
        static_assert(offsetof(NativeProgram, nativeEntrypoints) == 0x40);
        static_assert(sizeof(NativeProgram) == 0x80);

        struct TunablesLaunchData final
        {
            std::int64_t context{6};
            std::int64_t contentModifier{27};
        };

        static_assert(sizeof(TunablesLaunchData) == 0x10);

        struct Entry final
        {
            std::size_t globalIndex{};
            std::string name;
            TunableValueType type{TunableValueType::Unknown};
            std::int64_t originalRaw{};
        };

        struct CapturedRegistration final
        {
            std::uint32_t hash{};
            TunableValueType type{TunableValueType::Unknown};
        };

        struct PatchedSlot final
        {
            std::size_t index{static_cast<std::size_t>(-1)};
            Native::NativeHandler original{};
        };

        static constexpr std::size_t InvalidSlot = static_cast<std::size_t>(-1);
        static constexpr std::size_t GlobalPageSlots = 1u << 18;
        static constexpr std::uint32_t TuneablesProcessingHash = Joaat("tuneables_processing");
        static constexpr std::uint32_t TunablesRegistrationHash = Joaat("tunables_registration");
        static constexpr std::uint32_t StartupHash = Joaat("startup");
        static constexpr int ScriptStackSize = 1424;
        static constexpr int FirstJunkValue = 0x1000000;

        // Current Enhanced targets verified against YimMenuV2's enhanced crossmap.
        static constexpr std::array<NativeDescriptor, static_cast<std::size_t>(RegistrarNative::Count)> RegistrarNatives{{
            {0x4EDE34FBADD967A6ull, "WAIT"},
            {0x625263BFD08AE230ull, "REQUEST_SCRIPT_WITH_NAME_HASH"},
            {0x65F606616F48186Bull, "HAS_SCRIPT_WITH_NAME_HASH_LOADED"},
            {0xC4BB298BD441BE78ull, "START_NEW_SCRIPT_WITH_NAME_HASH_AND_ARGS"},
            {0xD21650BDA0F10841ull, "SET_SCRIPT_WITH_NAME_HASH_AS_NO_LONGER_NEEDED"},
            {0x486FF5D06E9659F1ull, "GET_NUMBER_OF_THREADS_RUNNING_THE_SCRIPT_WITH_THIS_HASH"},
            {0x0D94071E55F4C9CEull, "_NETWORK_GET_TUNABLES_REGISTRATION_INT"},
            {0xB327CF1B8C2C0EA3ull, "_NETWORK_GET_TUNABLES_REGISTRATION_BOOL"},
            {0x367E5E33E7F0DD1Aull, "_NETWORK_GET_TUNABLES_REGISTRATION_FLOAT"},
        }};

        TunableRegistry() = default;

        [[nodiscard]] static std::string HexHash(std::uint32_t hash)
        {
            constexpr char digits[] = "0123456789ABCDEF";
            std::string result(8, '0');
            for (int index = 7; index >= 0; --index)
            {
                result[static_cast<std::size_t>(index)] = digits[hash & 0xFu];
                hash >>= 4;
            }
            return result;
        }

        template<typename T>
        bool QueueWriteTyped(std::uint32_t hash, T value, const char* label) noexcept
        {
            std::size_t globalIndex{};
            {
                std::scoped_lock lock(m_Mutex);
                const auto found = m_Entries.find(hash);
                if (found == m_Entries.end())
                    return false;
                globalIndex = found->second.globalIndex;
            }

            const std::string typeLabel = label ? label : "VALUE";
            return Runtime::GameRuntime::Get().Enqueue([this, hash, globalIndex, value, typeLabel] {
                auto** globals = Script::ScriptRuntime::Get().Globals();
                auto* target = globals ? Script::ScriptGlobal(globalIndex).As<T>(globals) : nullptr;
                bool success = target != nullptr;
                if (target)
                {
                    *target = value;
                    success = *target == value;
                }

                SetMessage(success
                    ? std::string("Applied ") + typeLabel + " edit to tunable 0x" + HexHash(hash)
                    : std::string("Failed ") + typeLabel + " edit for tunable 0x" + HexHash(hash));
                m_Revision.fetch_add(1, std::memory_order_acq_rel);
            });
        }

        [[nodiscard]] static bool IsExecutableAddress(std::uintptr_t address) noexcept
        {
            if (address == 0)
                return false;

            MEMORY_BASIC_INFORMATION memory{};
            if (::VirtualQuery(reinterpret_cast<const void*>(address), &memory, sizeof(memory)) != sizeof(memory))
                return false;
            if (memory.State != MEM_COMMIT || (memory.Protect & PAGE_GUARD) != 0 || memory.Protect == PAGE_NOACCESS)
                return false;

            switch (memory.Protect & 0xFF)
            {
            case PAGE_EXECUTE:
            case PAGE_EXECUTE_READ:
            case PAGE_EXECUTE_READWRITE:
            case PAGE_EXECUTE_WRITECOPY:
                return true;
            default:
                return false;
            }
        }

        void SetMessage(std::string message)
        {
            std::scoped_lock lock(m_Mutex);
            m_Message = std::move(message);
        }

        [[nodiscard]] bool QueueNextTick() noexcept
        {
            if (!IsRunning())
                return false;

            bool expected = false;
            if (!m_TickQueued.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return true;

            if (Runtime::GameRuntime::Get().Enqueue([this] { TickOnGameThread(); }))
                return true;

            m_TickQueued.store(false, std::memory_order_release);
            return false;
        }

        template<typename Return, typename... Args>
        [[nodiscard]] std::optional<Return> Invoke(RegistrarNative id, Args&&... args) noexcept
        {
            static_assert(!std::is_void_v<Return>);
            const auto index = static_cast<std::size_t>(id);
            if (index >= m_RegistrarHandlers.size())
                return std::nullopt;

            auto* tls = Types::TlsContext::Get();
            if (!Native::NativeRegistry::Get().CanInvokeOnCurrentThread()
                || !tls || !tls->scriptThreadActive || !tls->currentScriptThread)
            {
                return std::nullopt;
            }

            const auto handler = m_RegistrarHandlers[index];
            if (!handler)
                return std::nullopt;

            Native::CallContext context;
            if (!(context.PushArg(std::forward<Args>(args)) && ...))
                return std::nullopt;
            handler(&context);
            context.FixVectors();
            return context.GetReturnValue<Return>();
        }

        template<typename... Args>
        bool InvokeVoid(RegistrarNative id, Args&&... args) noexcept
        {
            const auto index = static_cast<std::size_t>(id);
            if (index >= m_RegistrarHandlers.size())
                return false;

            auto* tls = Types::TlsContext::Get();
            if (!Native::NativeRegistry::Get().CanInvokeOnCurrentThread()
                || !tls || !tls->scriptThreadActive || !tls->currentScriptThread)
            {
                return false;
            }

            const auto handler = m_RegistrarHandlers[index];
            if (!handler)
                return false;

            Native::CallContext context;
            if (!(context.PushArg(std::forward<Args>(args)) && ...))
                return false;
            handler(&context);
            context.FixVectors();
            return true;
        }

        [[nodiscard]] bool ResolveRegistrarNativesOnGameThread() noexcept
        {
            if (m_NativesReady.load(std::memory_order_acquire))
                return true;
            if (!Native::NativeRegistry::Get().IsReady()
                || !Native::NativeRegistry::Get().CanInvokeOnCurrentThread())
            {
                return false;
            }

            const auto initNativeTables = GamePointers::Get().InitNativeTables();
            if (!initNativeTables)
                return false;

            std::array<std::uint64_t, RegistrarNatives.size()> slots{};
            for (std::size_t index = 0; index < RegistrarNatives.size(); ++index)
                slots[index] = RegistrarNatives[index].hash;

            NativeProgram program{};
            program.nativeCount = static_cast<std::uint32_t>(slots.size());
            program.nativeEntrypoints = reinterpret_cast<Native::NativeHandler*>(slots.data());
            initNativeTables(&program);

            for (std::size_t index = 0; index < slots.size(); ++index)
            {
                const auto address = static_cast<std::uintptr_t>(slots[index]);
                if (!IsExecutableAddress(address))
                {
                    TUTONES_LOG_WARN(
                        "game.tunables",
                        std::string("Could not resolve registrar native ") + RegistrarNatives[index].name);
                    m_RegistrarHandlers.fill(nullptr);
                    return false;
                }
                m_RegistrarHandlers[index] = reinterpret_cast<Native::NativeHandler>(address);
            }

            m_NativesReady.store(true, std::memory_order_release);
            TUTONES_LOG_INFO("game.tunables", "Resolved Enhanced native handlers required for tunable registration");
            return true;
        }

        void TickOnGameThread() noexcept
        {
            m_TickQueued.store(false, std::memory_order_release);
            if (!IsRunning())
                return;

            auto& scripts = Script::ScriptRuntime::Get();
            auto** globals = scripts.Globals();
            const bool globalsReady = scripts.IsReady() && globals != nullptr;
            m_GlobalsReady.store(globalsReady, std::memory_order_release);

            if (!globalsReady)
            {
                SetMessage("Waiting for shared script globals");
                RequeueOrLog();
                return;
            }

            if (!ResolveRegistrarNativesOnGameThread())
            {
                SetMessage("Waiting for registrar native handlers");
                RequeueOrLog();
                return;
            }

            const auto phase = static_cast<Phase>(m_Phase.load(std::memory_order_acquire));
            if (phase == Phase::Complete)
                return;

            if (phase == Phase::Running)
            {
                const auto count = Invoke<std::int32_t>(
                    RegistrarNative::GetNumberOfThreadsRunningScriptWithHash,
                    TuneablesProcessingHash);
                if (!count || *count > 0)
                {
                    SetMessage("Caching Rockstar tunable registrations");
                    RequeueOrLog();
                    return;
                }

                FinalizeRegistrationPass(globals);
                return;
            }

            const auto startupCount = Invoke<std::int32_t>(
                RegistrarNative::GetNumberOfThreadsRunningScriptWithHash,
                StartupHash);
            if (!startupCount || *startupCount > 0)
            {
                SetMessage("Waiting for GTA startup globals");
                RequeueOrLog();
                return;
            }

            const auto existing = Invoke<std::int32_t>(
                RegistrarNative::GetNumberOfThreadsRunningScriptWithHash,
                TuneablesProcessingHash);
            if (!existing || *existing > 0)
            {
                SetMessage("Waiting for existing tuneables_processing instance");
                RequeueOrLog();
                return;
            }

            static_cast<void>(InvokeVoid(RegistrarNative::RequestScriptWithNameHash, TuneablesProcessingHash));
            static_cast<void>(InvokeVoid(RegistrarNative::RequestScriptWithNameHash, TunablesRegistrationHash));

            const auto processingLoaded = Invoke<std::int32_t>(
                RegistrarNative::HasScriptWithNameHashLoaded,
                TuneablesProcessingHash);
            const auto registrationLoaded = Invoke<std::int32_t>(
                RegistrarNative::HasScriptWithNameHashLoaded,
                TunablesRegistrationHash);
            auto* processingProgram = scripts.FindProgram(TuneablesProcessingHash);
            auto* registrationProgram = scripts.FindProgram(TunablesRegistrationHash);

            if (!processingLoaded || !registrationLoaded || *processingLoaded == 0 || *registrationLoaded == 0
                || !processingProgram || !registrationProgram)
            {
                SetMessage("Loading tuneables_processing and tunables_registration");
                RequeueOrLog();
                return;
            }

            if (!BeginRegistrationPass(globals, processingProgram, registrationProgram))
            {
                SetMessage("Tunable registration setup failed - retrying");
                CleanupRegistrationPass(false);
                RequeueOrLog();
                return;
            }

            TunablesLaunchData args{};
            const auto started = Invoke<std::int32_t>(
                RegistrarNative::StartNewScriptWithNameHashAndArgs,
                TuneablesProcessingHash,
                &args,
                std::int32_t{2},
                std::int32_t{ScriptStackSize});

            if (!started || *started == 0)
            {
                SetMessage("Failed to start tuneables_processing - retrying");
                CleanupRegistrationPass(false);
                RequeueOrLog();
                return;
            }

            m_Phase.store(static_cast<std::uint8_t>(Phase::Running), std::memory_order_release);
            SetMessage("Caching Rockstar tunable registrations");
            RequeueOrLog();
        }

        void RequeueOrLog() noexcept
        {
            if (IsRunning() && !QueueNextTick())
                TUTONES_LOG_ERROR("game.tunables", "Tunable registry lost its GTA script-thread scheduling slot");
        }

        [[nodiscard]] bool BeginRegistrationPass(
            std::int64_t** globals,
            Types::ScriptProgram* processingProgram,
            Types::ScriptProgram* registrationProgram) noexcept
        {
            if (!globals || !processingProgram || !registrationProgram
                || !processingProgram->nativeEntrypoints || processingProgram->nativeCount == 0
                || registrationProgram->globalCount <= BaseGlobal)
            {
                return false;
            }

            const auto numTunables = static_cast<std::size_t>(registrationProgram->globalCount) - BaseGlobal;
            const auto baseSlot = BaseGlobal & (GlobalPageSlots - 1);
            if (numTunables == 0 || numTunables > GlobalPageSlots - baseSlot)
            {
                TUTONES_LOG_WARN("game.tunables", "tunables_registration reported an invalid global range");
                return false;
            }

            auto* base = Script::ScriptGlobal(BaseGlobal).As<std::int64_t>(globals);
            if (!base)
                return false;

            m_TunablesBackup.assign(base, base + numTunables);
            m_NumTunables.store(numTunables, std::memory_order_release);

            {
                std::scoped_lock lock(m_CacheMutex);
                m_JunkValues.clear();
                m_CurrentJunkValue = FirstJunkValue;
            }

            if (!PatchRegistrationNatives(processingProgram))
            {
                m_TunablesBackup.clear();
                m_NumTunables.store(0, std::memory_order_release);
                return false;
            }

            m_Caching.store(true, std::memory_order_release);
            TUTONES_LOG_INFO(
                "game.tunables",
                std::string("Starting tunable registration pass across ")
                    + std::to_string(numTunables)
                    + " Global_262145 slots");
            return true;
        }

        [[nodiscard]] bool PatchRegistrationNatives(Types::ScriptProgram* program) noexcept
        {
            if (!program || !program->nativeEntrypoints || program->nativeCount == 0)
                return false;

            m_PatchedProgram = program;
            m_WaitSlot = {};
            m_IntSlot = {};
            m_BoolSlot = {};
            m_FloatSlot = {};

            const auto patch = [program](
                Native::NativeHandler target,
                Native::NativeHandler replacement,
                PatchedSlot& slot) noexcept {
                if (!target || !replacement)
                    return false;
                for (std::size_t index = 0; index < program->nativeCount; ++index)
                {
                    if (program->nativeEntrypoints[index] != target)
                        continue;
                    slot.index = index;
                    slot.original = program->nativeEntrypoints[index];
                    program->nativeEntrypoints[index] = replacement;
                    return true;
                }
                return false;
            };

            const bool ok = patch(
                    m_RegistrarHandlers[static_cast<std::size_t>(RegistrarNative::Wait)],
                    &WaitHook,
                    m_WaitSlot)
                && patch(
                    m_RegistrarHandlers[static_cast<std::size_t>(RegistrarNative::RegistrationInt)],
                    &RegistrationIntHook,
                    m_IntSlot)
                && patch(
                    m_RegistrarHandlers[static_cast<std::size_t>(RegistrarNative::RegistrationBool)],
                    &RegistrationBoolHook,
                    m_BoolSlot)
                && patch(
                    m_RegistrarHandlers[static_cast<std::size_t>(RegistrarNative::RegistrationFloat)],
                    &RegistrationFloatHook,
                    m_FloatSlot);

            if (!ok)
            {
                RestoreRegistrationNatives();
                TUTONES_LOG_WARN("game.tunables", "tuneables_processing did not expose all required registration native slots");
                return false;
            }
            return true;
        }

        void RestoreRegistrationNatives() noexcept
        {
            if (!m_PatchedProgram)
                return;

            const auto restore = [this](const PatchedSlot& slot) noexcept {
                if (slot.index == InvalidSlot || !slot.original || !m_PatchedProgram
                    || slot.index >= m_PatchedProgram->nativeCount || !m_PatchedProgram->nativeEntrypoints)
                {
                    return;
                }
                m_PatchedProgram->nativeEntrypoints[slot.index] = slot.original;
            };

            restore(m_WaitSlot);
            restore(m_IntSlot);
            restore(m_BoolSlot);
            restore(m_FloatSlot);
            m_PatchedProgram = nullptr;
            m_WaitSlot = {};
            m_IntSlot = {};
            m_BoolSlot = {};
            m_FloatSlot = {};
        }

        void FinalizeRegistrationPass(std::int64_t** globals) noexcept
        {
            const auto numTunables = m_NumTunables.load(std::memory_order_acquire);
            auto* base = globals ? Script::ScriptGlobal(BaseGlobal).As<std::int64_t>(globals) : nullptr;
            if (!base || numTunables == 0 || m_TunablesBackup.size() != numTunables)
            {
                SetMessage("Tunable registration finalization lost the global block");
                CleanupRegistrationPass(false);
                RequeueOrLog();
                return;
            }

            std::unordered_map<int, CapturedRegistration> junkValues;
            {
                std::scoped_lock lock(m_CacheMutex);
                junkValues = m_JunkValues;
            }

            std::unordered_map<std::uint32_t, Entry> registered;
            registered.reserve(junkValues.size());
            for (std::size_t offset = 0; offset < numTunables; ++offset)
            {
                int value{};
                std::memcpy(&value, &base[offset], sizeof(value));
                const auto found = junkValues.find(value);
                if (found == junkValues.end())
                    continue;

                Entry entry{};
                entry.globalIndex = BaseGlobal + offset;
                entry.type = found->second.type;
                entry.originalRaw = m_TunablesBackup[offset];
                registered.emplace(found->second.hash, std::move(entry));
            }

            std::copy(m_TunablesBackup.begin(), m_TunablesBackup.end(), base);
            m_Caching.store(false, std::memory_order_release);
            RestoreRegistrationNatives();
            static_cast<void>(InvokeVoid(RegistrarNative::SetScriptWithNameHashAsNoLongerNeeded, TuneablesProcessingHash));
            static_cast<void>(InvokeVoid(RegistrarNative::SetScriptWithNameHashAsNoLongerNeeded, TunablesRegistrationHash));

            {
                std::scoped_lock lock(m_Mutex);
                m_Entries = std::move(registered);
                m_Message = m_Entries.empty()
                    ? "Tunable registration produced no mappings"
                    : std::string("Ready - ") + std::to_string(m_Entries.size()) + " Rockstar tunables registered";
            }

            const auto count = Snapshot().registeredCount;
            m_TunablesBackup.clear();
            {
                std::scoped_lock lock(m_CacheMutex);
                m_JunkValues.clear();
                m_CurrentJunkValue = FirstJunkValue;
            }

            if (count == 0)
            {
                m_Initialized.store(false, std::memory_order_release);
                m_Phase.store(static_cast<std::uint8_t>(Phase::Waiting), std::memory_order_release);
                m_NumTunables.store(0, std::memory_order_release);
                TUTONES_LOG_WARN("game.tunables", "tuneables_processing finished but no hash-to-global mappings were captured");
                RequeueOrLog();
                return;
            }

            m_Initialized.store(true, std::memory_order_release);
            m_Phase.store(static_cast<std::uint8_t>(Phase::Complete), std::memory_order_release);
            m_Revision.fetch_add(1, std::memory_order_acq_rel);
            TUTONES_LOG_INFO(
                "game.tunables",
                std::string("Tunable registry initialized with ")
                    + std::to_string(count)
                    + " Rockstar hash-to-global mappings");
        }

        void CleanupRegistrationPass(bool releaseScripts) noexcept
        {
            m_Caching.store(false, std::memory_order_release);

            auto** globals = Script::ScriptRuntime::Get().Globals();
            const auto numTunables = m_NumTunables.load(std::memory_order_acquire);
            if (globals && !m_TunablesBackup.empty() && m_TunablesBackup.size() == numTunables)
            {
                if (auto* base = Script::ScriptGlobal(BaseGlobal).As<std::int64_t>(globals))
                    std::copy(m_TunablesBackup.begin(), m_TunablesBackup.end(), base);
            }

            RestoreRegistrationNatives();
            if (releaseScripts && m_NativesReady.load(std::memory_order_acquire))
            {
                static_cast<void>(InvokeVoid(RegistrarNative::SetScriptWithNameHashAsNoLongerNeeded, TuneablesProcessingHash));
                static_cast<void>(InvokeVoid(RegistrarNative::SetScriptWithNameHashAsNoLongerNeeded, TunablesRegistrationHash));
            }

            m_TunablesBackup.clear();
            m_NumTunables.store(0, std::memory_order_release);
            {
                std::scoped_lock lock(m_CacheMutex);
                m_JunkValues.clear();
                m_CurrentJunkValue = FirstJunkValue;
            }
            if (IsRunning())
                m_Phase.store(static_cast<std::uint8_t>(Phase::Waiting), std::memory_order_release);
        }

        void ResetTransientState() noexcept
        {
            m_RegistrarHandlers.fill(nullptr);
            m_PatchedProgram = nullptr;
            m_WaitSlot = {};
            m_IntSlot = {};
            m_BoolSlot = {};
            m_FloatSlot = {};
            m_NumTunables.store(0, std::memory_order_release);
            m_TunablesBackup.clear();
            std::scoped_lock lock(m_CacheMutex);
            m_JunkValues.clear();
            m_CurrentJunkValue = FirstJunkValue;
        }

        static void WaitHook(Native::NativeCallContext* context) noexcept
        {
            auto& self = Get();
            if (self.m_Caching.load(std::memory_order_acquire))
                return;
            if (self.m_WaitSlot.original)
                self.m_WaitSlot.original(context);
        }

        static void RegistrationIntHook(Native::NativeCallContext* context) noexcept
        {
            auto& self = Get();
            self.CaptureRegistration(context, self.m_IntSlot.original, TunableValueType::Int);
        }

        static void RegistrationBoolHook(Native::NativeCallContext* context) noexcept
        {
            auto& self = Get();
            self.CaptureRegistration(context, self.m_BoolSlot.original, TunableValueType::Bool);
        }

        static void RegistrationFloatHook(Native::NativeCallContext* context) noexcept
        {
            auto& self = Get();
            self.CaptureRegistration(context, self.m_FloatSlot.original, TunableValueType::Float);
        }

        void CaptureRegistration(
            Native::NativeCallContext* context,
            Native::NativeHandler original,
            TunableValueType type) noexcept
        {
            if (!context)
                return;
            if (!m_Caching.load(std::memory_order_acquire))
            {
                if (original)
                    original(context);
                return;
            }

            const auto hash = context->GetArg<std::uint32_t>(0);
            int junk{};
            {
                std::scoped_lock lock(m_CacheMutex);
                junk = m_CurrentJunkValue++;
                m_JunkValues.emplace(junk, CapturedRegistration{hash, type});
            }

            // Write sentinel bits directly for INT/BOOL/FLOAT alike. The registration
            // script stores those bits in the tunable global, letting us recover the exact
            // hash-to-slot relationship after the script exits.
            context->SetReturnValue<int>(junk);
        }

        std::atomic<bool> m_Running{false};
        std::atomic<bool> m_Initialized{false};
        std::atomic<bool> m_GlobalsReady{false};
        std::atomic<bool> m_Caching{false};
        std::atomic<bool> m_TickQueued{false};
        std::atomic<bool> m_NativesReady{false};
        std::atomic<std::uint8_t> m_Phase{static_cast<std::uint8_t>(Phase::Waiting)};
        std::atomic<std::size_t> m_NumTunables{0};
        std::atomic<std::uint64_t> m_Revision{0};

        std::array<Native::NativeHandler, static_cast<std::size_t>(RegistrarNative::Count)> m_RegistrarHandlers{};
        Types::ScriptProgram* m_PatchedProgram{};
        PatchedSlot m_WaitSlot{};
        PatchedSlot m_IntSlot{};
        PatchedSlot m_BoolSlot{};
        PatchedSlot m_FloatSlot{};
        std::vector<std::int64_t> m_TunablesBackup;

        mutable std::mutex m_CacheMutex;
        std::unordered_map<int, CapturedRegistration> m_JunkValues;
        int m_CurrentJunkValue{FirstJunkValue};

        mutable std::mutex m_Mutex;
        std::unordered_map<std::uint32_t, Entry> m_Entries;
        std::string m_Message{"Stopped"};
    };

    class Tunable final
    {
    public:
        constexpr explicit Tunable(std::uint32_t hash) noexcept
            : m_Hash(hash)
        {
        }

        constexpr explicit Tunable(std::string_view name) noexcept
            : m_Hash(Joaat(name))
        {
        }

        [[nodiscard]] bool IsReady() const noexcept
        {
            const auto global = TunableRegistry::Get().Resolve(m_Hash);
            auto** globals = Script::ScriptRuntime::Get().Globals();
            return global.has_value() && globals && global->As<std::int64_t>(globals) != nullptr;
        }

        template<typename T>
        [[nodiscard]] std::optional<T> Get() const noexcept
        {
            const auto global = TunableRegistry::Get().Resolve(m_Hash);
            auto** globals = Script::ScriptRuntime::Get().Globals();
            if (!global || !globals)
                return std::nullopt;
            const T* value = global->As<T>(globals);
            if (!value)
                return std::nullopt;
            return *value;
        }

        template<typename T>
        bool Set(T value) const noexcept
        {
            const auto global = TunableRegistry::Get().Resolve(m_Hash);
            auto** globals = Script::ScriptRuntime::Get().Globals();
            if (!global || !globals)
                return false;
            T* target = global->As<T>(globals);
            if (!target)
                return false;
            *target = value;
            return *target == value;
        }

        [[nodiscard]] constexpr std::uint32_t Hash() const noexcept
        {
            return m_Hash;
        }

    private:
        std::uint32_t m_Hash{};
    };
}
