#include "CapabilityRegistry.hpp"

#include "../core/CoreServices.hpp"
#include "../game/native/NativeRegistry.hpp"
#include "../game/script/ScriptRuntime.hpp"
#include "../runtime/GameRuntime.hpp"

#include <cstdint>

namespace Tutones::Backend
{
    namespace
    {
        constexpr std::uint32_t Joaat(const char* text) noexcept
        {
            std::uint32_t hash{};
            while (text && *text)
            {
                char c = *text++;
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

        constexpr std::uint32_t FreemodeHash = Joaat("freemode");
        constexpr std::uint32_t VehicleRewardHash = Joaat("am_mp_vehicle_reward");
    }

    void CapabilityRegistry::Refresh() noexcept
    {
        const bool coreReady = Core::Services::Get().IsInitialized();
        const bool gameReady = Runtime::GameRuntime::Get().IsInitialized();
        const bool nativeReady = Game::Native::NativeRegistry::Get().IsReady();

        auto& scripts = Game::Script::ScriptRuntime::Get();
        const bool scriptReady = scripts.IsReady();
        const bool globalsReady = scriptReady && scripts.Globals() != nullptr;
        const bool vmReady = scriptReady && scripts.ScriptVm() != nullptr;
        const bool freemodeReady = scriptReady && scripts.FindThread(FreemodeHash) != nullptr;
        const bool rewardReady = scriptReady && scripts.FindThread(VehicleRewardHash) != nullptr;

        Set(Capability::CoreServices, coreReady,
            coreReady ? "Core services initialized" : "Core services offline");
        Set(Capability::GameRuntime, gameReady,
            gameReady ? "GTA scheduler hook active" : "GTA scheduler hook offline");
        Set(Capability::NativeRuntime, nativeReady,
            nativeReady ? "Native handler table ready" : "Native handler table not ready yet");
        Set(Capability::ScriptRuntime, scriptReady,
            scriptReady ? "Script thread/program tables available" : "Script runtime incomplete");
        Set(Capability::ScriptGlobals, globalsReady,
            globalsReady ? "Script globals table available" : "Script globals unavailable");
        Set(Capability::ScriptVm, vmReady,
            vmReady ? "Script VM available" : "Script VM unavailable");
        Set(Capability::FreemodeScript, freemodeReady,
            freemodeReady ? "freemode thread active" : "freemode thread not active");
        Set(Capability::OnlineSession, freemodeReady,
            freemodeReady ? "Online session inferred from active freemode thread" : "Online session not established");
        Set(Capability::VehicleRewardScript, rewardReady,
            rewardReady ? "am_mp_vehicle_reward thread active" : "am_mp_vehicle_reward thread not active");

        std::scoped_lock lock(m_Mutex);
        ++m_Snapshot.revision;
    }

    void CapabilityRegistry::Reset() noexcept
    {
        std::scoped_lock lock(m_Mutex);
        m_Snapshot = {};
    }

    bool CapabilityRegistry::Has(Capability capability) const noexcept
    {
        std::scoped_lock lock(m_Mutex);
        return m_Snapshot.Has(capability);
    }

    bool CapabilityRegistry::HasAll(const std::vector<Capability>& capabilities) const noexcept
    {
        std::scoped_lock lock(m_Mutex);
        for (const auto capability : capabilities)
        {
            if (!m_Snapshot.Has(capability))
                return false;
        }
        return true;
    }

    std::vector<Capability> CapabilityRegistry::Missing(const std::vector<Capability>& capabilities) const
    {
        std::vector<Capability> missing;
        std::scoped_lock lock(m_Mutex);
        for (const auto capability : capabilities)
        {
            if (!m_Snapshot.Has(capability))
                missing.push_back(capability);
        }
        return missing;
    }

    CapabilitySnapshot CapabilityRegistry::Snapshot() const
    {
        std::scoped_lock lock(m_Mutex);
        return m_Snapshot;
    }

    void CapabilityRegistry::Set(Capability capability, bool available, std::string detail)
    {
        const auto index = static_cast<std::size_t>(capability);
        std::scoped_lock lock(m_Mutex);
        if (index >= m_Snapshot.available.size())
            return;
        m_Snapshot.available[index] = available;
        m_Snapshot.detail[index] = std::move(detail);
    }
}
