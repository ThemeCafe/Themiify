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

namespace UI {

    using ClickCallbackSignature = void();
    using ClickFunction = std::move_only_function<ClickCallbackSignature>;

    struct Button {
        std::string label = {};
        std::string tooltip = {};
        bool is_default = false;
        ClickFunction on_click = {};
    };


    struct ButtonHBox {
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

    ImVec2
    max(const ImVec2& a,
        const ImVec2& b)
        noexcept;

    void
    ShowLastBB();

    void
    Title(const std::string& text);

} // namespace UI
