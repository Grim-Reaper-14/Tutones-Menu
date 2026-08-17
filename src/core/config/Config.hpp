#pragma once

#include "../logging/LogLevel.hpp"

#include <cstdint>
#include <filesystem>
#include <string>

namespace Tutones::Core::Config
{
    struct Settings final
    {
        std::string menuName{"Tutones Menu"};
        std::uint32_t configVersion{1};
        bool menuEnabled{false};
        bool debugLogging{true};
        Logging::LogLevel minimumLogLevel{Logging::LogLevel::Trace};
        bool consoleLogging{true};
        bool debuggerLogging{true};
        bool fileLogging{true};
        std::uint32_t logRetentionFiles{5};
        std::uint64_t logMaxBytes{8 * 1024 * 1024};
        std::uint32_t uiWidth{980};
        std::uint32_t uiHeight{620};
        std::uint32_t accentR{147};
        std::uint32_t accentG{190};
        std::uint32_t accentB{66};
    };

    class Service final
    {
    public:
        static Service& Get() noexcept;

        bool Load(const std::filesystem::path& path);
        bool Save(const std::filesystem::path& path) const;
        void Reset() noexcept;

        [[nodiscard]] const Settings& Current() const noexcept;
        [[nodiscard]] Settings& Current() noexcept;
        [[nodiscard]] bool IsLoaded() const noexcept;

    private:
        Service() = default;
        ~Service() = default;
        Service(const Service&) = delete;
        Service& operator=(const Service&) = delete;

        Settings m_Settings{};
        bool m_Loaded{};
    };

    [[nodiscard]] inline Service& Get() noexcept
    {
        return Service::Get();
    }
}
