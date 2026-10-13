#pragma once
//
// menustyle.h - custom dark theme so the menu doesn't look like stock ImGui.
//
#include "imgui/imgui.h"

inline void ApplyMenuStyle()
{
    ImGuiStyle& s = ImGui::GetStyle();
    ImVec4* c = s.Colors;

    // shape
    s.WindowRounding    = 8.0f;
    s.ChildRounding     = 8.0f;
    s.FrameRounding     = 6.0f;
    s.PopupRounding     = 6.0f;
    s.GrabRounding      = 6.0f;
    s.TabRounding       = 6.0f;
    s.ScrollbarRounding = 6.0f;
    s.WindowBorderSize  = 0.0f;
    s.FrameBorderSize   = 0.0f;
    s.PopupBorderSize   = 0.0f;
    s.WindowPadding     = ImVec2(14, 14);
    s.FramePadding      = ImVec2(10, 6);
    s.ItemSpacing       = ImVec2(10, 8);
    s.ItemInnerSpacing  = ImVec2(8, 6);
    s.ScrollbarSize     = 12.0f;
    s.GrabMinSize       = 10.0f;
    s.WindowTitleAlign  = ImVec2(0.5f, 0.5f);

    // accent (purple, matches the default glow color)
    const ImVec4 accent    = ImVec4(0.56f, 0.26f, 0.88f, 1.00f);
    const ImVec4 accentHov = ImVec4(0.64f, 0.36f, 0.94f, 1.00f);
    const ImVec4 accentAct = ImVec4(0.49f, 0.20f, 0.80f, 1.00f);

    c[ImGuiCol_WindowBg]           = ImVec4(0.09f, 0.09f, 0.11f, 0.98f);
    c[ImGuiCol_ChildBg]            = ImVec4(0.11f, 0.11f, 0.13f, 1.00f);
    c[ImGuiCol_PopupBg]            = ImVec4(0.10f, 0.10f, 0.12f, 0.98f);
    c[ImGuiCol_Border]             = ImVec4(0.20f, 0.20f, 0.24f, 0.50f);
    c[ImGuiCol_Text]               = ImVec4(0.90f, 0.90f, 0.92f, 1.00f);
    c[ImGuiCol_TextDisabled]       = ImVec4(0.45f, 0.45f, 0.50f, 1.00f);
    c[ImGuiCol_FrameBg]            = ImVec4(0.16f, 0.16f, 0.19f, 1.00f);
    c[ImGuiCol_FrameBgHovered]     = ImVec4(0.22f, 0.22f, 0.26f, 1.00f);
    c[ImGuiCol_FrameBgActive]      = ImVec4(0.26f, 0.26f, 0.31f, 1.00f);
    c[ImGuiCol_TitleBg]            = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
    c[ImGuiCol_TitleBgActive]      = ImVec4(0.07f, 0.07f, 0.09f, 1.00f);
    c[ImGuiCol_CheckMark]          = accent;
    c[ImGuiCol_SliderGrab]         = accent;
    c[ImGuiCol_SliderGrabActive]   = accentAct;
    c[ImGuiCol_Button]             = ImVec4(0.18f, 0.18f, 0.22f, 1.00f);
    c[ImGuiCol_ButtonHovered]      = accentHov;
    c[ImGuiCol_ButtonActive]       = accentAct;
    c[ImGuiCol_Header]             = accent;
    c[ImGuiCol_HeaderHovered]      = accentHov;
    c[ImGuiCol_HeaderActive]       = accentAct;
    c[ImGuiCol_Tab]                = ImVec4(0.14f, 0.14f, 0.17f, 1.00f);
    c[ImGuiCol_TabHovered]         = accentHov;
    c[ImGuiCol_TabActive]          = accent;
    c[ImGuiCol_TabUnfocused]       = ImVec4(0.14f, 0.14f, 0.17f, 1.00f);
    c[ImGuiCol_TabUnfocusedActive] = accentAct;
    c[ImGuiCol_ScrollbarGrab]      = ImVec4(0.28f, 0.28f, 0.34f, 1.00f);
    c[ImGuiCol_ScrollbarGrabHovered] = accentHov;
    c[ImGuiCol_ScrollbarGrabActive]  = accentAct;
    c[ImGuiCol_SeparatorHovered]   = accentHov;
    c[ImGuiCol_SeparatorActive]    = accentAct;
}
