/*
 * Themiify - A theme manager for the Nintendo Wii U
 * Copyright (C) 2026 Fangal-Airbag
 * Copyright (C) 2026 AlphaCraft9658
 * Copyright (C) 2026 Daniel K. O. <dkosmari>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <concepts>
#include <functional>
#include <string>
#include <vector>

#include <imgui.h>

namespace UI {

    extern const ImVec4 enabled_color;
    extern const ImVec4 installed_color;
    extern const ImVec4 update_color;

    extern const std::string installed_icon;
    extern const std::string update_icon;

    struct ButtonHBox {

        using ClickCallbackSignature = void();
        using ClickFunction = std::move_only_function<ClickCallbackSignature>;

        struct Button {
            std::string label = {};
            std::string tooltip = {};
            bool is_default = false;
            ClickFunction on_click = {};
        };

        float halign = 0.5f;
        float valign = -1;
        std::vector<Button> buttons = {};

        ImVec2 button_size = {}; // calculated
        float total_width = 0; // calculated

        void
        add(Button&& b);

        void
        add(const std::string& label,
            const std::string& tooltip,
            bool is_default,
            ClickFunction on_click);

        void
        add(const std::string& label,
            bool is_default,
            ClickFunction on_click);

            void
        add(const std::string& label,
            ClickFunction on_click);

        void
        show();

    private:

        void
        update();

    }; // struct ButtonHBox

    void
    initialize();

    void
    finalize();

    bool
    Button(const std::string& label,
           const ImVec2& size = {0, 0});

    bool
    Checkbox(const std::string& label,
             bool& variable);

    void
    CloseCurrentPopup(bool silent = false);

    bool
    CollapsingHeader(const std::string& label,
                     ImGuiTreeNodeFlags flags = 0);

    ImVec2
    max(const ImVec2& a,
        const ImVec2& b)
        noexcept;

    bool
    OpenPopup(const std::string& popup_id);

    void
    PlaySFXClick();

    void
    PlaySFXPopupClose();

    void
    PlaySFXPopupOpen();

    void
    PlaySFXQRScan();

    void
    PlaySFXTabSwitch();

    bool
    Selectable(const std::string& label,
               bool selected,
               ImGuiSelectableFlags flags = 0,
               const ImVec2& size = {0, 0});

    void
    ShowLastBB();

    void
    Title(const std::string& text);

} // namespace UI
