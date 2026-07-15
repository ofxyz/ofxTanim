// REF (tanim): originally based on the imguizmo's ImSequencer.cpp:
// https://github.com/CedricGuillemet/ImGuizmo/blob/71f14292205c3317122b39627ed98efce137086a/ImSequencer.cpp

// https://github.com/CedricGuillemet/ImGuizmo
// v1.92.5 WIP
//
// The MIT License(MIT)
//
// Copyright(c) 2016-2021 Cedric Guillemet
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files(the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and / or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions :
//
// The above copyright notice and this permission notice shall be included in all
// copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
// SOFTWARE.

#include "tanim/timeliner.hpp"
#include "tanim/timeline.hpp"
#include "tanim/helpers.hpp"
#include "tanim/tanim_user.hpp"

#include <algorithm>
#include <cmath>

namespace tanim::internal
{

static bool TimelinerAddDelButton(ImDrawList* draw_list, ImVec2 pos, bool add = true)
{
    const ImGuiIO& io = ImGui::GetIO();
    const float sc = ImGui::GetFontSize() / 13.0f;
    const float sz = std::roundf(16.0f * sc);
    ImRect btn_rect(pos, ImVec2(pos.x + sz, pos.y + sz));
    const bool over_btn = btn_rect.Contains(io.MousePos);
    const bool contained_click = over_btn && btn_rect.Contains(io.MouseClickedPos[0]);
    const bool clicked_btn = contained_click && io.MouseReleased[0];
    const unsigned int btn_color = over_btn ? 0xAAEAFFAA : 0x77A3B2AA;
    if (contained_click && io.MouseDownDuration[0] > 0) btn_rect.Expand(2.0f);

    const float mid_y = pos.y + sz / 2.0f - 0.5f;
    const float mid_x = pos.x + sz / 2.0f - 0.5f;
    const float pad   = std::roundf(3.0f * sc);
    draw_list->AddRect(btn_rect.Min, btn_rect.Max, btn_color, 4);
    draw_list->AddLine(ImVec2(btn_rect.Min.x + pad, mid_y), ImVec2(btn_rect.Max.x - pad, mid_y), btn_color, 2);
    if (add) draw_list->AddLine(ImVec2(mid_x, btn_rect.Min.y + pad), ImVec2(mid_x, btn_rect.Max.y - pad), btn_color, 2);
    return clicked_btn;
}

bool Timeliner(TimelineData& data,
               int* current_frame,
               bool* expanded,
               int* selected_sequence,
               int* first_frame,
               int timeliner_flags)
{
    bool ret = false;
    ImGuiIO& io = ImGui::GetIO();
    int cx = static_cast<int>(io.MousePos.x);
    int cy = static_cast<int>(io.MousePos.y);

    // Scale all pixel dimensions relative to ImGui's default 13 px font so the
    // timeliner remains readable at any font size / DPI setting.
    const float sc = ImGui::GetFontSize() / 13.0f;

    static float frame_pixel_width = 10.f;
    static float frame_pixel_width_target = 10.f;

    static int moving_entry = -1;
    static int moving_pos = -1;
    static int moving_part = -1;
    int del_entry = -1;
    // int dup_entry = -1;
    const int item_height = static_cast<int>(20.0f * sc);

    bool popup_opened = false;
    int sequence_count = Timeline::GetSequenceCount(data);
    if (!sequence_count) return false;
    ImGui::BeginGroup();

    ImDrawList* draw_list = ImGui::GetWindowDrawList();
    ImVec2 canvas_pos = ImGui::GetCursorScreenPos();      // ImDrawList API uses screen coordinates!
    ImVec2 canvas_size = ImGui::GetContentRegionAvail();  // Resize canvas to what's available
    int first_frame_used = first_frame ? *first_frame : 0;

    // Legend column width: draggable splitter in expanded timeliner. Static persists across frames.
    static float s_timeliner_legend_px = -1.f;
    static bool s_timeliner_legend_split_drag = false;

    const float legend_min = std::max(92.0f * sc, static_cast<float>(item_height + 48));
    const float legend_ideal = 168.0f * sc;
    const float legend_drag_max = std::max(legend_min + 120.0f * sc, 520.0f * sc);
    const float timeline_min_px = std::max(96.0f * sc, 72.0f);
    const float by_panel = canvas_size.x * 0.34f;

    auto reclampLegend = [&]() {
        const float mx = std::max(legend_min, canvas_size.x - timeline_min_px);
        const float hi = std::min(legend_drag_max, mx);
        const float defaults =
            ImClamp(std::min(legend_ideal, std::max(legend_min, by_panel)), legend_min, hi);
        if (s_timeliner_legend_px < 0.f)
        {
            s_timeliner_legend_px = defaults;
        }
        s_timeliner_legend_px = ImClamp(s_timeliner_legend_px, legend_min, hi);
    };
    reclampLegend();

    int legend_width = static_cast<int>(std::lround(s_timeliner_legend_px));

    int controlHeight = sequence_count * item_height;
    for (int i = 0; i < sequence_count; i++) controlHeight += static_cast<int>(Timeline::GetCustomHeight(data, i));
    int frame_count = ImMax(Timeline::GetMaxFrame(data) - Timeline::GetMinFrame(data), 1);

    static bool moving_scroll_bar = false;
    static bool moving_current_frame = false;
    struct CustomDraw
    {
        int m_index;
        ImRect m_custom_rect;
        ImRect m_legend_rect;
        ImRect m_clipping_rect;
        ImRect m_legend_clipping_rect;
    };
    ImVector<CustomDraw> custom_draws;
    ImVector<CustomDraw> compact_custom_draws;

    ImRect region_rect(canvas_pos, canvas_pos + canvas_size);

    frame_pixel_width_target = ImClamp(frame_pixel_width_target, 0.1f, 50.f);

    frame_pixel_width = ImLerp(frame_pixel_width, frame_pixel_width_target, 0.33f);

    // --
    if (expanded && !*expanded)
    {
        ImGui::InvisibleButton("canvas", ImVec2(canvas_size.x - canvas_pos.x, static_cast<float>(item_height)));
        draw_list->AddRectFilled(canvas_pos, ImVec2(canvas_size.x + canvas_pos.x, canvas_pos.y + item_height), 0xFF3D3837, 0);
        char tmps[512];
        ImFormatString(tmps, IM_ARRAYSIZE(tmps), Timeline::GetName(data).c_str(), frame_count, sequence_count);
        draw_list->AddText(ImVec2(canvas_pos.x + std::roundf(26.0f * sc), canvas_pos.y + 2), 0xFFFFFFFF, tmps);
    }
    else
    {
        bool has_scroll_bar(true);
        /*
        int framesPixelWidth = int(frameCount * framePixelWidth);
        if ((framesPixelWidth + legendWidth) >= canvas_size.x)
        {
            hasScrollBar = true;
        }
        */
        // test scroll area
        ImVec2 header_size(canvas_size.x, static_cast<float>(item_height));
        ImVec2 scroll_bar_size(canvas_size.x, std::roundf(14.0f * sc));

        const float splitter_max_y = canvas_pos.y + canvas_size.y - (has_scroll_bar ? scroll_bar_size.y : 0.f);
        const float splitter_half_w = std::max(3.0f, 5.0f * sc);
        ImRect legend_split_hit(ImVec2(canvas_pos.x + static_cast<float>(legend_width) - splitter_half_w, canvas_pos.y),
                                ImVec2(canvas_pos.x + static_cast<float>(legend_width) + splitter_half_w, splitter_max_y));
        const bool legend_split_hover =
            legend_split_hit.Contains(io.MousePos) && ImGui::IsWindowHovered(ImGuiHoveredFlags_AllowWhenBlockedByPopup);
        const bool block_legend_split =
            (moving_entry >= 0) || moving_scroll_bar || moving_current_frame;

        if (block_legend_split)
        {
            s_timeliner_legend_split_drag = false;
        }
        else if (s_timeliner_legend_split_drag)
        {
            if (!io.MouseDown[0])
            {
                s_timeliner_legend_split_drag = false;
            }
            else
            {
                s_timeliner_legend_px += io.MouseDelta.x;
                reclampLegend();
                legend_width = static_cast<int>(std::lround(s_timeliner_legend_px));
            }
        }
        else if (legend_split_hover && io.MouseClicked[0])
        {
            s_timeliner_legend_split_drag = true;
        }

        const bool splitter_draw_hot = legend_split_hover || s_timeliner_legend_split_drag;
        if (!block_legend_split && splitter_draw_hot)
        {
            ImGui::SetMouseCursor(ImGuiMouseCursor_ResizeEW);
        }

        const int visible_frame_count = static_cast<int>(floorf((canvas_size.x - legend_width) / frame_pixel_width));
        const float bar_width_ratio = ImMin(visible_frame_count / static_cast<float>(frame_count), 1.f);
        const float bar_width_in_pixels = bar_width_ratio * (canvas_size.x - legend_width);

        static bool panning_view = false;
        static ImVec2 panning_view_source;
        static int panning_view_frame;
        if (ImGui::IsWindowFocused() && io.KeyAlt && io.MouseDown[2])
        {
            if (!panning_view)
            {
                panning_view_source = io.MousePos;
                panning_view = true;
                panning_view_frame = *first_frame;
            }
            *first_frame = panning_view_frame -
                           static_cast<int>((io.MousePos.x - panning_view_source.x) / frame_pixel_width);
            *first_frame = ImClamp(*first_frame,
                                    Timeline::GetMinFrame(data),
                                    Timeline::GetMaxFrame(data) - visible_frame_count);
        }
        if (panning_view && !io.MouseDown[2])
        {
            panning_view = false;
        }

        frame_count = Timeline::GetMaxFrame(data) - Timeline::GetMinFrame(data);
        if (visible_frame_count >= frame_count && first_frame)
        {
            *first_frame = Timeline::GetMinFrame(data);
        }

        ImGui::InvisibleButton("topBar", header_size);
        draw_list->AddRectFilled(canvas_pos, canvas_pos + header_size, 0xFFFF0000, 0);
        ImVec2 child_frame_pos = ImGui::GetCursorScreenPos();
        ImVec2 child_frame_size(canvas_size.x, canvas_size.y - 8.f - header_size.y - (has_scroll_bar ? scroll_bar_size.y : 0));
        ImGui::PushStyleColor(ImGuiCol_FrameBg, 0);
        ImGui::BeginChild(889, child_frame_size, ImGuiChildFlags_FrameStyle);
        data.m_focused = ImGui::IsWindowFocused();
        ImGui::InvisibleButton("contentBar", ImVec2(canvas_size.x, static_cast<float>(controlHeight)));
        const ImVec2 content_min = ImGui::GetItemRectMin();
        const ImVec2 content_max = ImGui::GetItemRectMax();
        const float content_height = content_max.y - content_min.y;

        // full background
        draw_list->AddRectFilled(canvas_pos, canvas_pos + canvas_size, 0xFF242424, 0);

        // current frame top
        ImRect topRect(ImVec2(canvas_pos.x + legend_width, canvas_pos.y),
                       ImVec2(canvas_pos.x + canvas_size.x, canvas_pos.y + item_height));

        if (!moving_current_frame && !moving_scroll_bar && moving_entry == -1 && timeliner_flags & TIMELINER_CHANGE_FRAME &&
            current_frame && *current_frame >= 0 && topRect.Contains(io.MousePos) && io.MouseDown[0])
        {
            moving_current_frame = true;
        }
        if (moving_current_frame)
        {
            if (frame_count)
            {
                *current_frame = static_cast<int>((io.MousePos.x - topRect.Min.x) / frame_pixel_width) + first_frame_used;
                if (*current_frame < Timeline::GetMinFrame(data)) *current_frame = Timeline::GetMinFrame(data);
                if (*current_frame >= Timeline::GetMaxFrame(data)) *current_frame = Timeline::GetMaxFrame(data);
            }
            if (!io.MouseDown[0]) moving_current_frame = false;
        }

        // header
        /* removed AddSequence here, because we do it in another place
        draw_list->AddRectFilled(canvas_pos, ImVec2(canvas_size.x + canvas_pos.x, canvas_pos.y + ItemHeight), 0xFF3D3837, 0);
        if (timeliner_flags & TIMELINER_ADD_SEQUENCE)
        {
            if (TimelinerAddDelButton(draw_list, ImVec2(canvas_pos.x + legendWidth - ItemHeight, canvas_pos.y + 2), true))
                ImGui::OpenPopup("addEntry");

            if (ImGui::BeginPopup("addEntry"))
            {
                for (int i = 0; i < Timeline::GetSequenceCount(data); i++)
                    if (ImGui::Selectable(GEtSe timeline->GetSequenceTypeName(i)))
                    {
                        timeline->AddSequence(i);
                        *selected_sequence = timeline->GetSequenceCount() - 1;
                    }

                ImGui::EndPopup();
                popupOpened = true;
            }
        }
        */

        // header frame number and lines
        int mod_frame_count = 10;
        int frame_step = 1;
        while ((mod_frame_count * frame_pixel_width) < 150)
        {
            mod_frame_count *= 2;
            frame_step *= 2;
        }
        int half_mod_frame_count = mod_frame_count / 2;

        auto drawLine = [&](int i, int regionHeight)
        {
            const bool base_index =
                ((i % mod_frame_count) == 0) || (i == Timeline::GetMaxFrame(data) || i == Timeline::GetMinFrame(data));
            const bool half_index = (i % half_mod_frame_count) == 0;
            const int px = static_cast<int>(canvas_pos.x) + static_cast<int>(i * frame_pixel_width) + legend_width -
                           static_cast<int>(first_frame_used * frame_pixel_width);
            const int tiret_start = base_index ? 4 : (half_index ? 10 : 14);
            const int tiret_end = base_index ? regionHeight : item_height;

            if (px <= (canvas_size.x + canvas_pos.x) && px >= (canvas_pos.x + legend_width))
            {
                draw_list->AddLine(ImVec2(static_cast<float>(px), canvas_pos.y + static_cast<float>(tiret_start)),
                                   ImVec2(static_cast<float>(px), canvas_pos.y + static_cast<float>(tiret_end) - 1),
                                   0xFF606060,
                                   1);

                draw_list->AddLine(ImVec2(static_cast<float>(px), canvas_pos.y + static_cast<float>(item_height)),
                                   ImVec2(static_cast<float>(px), canvas_pos.y + static_cast<float>(regionHeight) - 1),
                                   0x30606060,
                                   1);
            }

            if (base_index && px > (canvas_pos.x + legend_width))
            {
                char tmps[512];
                ImFormatString(tmps, IM_ARRAYSIZE(tmps), "%d", i);
                draw_list->AddText(ImVec2(static_cast<float>(px) + 3.f, canvas_pos.y), 0xFFBBBBBB, tmps);
            }
        };

        auto drawLineContent = [&](int i, int /*regionHeight*/)
        {
            const int px = static_cast<int>(canvas_pos.x) + static_cast<int>(i * frame_pixel_width) + legend_width -
                           static_cast<int>(first_frame_used * frame_pixel_width);
            const int tiret_start = static_cast<int>(content_min.y);
            const int tiret_end = static_cast<int>(content_max.y);

            if (px <= (canvas_size.x + canvas_pos.x) && px >= (canvas_pos.x + legend_width))
            {
                // draw_list->AddLine(ImVec2((float)px, canvas_pos.y + (float)tiretStart), ImVec2((float)px, canvas_pos.y +
                // (float)tiretEnd - 1), 0xFF606060, 1);

                draw_list->AddLine(ImVec2(static_cast<float>(px), static_cast<float>(tiret_start)),
                                   ImVec2(static_cast<float>(px), static_cast<float>(tiret_end)),
                                   0x30606060,
                                   1);
            }
        };
        for (int i = Timeline::GetMinFrame(data); i <= Timeline::GetMaxFrame(data); i += frame_step)
        {
            drawLine(i, item_height);
        }
        drawLine(Timeline::GetMinFrame(data), item_height);
        drawLine(Timeline::GetMaxFrame(data), item_height);
        /*
                 draw_list->AddLine(canvas_pos, ImVec2(canvas_pos.x, canvas_pos.y + controlHeight), 0xFF000000, 1);
                 draw_list->AddLine(ImVec2(canvas_pos.x, canvas_pos.y + ItemHeight), ImVec2(canvas_size.x, canvas_pos.y +
           ItemHeight), 0xFF000000, 1);
                 */
        // clip content

        draw_list->PushClipRect(child_frame_pos, child_frame_pos + child_frame_size, true);

        // draw item names in the legend rect on the left
        size_t custom_height = 0;
        for (int i = 0; i < sequence_count; i++)
        {
            ImVec2 tpos(content_min.x + 3, content_min.y + i * item_height + 2 + custom_height);
            const float del_btn_x = content_min.x + static_cast<float>(legend_width - item_height + 2 - 10);
            draw_list->PushClipRect(ImVec2(content_min.x, tpos.y - 1.0f),
                                    ImVec2(del_btn_x - 2.0f, tpos.y + static_cast<float>(item_height) + 2.0f),
                                    true);
            draw_list->AddText(tpos, 0xFFFFFFFF, Timeline::GetSequenceLabel(data, i).c_str());
            draw_list->PopClipRect();

            if (timeliner_flags & TIMELINER_DELETE_SEQUENCE)
            {
                if (TimelinerAddDelButton(draw_list,
                                          ImVec2(content_min.x + legend_width - item_height + 2 - 10, tpos.y + 2),
                                          false))
                {
                    del_entry = i;
                }

                /*
                if (TimelinerAddDelButton(draw_list,
                                          ImVec2(content_min.x + legend_width - item_height - item_height + 2 - 10, tpos.y + 2),
                                          true))
                {
                    dup_entry = i;
                }
                */
            }
            custom_height += Timeline::GetCustomHeight(data, i);
        }

        // slots background
        custom_height = 0;
        for (int i = 0; i < sequence_count; i++)
        {
            unsigned int col = (i & 1) ? 0xFF3A3636 : 0xFF413D3D;

            size_t local_custom_height = Timeline::GetCustomHeight(data, i);
            ImVec2 pos = ImVec2(content_min.x + legend_width, content_min.y + item_height * i + 1 + custom_height);
            ImVec2 sz = ImVec2(canvas_size.x + canvas_pos.x, pos.y + item_height - 1 + local_custom_height);
            if (!popup_opened && cy >= pos.y && cy < pos.y + (item_height + local_custom_height) && moving_entry == -1 &&
                cx > content_min.x && cx < content_min.x + canvas_size.x)
            {
                col += 0x80201008;
                pos.x -= legend_width;
            }
            draw_list->AddRectFilled(pos, sz, col, 0);
            custom_height += local_custom_height;
        }

        draw_list->PushClipRect(child_frame_pos + ImVec2(static_cast<float>(legend_width), 0.f),
                                child_frame_pos + child_frame_size,
                                true);

        // vertical frame lines in content area
        for (int i = Timeline::GetMinFrame(data); i <= Timeline::GetMaxFrame(data); i += frame_step)
        {
            drawLineContent(i, static_cast<int>(content_height));
        }
        drawLineContent(Timeline::GetMinFrame(data), static_cast<int>(content_height));
        drawLineContent(Timeline::GetMaxFrame(data), static_cast<int>(content_height));

        // selection
        bool selected = selected_sequence && (*selected_sequence >= 0);
        if (selected)
        {
            custom_height = 0;
            for (int i = 0; i < *selected_sequence; i++) custom_height += Timeline::GetCustomHeight(data, i);
            draw_list->AddRectFilled(
                ImVec2(content_min.x, content_min.y + item_height * *selected_sequence + custom_height),
                ImVec2(content_min.x + canvas_size.x, content_min.y + item_height * (*selected_sequence + 1) + custom_height),
                0x801080FF,
                1.f);
        }

        // slots
        custom_height = 0;
        for (int i = 0; i < sequence_count; i++)
        {
            int start = Timeline::GetSequenceFirstFrame(data, i);
            int end = Timeline::GetSequenceLastFrame(data, i);
            unsigned int color = Timeline::GetColor(data);
            size_t local_custom_height = Timeline::GetCustomHeight(data, i);

            ImVec2 pos = ImVec2(content_min.x + legend_width - first_frame_used * frame_pixel_width,
                                content_min.y + item_height * i + 1 + custom_height);
            ImVec2 slot_p1(pos.x + start * frame_pixel_width, pos.y + 2);
            ImVec2 slot_p2(pos.x + end * frame_pixel_width + frame_pixel_width, pos.y + item_height - 2);
            ImVec2 slot_p3(pos.x + end * frame_pixel_width + frame_pixel_width, pos.y + item_height - 2 + local_custom_height);
            unsigned int slot_color = color | 0xFF000000;
            unsigned int slot_color_half = (color & 0xFFFFFF) | 0x40000000;

            if (slot_p1.x <= (canvas_size.x + content_min.x) && slot_p2.x >= (content_min.x + legend_width))
            {
                draw_list->AddRectFilled(slot_p1, slot_p3, slot_color_half, 2);
                draw_list->AddRectFilled(slot_p1, slot_p2, slot_color, 2);
            }
            if (ImRect(slot_p1, slot_p2).Contains(io.MousePos) && io.MouseDoubleClicked[0])
            {
                Timeline::DoubleClick(data, i);
            }
            // Ensure grabbable handles
            const float max_handle_width = slot_p2.x - slot_p1.x / 3.0f;
            const float min_handle_width = ImMin(10.0f, max_handle_width);
            const float handle_width = ImClamp(frame_pixel_width / 2.0f, min_handle_width, max_handle_width);
            ImRect rects[3] = {ImRect(slot_p1, ImVec2(slot_p1.x + handle_width, slot_p2.y)),
                               ImRect(ImVec2(slot_p2.x - handle_width, slot_p1.y), slot_p2),
                               ImRect(slot_p1, slot_p2)};

            const unsigned int quadColor[] = {0xFFFFFFFF, 0xFFFFFFFF, slot_color + (selected ? 0 : 0x202020)};
            if (moving_entry == -1 &&
                (timeliner_flags & TIMELINER_EDIT_STARTEND))  // TODOFOCUS && backgroundRect.Contains(io.MousePos))
            {
                for (int j = 2; j >= 0; j--)
                {
                    ImRect& rc = rects[j];
                    if (!rc.Contains(io.MousePos)) continue;
                    draw_list->AddRectFilled(rc.Min, rc.Max, quadColor[j], 2);
                }

                for (int j = 0; j < 3; j++)
                {
                    ImRect& rc = rects[j];
                    if (!rc.Contains(io.MousePos)) continue;
                    if (!ImRect(child_frame_pos, child_frame_pos + child_frame_size).Contains(io.MousePos)) continue;
                    if (ImGui::IsMouseClicked(0) && !moving_scroll_bar && !moving_current_frame)
                    {
                        moving_entry = i;
                        moving_pos = cx;
                        moving_part = j + 1;
                        Timeline::BeginEdit(data, moving_entry);
                        break;
                    }
                }
            }

            // custom draw
            if (local_custom_height > 0)
            {
                ImVec2 rp(canvas_pos.x, content_min.y + item_height * i + 1 + custom_height);
                ImRect custom_rect(
                    rp + ImVec2(legend_width - (first_frame_used - Timeline::GetMinFrame(data) - 0.5f) * frame_pixel_width,
                                static_cast<float>(item_height)),
                    rp +
                        ImVec2(legend_width + (Timeline::GetMaxFrame(data) - first_frame_used - 0.5f + 2.f) * frame_pixel_width,
                               static_cast<float>(local_custom_height + item_height)));
                ImRect clipping_rect(rp + ImVec2(static_cast<float>(legend_width), static_cast<float>(item_height)),
                                     rp + ImVec2(canvas_size.x, static_cast<float>(local_custom_height + item_height)));

                ImRect legend_rect(rp + ImVec2(0.f, static_cast<float>(item_height)),
                                   rp + ImVec2(static_cast<float>(legend_width),
                                               static_cast<float>(item_height + local_custom_height)));
                ImRect legend_clipping_rect(legend_rect.Min, legend_rect.Max);
                custom_draws.push_back({i, custom_rect, legend_rect, clipping_rect, legend_clipping_rect});
            }
            else
            {
                ImVec2 rp(canvas_pos.x, content_min.y + item_height * i + custom_height);
                ImRect custom_rect(
                    rp + ImVec2(legend_width - (first_frame_used - Timeline::GetMinFrame(data) - 0.5f) * frame_pixel_width,
                                static_cast<float>(0.f)),
                    rp +
                        ImVec2(legend_width + (Timeline::GetMaxFrame(data) - first_frame_used - 0.5f + 2.f) * frame_pixel_width,
                               static_cast<float>(item_height)));
                ImRect clipping_rect(rp + ImVec2(static_cast<float>(legend_width), static_cast<float>(0.f)),
                                     rp + ImVec2(canvas_size.x, static_cast<float>(item_height)));

                compact_custom_draws.push_back({i, custom_rect, ImRect(), clipping_rect, ImRect()});
            }
            custom_height += local_custom_height;
        }

        // moving
        if (/*backgroundRect.Contains(io.MousePos) && */ moving_entry >= 0)
        {
#if IMGUI_VERSION_NUM >= 18723
            ImGui::SetNextFrameWantCaptureMouse(true);
#else
            ImGui::CaptureMouseFromApp();
#endif
            int diff_frame = static_cast<int>((cx - moving_pos) / frame_pixel_width);
            if (std::abs(diff_frame) > 0)
            {
                int start = Timeline::GetSequenceFirstFrame(data, moving_entry);
                int end = Timeline::GetSequenceLastFrame(data, moving_entry);
                int l_old = start;
                int r_old = end;
                if (selected_sequence) *selected_sequence = moving_entry;
                int& l = start;
                int& r = end;
                if (moving_part & 1) l += diff_frame;
                if (moving_part & 2) r += diff_frame;
                if (l < 0)
                {
                    if (moving_part & 2) r -= l;
                    l = 0;
                }
                if (moving_part & 1 && l > r) l = r;
                if (moving_part & 2 && r < l) r = l;

                const bool start_changed = l_old != l;
                const bool end_changed = r_old != r;
                if (start_changed && end_changed)
                {
                    int diff = r - r_old;
                    Timeline::MoveSequence(data, moving_entry, diff);
                }
                else
                {
                    if (r_old != r)
                    {
                        Timeline::EditSequenceLastFrame(data, moving_entry, r);
                    }
                    else if (l_old != l)
                    {
                        Timeline::EditSequenceFirstFrame(data, moving_entry, l);
                    }
                }

                moving_pos += static_cast<int>(diff_frame * frame_pixel_width);
            }
            if (!io.MouseDown[0])
            {
                // single select
                if (!diff_frame && moving_part && selected_sequence)
                {
                    *selected_sequence = moving_entry;
                    ret = true;
                }

                moving_entry = -1;
                Timeline::EndEdit(data);
            }
        }

        // cursor
        if (current_frame && first_frame && *current_frame >= *first_frame && *current_frame <= Timeline::GetMaxFrame(data))
        {
            static constexpr float CURSOR_WIDTH = 8.f;
            float cursorOffset = content_min.x + legend_width + (*current_frame - first_frame_used) * frame_pixel_width +
                                 frame_pixel_width / 2 - CURSOR_WIDTH * 0.5f;
            draw_list->AddLine(ImVec2(cursorOffset, canvas_pos.y),
                               ImVec2(cursorOffset, content_max.y),
                               0xA02A2AFF,
                               CURSOR_WIDTH);
            char tmps[512];
            ImFormatString(tmps, IM_ARRAYSIZE(tmps), "%d", *current_frame);
            draw_list->AddText(ImVec2(cursorOffset + 10, canvas_pos.y + 2), 0xFF2A2AFF, tmps);
        }

        draw_list->PopClipRect();
        draw_list->PopClipRect();

        for (auto& custom_draw : custom_draws)
        {
            Timeline::CustomDraw(data,
                                 custom_draw.m_index,
                                 draw_list,
                                 custom_draw.m_custom_rect,
                                 custom_draw.m_legend_rect,
                                 custom_draw.m_clipping_rect,
                                 custom_draw.m_legend_clipping_rect);
        }

        for (auto& custom_draw : compact_custom_draws)
        {
            Timeline::CustomDrawCompact(data,
                                        custom_draw.m_index,
                                        draw_list,
                                        custom_draw.m_custom_rect,
                                        custom_draw.m_clipping_rect);
        }

        // copy paste
        if (timeliner_flags & TIMELINER_COPYPASTE)
        {
            ImRect rect_copy(ImVec2(content_min.x + 100, canvas_pos.y + 2),
                             ImVec2(content_min.x + 100 + 30, canvas_pos.y + item_height - 2));
            bool in_rect_copy = rect_copy.Contains(io.MousePos);
            unsigned int copy_color = in_rect_copy ? 0xFF1080FF : 0xFF000000;
            draw_list->AddText(rect_copy.Min, copy_color, "Copy");

            ImRect rect_paste(ImVec2(content_min.x + 140, canvas_pos.y + 2),
                              ImVec2(content_min.x + 140 + 30, canvas_pos.y + item_height - 2));
            bool in_rect_paste = rect_paste.Contains(io.MousePos);
            unsigned int paste_color = in_rect_paste ? 0xFF1080FF : 0xFF000000;
            draw_list->AddText(rect_paste.Min, paste_color, "Paste");

            if (in_rect_copy && io.MouseReleased[0])
            {
                Timeline::Copy(data);
            }
            if (in_rect_paste && io.MouseReleased[0])
            {
                Timeline::Paste(data);
            }
        }
        //

        const ImU32 splitter_col =
            splitter_draw_hot ? IM_COL32(215, 225, 255, 235) : IM_COL32(120, 128, 150, 165);
        const float split_draw_x =
            std::floor(canvas_pos.x + static_cast<float>(legend_width)) + 0.5f;
        draw_list->AddLine(ImVec2(split_draw_x, canvas_pos.y),
                           ImVec2(split_draw_x, splitter_max_y),
                           splitter_col,
                           splitter_draw_hot ? 2.0f : 1.0f);

        ImGui::EndChild();
        ImGui::PopStyleColor();
        if (has_scroll_bar)
        {
            ImGui::InvisibleButton("scrollBar", scroll_bar_size);
            ImVec2 scroll_bar_min = ImGui::GetItemRectMin();
            ImVec2 scroll_bar_max = ImGui::GetItemRectMax();

            // ratio = number of frames visible in control / number to total frames

            float start_frame_offset =
                (static_cast<float>(first_frame_used - Timeline::GetMinFrame(data)) / static_cast<float>(frame_count)) *
                (canvas_size.x - legend_width);
            ImVec2 scroll_bar_a(scroll_bar_min.x + legend_width, scroll_bar_min.y - 2);
            ImVec2 scroll_bar_b(scroll_bar_min.x + canvas_size.x, scroll_bar_max.y - 1);
            draw_list->AddRectFilled(scroll_bar_a, scroll_bar_b, 0xFF222222, 0);

            ImRect scroll_bar_rect(scroll_bar_a, scroll_bar_b);
            bool in_scroll_bar = scroll_bar_rect.Contains(io.MousePos);

            draw_list->AddRectFilled(scroll_bar_a, scroll_bar_b, 0xFF101010, 8);

            ImVec2 scroll_bar_c(scroll_bar_min.x + legend_width + start_frame_offset, scroll_bar_min.y);
            ImVec2 scroll_bar_d(scroll_bar_min.x + legend_width + bar_width_in_pixels + start_frame_offset,
                                scroll_bar_max.y - 2);
            draw_list->AddRectFilled(scroll_bar_c,
                                     scroll_bar_d,
                                     (in_scroll_bar || moving_scroll_bar) ? 0xFF606060 : 0xFF505050,
                                     6);

            ImRect bar_handle_left(scroll_bar_c, ImVec2(scroll_bar_c.x + 14, scroll_bar_d.y));
            ImRect bar_handle_right(ImVec2(scroll_bar_d.x - 14, scroll_bar_c.y), scroll_bar_d);

            bool on_left = bar_handle_left.Contains(io.MousePos);
            bool on_right = bar_handle_right.Contains(io.MousePos);

            static bool sizing_r_bar = false;
            static bool sizing_l_bar = false;

            draw_list->AddRectFilled(bar_handle_left.Min,
                                     bar_handle_left.Max,
                                     (on_left || sizing_l_bar) ? 0xFFAAAAAA : 0xFF666666,
                                     6);

            draw_list->AddRectFilled(bar_handle_right.Min,
                                     bar_handle_right.Max,
                                     (on_right || sizing_r_bar) ? 0xFFAAAAAA : 0xFF666666,
                                     6);

            ImRect scrollBarThumb(scroll_bar_c, scroll_bar_d);
            static constexpr float MIN_BAR_WIDTH = 44.f;
            if (sizing_r_bar)
            {
                if (!io.MouseDown[0])
                {
                    sizing_r_bar = false;
                }
                else
                {
                    float bar_new_width = ImMax(bar_width_in_pixels + io.MouseDelta.x, MIN_BAR_WIDTH);
                    float bar_ratio = bar_new_width / bar_width_in_pixels;
                    frame_pixel_width_target = frame_pixel_width = frame_pixel_width / bar_ratio;
                    int new_visible_frame_count = static_cast<int>((canvas_size.x - legend_width) / frame_pixel_width_target);
                    int last_frame = *first_frame + new_visible_frame_count;
                    if (last_frame > Timeline::GetMaxFrame(data))
                    {
                        frame_pixel_width_target = frame_pixel_width =
                            (canvas_size.x - legend_width) / static_cast<float>(Timeline::GetMaxFrame(data) - *first_frame);
                    }
                }
            }
            else if (sizing_l_bar)
            {
                if (!io.MouseDown[0])
                {
                    sizing_l_bar = false;
                }
                else
                {
                    if (fabsf(io.MouseDelta.x) > FLT_EPSILON)
                    {
                        float bar_new_width = ImMax(bar_width_in_pixels - io.MouseDelta.x, MIN_BAR_WIDTH);
                        float bar_ratio = bar_new_width / bar_width_in_pixels;
                        float previous_frame_pixel_width_target = frame_pixel_width_target;
                        frame_pixel_width_target = frame_pixel_width = frame_pixel_width / bar_ratio;
                        int new_visible_frame_count = static_cast<int>(visible_frame_count / bar_ratio);
                        int new_first_frame = *first_frame + new_visible_frame_count - visible_frame_count;
                        new_first_frame =
                            ImClamp(new_first_frame,
                                    Timeline::GetMinFrame(data),
                                    ImMax(Timeline::GetMaxFrame(data) - visible_frame_count, Timeline::GetMinFrame(data)));
                        if (new_first_frame == *first_frame)
                        {
                            frame_pixel_width = frame_pixel_width_target = previous_frame_pixel_width_target;
                        }
                        else
                        {
                            *first_frame = new_first_frame;
                        }
                    }
                }
            }
            else
            {
                if (moving_scroll_bar)
                {
                    if (!io.MouseDown[0])
                    {
                        moving_scroll_bar = false;
                    }
                    else
                    {
                        float frames_per_pixel_in_bar = bar_width_in_pixels / static_cast<float>(visible_frame_count);
                        *first_frame = static_cast<int>((io.MousePos.x - panning_view_source.x) / frames_per_pixel_in_bar) -
                                       panning_view_frame;
                        *first_frame =
                            ImClamp(*first_frame,
                                    Timeline::GetMinFrame(data),
                                    ImMax(Timeline::GetMaxFrame(data) - visible_frame_count, Timeline::GetMinFrame(data)));
                    }
                }
                else
                {
                    if (scrollBarThumb.Contains(io.MousePos) && ImGui::IsMouseClicked(0) && first_frame &&
                        !moving_current_frame && moving_entry == -1)
                    {
                        moving_scroll_bar = true;
                        panning_view_source = io.MousePos;
                        panning_view_frame = -*first_frame;
                    }
                    if (!sizing_r_bar && on_right && ImGui::IsMouseClicked(0)) sizing_r_bar = true;
                    if (!sizing_l_bar && on_left && ImGui::IsMouseClicked(0)) sizing_l_bar = true;
                }
            }
        }
    }

    ImGui::EndGroup();

    if (region_rect.Contains(io.MousePos))
    {
        bool overCustomDraw = false;
        for (auto& custom : custom_draws)
        {
            if (custom.m_custom_rect.Contains(io.MousePos))
            {
                overCustomDraw = true;
            }
        }
        if (overCustomDraw)
        {
        }
        else
        {
#if 0
            frameOverCursor = *first_frame + (int)(visible_frame_count * ((io.MousePos.x - (float)legend_width - canvas_pos.x) / (canvas_size.x - legend_width)));
            //frameOverCursor = max(min(*firstFrame - visibleFrameCount / 2, frameCount - visibleFrameCount), 0);

            /**firstFrame -= frameOverCursor;
            *firstFrame *= framePixelWidthTarget / framePixelWidth;
            *firstFrame += frameOverCursor;*/
            if (io.MouseWheel < -FLT_EPSILON)
            {
               *first_frame -= frameOverCursor;
               *first_frame = int(*first_frame * 1.1f);
               frame_pixel_width_target *= 0.9f;
               *first_frame += frameOverCursor;
            }

            if (io.MouseWheel > FLT_EPSILON)
            {
               *first_frame -= frameOverCursor;
               *first_frame = int(*first_frame * 0.9f);
               frame_pixel_width_target *= 1.1f;
               *first_frame += frameOverCursor;
            }
#endif
        }
    }

    /*
    if (expanded)
    {
        if (TimelinerAddDelButton(draw_list, ImVec2(canvas_pos.x + 2, canvas_pos.y + 2), !*expanded)) *expanded = !*expanded;
    }
    */

    if (del_entry != -1)
    {
        Timeline::DeleteSequence(data, del_entry);
        if (selected_sequence && (*selected_sequence == del_entry || *selected_sequence >= Timeline::GetSequenceCount(data)))
            *selected_sequence = -1;
    }

    /*
    if (dup_entry != -1)
    {
        timeline->Duplicate(dup_entry);
    }
    */

    return ret;
}

}  // namespace tanim::internal
