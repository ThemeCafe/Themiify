/*
 * Themiify - A theme manager for the Nintendo Wii U
 * Copyright (C) 2026 Fangal-Airbag
 * Copyright (C) 2026 AlphaCraft9658
 * Copyright (C) 2026 Daniel K. O. <dkosmari>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <iostream>
#include <filesystem>
#include <string>

#include <imgui.h>
#include <imgui_raii.h>

#include "InstallThemePopup.h"

#include "../IconsFontAwesome4.h"
#include "../PluginManager.h"
#include "../UI.h"
#include "../utils.h"
#include "ManageThemesScreen.h"

using std::cerr;
using std::cout;
using std::endl;
using namespace std::literals;

namespace InstallThemePopup {

    namespace {

        /*-------*/
        /* Types */
        /*-------*/

        enum class State {
            hidden,
            confirmation,
            start_install,
            installing,
            error,
            success,
        };

        /*-----------*/
        /* Constants */
        /*-----------*/

        const std::string popup_id = "Install Theme";

        /*-----------*/
        /* Variables */
        /*-----------*/

        State state = State::hidden;

        bool popup_queued;

        std::filesystem::path utheme;
        ThemeManager::ConstMetadataPtr metadata;
        bool enable_theme = true;

        std::vector<std::string> progress_messages;
        std::string error_message;
        bool scroll_to_bottom;

        /*-----------------------*/
        /* Function declarations */
        /*-----------------------*/

        void
        action_close();

        void
        error_handler(const std::string& msg);

        void
        progress_handler(const std::string& msg);

        void
        show_messages();

        void
        show_state_confirmation();

        void
        show_state_error();

        void
        show_state_start_install();

        void
        show_state_success();

        void
        success_handler();

        /*----------------------*/
        /* Function definitions */
        /*----------------------*/

        void
        action_close() {
            ImGui::CloseCurrentPopup();
            state = State::hidden;
        }

        void
        error_handler(const std::string& msg) {
            cerr << "ERROR: " << msg << endl;
            error_message = msg;
            state = State::error;
            scroll_to_bottom = true;
        }

        void
        progress_handler(const std::string& msg) {
            cout << "PROGRESS: " << msg << endl;
            progress_messages.push_back(msg);
            scroll_to_bottom = true;
        }

        void
        show_messages() {
            using namespace ImGui::RAII;

            Font msg_font{nullptr, 24};

            ImVec2 size{0.0f, 8 * ImGui::GetTextLineHeightWithSpacing()};
            if (Child messages_box{"messages_box",
                                   size,
                                   ImGuiChildFlags_None,
                                   ImGuiWindowFlags_NoSavedSettings}) {

                {

                    // Show progress messages.
                    if (!progress_messages.empty()) {
                        for (const auto& msg : progress_messages)
                            ImGui::TextWrapped(msg);
                        ImGui::Spacing();
                    }

                    // Show error message.
                    {
                        StyleColor red_text{ImGuiCol_Text, {1.0f, 0.25f, 0.25f, 1.0f}};
                        if (!error_message.empty())
                            ImGui::TextWrapped(error_message);
                    }

                    if (scroll_to_bottom) {
                        ImGui::SetScrollHereY(1.0f);
                        scroll_to_bottom = false;
                    }
                }
            }
        }

        void
        show_state_confirmation() {
            UI::Title("Confirm theme installation");

            ImGui::FormatTextWrapped("Would you like to install:\n{} ?",
                                     metadata->themeName);

            std::string enable_label = (PluginManager::IsShuffling() ? "Enable"s : "Apply"s)
                + " theme after installation"s;
            ImGui::Checkbox(enable_label, enable_theme);

            UI::ButtonHBox buttons;
            buttons.add(ICON_FA_TIMES " Cancel", action_close);
            buttons.add(
                ICON_FA_COGS " Install",
                true,
                []
                {
                    state = State::start_install;
                }
            );
            buttons.show();
        }

        void
        show_state_error() {
            UI::Title("Installation failed!");

            show_messages();

            UI::ButtonHBox buttons;
            buttons.add(ICON_FA_TIMES " Close", true, action_close);
            buttons.show();
        }

        void
        show_state_installing() {
            UI::Title("Installing");

            ImGui::FormatTextWrapped("Installing {}...", metadata->themeName);

            ImGui::Text("This may take time, do not turn off your Wii U.");

            show_messages();

            UI::ButtonHBox buttons;
            buttons.add(ICON_FA_TIMES " Cancel", true, ThemeManager::CancelInstall);
            buttons.show();
        }

        void
        show_state_start_install() {
            state = State::installing;
            ThemeManager::Install(utheme,
                                  metadata,
                                  enable_theme,
                                  progress_handler,
                                  success_handler,
                                  error_handler);
        }

        void
        show_state_success() {
            UI::Title("Installation successful!");

            ImGui::TextWrapped(std::format("This file is not needed anymore:\n\"{}\".",
                                           utheme.filename().string()));
            ImGui::TextWrapped("Would you like to delete it?");

            UI::ButtonHBox buttons;
            buttons.add(ICON_FA_DOWNLOAD " Keep", action_close);
            buttons.add(
                ICON_FA_TRASH " Delete",
                true,
                []
                {
                    DeletePath(utheme);
                    action_close();
                    ThemeManager::RefreshUThemes();
                }
            );
            buttons.show();
        }

        void
        success_handler() {
            state = State::success;
            scroll_to_bottom = true;
        }


    } // namespace

    /*------------------*/
    /* Public functions */
    /*------------------*/

    void
    open(const std::filesystem::path &utheme_,
              const ThemeManager::ConstMetadataPtr &metadata_,
              bool skipConfirmation,
              bool enableTheme) {
        utheme = utheme_;
        metadata = metadata_;

        if (skipConfirmation)
            state = State::start_install;
        else
            state = State::confirmation;

        enable_theme = enableTheme;
        popup_queued = true;

        progress_messages.clear();
        error_message.clear();
    }

    void
    process_ui() {
        using namespace ImGui::RAII;
        if (state == State::hidden)
            return;

        if (popup_queued) {
            ImGui::OpenPopup(popup_id);
            popup_queued = false;
        }

        auto center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, {0.5f, 0.5f});
        ImGui::SetNextWindowSize({900, 0}, ImGuiCond_Always);
        PopupModal popup{
            popup_id,
            nullptr,
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoDecoration |
            ImGuiWindowFlags_NoMove |
            ImGuiWindowFlags_NoSavedSettings
        };

        if (!popup) {
            state = State::hidden;
            return;
        }

        switch (state) {
            case State::confirmation:
                show_state_confirmation();
                break;

            case State::start_install:
                show_state_start_install();
                break;

            case State::installing:
                show_state_installing();
                break;

            case State::error:
                show_state_error();
                break;

            case State::success:
                show_state_success();
                break;

            default:
                ;
        }
    }

} // namespace InstallThemePopup
