#pragma once

#include "../../third_party/imgui/imgui.h"

namespace Win11Theme {

inline void ApplyFluentDarkTheme() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // --- Windows 11 Geometry & Metrics ---
    style.WindowRounding    = 8.0f;
    style.ChildRounding     = 6.0f;
    style.FrameRounding     = 5.0f;
    style.PopupRounding     = 8.0f;
    style.ScrollbarRounding = 9.0f;
    style.GrabRounding      = 4.0f;
    style.TabRounding       = 6.0f;

    style.WindowPadding     = ImVec2(14.0f, 14.0f);
    style.FramePadding      = ImVec2(10.0f, 6.0f);
    style.ItemSpacing       = ImVec2(10.0f, 8.0f);
    style.ItemInnerSpacing  = ImVec2(8.0f, 6.0f);
    style.TouchExtraPadding = ImVec2(0.0f, 0.0f);
    style.IndentSpacing     = 20.0f;
    style.ScrollbarSize     = 12.0f;
    style.GrabMinSize       = 10.0f;

    style.WindowBorderSize  = 1.0f;
    style.ChildBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.TabBorderSize     = 0.0f;

    // --- Windows 11 Mica Dark Palette ---
    const ImVec4 bg_canvas        = ImVec4(0.125f, 0.125f, 0.125f, 1.00f); // #202020
    const ImVec4 bg_card          = ImVec4(0.165f, 0.165f, 0.165f, 1.00f); // #2A2A2A
    const ImVec4 bg_popup         = ImVec4(0.180f, 0.180f, 0.180f, 0.98f); // #2E2E2E
    const ImVec4 bg_frame         = ImVec4(0.200f, 0.200f, 0.200f, 1.00f); // #333333
    const ImVec4 bg_frame_hover   = ImVec4(0.250f, 0.250f, 0.250f, 1.00f);
    const ImVec4 bg_frame_active  = ImVec4(0.280f, 0.280f, 0.280f, 1.00f);

    const ImVec4 accent           = ImVec4(0.000f, 0.471f, 0.831f, 1.00f); // #0078D4 (Win11 Cobalt)
    const ImVec4 accent_hover     = ImVec4(0.100f, 0.540f, 0.920f, 1.00f); // #1A8AEB
    const ImVec4 accent_active    = ImVec4(0.000f, 0.400f, 0.720f, 1.00f); // #0066B8
    const ImVec4 accent_subtle    = ImVec4(0.000f, 0.471f, 0.831f, 0.25f);

    const ImVec4 border_subtle    = ImVec4(1.000f, 1.000f, 1.000f, 0.08f);
    const ImVec4 border_focus     = ImVec4(0.000f, 0.471f, 0.831f, 0.80f);

    const ImVec4 text_primary     = ImVec4(0.960f, 0.960f, 0.960f, 1.00f);
    const ImVec4 text_secondary   = ImVec4(0.650f, 0.650f, 0.650f, 1.00f);
    const ImVec4 text_disabled    = ImVec4(0.420f, 0.420f, 0.420f, 1.00f);

    // Apply colors
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
    colors[ImGuiCol_HeaderHovered]         = ImVec4(0.00f, 0.471f, 0.831f, 0.40f);
    colors[ImGuiCol_HeaderActive]          = ImVec4(0.00f, 0.471f, 0.831f, 0.60f);
    colors[ImGuiCol_Separator]             = border_subtle;
    colors[ImGuiCol_SeparatorHovered]      = accent;
    colors[ImGuiCol_SeparatorActive]       = accent_active;
    colors[ImGuiCol_ResizeGrip]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_ResizeGripHovered]     = accent_hover;
    colors[ImGuiCol_ResizeGripActive]      = accent_active;
    colors[ImGuiCol_Tab]                   = ImVec4(0.15f, 0.15f, 0.15f, 1.00f);
    colors[ImGuiCol_TabHovered]            = ImVec4(0.24f, 0.24f, 0.24f, 1.00f);
    colors[ImGuiCol_TabSelected]           = bg_card;
    colors[ImGuiCol_TabDimmed]             = ImVec4(0.13f, 0.13f, 0.13f, 1.00f);
    colors[ImGuiCol_TabDimmedSelected]     = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_PlotLines]             = accent;
    colors[ImGuiCol_PlotLinesHovered]      = accent_hover;
    colors[ImGuiCol_PlotHistogram]         = accent;
    colors[ImGuiCol_PlotHistogramHovered]  = accent_hover;
    colors[ImGuiCol_TableHeaderBg]         = ImVec4(0.18f, 0.18f, 0.18f, 1.00f);
    colors[ImGuiCol_TableBorderStrong]     = border_subtle;
    colors[ImGuiCol_TableBorderLight]      = ImVec4(1.00f, 1.00f, 1.00f, 0.04f);
    colors[ImGuiCol_TableRowBg]            = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
    colors[ImGuiCol_TableRowBgAlt]         = ImVec4(1.00f, 1.00f, 1.00f, 0.02f);
    colors[ImGuiCol_TextSelectedBg]        = accent_subtle;
    colors[ImGuiCol_DragDropTarget]        = accent;
    colors[ImGuiCol_NavHighlight]          = accent;
    colors[ImGuiCol_NavWindowingHighlight] = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
    colors[ImGuiCol_NavWindowingDimBg]     = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
    colors[ImGuiCol_ModalWindowDimBg]      = ImVec4(0.00f, 0.00f, 0.00f, 0.65f);
}

} // namespace Win11Theme
