#pragma once

#include "V11Theme.hpp"
#include <imgui.h>
#include <string>

namespace Tutones::UI
{
    struct ThemeDefinition final
    {
        std::string name{"Default V11"};
        ImVec4 accent{};
        ImVec4 accentHover{};
        ImVec4 accentDark{};
        ImVec4 windowBg{};
        ImVec4 bodyBg{};
        ImVec4 footerBg{};
        ImVec4 railBg{};
        ImVec4 panelBg{};
        ImVec4 panelBorder{};
        ImVec4 separator{};
        ImVec4 controlBg{};
        ImVec4 controlHover{};
        ImVec4 mutedText{};
        std::string headerImage;
        std::string footerImage;
        std::string backgroundImage;
        float backgroundOpacity{0.36f};
        std::string fontFile;
        float fontSize{15.0f};
    };

    inline ThemeDefinition DefaultTheme() noexcept
    {
        ThemeDefinition t;
        t.accent = {2.f/255.f,108.f/255.f,249.f/255.f,1.f};
        t.accentHover = {15.f/255.f,143.f/255.f,239.f/255.f,1.f};
        t.accentDark = {1.f/255.f,26.f/255.f,75.f/255.f,1.f};
        t.windowBg = {1.f/255.f,3.f/255.f,7.f/255.f,1.f};
        t.bodyBg = {4.f/255.f,7.f/255.f,12.f/255.f,1.f};
        t.footerBg = {3.f/255.f,7.f/255.f,13.f/255.f,1.f};
        t.railBg = {6.f/255.f,10.f/255.f,17.f/255.f,.96f};
        t.panelBg = {7.f/255.f,11.f/255.f,18.f/255.f,.98f};
        t.panelBorder = {2.f/255.f,108.f/255.f,249.f/255.f,.28f};
        t.separator = {2.f/255.f,108.f/255.f,249.f/255.f,.22f};
        t.controlBg = {10.f/255.f,16.f/255.f,25.f/255.f,1.f};
        t.controlHover = {9.f/255.f,34.f/255.f,66.f/255.f,1.f};
        t.mutedText = {139.f/255.f,153.f/255.f,177.f/255.f,1.f};
        return t;
    }
}
