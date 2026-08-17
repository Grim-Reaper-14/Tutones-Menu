#pragma once

#include <filesystem>

namespace Tutones::Core
{
    class Services final
    {
    public:
        static Services& Get() noexcept;

        bool Initialize(const std::filesystem::path& moduleDirectory);
        void Shutdown() noexcept;

        [[nodiscard]] bool IsInitialized() const noexcept;
        [[nodiscard]] const std::filesystem::path& ModuleDirectory() const noexcept;

    private:
        Services() = default;
        ~Services() = default;
        Services(const Services&) = delete;
        Services& operator=(const Services&) = delete;

        std::filesystem::path m_ModuleDirectory;
        bool m_Initialized{};
    };
}
