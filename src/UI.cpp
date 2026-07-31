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

#include <SDL_mixer.h>

#include <imgui_raii.h>
#include <imgui_stdlib.h>

#include "UI.h"

#include "IconsFontAwesome4.h"
#include "tracer.hpp"

using std::cout;
using std::endl;

namespace UI {

    namespace {

        /*-----------*/
        /* Constants */
        /*-----------*/

        const float title_size = 40;

        /*-----------*/
        /* Variables */
        /*-----------*/

        Mix_Chunk* sfx_click;
        Mix_Chunk* sfx_popup_close;
        Mix_Chunk* sfx_popup_open;
        Mix_Chunk* sfx_qr_scan;
        Mix_Chunk* sfx_tab_switch;

    } // namespace

    /*------------------*/
    /* Public constants */
    /*------------------*/

    const ImVec4 enabled_color   = { 1.0f, 1.0f, 0.0f, 1.0f };
    const ImVec4 installed_color = { 0.0f, 1.0f, 0.3f, 1.0f };
    const ImVec4 update_color    = { 0.4f, 0.8f, 1.0f, 1.0f };

    const std::string installed_icon = ICON_FA_CHECK;
    const std::string update_icon = ICON_FA_CLOUD_DOWNLOAD;

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
            if (UI::Button(b.label, button_size))
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

    void
    initialize()
    {
        TRACE_FUNC;

        sfx_click       = Mix_LoadWAV("/vol/content/sound/sfx-click.flac");
        sfx_popup_close = Mix_LoadWAV("/vol/content/sound/sfx-popup-close.flac");
        sfx_popup_open  = Mix_LoadWAV("/vol/content/sound/sfx-popup-open.flac");
        sfx_qr_scan     = Mix_LoadWAV("/vol/content/sound/sfx-qr-scan.flac");
        sfx_tab_switch  = Mix_LoadWAV("/vol/content/sound/sfx-tab-switch.flac");
    }

    void
    finalize()
    {
        TRACE_FUNC;

        Mix_FreeChunk(sfx_click);
        sfx_click = nullptr;

        Mix_FreeChunk(sfx_popup_close);
        sfx_popup_close = nullptr;

        Mix_FreeChunk(sfx_popup_open);
        sfx_popup_open = nullptr;

        Mix_FreeChunk(sfx_qr_scan);
        sfx_qr_scan = nullptr;

        Mix_FreeChunk(sfx_tab_switch);
        sfx_tab_switch = nullptr;

    }

    bool
    Button(const std::string& label,
           const ImVec2& size)
    {
        bool result = ImGui::Button(label, size);
        if (result)
            PlaySFXClick();
        return result;
    }

    bool
    Checkbox(const std::string& label,
             bool& variable)
    {
        bool result = ImGui::Checkbox(label, variable);
        if (result)
            PlaySFXClick();
        return result;
    }

    void
    CloseCurrentPopup(bool silent)
    {
        ImGui::CloseCurrentPopup();
        if (!silent)
            PlaySFXPopupClose();
    }

    bool
    CollapsingHeader(const std::string& label,
                     ImGuiTreeNodeFlags flags)
    {
        bool result = ImGui::CollapsingHeader(label, flags);
        if (ImGui::IsItemClicked())
            PlaySFXClick();
        return result;
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

    bool
    OpenPopup(const std::string& popup_id)
    {
        bool result = ImGui::OpenPopup(popup_id);
        if (result)
            PlaySFXPopupOpen();
        return result;
    }

    void
    PlaySFXClick()
    {
        if (sfx_click)
            Mix_PlayChannel(-1, sfx_click, 0);
    }

    void
    PlaySFXPopupClose()
    {
        if (sfx_popup_close)
            Mix_PlayChannel(-1, sfx_popup_close, 0);
    }

    void
    PlaySFXPopupOpen()
    {
        if (sfx_popup_open)
            Mix_PlayChannel(-1, sfx_popup_open, 0);
    }

    void
    PlaySFXQRScan()
    {
        if (sfx_qr_scan)
            Mix_PlayChannel(-1, sfx_qr_scan, 0);
    }

    void
    PlaySFXTabSwitch()
    {
        if (sfx_tab_switch)
            Mix_PlayChannel(-1, sfx_tab_switch, 0);
    }

    bool
    Selectable(const std::string& label,
               bool selected,
               ImGuiSelectableFlags flags,
               const ImVec2& size)
    {
        bool result = ImGui::Selectable(label, selected, flags, size);
        if (result)
            PlaySFXClick();
        return result;
    }

    void
    ShowLastBB()
    {
        auto min = ImGui::GetItemRectMin();
        auto max = ImGui::GetItemRectMax();
        // auto diff = max - min;
        // cout << "BB: [" << diff.x << " x " << diff.y << "]" << endl;
        ImU32 col = ImGui::GetColorU32(ImVec4{1.0f, 0.0f, 0.0f, 0.5f});
        auto draw_list = ImGui::GetWindowDrawList();
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
