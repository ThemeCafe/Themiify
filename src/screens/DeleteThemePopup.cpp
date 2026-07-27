/*
 * Themiify - A theme manager for the Nintendo Wii U
 * Copyright (C) 2026 Fangal-Airbag
 * Copyright (C) 2026 AlphaCraft9658
 * Copyright (C) 2026 Daniel K. O. <dkosmari>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <string>
#include <utility>

#include <imgui.h>
#include <imgui_raii.h>
#include <imgui_stdlib.h>

#include "DeleteThemePopup.h"
#include "ManageThemesScreen.h"
#include "../IconsFontAwesome4.h"
#include "../UI.h"

using namespace std::literals;

namespace DeleteThemePopup {

    namespace {

        /*-------*/
        /* Types */
        /*-------*/

        enum class State {
            hidden,
            queued,
            visible,
        };

        /*-----------*/
        /* Constants */
        /*-----------*/

        const std::string popup_id = "DeleteThemePopup"s;

        /*-----------*/
        /* Variables */
        /*-----------*/

        State state;

        ThemeManager::ConstThemePtr installedTheme;

    } // namespace

    /*------------------*/
    /* Public functions */
    /*------------------*/

    void
    open(ThemeManager::ConstThemePtr installed_theme) {
        state = State::queued;
        installedTheme = std::move(installed_theme);
    }

    void
    process_ui() {
        using namespace ImGui::RAII;
        if (state == State::hidden)
            return;

        if (state == State::queued) {
            ImGui::OpenPopup(popup_id);
            state = State::visible;
        }

        auto center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, {0.5f, 0.5f});
        ImGui::SetNextWindowSize({600, 0}, ImGuiCond_Always);
        PopupModal popup{popup_id, nullptr,
                         ImGuiWindowFlags_NoSavedSettings |
                         // ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoTitleBar |
                         ImGuiWindowFlags_NoResize};

        if (!popup) {
            state = State::hidden;
            return;
        }

        UI::Title("Delete Confirmation");

        ImGui::FormatTextWrapped("Would you like to delete \"{}\"?",
                                 installedTheme->metadata.themeName);

        UI::ButtonHBox buttons;
        buttons.add(ICON_FA_TIMES " Cancel",
                    []
                    {
                        ImGui::CloseCurrentPopup();
                        state = State::hidden;
                        installedTheme.reset();
                    });
        buttons.add(ICON_FA_TRASH " Delete",
                    true,
                    []
                    {
                        ThemeManager::Uninstall(installedTheme);
                        ImGui::CloseCurrentPopup();
                        state = State::hidden;
                        installedTheme.reset();
                    });
        buttons.show();
    }

} // namespace DeleteThemePopup
