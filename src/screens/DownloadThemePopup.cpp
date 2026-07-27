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
#include <vector>

#include <imgui.h>
#include <imgui_raii.h>

#include "DownloadThemePopup.h"

#include "../DownloadManager.h"
#include "../humanize.hpp"
#include "../IconsFontAwesome4.h"
#include "../PluginManager.h"
#include "../ThemeManager.h"
#include "../UI.h"
#include "InstallThemePopup.h"

using std::cout;
using std::endl;
using namespace std::literals;

namespace DownloadThemePopup {

    namespace {

        /*-------*/
        /* Types */
        /*-------*/

        enum class State {
            hidden,
            queued,
            confirmation,
            downloading,
            error,
            success,
        };

        /*-----------*/
        /* Constants */
        /*-----------*/

        const std::string popup_id = "Download Theme";

        /*-----------*/
        /* Variables */
        /*-----------*/

        State state;
        std::string utheme_url;
        std::filesystem::path utheme_filename;

        std::string transfer_name;
        std::string error_message;

        bool enable_theme = true;

        /*-----------------------*/
        /* Function declarations */
        /*-----------------------*/

        void
        on_download_error(const std::exception& e);

        void
        on_download_success(const DownloadManager::Info& info);

        void
        show_confirmation();

        void
        show_downloading();

        void
        show_error();

        void
        show_success();

        /*----------------------*/
        /* Function definitions */
        /*----------------------*/

        void
        on_download_error(const std::exception& e) {
            state = State::error;
            error_message = e.what();
        }

        void
        on_download_success(const DownloadManager::Info& info) {
            cout << "Finished " << info.filename << endl;
            state = State::success;
            ThemeManager::RefreshUThemes();
        }

        void
        show_confirmation() {
            UI::Title("Download Confirmation");

            ImGui::FormatTextWrapped("Would you like to download the theme:\n{} ?",
                                     transfer_name);

            UI::ButtonHBox buttons;
            buttons.valign = 1.0f;
            buttons.add(ICON_FA_TIMES " Cancel",
                        []
                        {
                            ImGui::CloseCurrentPopup();
                            state = State::hidden;
                        });
            buttons.add(ICON_FA_DOWNLOAD " Download",
                        true,
                        []
                        {
                            state = State::downloading;
                            if (!DownloadManager::add(utheme_url,
                                                      utheme_filename,
                                                      on_download_success,
                                                      on_download_error)) {
                                state = State::error;
                                error_message = "Failed to queue download transfer";
                            }
                        });
            buttons.show();
        }

        void
        show_downloading() {
            UI::Title("Downloading Theme...");

            ImGui::TextWrapped(transfer_name);

            ImGui::TextWrapped(utheme_url);

            ImGui::FormatTextWrapped("Saving to: {}", utheme_filename.filename().string());

            auto info = DownloadManager::get_info(utheme_url);

            //auto speed = humanize::value_bin(info->speed) + "B/s";
            //ImGui::Text("DL speed: %s", speed.data());

            // Place the progress bar on the bottom.
            ImGui::SetCursorPosY(ImGui::GetCursorPosY()
                                 + ImGui::GetContentRegionAvail().y
                                 - ImGui::GetFrameHeight());

            ImGui::ProgressBar(info->progress);

            // TODO: downloads should be cancelable, should have a cancel button here.
        }

        void
        show_error() {
            UI::Title("ERROR!");

            ImGui::TextWrapped(error_message);

            if (ImGui::Button("Close")) {
                DownloadManager::clear_all();
                state = State::hidden;
            }
        }

        void
        show_success() {
            UI::Title("Download successful!");

            ImGui::TextWrapped("Would you like to install this theme for the StyleMiiU plugin?");

            const std::string enable_label =
                (PluginManager::IsShuffling() ? "Enable"s : "Apply"s)
                + " theme after installation"s;
            ImGui::Checkbox(enable_label, enable_theme);

            UI::ButtonHBox buttons;
            buttons.valign = 1.0f;
            buttons.add(ICON_FA_TIMES " Cancel",
                        []
                        {
                            ImGui::CloseCurrentPopup();
                            state = State::hidden;
                        });
            buttons.add(ICON_FA_COGS " Install",
                        true,
                        []
                        {
                            ImGui::CloseCurrentPopup();
                            state = State::hidden;
                            // TODO: report error if a .utheme is missing metadata.
                            if (auto meta = ThemeManager::ReadUThemeMetadata(utheme_filename))
                                InstallThemePopup::open(utheme_filename, meta, true, enable_theme);
                        });
            buttons.show();
        }

    } // namespace

    /*------------------*/
    /* Public functions */
    /*------------------*/

    void
    open(const ThemezerAPI::WiiuThemeSmall &theme_data) {
        state = State::queued;
        transfer_name = theme_data.name;
        utheme_url = theme_data.downloadUrl;
        utheme_filename = ThemeManager::CalcUThemePath(theme_data.slug, theme_data.hexId);
        error_message.clear();
    }

    void
    open(const std::string& url) {
        state = State::queued;
        transfer_name = "[QR] \"" + url + "\"";
        utheme_url = url;
        utheme_filename = ThemeManager::CalcUThemePath(url);
        error_message.clear();
    }

    void
    open(const ThemezerAPI::WiiuInstallThemeLookup &theme)
    {
        state = State::queued;
        transfer_name = theme.name;
        utheme_url = theme.downloadUrl;
        utheme_filename = ThemeManager::CalcUThemePath(theme.name, theme.quickId);
        error_message.clear();
    }

    void
    process_ui() {
        using namespace ImGui::RAII;
        if (state == State::hidden)
            return;

        if (state == State::queued) {
            // NOTE: open popup before we create the popup.
            ImGui::OpenPopup(popup_id);
            state = State::confirmation;
        }

        auto viewport = ImGui::GetMainViewport();
        // WORKAROUND: setting an initial size helps with the initial position not jumping around.
        ImGui::SetNextWindowSize(viewport->Size * 0.7f, ImGuiCond_Appearing);
        auto center = viewport->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, {0.5f, 0.5f});
        PopupModal popup{popup_id, nullptr,
                         ImGuiWindowFlags_NoDecoration |
                         ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoSavedSettings};

        if (!popup) {
            state = State::hidden;
            return;
        }

        switch (state) {
            case State::confirmation:
                show_confirmation();
                break;

            case State::downloading:
                show_downloading();
                break;

            case State::error:
                show_error();
                break;

            case State::success:
                show_success();
                break;

            default:
                ;
        }

    }
}
