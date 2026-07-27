/*
 * Themiify - A theme manager for the Nintendo Wii U
 * Copyright (C) 2026 Fangal-Airbag
 * Copyright (C) 2026 AlphaCraft9658
 * Copyright (C) 2026 Daniel K. O. <dkosmari>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cmath>
#include <iostream>
#include <ranges>
#include <utility>
#include <vector>

#include <imgui.h>
#include <imgui_raii.h>
#include <imgui_stdlib.h>

#include "UI.h"

using std::cout;
using std::endl;

namespace UI {

    namespace {

        /*-----------*/
        /* Constants */
        /*-----------*/

        const float title_size = 40;

    } // namespace

    /*------------------*/
    /* Public functions */
    /*------------------*/

    void
    ButtonHBox::add(Button&& button)
    {
        buttons.push_back(std::move(button));
        update();
    }

    void
    ButtonHBox::add(const std::string& label,
                    const std::string& tooltip,
                    bool is_default,
                    ClickFunction on_click)
    {
        buttons.emplace_back(label, tooltip, is_default, std::move(on_click));
        update();
    }

    void
    ButtonHBox::add(const std::string& label,
                    bool is_default,
                    ClickFunction on_click)
    {
        add(label, {}, is_default, std::move(on_click));
    }

    void
    ButtonHBox::add(const std::string& label,
                    ClickFunction on_click)
    {
        add(label, {}, false, std::move(on_click));
    }

    void
    ButtonHBox::show()
    {
        const auto available = ImGui::GetContentRegionAvail();

        if (halign >= 0) {
            const float empty_hspace = available.x - total_width;
            if (empty_hspace > 0)
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + empty_hspace * halign);
        }

        if (valign >= 0) {
            const float empty_vspace = available.y - button_size.y;
            if (empty_vspace > 0)
                ImGui::SetCursorPosY(ImGui::GetCursorPosY() + empty_vspace * valign);
        }

        for (auto [idx, b] : buttons | std::views::enumerate) {
            if (idx > 0)
                ImGui::SameLine();
            if (ImGui::Button(b.label, button_size))
                if (b.on_click)
                    b.on_click();
            if (!b.tooltip.empty())
                ImGui::SetItemTooltip(b.tooltip);
            if (b.is_default)
                ImGui::SetItemDefaultFocus();
        }
    }

    void
    ButtonHBox::update()
    {
        if (buttons.empty())
            return;

        const auto& style = ImGui::GetStyle();

        auto size = ImGui::CalcTextSize(buttons.back().label) + 2 * style.FramePadding;
        button_size = max(button_size, size);

        const auto n = buttons.size();
        total_width = n * button_size.x + (n - 1) * style.ItemSpacing.x;
    }

    ImVec2
    max(const ImVec2& a,
        const ImVec2& b)
        noexcept
    {
        return {
            std::fmax(a.x, b.x),
            std::fmax(a.y, b.y)
        };
    }

    void
    ShowLastBB()
    {
        auto min = ImGui::GetItemRectMin();
        auto max = ImGui::GetItemRectMax();
        // auto diff = max - min;
        // cout << "BB: [" << diff.x << " x " << diff.y << "]" << endl;
        ImU32 col = ImGui::GetColorU32(ImVec4{1.0f, 0.0f, 0.0f, 0.5f});
        auto draw_list = ImGui::GetForegroundDrawList();
        draw_list->AddRect(min, max, col);
    }

    void
    Title(const std::string& text)
    {
        using namespace ImGui::RAII;
        {
            Font title_font{nullptr, title_size};
            ImGui::Text(text);
        }
        ImGui::Separator();
    }

} // namespace UI
