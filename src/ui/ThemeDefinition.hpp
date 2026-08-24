#pragma once

#include "V11Theme.hpp"
#include <imgui.h>
#include <string>

namespace Tutones::UI
{
    struct ThemeDefinition final
    {
        std::string name{"Tutones V12 Red"};
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
        float backgroundOpacity{0.18f};
        std::string fontFile;
        float fontSize{15.0f};
    };

    inline ThemeDefinition DefaultTheme() noexcept
    {
        ThemeDefinition t;
        t.accent = {229.f/255.f,42.f/255.f,48.f/255.f,1.f};
        t.accentHover = {248.f/255.f,66.f/255.f,72.f/255.f,1.f};
        t.accentDark = {74.f/255.f,10.f/255.f,15.f/255.f,1.f};
        t.windowBg = {3.f/255.f,5.f/255.f,8.f/255.f,1.f};
        t.bodyBg = {5.f/255.f,8.f/255.f,12.f/255.f,1.f};
        t.footerBg = {4.f/255.f,7.f/255.f,10.f/255.f,1.f};
        t.railBg = {7.f/255.f,10.f/255.f,14.f/255.f,.985f};
        t.panelBg = {9.f/255.f,12.f/255.f,17.f/255.f,.985f};
        t.panelBorder = {78.f/255.f,83.f/255.f,91.f/255.f,.38f};
        t.separator = {90.f/255.f,94.f/255.f,103.f/255.f,.26f};
        t.controlBg = {12.f/255.f,16.f/255.f,22.f/255.f,1.f};
        t.controlHover = {35.f/255.f,18.f/255.f,23.f/255.f,1.f};
        t.mutedText = {151.f/255.f,157.f/255.f,168.f/255.f,1.f};
        return t;
    }
}
