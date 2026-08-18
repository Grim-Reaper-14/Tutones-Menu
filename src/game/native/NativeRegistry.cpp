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
            {0xD80958FC74E988A6ull, "PLAYER_PED_ID"},
            {0x7239B21A38F536BAull, "DOES_ENTITY_EXIST"},
            {0x9F47B058362C84B5ull, "GET_ENTITY_MODEL"},
            {0x997ABD671D25CA0Bull, "IS_PED_IN_ANY_VEHICLE"},
            {0x9A9112A0FE9A4713ull, "GET_VEHICLE_PED_IS_IN"},

            {0xA19435F193E081ACull, "GET_VEHICLE_COLOURS"},
            {0x4F1D4BE3A7F24601ull, "SET_VEHICLE_COLOURS"},
            {0x3BC4245933A166F7ull, "GET_VEHICLE_EXTRA_COLOURS"},
            {0x2036F561ADD12E33ull, "SET_VEHICLE_EXTRA_COLOURS"},
            {0xE8D65CA700C9A693ull, "GET_VEHICLE_MOD_COLOR_1"},
            {0x43FEB945EE7F85B8ull, "SET_VEHICLE_MOD_COLOR_1"},
            {0x81592BE4E3878728ull, "GET_VEHICLE_MOD_COLOR_2"},
            {0x816562BADFDEC83Eull, "SET_VEHICLE_MOD_COLOR_2"},
            {0xF095C0405307B21Bull, "GET_IS_VEHICLE_PRIMARY_COLOUR_CUSTOM"},
            {0x910A32E7AAD2656Cull, "GET_IS_VEHICLE_SECONDARY_COLOUR_CUSTOM"},
            {0xB64CF2CCA9D95F52ull, "GET_VEHICLE_CUSTOM_PRIMARY_COLOUR"},
            {0x8389CD56CA8072DCull, "GET_VEHICLE_CUSTOM_SECONDARY_COLOUR"},
            {0x7141766F91D15BEAull, "SET_VEHICLE_CUSTOM_PRIMARY_COLOUR"},
            {0x36CED73BFED89754ull, "SET_VEHICLE_CUSTOM_SECONDARY_COLOUR"},
            {0x55E1D2758F34E437ull, "CLEAR_VEHICLE_CUSTOM_PRIMARY_COLOUR"},
            {0x5FFBDEEC3E8E2009ull, "CLEAR_VEHICLE_CUSTOM_SECONDARY_COLOUR"},

            {0x1F2AA07F00B3217Aull, "SET_VEHICLE_MOD_KIT"},
            {0xB3ED1BFB4BE636DCull, "GET_VEHICLE_WHEEL_TYPE"},
            {0x487EB21CC7295BA1ull, "SET_VEHICLE_WHEEL_TYPE"},
            {0x6AF0636DDEDCB6DDull, "SET_VEHICLE_MOD"},
            {0x772960298DA26FDBull, "GET_VEHICLE_MOD"},
            {0xB3924ECD70E095DCull, "GET_VEHICLE_MOD_VARIATION"},
            {0xE38E9162A2500646ull, "GET_NUM_VEHICLE_MODS"},
            {0x92D619E420858204ull, "REMOVE_VEHICLE_MOD"},
            {0x2A1F4F37F95BAD08ull, "TOGGLE_VEHICLE_MOD"},
            {0x84B233A8C8FC8AE7ull, "IS_TOGGLE_MOD_ON"},
            {0x115722B1B9C14C1Cull, "SET_VEHICLE_FIXED"},
            {0x79D3B596FE44EE8Bull, "SET_VEHICLE_DIRT_LEVEL"},
            {0x49733E92263139D1ull, "SET_VEHICLE_ON_GROUND_PROPERLY"},

            {0x4F8644AF03D0E0D6ull, "PLAYER_ID"},
            {0xEEF059FAD016D209ull, "GET_ENTITY_HEALTH"},
            {0x15D757606D170C3Cull, "GET_ENTITY_MAX_HEALTH"},
            {0x6B76DC1F3AE6E6A3ull, "SET_ENTITY_HEALTH"},
            {0x3882114BDE571AD4ull, "SET_ENTITY_INVINCIBLE"},
            {0xEA1C610A04DB6BBBull, "SET_ENTITY_VISIBLE"},
            {0x9483AF821605B1D8ull, "GET_PED_ARMOUR"},
            {0xCEA04D83135264CCull, "SET_PED_ARMOUR"},
            {0xB128377056A54E2Aull, "SET_PED_CAN_RAGDOLL"},
            {0xE28E54788CE8F12Dull, "GET_PLAYER_WANTED_LEVEL"},
            {0x39FF19C64EF7DA5Bull, "SET_PLAYER_WANTED_LEVEL"},
            {0xE0A7D1E497FFCD6Full, "SET_PLAYER_WANTED_LEVEL_NOW"},
            {0xB302540597885499ull, "CLEAR_PLAYER_WANTED_LEVEL"},
            {0x32C62AA929C2DA6Aull, "SET_POLICE_IGNORE_PLAYER"},
            {0x8EEDA153AD141BA4ull, "SET_EVERYONE_IGNORE_PLAYER"},
            {0x57FFF03E423A4C0Bull, "SET_SUPER_JUMP_THIS_FRAME"},
            {0x6DB47AA77FD94E09ull, "SET_RUN_SPRINT_MULTIPLIER_FOR_PLAYER"},
            {0xA91C6F0FF7D16A13ull, "SET_SWIM_MULTIPLIER_FOR_PLAYER"},
            {0xA352C1B864CAFD33ull, "RESTORE_PLAYER_STAMINA"},
            {0x35B9E0803292B641ull, "IS_MODEL_IN_CDIMAGE"},
            {0xC0296A2EDF545E92ull, "IS_MODEL_VALID"},
            {0x75816577FEA6DAD5ull, "IS_MODEL_A_PED"},
            {0x963D27A58DF860ACull, "REQUEST_MODEL"},
            {0x98A4EB5D89A0C952ull, "HAS_MODEL_LOADED"},
            {0xE532F5D78798DAABull, "SET_MODEL_AS_NO_LONGER_NEEDED"},
            {0x00A1CADD00108836ull, "SET_PLAYER_MODEL"},
            {0x67F3780DD425D4FCull, "GET_PED_DRAWABLE_VARIATION"},
            {0x27561561732A7842ull, "GET_NUMBER_OF_PED_DRAWABLE_VARIATIONS"},
            {0x04A355E041E004E6ull, "GET_PED_TEXTURE_VARIATION"},
            {0x8F7156A3142A6BADull, "GET_NUMBER_OF_PED_TEXTURE_VARIATIONS"},
            {0xE3DD5F2A84B42281ull, "GET_PED_PALETTE_VARIATION"},
            {0x262B14F48D29DE80ull, "SET_PED_COMPONENT_VARIATION"},
            {0xC8A9481A01E63C28ull, "SET_PED_RANDOM_COMPONENT_VARIATION"},
            {0x45EEE61580806D63ull, "SET_PED_DEFAULT_COMPONENT_VARIATION"},
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

        TUTONES_LOG_INFO("game.native", "Initializing focused GTA Enhanced native handler table");

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
        TUTONES_LOG_INFO("game.native", "GTA Enhanced native handlers cached successfully");
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
