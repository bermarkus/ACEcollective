#pragma once

// =====================================================================================
// ACEcollective UI Theme
// -------------------------------------------------------------------------------------
// Ported verbatim from ACEapp's include/ui/theme.h so the two apps in the ACEapp
// family share one visual language. Single source of truth for colors, spacing,
// rounding, and shared UI helpers -- all panels should pull visual constants from
// this file rather than defining their own magic values in-line.
//
// Pure header-only: no new translation units are added to the build.
// All helpers are inline and use only ImGui APIs (fully cross-platform).
// =====================================================================================

#include "imgui/imgui.h"
#include <cstdarg>
#include <cstdio>

namespace ace {
namespace ui {
namespace theme {

// -------------------------------------------------------------------------------------
// Colors
// -------------------------------------------------------------------------------------

// Workspace background (teal used for the main application window and signal area).
// Value: #384848
inline const ImVec4 kBgWorkspace = ImVec4(0.22f, 0.28f, 0.28f, 1.0f);

// Control-panel background (near-black, contrasts against the teal workspace).
inline const ImVec4 kBgPanel         = ImVec4(0.12f, 0.14f, 0.14f, 1.0f);
inline const ImVec4 kBgPanelTitle    = ImVec4(0.10f, 0.12f, 0.12f, 1.0f);
inline const ImVec4 kBgPanelTitleAct = ImVec4(0.08f, 0.10f, 0.10f, 1.0f);

// Panel borders / separators
inline const ImVec4 kBorderSubtle = ImVec4(0.3f, 0.3f, 0.3f, 0.5f);

// Canvas background for detached analysis windows (InterBrainConnectivityPanel).
// Slightly off-black to keep faint detail visible.
inline const ImU32 kBgCanvas = IM_COL32(15, 15, 15, 255);

// One shared hover color for every interactive element -- buttons, dropdowns/
// sliders/checkboxes/inputs, and collapsing-header rows all light up the same
// light blue on mouse-over, instead of each widget kind having its own
// hover shade. Solid/opaque so it's identical regardless of what's under it.
inline const ImVec4 kHoverAccent = ImVec4(0.26f, 0.59f, 0.98f, 1.0f);

// One shared base/idle color for every interactive control surface --
// buttons, dropdowns/sliders/checkboxes/inputs, and collapsing-header rows --
// so they all read as the same "clickable" blue instead of some being gray
// and others blue.
inline const ImVec4 kControlBg       = ImVec4(0.27f, 0.40f, 0.55f, 1.0f);
inline const ImVec4 kControlBgActive = ImVec4(0.20f, 0.30f, 0.42f, 1.0f);

// Button palette
inline const ImVec4 kButton        = kControlBg;
inline const ImVec4 kButtonHovered = kHoverAccent;
inline const ImVec4 kButtonActive  = kControlBgActive;

// Frame backgrounds for sliders/checkboxes/inputs/combos.
inline const ImVec4 kFrameBg        = kControlBg;
inline const ImVec4 kFrameBgHovered = kHoverAccent;
inline const ImVec4 kFrameBgActive  = kControlBgActive;

// Accent for active/selected elements (blue used for toggle-button highlight)
inline const ImVec4 kAccentActive   = ImVec4(0.16f, 0.29f, 0.48f, 1.0f);
inline const ImVec4 kAccentInactive = ImVec4(0.20f, 0.20f, 0.20f, 1.0f);

// Green "running/active" button triad (e.g. a toggle button while its action
// is currently in progress).
inline const ImVec4 kActionRunning         = ImVec4(0.2f, 0.7f, 0.2f, 1.0f);
inline const ImVec4 kActionRunningHovered  = ImVec4(0.3f, 0.8f, 0.3f, 1.0f);
inline const ImVec4 kActionRunningActive   = ImVec4(0.1f, 0.6f, 0.1f, 1.0f);

// Native ImGui tab colors -- keep aligned with the custom top-bar tabs so that
// nested BeginTabBar tabs share the same visual language as the main navigation.
inline const ImVec4 kTab             = ImVec4(0.25f, 0.27f, 0.27f, 1.0f);
inline const ImVec4 kTabHovered      = ImVec4(0.32f, 0.34f, 0.34f, 1.0f);
inline const ImVec4 kTabActive       = ImVec4(0.08f, 0.10f, 0.10f, 1.0f);
inline const ImVec4 kTabUnfocused    = ImVec4(0.18f, 0.20f, 0.20f, 1.0f);
inline const ImVec4 kTabUnfocusedAct = ImVec4(0.12f, 0.14f, 0.14f, 1.0f);

// Status / semantic text colors
inline const ImVec4 kStatusGood    = ImVec4(0.30f, 0.80f, 0.40f, 1.0f);
inline const ImVec4 kStatusWarn    = ImVec4(1.00f, 0.70f, 0.20f, 1.0f);
inline const ImVec4 kStatusError   = ImVec4(0.90f, 0.30f, 0.30f, 1.0f);
inline const ImVec4 kStatusInfo    = ImVec4(0.60f, 0.75f, 0.95f, 1.0f);
inline const ImVec4 kStatusMuted   = ImVec4(0.60f, 0.60f, 0.60f, 1.0f);

// Section header color (slightly brighter than body text to draw the eye)
inline const ImVec4 kSectionHeader = ImVec4(0.85f, 0.90f, 1.00f, 1.0f);

// Coherence-strength ramp (Low -> High), reused from ACEapp's signal-quality
// ramp so "how strong is this connection" reads with the same color language
// as "how good is this signal" -- e.g. an arc's color in the two-hemisphere
// view scales along this same five-stop gradient.
inline const ImVec4 kCoherenceLow       = ImVec4(0.424f, 0.459f, 0.490f, 1.0f);
inline const ImVec4 kCoherenceModerate  = ImVec4(1.000f, 0.757f, 0.027f, 1.0f);
inline const ImVec4 kCoherenceHigh      = ImVec4(1.000f, 0.522f, 0.106f, 1.0f);
inline const ImVec4 kCoherenceVeryHigh  = ImVec4(0.863f, 0.208f, 0.271f, 1.0f);

// Per-participant identity colors -- used to tint each participant's channel
// ring/label in the two-hemisphere view so up to 6 simultaneous participants
// stay visually distinguishable. Cycles if there are ever more than 6.
inline const ImVec4 kParticipantColors[6] = {
    ImVec4(0.36f, 0.72f, 0.90f, 1.0f), // blue
    ImVec4(0.90f, 0.55f, 0.30f, 1.0f), // orange
    ImVec4(0.55f, 0.80f, 0.40f, 1.0f), // green
    ImVec4(0.85f, 0.45f, 0.75f, 1.0f), // pink
    ImVec4(0.80f, 0.75f, 0.35f, 1.0f), // yellow
    ImVec4(0.65f, 0.55f, 0.90f, 1.0f), // violet
};

// Convert a theme ImVec4 color to a packed ImU32, optionally overriding alpha
// (0-1 range).
inline ImU32 to_im_col32(const ImVec4& c, float alpha_override = -1.0f)
{
    float a = (alpha_override >= 0.0f) ? alpha_override : c.w;
    return IM_COL32(
        static_cast<int>(c.x * 255.0f + 0.5f),
        static_cast<int>(c.y * 255.0f + 0.5f),
        static_cast<int>(c.z * 255.0f + 0.5f),
        static_cast<int>(a * 255.0f + 0.5f));
}

// -------------------------------------------------------------------------------------
// Spacing, rounding, sizing
// -------------------------------------------------------------------------------------

inline constexpr float kSpaceXS = 4.0f;
inline constexpr float kSpaceS  = 8.0f;
inline constexpr float kSpaceM  = 12.0f;
inline constexpr float kSpaceL  = 16.0f;

inline constexpr float kRoundingPanel     = 8.0f;
inline constexpr float kRoundingFrame     = 4.0f;
inline constexpr float kRoundingPopup     = 4.0f;
inline constexpr float kRoundingTab       = 4.0f;
inline constexpr float kRoundingScrollbar = 3.0f;
inline constexpr float kRoundingGrab      = 3.0f;

inline constexpr float kFontScaleEmphasis = 1.25f;
inline constexpr float kFontScaleLarge    = 1.40f;

// -------------------------------------------------------------------------------------
// Button sizing
// -------------------------------------------------------------------------------------

inline float equal_button_width(int count)
{
    if (count <= 0) return 0.0f;
    float avail = ImGui::GetContentRegionAvail().x;
    float spacing = ImGui::GetStyle().ItemSpacing.x * static_cast<float>(count - 1);
    return (avail - spacing) / static_cast<float>(count);
}

// -------------------------------------------------------------------------------------
// Status level enum (for text_status helper)
// -------------------------------------------------------------------------------------

enum class StatusLevel {
    Good,
    Warn,
    Error,
    Info,
    Muted,
};

inline const ImVec4& status_color(StatusLevel level)
{
    switch (level) {
        case StatusLevel::Good:  return kStatusGood;
        case StatusLevel::Warn:  return kStatusWarn;
        case StatusLevel::Error: return kStatusError;
        case StatusLevel::Info:  return kStatusInfo;
        case StatusLevel::Muted: return kStatusMuted;
    }
    return kStatusInfo;
}

inline void text_status(StatusLevel level, const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    ImGui::PushStyleColor(ImGuiCol_Text, status_color(level));
    ImGui::TextV(fmt, args);
    ImGui::PopStyleColor();
    va_end(args);
}

inline void section_header(const char* label, const char* help_marker = nullptr)
{
    ImGui::Spacing();
    ImGui::PushStyleColor(ImGuiCol_Text, kSectionHeader);
    ImGui::TextUnformatted(label);
    ImGui::PopStyleColor();

    if (help_marker && help_marker[0] != '\0') {
        ImGui::SameLine();
        ImGui::TextDisabled("(?)");
        if (ImGui::IsItemHovered()) {
            ImGui::BeginTooltip();
            ImGui::PushTextWrapPos(ImGui::GetFontSize() * 35.0f);
            ImGui::TextUnformatted(help_marker);
            ImGui::PopTextWrapPos();
            ImGui::EndTooltip();
        }
    }

    ImGui::Separator();
    ImGui::Spacing();
}

// -------------------------------------------------------------------------------------
// Analysis window helpers, ported verbatim from ACEapp's theme.h.
// -------------------------------------------------------------------------------------

inline bool begin_analysis_window(const char* title,
                                  bool* p_visible,
                                  bool* p_maximized,
                                  const ImVec2& default_size = ImVec2(0, 0))
{
    if (p_maximized && *p_maximized) {
        ImGuiViewport* viewport = ImGui::GetMainViewport();
        ImGui::SetNextWindowPos(viewport->Pos);
        ImGui::SetNextWindowSize(viewport->Size);
    } else {
        ImGuiViewport* vp = ImGui::GetMainViewport();
        ImVec2 size = (default_size.x > 0.0f && default_size.y > 0.0f)
            ? default_size
            : ImVec2(vp->Size.x * 0.5f, vp->Size.y * 0.5f);
        ImVec2 pos(vp->Pos.x + (vp->Size.x - size.x) * 0.5f,
                   vp->Pos.y + (vp->Size.y - size.y) * 0.5f);
        ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(pos,  ImGuiCond_FirstUseEver);
    }

    ImGuiWindowFlags flags = ImGuiWindowFlags_NoCollapse;
    if (p_maximized && *p_maximized) {
        flags |= ImGuiWindowFlags_NoResize | ImGuiWindowFlags_NoMove;
    }

    bool open = ImGui::Begin(title, p_visible, flags);

    if (open && p_maximized) {
        const float btn_w = 100.0f;
        ImGui::SameLine(ImGui::GetWindowWidth() - btn_w - 20.0f);
        const char* label = *p_maximized ? "Minimize" : "Maximize";
        if (ImGui::Button(label, ImVec2(btn_w, 0))) {
            const bool was_maximized = *p_maximized;
            *p_maximized = !*p_maximized;

            if (was_maximized) {
                ImGuiViewport* vp = ImGui::GetMainViewport();
                ImVec2 size(vp->Size.x * 0.5f, vp->Size.y * 0.5f);
                ImVec2 pos(vp->Pos.x + (vp->Size.x - size.x) * 0.5f,
                           vp->Pos.y + (vp->Size.y - size.y) * 0.5f);
                ImGui::SetWindowSize(size);
                ImGui::SetWindowPos(pos);
            }
        }
    }

    return open;
}

inline void end_analysis_window()
{
    ImGui::End();
}

} // namespace theme
} // namespace ui
} // namespace ace
