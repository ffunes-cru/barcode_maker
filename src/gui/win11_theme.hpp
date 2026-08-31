#pragma once

#include "../../third_party/imgui/imgui.h"

namespace Win11Theme {

inline void ApplyBalancedFluentTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // --- Balanced Windows 11 Geometry ---
    style.WindowRounding    = 6.0f;
    style.ChildRounding     = 5.0f;
    style.FrameRounding     = 4.0f;
    style.PopupRounding     = 6.0f;
    style.ScrollbarRounding = 6.0f;
    style.GrabRounding      = 4.0f;
    style.TabRounding       = 4.0f;

    style.WindowPadding     = ImVec2(12.0f, 12.0f);
    style.FramePadding      = ImVec2(10.0f, 6.0f);
    style.ItemSpacing       = ImVec2(10.0f, 8.0f);
    style.ItemInnerSpacing  = ImVec2(6.0f, 6.0f);
    style.IndentSpacing     = 18.0f;
    style.ScrollbarSize     = 12.0f;
    style.GrabMinSize       = 10.0f;

    style.WindowBorderSize  = 1.0f;
    style.ChildBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.TabBorderSize     = 1.0f;

    // --- Windows 11 Balanced Dark Palette ---
    const ImVec4 bg_canvas        = ImVec4(0.12f, 0.12f, 0.12f, 1.00f); // #1E1E1E
    const ImVec4 bg_card          = ImVec4(0.16f, 0.16f, 0.16f, 1.00f); // #282828
    const ImVec4 bg_popup         = ImVec4(0.18f, 0.18f, 0.18f, 0.98f);
    const ImVec4 bg_frame         = ImVec4(0.20f, 0.20f, 0.20f, 1.00f); // #333333
    const ImVec4 bg_frame_hover   = ImVec4(0.26f, 0.26f, 0.26f, 1.00f);
    const ImVec4 bg_frame_active  = ImVec4(0.29f, 0.29f, 0.29f, 1.00f);

    const ImVec4 accent           = ImVec4(0.00f, 0.47f, 0.83f, 1.00f); // #0078D4 (Cobalt)
    const ImVec4 accent_hover     = ImVec4(0.10f, 0.54f, 0.90f, 1.00f);
    const ImVec4 accent_active    = ImVec4(0.00f, 0.40f, 0.72f, 1.00f);
    const ImVec4 accent_subtle    = ImVec4(0.00f, 0.47f, 0.83f, 0.25f);

    const ImVec4 border_subtle    = ImVec4(1.00f, 1.00f, 1.00f, 0.12f);
    const ImVec4 text_primary     = ImVec4(0.95f, 0.95f, 0.95f, 1.00f);
    const ImVec4 text_secondary   = ImVec4(0.65f, 0.65f, 0.65f, 1.00f);
    const ImVec4 text_disabled    = ImVec4(0.45f, 0.45f, 0.45f, 1.00f);

    colors[ImGuiCol_Text]                  = text_primary;
    colors[ImGuiCol_TextDisabled]          = text_disabled;
    colors[ImGuiCol_WindowBg]              = bg_canvas;
    colors[ImGuiCol_ChildBg]               = bg_card;
    colors[ImGuiCol_PopupBg]               = bg_popup;
    colors[ImGuiCol_Border]                = border_subtle;
    colors[ImGuiCol_BorderShadow]          = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_FrameBg]               = bg_frame;
    colors[ImGuiCol_FrameBgHovered]        = bg_frame_hover;
    colors[ImGuiCol_FrameBgActive]         = bg_frame_active;
    colors[ImGuiCol_TitleBg]               = bg_canvas;
    colors[ImGuiCol_TitleBgActive]         = bg_canvas;
    colors[ImGuiCol_TitleBgCollapsed]      = bg_canvas;
    colors[ImGuiCol_MenuBarBg]             = bg_card;
    colors[ImGuiCol_ScrollbarBg]           = ImVec4(0.10f, 0.10f, 0.10f, 0.00f);
    colors[ImGuiCol_ScrollbarGrab]         = ImVec4(0.35f, 0.35f, 0.35f, 0.60f);
    colors[ImGuiCol_ScrollbarGrabHovered]  = ImVec4(0.45f, 0.45f, 0.45f, 0.80f);
    colors[ImGuiCol_ScrollbarGrabActive]   = ImVec4(0.55f, 0.55f, 0.55f, 1.00f);
    colors[ImGuiCol_CheckMark]             = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    colors[ImGuiCol_SliderGrab]            = accent;
    colors[ImGuiCol_SliderGrabActive]      = accent_active;
    colors[ImGuiCol_Button]                = ImVec4(0.22f, 0.22f, 0.22f, 1.00f);
    colors[ImGuiCol_ButtonHovered]         = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
    colors[ImGuiCol_ButtonActive]          = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_Header]                = accent_subtle;
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.00f, 0.47f, 0.83f, 0.40f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.00f, 0.47f, 0.83f, 0.60f);
    colors[ImGuiCol_Separator]             = border_subtle;
    colors[ImGuiCol_SeparatorHovered]      = accent;
    colors[ImGuiCol_SeparatorActive]       = accent_active;
    colors[ImGuiCol_ResizeGrip]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_ResizeGripHovered]     = accent_hover;
    colors[ImGuiCol_ResizeGripActive]      = accent_active;

    // Distinct TabBar styling (Clear contrast between active and inactive tabs)
    colors[ImGuiCol_Tab]                   = ImVec4(0.20f, 0.20f, 0.20f, 1.00f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.28f, 0.28f, 0.28f, 1.00f);
    colors[ImGuiCol_TabSelected]           = ImVec4(0.00f, 0.47f, 0.83f, 1.00f);
    colors[ImGuiCol_TabDimmed]             = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_TabDimmedSelected]     = ImVec4(0.00f, 0.38f, 0.68f, 1.00f);

    colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]     = border_subtle;
    colors[ImGuiCol_TableBorderLight]      = ImVec4(1.00f, 1.00f, 1.00f, 0.04f);
    colors[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.00f, 1.00f, 1.00f, 0.02f);
    colors[ImGuiCol_TextSelectedBg]        = accent_subtle;
    colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.00f, 0.00f, 0.00f, 0.65f);
}

} // namespace Win11Theme
