#pragma once
#include "imgui.h"
#include "imgui_internal.h"

#ifndef IMGUI_DEFINE_MATH_OPERATORS
#define IMGUI_DEFINE_MATH_OPERATORS
#endif

// Helper for ImVec2 math
inline ImVec2 operator+(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x + rhs.x, lhs.y + rhs.y); }
inline ImVec2 operator-(const ImVec2& lhs, const ImVec2& rhs) { return ImVec2(lhs.x - rhs.x, lhs.y - rhs.y); }
inline ImVec2& operator+=(ImVec2& lhs, const ImVec2& rhs) { lhs.x += rhs.x; lhs.y += rhs.y; return lhs; }

// Windows 98 color constants
static const ImU32 WIN98_WHITE = IM_COL32(255, 255, 255, 255);
static const ImU32 WIN98_BLACK = IM_COL32(0, 0, 0, 255);
static const ImU32 WIN98_GRAY = IM_COL32(192, 192, 192, 255);
static const ImU32 WIN98_LIGHT_GRAY = IM_COL32(223, 223, 223, 255);
static const ImU32 WIN98_DARK_GRAY = IM_COL32(128, 128, 128, 255);

// Authentic Windows 98 3D border rendering (extracted from imgui-win98 fork)
inline void WinAddRect(ImDrawList* draw_list, const ImVec2& min, const ImVec2& max, bool inset) {
    ImU32 top_left = IM_COL32(255, 255, 255, 255);
    ImU32 bottom_right = IM_COL32(0, 0, 0, 255);
    ImU32 top_left_inner = IM_COL32(223, 223, 223, 255);
    ImU32 bottom_right_inner = IM_COL32(128, 128, 128, 255);

    if (inset) {
        ImU32 tmp = top_left; top_left = bottom_right; bottom_right = tmp;
        tmp = top_left_inner; top_left_inner = bottom_right_inner; bottom_right_inner = tmp;
    }

    ImVec2 a = min + ImVec2(0.5f, 0.5f);
    ImVec2 b = max - ImVec2(0.5f, 0.5f);

    // Outer border
    draw_list->PathLineTo(ImVec2(a.x, b.y));
    draw_list->PathLineTo(a);
    draw_list->PathLineTo(ImVec2(b.x, a.y));
    draw_list->PathStroke(top_left, false, 1.0f);

    draw_list->PathLineTo(ImVec2(a.x, b.y));
    draw_list->PathLineTo(b);
    draw_list->PathLineTo(ImVec2(b.x, a.y));
    draw_list->PathStroke(bottom_right, false, 1.0f);

    // Inner border
    draw_list->PathLineTo(ImVec2(a.x, b.y) + ImVec2(1, -1));
    draw_list->PathLineTo(a + ImVec2(1, 1));
    draw_list->PathLineTo(ImVec2(b.x, a.y) + ImVec2(-1, 1));
    draw_list->PathStroke(top_left_inner, false, 1.0f);

    draw_list->PathLineTo(ImVec2(a.x, b.y) + ImVec2(1, -1));
    draw_list->PathLineTo(b + ImVec2(-1, -1));
    draw_list->PathLineTo(ImVec2(b.x, a.y) + ImVec2(-1, 1));
    draw_list->PathStroke(bottom_right_inner, false, 1.0f);
}

inline void ApplyWin98Style() {
    ImGuiStyle& style = ImGui::GetStyle();
    ImVec4* colors = style.Colors;

    // Windows 98 colors
    const ImVec4 win98Gray = ImVec4(0.75f, 0.75f, 0.75f, 1.00f);
    const ImVec4 win98White = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
    const ImVec4 win98Black = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
    const ImVec4 win98Blue = ImVec4(0.00f, 0.00f, 0.50f, 1.00f);
    const ImVec4 win98DarkGray = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);

    // Main colors
    colors[ImGuiCol_Text] = win98Black;
    colors[ImGuiCol_TextDisabled] = win98DarkGray;
    colors[ImGuiCol_WindowBg] = win98Gray;
    colors[ImGuiCol_ChildBg] = win98Gray;
    colors[ImGuiCol_PopupBg] = win98Gray;
    colors[ImGuiCol_Border] = win98Black;
    colors[ImGuiCol_BorderShadow] = ImVec4(0.0f, 0.0f, 0.0f, 0.0f);
    colors[ImGuiCol_FrameBg] = win98White;
    colors[ImGuiCol_FrameBgHovered] = win98White;
    colors[ImGuiCol_FrameBgActive] = win98White;
    colors[ImGuiCol_TitleBg] = win98Blue;
    colors[ImGuiCol_TitleBgActive] = win98Blue;
    colors[ImGuiCol_TitleBgCollapsed] = win98Blue;
    colors[ImGuiCol_MenuBarBg] = win98Gray;
    colors[ImGuiCol_ScrollbarBg] = win98Gray;
    colors[ImGuiCol_ScrollbarGrab] = win98Gray;
    colors[ImGuiCol_ScrollbarGrabHovered] = win98DarkGray;
    colors[ImGuiCol_ScrollbarGrabActive] = win98Black;
    colors[ImGuiCol_CheckMark] = win98Black;
    colors[ImGuiCol_SliderGrab] = win98Gray;
    colors[ImGuiCol_SliderGrabActive] = win98DarkGray;
    colors[ImGuiCol_Button] = win98Gray;
    colors[ImGuiCol_ButtonHovered] = win98Gray;
    colors[ImGuiCol_ButtonActive] = win98Gray;
    colors[ImGuiCol_Header] = win98Gray;
    colors[ImGuiCol_HeaderHovered] = win98Gray;
    colors[ImGuiCol_HeaderActive] = win98Gray;
    colors[ImGuiCol_Separator] = win98DarkGray;
    colors[ImGuiCol_Tab] = win98Gray;
    colors[ImGuiCol_TabHovered] = win98Gray;
    colors[ImGuiCol_TabActive] = win98Gray;

    // Windows 98 style settings
    style.WindowPadding = ImVec2(2, 2);
    style.FramePadding = ImVec2(4, 2);
    style.ItemSpacing = ImVec2(8, 4);
    style.ItemInnerSpacing = ImVec2(4, 4);
    style.IndentSpacing = 20.0f;
    style.ScrollbarSize = 16.0f;
    style.GrabMinSize = 16.0f;

    // Square corners (classic Win98)
    style.WindowRounding = 0.0f;
    style.ChildRounding = 0.0f;
    style.FrameRounding = 0.0f;
    style.PopupRounding = 0.0f;
    style.ScrollbarRounding = 0.0f;
    style.GrabRounding = 0.0f;
    style.TabRounding = 0.0f;

    // Borders - Edit Borders here
    style.FrameBorderSize = 0.0f;
    style.WindowBorderSize = 1.0f;
    style.WindowTitleAlign = ImVec2(0.0f, 0.5f);
}

// Draw an authentic Windows 98 title bar
inline void DrawWin98TitleBar(const char* title, bool* p_open = nullptr, bool show_collapse = false) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* draw_list = window->DrawList;
    ImGuiContext& g = *GImGui;

    float title_bar_height = 20.0f; // Windows 98 title bar height
    ImVec2 title_bar_min = window->Pos;
    ImVec2 title_bar_max = ImVec2(window->Pos.x + window->Size.x, window->Pos.y + title_bar_height);

    // Check if title bar is being interacted with (for dragging)
    ImGuiID title_bar_id = ImGui::GetID("##TitleBar");
    ImRect title_bar_rect(title_bar_min, title_bar_max);

    // Make title bar draggable
    ImGui::SetCursorScreenPos(title_bar_min);
    ImGui::InvisibleButton("##TitleBarButton", ImVec2(window->Size.x - 40, title_bar_height));
    if (ImGui::IsItemActive() && ImGui::IsMouseDragging(ImGuiMouseButton_Left)) {
        // Move window
        ImVec2 delta = ImGui::GetIO().MouseDelta;
        ImVec2 new_pos = window->Pos + delta;
        ImGui::SetWindowPos(window, new_pos);
    }

    // Windows 98 gradient colors
    bool is_focused = ImGui::IsWindowFocused(ImGuiFocusedFlags_RootWindow);
    ImU32 col_left, col_right;

    if (is_focused) {
        // Active window - blue gradient (0,0,128) to (16,132,208)
        col_left = IM_COL32(0, 0, 128, 255);
        col_right = IM_COL32(16, 132, 208, 255);
    }
    else {
        // Inactive window - gray gradient
        col_left = IM_COL32(128, 128, 128, 255);
        col_right = IM_COL32(181, 181, 181, 255);
    }

    // Draw gradient title bar
    draw_list->AddRectFilledMultiColor(title_bar_min, title_bar_max,
        col_left, col_right, col_right, col_left);

    // Draw title text in white with slight shadow
    ImVec2 text_pos = ImVec2(title_bar_min.x + 6, title_bar_min.y + 3);
    draw_list->AddText(text_pos, IM_COL32(255, 255, 255, 255), title);

    // Draw close button (X)
    if (p_open) {
        float button_size = 16.0f;
        ImVec2 close_pos = ImVec2(title_bar_max.x - button_size - 2, title_bar_min.y + 2);
        ImRect close_bb(close_pos, close_pos + ImVec2(button_size, 14.0f));

        ImGui::SetCursorScreenPos(close_pos);
        ImGui::PushID("##CLOSE");
        bool close_hovered = ImGui::IsMouseHoveringRect(close_bb.Min, close_bb.Max);
        bool close_held = close_hovered && ImGui::IsMouseDown(ImGuiMouseButton_Left);
        bool close_pressed = close_hovered && ImGui::IsMouseReleased(ImGuiMouseButton_Left);

        // Draw close button background and border
        draw_list->AddRectFilled(close_bb.Min, close_bb.Max, WIN98_GRAY);
        WinAddRect(draw_list, close_bb.Min, close_bb.Max, close_held);

        // Draw X symbol (using proper multiplication sign)
        draw_list->AddText(ImVec2(close_bb.Min.x + 3.5f, close_bb.Min.y), WIN98_BLACK, "x");

        ImGui::PopID();

        if (close_pressed) {
            *p_open = false;
        }
    }

    // Add spacing after title bar
    ImGui::SetCursorPos(ImVec2(4, title_bar_height + 2));
}

// Authentic Windows 98 button using the exact fork technique
inline bool Win98Button(const char* label, const ImVec2& size = ImVec2(0, 0)) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    if (window->SkipItems)
        return false;

    ImGuiContext& g = *GImGui;
    const ImGuiStyle& style = g.Style;
    const ImGuiID id = window->GetID(label);
    const ImVec2 label_size = ImGui::CalcTextSize(label, NULL, true);

    ImVec2 pos = window->DC.CursorPos;
    ImVec2 button_size = ImGui::CalcItemSize(size, label_size.x + style.FramePadding.x * 2.0f, label_size.y + style.FramePadding.y * 2.0f);

    const ImRect bb(pos, pos + button_size);
    ImGui::ItemSize(button_size, style.FramePadding.y);
    if (!ImGui::ItemAdd(bb, id))
        return false;

    bool hovered, held;
    bool pressed = ImGui::ButtonBehavior(bb, id, &hovered, &held);

    // Render (exactly like imgui-win98 fork)
    window->DrawList->AddRectFilled(bb.Min, bb.Max, WIN98_GRAY);
    WinAddRect(window->DrawList, bb.Min, bb.Max, (held && hovered));

    // Render text in black
    ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
    ImGui::RenderTextClipped(bb.Min + style.FramePadding, bb.Max - style.FramePadding, label, NULL, &label_size, ImVec2(0.5f, 0.5f), &bb);
    ImGui::PopStyleColor();

    return pressed;
}

// Windows 98 style group box
inline void BeginWin98GroupBox(const char* label, const ImVec2& size = ImVec2(0, 0)) {
    ImGui::BeginGroup();
    ImVec2 label_size = ImGui::CalcTextSize(label);
    ImGui::Dummy(ImVec2(6, label_size.y * 0.5f));
    ImGui::Indent(6);
}

inline void EndWin98GroupBox(const char* label) {
    ImGuiWindow* window = ImGui::GetCurrentWindow();
    ImDrawList* draw_list = window->DrawList;

    ImGui::Unindent(6);
    ImGui::Dummy(ImVec2(0, 6));
    ImGui::EndGroup();

    ImRect bb = ImRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    ImVec2 label_size = ImGui::CalcTextSize(label);
    float label_offset = 10.0f;

    // Draw etched border with gap for label
    draw_list->AddLine(ImVec2(bb.Min.x, bb.Min.y + label_size.y * 0.5f),
        ImVec2(bb.Min.x + label_offset - 2, bb.Min.y + label_size.y * 0.5f), WIN98_DARK_GRAY);
    draw_list->AddLine(ImVec2(bb.Min.x + label_offset + label_size.x + 2, bb.Min.y + label_size.y * 0.5f),
        ImVec2(bb.Max.x, bb.Min.y + label_size.y * 0.5f), WIN98_DARK_GRAY);
    draw_list->AddLine(ImVec2(bb.Min.x, bb.Min.y + label_size.y * 0.5f), ImVec2(bb.Min.x, bb.Max.y), WIN98_DARK_GRAY);
    draw_list->AddLine(ImVec2(bb.Min.x, bb.Max.y), ImVec2(bb.Max.x, bb.Max.y), WIN98_WHITE);
    draw_list->AddLine(ImVec2(bb.Max.x, bb.Max.y), ImVec2(bb.Max.x, bb.Min.y + label_size.y * 0.5f), WIN98_WHITE);

    // Draw label
    draw_list->AddText(ImVec2(bb.Min.x + label_offset, bb.Min.y), WIN98_BLACK, label);
}