#pragma once

#include <imgui.h>

namespace Tutones::UI
{
    inline const char* g_V11DescriptionText{};

    inline void BeginV11DescriptionFrame(const char* fallback) noexcept
    {
        g_V11DescriptionText = fallback;
    }

    inline void SetV11Description(const char* description) noexcept
    {
        if (description && description[0] != '\0')
            g_V11DescriptionText = description;
    }

    inline void DescribeLastV11Item(const char* description) noexcept
    {
        if (description && description[0] != '\0'
            && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        {
            g_V11DescriptionText = description;
        }
    }

    [[nodiscard]] inline const char* CurrentV11Description() noexcept
    {
        return g_V11DescriptionText;
    }
}
