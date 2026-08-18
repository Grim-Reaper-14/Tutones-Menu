#include "NativeRegistry.hpp"

#include "../../core/logging/Logger.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string>

namespace Tutones::Game::Native
{
    namespace
    {
        struct NativeDescriptor final
        {
            NativeHash hash;
            const char* name;
        };

        constexpr std::array<NativeDescriptor, static_cast<std::size_t>(NativeId::Count)> Descriptors{{
            {0x4A8C381C258A124Dull, "PLAYER_PED_ID"},
            {0xFC8BFE4B41177C22ull, "DOES_ENTITY_EXIST"},
            {0x4B423FAA24E8ABF0ull, "GET_ENTITY_MODEL"},
            {0xCFC0C995455A6204ull, "GET_ENTITY_HEADING"},
            {0xD1A6A821F5AC81DBull, "GET_ENTITY_COORDS"},
            {0x7F420695E3F776FBull, "IS_PED_IN_ANY_VEHICLE"},
            {0x6EF03BE64E058E2Full, "GET_VEHICLE_PED_IS_IN"},
            {0xCDA725BC2F170795ull, "GET_VEHICLE_PED_IS_USING"},

            {0xFF4B16F297D9CB3Eull, "GET_VEHICLE_COLOURS"},
            {0xD133EF7430EDCD09ull, "SET_VEHICLE_COLOURS"},
            {0x741D9B0685E67684ull, "GET_VEHICLE_EXTRA_COLOURS"},
            {0xBB361D7264AC4FD8ull, "SET_VEHICLE_EXTRA_COLOURS"},
            {0xB8090FC59766A88Cull, "GET_VEHICLE_MOD_COLOR_1"},
            {0xA5277ECCD081FCC1ull, "SET_VEHICLE_MOD_COLOR_1"},
            {0x07AE5F5D5A7D0936ull, "GET_VEHICLE_MOD_COLOR_2"},
            {0x941B1F179D6AE19Aull, "SET_VEHICLE_MOD_COLOR_2"},
            {0xA9D64A14804D119Bull, "GET_IS_VEHICLE_PRIMARY_COLOUR_CUSTOM"},
            {0x2C0B2BB7913E8DBAull, "GET_IS_VEHICLE_SECONDARY_COLOUR_CUSTOM"},
            {0xD9B9D4D1CCED7CA6ull, "GET_VEHICLE_CUSTOM_PRIMARY_COLOUR"},
            {0x04434FA56DED5500ull, "GET_VEHICLE_CUSTOM_SECONDARY_COLOUR"},
            {0x84F5FD9CD27457EEull, "SET_VEHICLE_CUSTOM_PRIMARY_COLOUR"},
            {0x593A3115B8AE759Bull, "SET_VEHICLE_CUSTOM_SECONDARY_COLOUR"},
            {0x963D9A7202C06F65ull, "CLEAR_VEHICLE_CUSTOM_PRIMARY_COLOUR"},
            {0x588D8FDC61F7CFADull, "CLEAR_VEHICLE_CUSTOM_SECONDARY_COLOUR"},

            {0xB5AD06DDA85E2E8Full, "SET_VEHICLE_MOD_KIT"},
            {0x6A375D21624F9187ull, "GET_VEHICLE_WHEEL_TYPE"},
            {0xE33678A9AE50A01Bull, "SET_VEHICLE_WHEEL_TYPE"},
            {0x8450270DC5896D39ull, "SET_VEHICLE_MOD"},
            {0x94C9CD3D66808551ull, "GET_VEHICLE_MOD"},
            {0xEFDD8C5443F6C9E4ull, "GET_VEHICLE_MOD_VARIATION"},
            {0x5B59C12A02157D00ull, "GET_NUM_VEHICLE_MODS"},
            {0xC87E4FAD00AEDD4Bull, "REMOVE_VEHICLE_MOD"},
            {0xF5501FF9869DAC7Cull, "TOGGLE_VEHICLE_MOD"},
            {0x1D5A665629D417A7ull, "IS_TOGGLE_MOD_ON"},
            {0xF698038C13845696ull, "SET_VEHICLE_FIXED"},
            {0x9452FE4900245259ull, "SET_VEHICLE_DIRT_LEVEL"},
            {0x1DE99C193C7EC64Bull, "SET_VEHICLE_ON_GROUND_PROPERLY"},
            {0xE074F21A4084FD1Full, "GET_VEHICLE_CLASS_FROM_NAME"},
            {0x93E7527CFECC7CD8ull, "GET_DISPLAY_NAME_FROM_VEHICLE_MODEL"},
            {0xF7AF4F159FF99F97ull, "GET_MAKE_NAME_FROM_VEHICLE_MODEL"},
            {0xF0CA45A211FFDCD9ull, "GET_CLOSEST_VEHICLE"},
            {0x1340575A0EEE0622ull, "GET_MOD_TEXT_LABEL"},
            {0xFACCDE46E24AD056ull, "GET_FILENAME_FOR_AUDIO_CONVERSATION"},
            {0x9D35AABAEE206518ull, "GET_VEHICLE_TYRE_SMOKE_COLOR"},
            {0x5DA0536AEAD1FF31ull, "SET_VEHICLE_TYRE_SMOKE_COLOR"},
            {0xD6BA8C57BDF9DEB9ull, "GET_VEHICLE_XENON_LIGHT_COLOR_INDEX"},
            {0x89D1FDCA3735A1E0ull, "SET_VEHICLE_XENON_LIGHT_COLOR_INDEX"},
            {0xF1B79038130E3C08ull, "GET_VEHICLE_NEON_ENABLED"},
            {0xE62930EC6FAABCA5ull, "SET_VEHICLE_NEON_ENABLED"},
            {0x64FEACF0AD019F1Full, "GET_VEHICLE_NEON_COLOUR"},
            {0xEAB8A43F6621850Full, "SET_VEHICLE_NEON_COLOUR"},
            {0xE6BE8A525BA6BD44ull, "GET_VEHICLE_TYRES_CAN_BURST"},
            {0x439C904840715871ull, "SET_VEHICLE_TYRES_CAN_BURST"},
            {0x4497678941C27E46ull, "GET_DRIFT_TYRES_SET"},
            {0x519F76A38952BBD0ull, "SET_DRIFT_TYRES"},

            {0x259BE71D8A81D4FAull, "PLAYER_ID"},
            {0x8D91ADE44AC79BC9ull, "GET_ENTITY_HEALTH"},
            {0xF8A78594664D23A6ull, "GET_ENTITY_MAX_HEALTH"},
            {0xD25E9BDC14A0B649ull, "SET_ENTITY_HEALTH"},
            {0x935364B4448CD584ull, "SET_ENTITY_INVINCIBLE"},
            {0x4285E11B28063EE0ull, "SET_ENTITY_VISIBLE"},
            {0xE5E6F6EFCE07789Aull, "GET_PED_ARMOUR"},
            {0x10A676E622A468AAull, "SET_PED_ARMOUR"},
            {0x9FF00EA9A61211D2ull, "SET_PED_CAN_RAGDOLL"},
            {0xE7B45027762DEFE7ull, "GET_PLAYER_WANTED_LEVEL"},
            {0xE20A252886E4FE1Dull, "SET_PLAYER_WANTED_LEVEL"},
            {0x42C9A22D6724F283ull, "SET_PLAYER_WANTED_LEVEL_NOW"},
            {0x3C482AC51A8E85DCull, "CLEAR_PLAYER_WANTED_LEVEL"},
            {0xDAA51A56DBEC0391ull, "SET_POLICE_IGNORE_PLAYER"},
            {0x3AFFD31224BF9207ull, "SET_EVERYONE_IGNORE_PLAYER"},
            {0x353BF8D85390AA39ull, "SET_SUPER_JUMP_THIS_FRAME"},
            {0xA52E1AE3848A506Bull, "SET_RUN_SPRINT_MULTIPLIER_FOR_PLAYER"},
            {0x289497A4BA9049E0ull, "SET_SWIM_MULTIPLIER_FOR_PLAYER"},
            {0x92EBF838856DCF63ull, "RESTORE_PLAYER_STAMINA"},
            {0xE7D342E0F16AAA8Full, "IS_MODEL_IN_CDIMAGE"},
            {0x441B9C85D0FFA9EDull, "IS_MODEL_VALID"},
            {0xBA4223DE7F0708BAull, "IS_MODEL_A_PED"},
            {0xAD1840C2E6AF7D5Eull, "IS_MODEL_A_VEHICLE"},
            {0xEC9DAA34BBB4658Cull, "REQUEST_MODEL"},
            {0x6252BC0DD8A320DBull, "HAS_MODEL_LOADED"},
            {0x55098D9E9AD58806ull, "SET_MODEL_AS_NO_LONGER_NEEDED"},
            {0x5779387E956077A6ull, "CREATE_VEHICLE"},
            {0x73CAFD2038E812B3ull, "SET_PED_INTO_VEHICLE"},
            {0x52E0301351FCDEC5ull, "SET_PLAYER_MODEL"},
            {0xC0120BBCC298EA2Full, "GET_PED_DRAWABLE_VARIATION"},
            {0x1A4EFE92822E3123ull, "GET_NUMBER_OF_PED_DRAWABLE_VARIATIONS"},
            {0xD6AED6BFCC58AF7Full, "GET_PED_TEXTURE_VARIATION"},
            {0x8401C77F508D70FDull, "GET_NUMBER_OF_PED_TEXTURE_VARIATIONS"},
            {0xDAF263B0E792EAECull, "GET_PED_PALETTE_VARIATION"},
            {0xD1C578C204015E1Full, "SET_PED_COMPONENT_VARIATION"},
            {0xC6E8E1D693021E9Eull, "SET_PED_RANDOM_COMPONENT_VARIATION"},
            {0x77EFA99E6A8FFC43ull, "SET_PED_DEFAULT_COMPONENT_VARIATION"},
        }};

        struct NativeProgram final
        {
            std::byte pad00[0x2C]{};
            std::uint32_t nativeCount{};
            std::byte pad30[0x10]{};
            NativeHandler* nativeEntrypoints{};
            std::byte pad48[0x38]{};
        };

        static_assert(offsetof(NativeProgram, nativeCount) == 0x2C);
        static_assert(offsetof(NativeProgram, nativeEntrypoints) == 0x40);
        static_assert(sizeof(NativeProgram) == 0x80);

        bool IsExecutableAddress(std::uintptr_t address) noexcept
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
    }

    NativeRegistry& NativeRegistry::Get() noexcept
    {
        static NativeRegistry instance;
        return instance;
    }

    bool NativeRegistry::Initialize(InitNativeTablesFn initNativeTables) noexcept
    {
        if (m_Ready.load(std::memory_order_acquire))
            return true;

        if (!initNativeTables)
        {
            TUTONES_LOG_ERROR("game.native", "Native registry received a null InitNativeTables pointer");
            return false;
        }

        if (!CanInvokeOnCurrentThread())
        {
            TUTONES_LOG_ERROR("game.native", "Native table initialization attempted outside the GTA script thread");
            return false;
        }

        TUTONES_LOG_INFO("game.native", "Initializing focused GTA Enhanced native handler table with YimMenuV2 crossmapped hashes");
        Core::Logging::Logger::Get().Flush();

        std::array<std::uint64_t, Descriptors.size()> slots{};
        for (std::size_t i = 0; i < Descriptors.size(); ++i)
            slots[i] = Descriptors[i].hash;

        NativeProgram program{};
        program.nativeCount = static_cast<std::uint32_t>(slots.size());
        program.nativeEntrypoints = reinterpret_cast<NativeHandler*>(slots.data());

        initNativeTables(&program);

        for (std::size_t i = 0; i < slots.size(); ++i)
        {
            const auto address = static_cast<std::uintptr_t>(slots[i]);
            if (!IsExecutableAddress(address))
            {
                std::string message("Native handler resolution failed for ");
                message += Descriptors[i].name;
                TUTONES_LOG_ERROR("game.native", message);
                Shutdown();
                return false;
            }
            m_Handlers[i] = reinterpret_cast<NativeHandler>(address);
        }

        m_Ready.store(true, std::memory_order_release);
        TUTONES_LOG_INFO("game.native", "GTA Enhanced crossmapped native handlers cached successfully");
        Core::Logging::Logger::Get().Flush();
        return true;
    }

    void NativeRegistry::Shutdown() noexcept
    {
        m_Ready.store(false, std::memory_order_release);
        m_Handlers.fill(nullptr);
        m_GameThreadId.store(0, std::memory_order_release);
    }

    void NativeRegistry::MarkGameThread(DWORD threadId) noexcept
    {
        const auto previous = m_GameThreadId.exchange(threadId, std::memory_order_acq_rel);
        if (previous == 0 && threadId != 0)
            TUTONES_LOG_INFO("game.native", "GTA script thread identified for native execution");
    }

    bool NativeRegistry::IsReady() const noexcept
    {
        return m_Ready.load(std::memory_order_acquire);
    }

    bool NativeRegistry::CanInvokeOnCurrentThread() const noexcept
    {
        const auto gameThread = m_GameThreadId.load(std::memory_order_acquire);
        return gameThread != 0 && gameThread == ::GetCurrentThreadId();
    }

    NativeHandler NativeRegistry::Handler(NativeId id) const noexcept
    {
        const auto index = static_cast<std::size_t>(id);
        return index < m_Handlers.size() ? m_Handlers[index] : nullptr;
    }

    const char* NativeRegistry::Name(NativeId id) const noexcept
    {
        const auto index = static_cast<std::size_t>(id);
        return index < Descriptors.size() ? Descriptors[index].name : "UNKNOWN_NATIVE";
    }

    NativeHash NativeRegistry::Hash(NativeId id) const noexcept
    {
        const auto index = static_cast<std::size_t>(id);
        return index < Descriptors.size() ? Descriptors[index].hash : 0;
    }
}
