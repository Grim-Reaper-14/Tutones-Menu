#pragma once

#include "../../game/GamePointers.hpp"
#include "../../game/native/NativeCallContext.hpp"
#include "../../game/native/NativeRegistry.hpp"
#include "../../runtime/GameRuntime.hpp"

#include <algorithm>
#include <array>
#include <atomic>
#include <cstddef>
#include <cstdint>
#include <mutex>
#include <span>
#include <vector>

namespace Tutones::Game::Recovery
{
    struct ClothingUnlockRange final
    {
        int first{};
        int last{};
    };

    struct ClothingUnlockGroup final
    {
        const char* name{};
        const char* packedFamily{};
        std::span<const ClothingUnlockRange> ranges{};
    };

    namespace ClothingUnlockData
    {
        inline constexpr auto Legacy = std::to_array<ClothingUnlockRange>({
            ClothingUnlockRange{110, 113}, ClothingUnlockRange{3483, 3492},
            ClothingUnlockRange{3496, 3505}, ClothingUnlockRange{3593, 3599},
            ClothingUnlockRange{3608, 3609}, ClothingUnlockRange{3616, 3616},
            ClothingUnlockRange{3750, 3750}, ClothingUnlockRange{3770, 3781},
            ClothingUnlockRange{3783, 3802}, ClothingUnlockRange{4247, 4269},
            ClothingUnlockRange{4333, 4335}, ClothingUnlockRange{6082, 6083},
            ClothingUnlockRange{6091, 6092}, ClothingUnlockRange{6097, 6097},
            ClothingUnlockRange{6106, 6106}, ClothingUnlockRange{6169, 6169},
            ClothingUnlockRange{6181, 6181}, ClothingUnlockRange{6303, 6304},
            ClothingUnlockRange{6316, 6317},
        });
        inline constexpr auto Executives = std::to_array<ClothingUnlockRange>({
            ClothingUnlockRange{7467, 7495}, ClothingUnlockRange{7515, 7528},
            ClothingUnlockRange{7551, 7551}, ClothingUnlockRange{7595, 7601},
        });
        inline constexpr auto Bikers = std::to_array<ClothingUnlockRange>({
            ClothingUnlockRange{9362, 9385}, ClothingUnlockRange{9426, 9440},
            ClothingUnlockRange{9443, 9443}, ClothingUnlockRange{9462, 9481},
        });
        inline constexpr auto Gunrunning = std::to_array<ClothingUnlockRange>({
            ClothingUnlockRange{15388, 15423}, ClothingUnlockRange{15425, 15427},
        });
        inline constexpr auto Doomsday = std::to_array<ClothingUnlockRange>({
            ClothingUnlockRange{18121, 18125}, ClothingUnlockRange{18134, 18137},
        });
        inline constexpr auto AfterHours = std::to_array<ClothingUnlockRange>({
            ClothingUnlockRange{15708, 15708}, ClothingUnlockRange{15710, 15710},
            ClothingUnlockRange{15717, 15717}, ClothingUnlockRange{15719, 15721},
            ClothingUnlockRange{15728, 15728}, ClothingUnlockRange{15730, 15732},
            ClothingUnlockRange{15735, 15735}, ClothingUnlockRange{15739, 15739},
            ClothingUnlockRange{15741, 15741}, ClothingUnlockRange{15743, 15743},
            ClothingUnlockRange{22124, 22132}, ClothingUnlockRange{22150, 22150},
            ClothingUnlockRange{22152, 22152}, ClothingUnlockRange{22159, 22159},
            ClothingUnlockRange{22162, 22162}, ClothingUnlockRange{22166, 22166},
            ClothingUnlockRange{22170, 22170}, ClothingUnlockRange{22172, 22172},
            ClothingUnlockRange{22174, 22174},
        });
        inline constexpr auto ArenaWar = std::to_array<ClothingUnlockRange>({
            ClothingUnlockRange{24970, 24970}, ClothingUnlockRange{24977, 24977},
            ClothingUnlockRange{25000, 25000}, ClothingUnlockRange{25005, 25006},
            ClothingUnlockRange{25018, 25099}, ClothingUnlockRange{25244, 25258},
            ClothingUnlockRange{25265, 25367},
        });
        inline constexpr auto Casino = std::to_array<ClothingUnlockRange>({
            ClothingUnlockRange{26968, 27088}, ClothingUnlockRange{27109, 27115},
            ClothingUnlockRange{27120, 27145}, ClothingUnlockRange{27147, 27182},
            ClothingUnlockRange{27184, 27213},
        });
        inline constexpr auto CasinoHeist = std::to_array<ClothingUnlockRange>({
            ClothingUnlockRange{28171, 28191}, ClothingUnlockRange{28197, 28222},
            ClothingUnlockRange{28224, 28227}, ClothingUnlockRange{28229, 28249},
            ClothingUnlockRange{28254, 28255},
        });
        inline constexpr auto Gen9 = std::to_array<ClothingUnlockRange>({
            ClothingUnlockRange{28319, 28321}, ClothingUnlockRange{28344, 28345},
            ClothingUnlockRange{28351, 28351}, ClothingUnlockRange{28393, 28427},
            ClothingUnlockRange{28447, 28478},
        });
        inline constexpr auto SummerSpecial = std::to_array<ClothingUnlockRange>({
            ClothingUnlockRange{30240, 30240}, ClothingUnlockRange{30254, 30295},
        });
        inline constexpr auto CayoPerico = std::to_array<ClothingUnlockRange>({
            ClothingUnlockRange{30355, 30372}, ClothingUnlockRange{30407, 30410},
            ClothingUnlockRange{30418, 30433}, ClothingUnlockRange{30524, 30557},
            ClothingUnlockRange{30563, 30631}, ClothingUnlockRange{30634, 30693},
            ClothingUnlockRange{30699, 30704},
        });
        inline constexpr auto Tuners = std::to_array<ClothingUnlockRange>({
            ClothingUnlockRange{31736, 31736}, ClothingUnlockRange{31760, 31764},
            ClothingUnlockRange{31766, 31777}, ClothingUnlockRange{31779, 31796},
            ClothingUnlockRange{31805, 31808}, ClothingUnlockRange{31826, 31828},
            ClothingUnlockRange{31830, 31830}, ClothingUnlockRange{31832, 31833},
            ClothingUnlockRange{31835, 31835}, ClothingUnlockRange{31837, 31838},
            ClothingUnlockRange{31840, 31840}, ClothingUnlockRange{31842, 31843},
            ClothingUnlockRange{31845, 31845}, ClothingUnlockRange{31847, 31848},
            ClothingUnlockRange{31850, 31850}, ClothingUnlockRange{31852, 31853},
            ClothingUnlockRange{31855, 31855}, ClothingUnlockRange{31857, 31858},
            ClothingUnlockRange{31860, 31860}, ClothingUnlockRange{31862, 31863},
            ClothingUnlockRange{31865, 31865}, ClothingUnlockRange{31867, 31868},
            ClothingUnlockRange{31870, 31870}, ClothingUnlockRange{31872, 31875},
            ClothingUnlockRange{31877, 31880}, ClothingUnlockRange{31882, 31885},
            ClothingUnlockRange{31887, 31890}, ClothingUnlockRange{31892, 31895},
            ClothingUnlockRange{31897, 31900}, ClothingUnlockRange{31902, 31903},
            ClothingUnlockRange{31905, 31905}, ClothingUnlockRange{31907, 31908},
            ClothingUnlockRange{31910, 31910}, ClothingUnlockRange{31912, 31913},
            ClothingUnlockRange{31915, 31915}, ClothingUnlockRange{31917, 31918},
            ClothingUnlockRange{31920, 31920}, ClothingUnlockRange{31922, 31923},
            ClothingUnlockRange{31925, 31925}, ClothingUnlockRange{31927, 31928},
            ClothingUnlockRange{31930, 31930}, ClothingUnlockRange{31932, 31932},
            ClothingUnlockRange{31935, 31935}, ClothingUnlockRange{31937, 31937},
            ClothingUnlockRange{31940, 31940}, ClothingUnlockRange{31942, 31942},
            ClothingUnlockRange{31945, 31945}, ClothingUnlockRange{31947, 31947},
            ClothingUnlockRange{31950, 31950}, ClothingUnlockRange{31952, 31952},
            ClothingUnlockRange{31955, 31955}, ClothingUnlockRange{31957, 31957},
            ClothingUnlockRange{31960, 31960}, ClothingUnlockRange{31962, 31962},
            ClothingUnlockRange{31965, 31965}, ClothingUnlockRange{31967, 31967},
            ClothingUnlockRange{31970, 31970}, ClothingUnlockRange{31972, 31972},
            ClothingUnlockRange{31975, 31975}, ClothingUnlockRange{31977, 31977},
            ClothingUnlockRange{31980, 31980}, ClothingUnlockRange{31982, 31982},
            ClothingUnlockRange{31985, 31985}, ClothingUnlockRange{31987, 31987},
            ClothingUnlockRange{31990, 31990}, ClothingUnlockRange{31992, 31992},
            ClothingUnlockRange{31995, 31995}, ClothingUnlockRange{31997, 31997},
            ClothingUnlockRange{32000, 32000}, ClothingUnlockRange{32002, 32002},
            ClothingUnlockRange{32005, 32005}, ClothingUnlockRange{32007, 32007},
            ClothingUnlockRange{32010, 32010}, ClothingUnlockRange{32012, 32012},
            ClothingUnlockRange{32015, 32015}, ClothingUnlockRange{32017, 32018},
            ClothingUnlockRange{32020, 32023}, ClothingUnlockRange{32025, 32028},
            ClothingUnlockRange{32030, 32033}, ClothingUnlockRange{32035, 32038},
            ClothingUnlockRange{32040, 32043}, ClothingUnlockRange{32045, 32048},
            ClothingUnlockRange{32050, 32053}, ClothingUnlockRange{32055, 32058},
            ClothingUnlockRange{32060, 32063}, ClothingUnlockRange{32065, 32074},
            ClothingUnlockRange{32084, 32084}, ClothingUnlockRange{32094, 32094},
            ClothingUnlockRange{32104, 32104}, ClothingUnlockRange{32114, 32114},
            ClothingUnlockRange{32124, 32124}, ClothingUnlockRange{32134, 32134},
            ClothingUnlockRange{32144, 32144}, ClothingUnlockRange{32154, 32154},
            ClothingUnlockRange{32164, 32164}, ClothingUnlockRange{32174, 32174},
            ClothingUnlockRange{32224, 32224}, ClothingUnlockRange{32273, 32273},
            ClothingUnlockRange{32275, 32275},
        });
        inline constexpr auto Contract = std::to_array<ClothingUnlockRange>({
            ClothingUnlockRange{32295, 32311}, ClothingUnlockRange{32315, 32316},
            ClothingUnlockRange{32407, 32409},
        });
        inline constexpr auto CriminalEnterprises = std::to_array<ClothingUnlockRange>({
            ClothingUnlockRange{34372, 34372}, ClothingUnlockRange{34375, 34375},
            ClothingUnlockRange{34378, 34411}, ClothingUnlockRange{34415, 34510},
        });
        inline constexpr auto DrugWars = std::to_array<ClothingUnlockRange>({
            ClothingUnlockRange{36703, 36704}, ClothingUnlockRange{36717, 36718},
            ClothingUnlockRange{36737, 36738}, ClothingUnlockRange{36751, 36752},
            ClothingUnlockRange{36759, 36759}, ClothingUnlockRange{36763, 36763},
            ClothingUnlockRange{36768, 36769}, ClothingUnlockRange{36774, 36776},
            ClothingUnlockRange{36782, 36784}, ClothingUnlockRange{36809, 36809},
        });
        inline constexpr auto Mercenaries = std::to_array<ClothingUnlockRange>({
            ClothingUnlockRange{41593, 41593}, ClothingUnlockRange{41802, 41802},
            ClothingUnlockRange{41885, 41913}, ClothingUnlockRange{41915, 41941},
            ClothingUnlockRange{41943, 41980}, ClothingUnlockRange{41994, 41994},
            ClothingUnlockRange{41996, 41996}, ClothingUnlockRange{42054, 42054},
            ClothingUnlockRange{42062, 42063}, ClothingUnlockRange{42111, 42111},
        });
        inline constexpr auto ChopShop = std::to_array<ClothingUnlockRange>({
            ClothingUnlockRange{42119, 42119}, ClothingUnlockRange{42128, 42146},
            ClothingUnlockRange{42152, 42217}, ClothingUnlockRange{42257, 42268},
            ClothingUnlockRange{42286, 42287}, ClothingUnlockRange{42294, 42297},
        });
        inline constexpr auto BottomDollar = std::to_array<ClothingUnlockRange>({
            ClothingUnlockRange{51215, 51258},
        });
        inline constexpr auto AgentsSabotage = std::to_array<ClothingUnlockRange>({
            ClothingUnlockRange{54569, 54569}, ClothingUnlockRange{54572, 54611},
            ClothingUnlockRange{54615, 54634}, ClothingUnlockRange{54651, 54651},
        });
        inline constexpr auto MoneyFronts = std::to_array<ClothingUnlockRange>({
            ClothingUnlockRange{54664, 54664}, ClothingUnlockRange{54682, 54707},
            ClothingUnlockRange{54711, 54712},
        });
        inline constexpr auto Safehouse = std::to_array<ClothingUnlockRange>({
            ClothingUnlockRange{51365, 51378}, ClothingUnlockRange{54769, 54772},
            ClothingUnlockRange{59978, 59980},
        });

        inline const std::array<ClothingUnlockGroup, 22>& Groups() noexcept
        {
            static const std::array<ClothingUnlockGroup, 22> groups{{
                {"Legacy / Heists & Events", "PSTAT / TUPSTAT / NGPSTAT", Legacy},
                {"Executives / Finance & Felony", "NGDLCPSTAT_BOOL", Executives},
                {"Bikers", "DLCBIKEPSTAT_BOOL", Bikers},
                {"Gunrunning", "DLCGUNPSTAT_BOOL", Gunrunning},
                {"Doomsday Heist", "GANGOPSPSTAT_BOOL", Doomsday},
                {"After Hours", "BUSINESSBATPSTAT_BOOL", AfterHours},
                {"Arena War", "ARENAWARSPSTAT_BOOL", ArenaWar},
                {"Diamond Casino & Resort", "CASINOPSTAT_BOOL", Casino},
                {"Diamond Casino Heist", "CASINOHSTPSTAT_BOOL", CasinoHeist},
                {"Expanded & Enhanced / Gen9", "Gen9 clothing packed bools", Gen9},
                {"Los Santos Summer Special", "SU20PSTAT_BOOL", SummerSpecial},
                {"Cayo Perico Heist", "HISLANDPSTAT_BOOL", CayoPerico},
                {"Los Santos Tuners", "TUNERPSTAT_BOOL (clothing-only filter)", Tuners},
                {"The Contract", "FIXERPSTAT_BOOL", Contract},
                {"The Criminal Enterprises", "DLC12022PSTAT_BOOL", CriminalEnterprises},
                {"Los Santos Drug Wars", "DLC22022PSTAT_BOOL", DrugWars},
                {"San Andreas Mercenaries", "DLC12023PSTAT_BOOL", Mercenaries},
                {"The Chop Shop", "DLC22023PSTAT_BOOL", ChopShop},
                {"Bottom Dollar Bounties", "DLC12024PSTAT_BOOL", BottomDollar},
                {"Agents of Sabotage", "DLC22024PSTAT_BOOL", AgentsSabotage},
                {"Money Fronts / 2025 Events", "verified 546xx / 547xx clothing bools", MoneyFronts},
                {"A Safehouse in the Hills / 2026 Events", "verified 513xx / 547xx / 599xx clothing bools", Safehouse},
            }};
            return groups;
        }

        inline std::size_t Count(const ClothingUnlockGroup& group) noexcept
        {
            std::size_t total{};
            for (const auto& range : group.ranges)
            {
                if (range.last >= range.first)
                    total += static_cast<std::size_t>(range.last - range.first + 1);
            }
            return total;
        }
    }

    struct ClothingUnlockSnapshot final
    {
        bool pending{};
        bool allGroups{};
        int groupIndex{-1};
        std::size_t completed{};
        std::size_t total{};
        std::size_t failed{};
        bool lastSucceeded{};
        bool haveResult{};
    };

    class ClothingUnlockRuntime final
    {
    public:
        static ClothingUnlockRuntime& Get() noexcept
        {
            static ClothingUnlockRuntime instance;
            return instance;
        }

        bool QueueGroup(std::size_t groupIndex)
        {
            if (groupIndex >= ClothingUnlockData::Groups().size()
                || !Native::NativeRegistry::Get().IsReady())
                return false;
            return Queue(false, groupIndex);
        }

        bool QueueAll()
        {
            if (!Native::NativeRegistry::Get().IsReady())
                return false;
            return Queue(true, 0);
        }

        [[nodiscard]] ClothingUnlockSnapshot Snapshot() const noexcept
        {
            std::scoped_lock lock(m_Mutex);
            return m_Snapshot;
        }

    private:
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

        // YimMenuV2 Enhanced crossmap values for the current 1158.13 family.
        static constexpr std::uint64_t GetPackedStatBoolHash = 0xA6D3C21763E25496ull;
        static constexpr std::uint64_t SetPackedStatBoolHash = 0xA595AA1819B05EA0ull;
        static constexpr std::size_t BatchSize = 48;

        bool Queue(bool allGroups, std::size_t groupIndex)
        {
            bool expected = false;
            if (!m_Pending.compare_exchange_strong(expected, true, std::memory_order_acq_rel))
                return false;

            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot = {};
                m_Snapshot.pending = true;
                m_Snapshot.allGroups = allGroups;
                m_Snapshot.groupIndex = allGroups ? -1 : static_cast<int>(groupIndex);
            }

            if (Runtime::GameRuntime::Get().Enqueue([this, allGroups, groupIndex] {
                    BeginOnGameThread(allGroups, groupIndex);
                }))
                return true;

            m_Pending.store(false, std::memory_order_release);
            std::scoped_lock lock(m_Mutex);
            m_Snapshot.pending = false;
            m_Snapshot.failed = 1;
            m_Snapshot.lastSucceeded = false;
            m_Snapshot.haveResult = true;
            return false;
        }

        void AppendGroup(const ClothingUnlockGroup& group)
        {
            for (const auto& range : group.ranges)
            {
                if (range.first < 0 || range.last < range.first)
                    continue;
                for (int id = range.first; id <= range.last; ++id)
                    m_Work.push_back(id);
            }
        }

        [[nodiscard]] bool SessionActiveOnGameThread() const noexcept
        {
            const bool* sessionStarted = GamePointers::Get().IsSessionStarted();
            return sessionStarted && *sessionStarted;
        }

        void BeginOnGameThread(bool allGroups, std::size_t groupIndex) noexcept
        {
            m_Work.clear();
            m_Cursor = 0;
            m_Failed = 0;

            if (!SessionActiveOnGameThread()
                || !Native::NativeRegistry::Get().IsReady()
                || !Native::NativeRegistry::Get().CanInvokeOnCurrentThread())
            {
                Finish(false, 1);
                return;
            }

            const auto& groups = ClothingUnlockData::Groups();
            if (allGroups)
            {
                for (const auto& group : groups)
                    AppendGroup(group);
            }
            else if (groupIndex < groups.size())
            {
                AppendGroup(groups[groupIndex]);
            }
            else
            {
                Finish(false, 1);
                return;
            }

            std::sort(m_Work.begin(), m_Work.end());
            m_Work.erase(std::unique(m_Work.begin(), m_Work.end()), m_Work.end());
            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot.total = m_Work.size();
            }

            if (m_Work.empty() || !ResolveHandlersOnGameThread())
            {
                Finish(false, m_Work.empty() ? 0 : m_Work.size());
                return;
            }
            ProcessBatchOnGameThread();
        }

        bool ResolveHandlersOnGameThread() noexcept
        {
            if (!Native::NativeRegistry::Get().CanInvokeOnCurrentThread())
                return false;

            const auto init = GamePointers::Get().InitNativeTables();
            if (!init)
                return false;

            std::array<std::uint64_t, 2> slots{GetPackedStatBoolHash, SetPackedStatBoolHash};
            NativeProgram program{};
            program.nativeCount = static_cast<std::uint32_t>(slots.size());
            program.nativeEntrypoints = reinterpret_cast<Native::NativeHandler*>(slots.data());
            init(&program);

            m_GetPackedBool = reinterpret_cast<Native::NativeHandler>(static_cast<std::uintptr_t>(slots[0]));
            m_SetPackedBool = reinterpret_cast<Native::NativeHandler>(static_cast<std::uintptr_t>(slots[1]));
            return m_GetPackedBool != nullptr && m_SetPackedBool != nullptr;
        }

        bool ReadPackedBoolOnGameThread(int id, bool& value) const noexcept
        {
            if (!m_GetPackedBool)
                return false;

            Native::CallContext context;
            if (!context.PushArg(id) || !context.PushArg(std::int32_t{-1}))
                return false;
            m_GetPackedBool(&context);
            context.FixVectors();
            value = context.GetReturnValue<std::int32_t>() != 0;
            return true;
        }

        bool SetPackedBoolOnGameThread(int id) const noexcept
        {
            if (!m_SetPackedBool)
                return false;

            Native::CallContext context;
            if (!context.PushArg(id)
                || !context.PushArg(std::int32_t{1})
                || !context.PushArg(std::int32_t{-1}))
                return false;
            m_SetPackedBool(&context);
            context.FixVectors();

            bool confirmed{};
            return ReadPackedBoolOnGameThread(id, confirmed) && confirmed;
        }

        void ProcessBatchOnGameThread() noexcept
        {
            if (!m_Pending.load(std::memory_order_acquire))
                return;

            if (!SessionActiveOnGameThread()
                || !Native::NativeRegistry::Get().IsReady()
                || !Native::NativeRegistry::Get().CanInvokeOnCurrentThread())
            {
                Finish(false, m_Failed + (m_Work.size() - m_Cursor));
                return;
            }

            const std::size_t end = std::min(m_Cursor + BatchSize, m_Work.size());
            for (; m_Cursor < end; ++m_Cursor)
            {
                bool unlocked{};
                if (ReadPackedBoolOnGameThread(m_Work[m_Cursor], unlocked) && unlocked)
                    continue;
                if (!SetPackedBoolOnGameThread(m_Work[m_Cursor]))
                    ++m_Failed;
            }

            {
                std::scoped_lock lock(m_Mutex);
                m_Snapshot.completed = m_Cursor;
                m_Snapshot.failed = m_Failed;
            }

            if (m_Cursor >= m_Work.size())
            {
                Finish(m_Failed == 0, m_Failed);
                return;
            }

            if (!Runtime::GameRuntime::Get().Enqueue([this] { ProcessBatchOnGameThread(); }))
                Finish(false, m_Failed + (m_Work.size() - m_Cursor));
        }

        void Finish(bool success, std::size_t failed) noexcept
        {
            m_Pending.store(false, std::memory_order_release);
            std::scoped_lock lock(m_Mutex);
            m_Snapshot.pending = false;
            m_Snapshot.completed = m_Work.size();
            m_Snapshot.total = m_Work.size();
            m_Snapshot.failed = failed;
            m_Snapshot.lastSucceeded = success;
            m_Snapshot.haveResult = true;
        }

        ClothingUnlockRuntime() = default;

        std::atomic<bool> m_Pending{false};
        mutable std::mutex m_Mutex;
        ClothingUnlockSnapshot m_Snapshot{};
        std::vector<int> m_Work;
        std::size_t m_Cursor{};
        std::size_t m_Failed{};
        Native::NativeHandler m_GetPackedBool{};
        Native::NativeHandler m_SetPackedBool{};
    };
}
