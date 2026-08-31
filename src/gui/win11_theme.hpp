#pragma once

#include "../../third_party/imgui/imgui.h"

namespace Win11Theme {

inline void ApplyFluentDarkTheme() {
    // Standard Dear ImGui Retro Dark Theme
    ImGui::StyleColorsDark();

    ImGuiStyle& style = ImGui::GetStyle();
    style.WindowRounding    = 0.0f;
    style.ChildRounding     = 0.0f;
    style.FrameRounding     = 0.0f;
    style.PopupRounding     = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.GrabRounding      = 0.0f;
    style.TabRounding       = 0.0f;
    style.WindowBorderSize  = 1.0f;
    style.FrameBorderSize   = 1.0f;
    style.PopupBorderSize   = 1.0f;
}

} // namespace Win11Theme
