/*
 * Themiify - A theme manager for the Nintendo Wii U
 * Copyright (C) 2026 Fangal-Airbag
 * Copyright (C) 2026 AlphaCraft9658
 * Copyright (C) 2026 Daniel K. O. <dkosmari>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <string>

#include <imgui.h>
#include <imgui_raii.h>
#include <imgui_stdlib.h>

#include "ConfirmExitPopup.h"

#include "../App.h"
#include "../IconsFontAwesome4.h"
#include "../UI.h"

namespace ConfirmExitPopup {

    namespace {

        enum class State {
            hidden,
            queued,
            visible,
        };

        State state;

        std::string popup_id;

    } // namespace

    void
    open() {
        state = State::queued;
    }

    void
    process_ui() {
        using namespace ImGui::RAII;

        if (state == State::hidden)
            return;

        if (state == State::queued) {
            state = State::visible;
            UI::OpenPopup(popup_id);
        }

        const auto center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, {0.5f, 0.5f});
        PopupModal popup{popup_id, nullptr,
                         ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoDecoration |
                         ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoSavedSettings};
        if (!popup) {
            // ImGui closed the popup
            UI::PlaySFXPopupClose();
            state = State::hidden;
            return;
        }

        UI::Title("Exit confirmation");

        ImGui::Text("Are you sure you want to exit?");

        UI::ButtonHBox buttons;
        buttons.add(ICON_FA_TIMES " Cancel",
                    []
                    {
                        UI::CloseCurrentPopup();
                        state = State::hidden;
                    });
        buttons.add(ICON_FA_SIGN_OUT " Exit",
                    true,
                    []
                    {
                        UI::CloseCurrentPopup(true);
                        state = State::hidden;
                        App::quit();
                    });
        buttons.show();
    }

} // namespace ConfirmExitPopup
