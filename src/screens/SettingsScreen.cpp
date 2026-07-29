/*
 * Themiify - A theme manager for the Nintendo Wii U
 * Copyright (C) 2026 Fangal-Airbag
 * Copyright (C) 2026 AlphaCraft9658
 * Copyright (C) 2026 Daniel K. O. <dkosmari>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "SettingsScreen.h"

#include "../App.h"
#include "../Config.h"
#include "../IconsFontAwesome4.h"
#include "../NavBar.h"
#include "../PluginManager.h"
#include "../ThemeManager.h"
#include "../tracer.hpp"
#include "../UI.h"
#include "../utils.h"
#include "SettingsPopup.h"

#include <iostream>

#include <SDL_mixer.h>

#include <imgui.h>
#include <imgui_raii.h>
#include <imgui_stdlib.h>

// Define this to help seeing the padding and spacing values for windows.
// #define DEBUG_BG_COLOR

using std::cout;
using std::cerr;
using std::endl;

using Config::cfg;

namespace SettingsScreen {

    namespace {

        /*-----------*/
        /* Variables */
        /*-----------*/

        bool bootIntegrityCheckPending;

        /*-----------------------*/
        /* Function declarations */
        /*-----------------------*/

        void
        help_marker(const std::string& msg,
                    bool same_line = true);

        void
        show_sound_options();

        void
        show_special_files_options();

        void
        show_stylemiiu_options();

        /*----------------------*/
        /* Function definitions */
        /*----------------------*/

        void
        help_marker(const std::string& msg,
                    bool same_line) {
            using namespace ImGui::RAII;
            if (same_line)
                ImGui::SameLine();
            ImGui::TextDisabled(ICON_FA_QUESTION_CIRCLE);
            if (ItemTooltip tooltip{}) {
                TextWrapPos text_wrap{600};
                ImGui::Text(msg);
            }
        }

        void
        show_sound_options() {
            using namespace ImGui::RAII;

            Indent one;

            ImGui::Text("Sound effects volume");
            {
                Indent two;
                if (ImGui::Slider("##sfx_volume", cfg.sfx_volume, 0, 100, "%d%%")) {
                    cout << "updated sfx volume" << endl;
                    App::update_mixer_volumes();
                }
                if (ImGui::IsItemActivated() || ImGui::IsItemDeactivated())
                    UI::PlaySFXClick();
                help_marker("Set the volume of sound effects.");
            }

            ImGui::Text("Background music volume:");
            {
                Indent two;
                if (ImGui::Slider("##bgm_volume", cfg.music_volume, 0, 100, "%d%%"))
                    App::update_mixer_volumes();
                if (ImGui::IsItemActivated() || ImGui::IsItemDeactivated())
                    UI::PlaySFXClick();
                help_marker("Set the volume of the background music.");
            }

            if (Mix_PlayingMusic())
                ImGui::FormatTextWrapped("Playing \"{}\" by \"{}\"",
                                         Mix_GetMusicTitleTag(nullptr),
                                         Mix_GetMusicArtistTag(nullptr));
        }

        void
        show_special_files_options() {
            ImGui::RAII::Indent one;

            if (UI::Button("Check integrity of Wii U Menu files"))
                SettingsPopup::open(SettingsPopup::OpenState::integrity);

            ImGui::SameLine();

            UI::Checkbox("Check at every boot", cfg.check_integrity_at_boot);

            if (UI::Button("Dump Wii U Menu files"))
                SettingsPopup::open(SettingsPopup::OpenState::dump);

            if (UI::Button("Clear Themiify cache"))
                SettingsPopup::open(SettingsPopup::OpenState::cache);
        }

        void
        show_stylemiiu_options() {
            using namespace ImGui::RAII;

            Indent one;

            if (auto pcfg = PluginManager::GetConfig()) {

                UI::Checkbox("Enable plugin", pcfg->themeManagerEnabled);
                help_marker("Set \"themeManagerEnabled\"");

                bool shuffle_value = pcfg->shuffleThemes;
                if (UI::Checkbox("Shuffle themes", shuffle_value))
                    PluginManager::ToggleShuffling();
                help_marker("Set \"suffleThemes\""); // NOTE: typo

                UI::Checkbox("Mash up themes", pcfg->mashupThemes);
                help_marker("Set \"mashupThemes\"");

                UI::Checkbox("Show notifications", pcfg->showNotification);
                help_marker("Set \"showNotification\"");

                if (UI::CollapsingHeader("Enabled themes:")) {
                    Indent two;
                    const ImVec2 themes_size = {
                        0,
                        8 * ImGui::GetTextLineHeightWithSpacing()
                    };
                    if (Child enabled_themes{"enabled_themes",
                                             themes_size,
                                             ImGuiChildFlags_Borders}) {
                        auto available_width = ImGui::GetContentRegionAvail().x;
                        ItemWidth set_width{available_width};
                        ThemeManager::ForEachInstalledTheme(
                            [](const ThemeManager::ConstThemePtr& theme)
                            {
                                bool enabled = PluginManager::IsEnabled(theme->path);
                                if (UI::Checkbox("##" + theme->path.filename().string(),
                                                    enabled)) {
                                    if (enabled)
                                        PluginManager::Enable(theme->path);
                                    else
                                        PluginManager::Disable(theme->path);
                                }
                                ImGui::SameLine();
                                ImGui::TextWrapped(theme->path.filename().string());
                            }
                        );
                    }
                }

                if (UI::Button("Manage installed themes..."))
                    NavBar::set_current_tab(NavBar::Tab::manage_themes);
                help_marker("Set \"enabledThemes\"");

            } else {
                ImGui::TextWrapped("Could not parse StyleMiiU configuration.");
            }

            if (UI::Button("Delete Style Mii U configuration")) {
                PluginManager::DeleteConfig();
            }
        }

        void
        show_themezer_options() {
            using namespace ImGui::RAII;

            Indent one;

            UI::Checkbox("Check for theme updates on startup",
                         cfg.check_themezer_updates_at_boot);
            help_marker("Will automaticaly connect to Themezer to check for updates"
                        " from the Manage Installed Themes screen.");
        }

    } // namespace

    /*------------------*/
    /* Public functions */
    /*------------------*/

    void
    initialize(SDL_Renderer * /*renderer*/) {
        TRACE_FUNC;
        bootIntegrityCheckPending = cfg.check_integrity_at_boot;
    }

    void
    finalize() {
        TRACE_FUNC;
    }

    void
    process_ui() {
        using namespace ImGui::RAII;

        {
            // NOTE: use a nested scope to not propagate changes to the popup.
#ifdef DEBUG_BG_COLOR
            StyleColor green_bg{ImGuiCol_ChildBg, {0.0, 0.5, 0.0, 1.0}};
#endif
            StyleVar padding{ImGuiStyleVar_WindowPadding, {6, 6}};
            if (Child settings_content{"SettingsContent", {0, 0},
                                       ImGuiChildFlags_AlwaysUseWindowPadding}) {
                {
                    Font font_guard{nullptr, 55};
                    ImGui::Text("Settings");
                }

                ImGui::SameLine();

                {
                    Font font_guard{nullptr, 25};
                    // Show text right-aligned.
                    const std::string text = "Themiify v" THEMIIFY_VERSION;
                    auto text_size = ImGui::CalcTextSize(text);
                    auto available = ImGui::GetContentRegionAvail();
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + available.x - text_size.x);
                    ImGui::Text(text);
                }

                ImGui::Separator();

                // Put options on their own child window, to scroll.
                if (Child settings_items{"SettingsOptions"}) {

                    if (UI::CollapsingHeader("Special files"))
                        show_special_files_options();

                    if (UI::CollapsingHeader("Themezer options"))
                        show_themezer_options();

                    if (UI::CollapsingHeader("Sound options"))
                        show_sound_options();

                    if (UI::CollapsingHeader("StyleMiiU options"))
                        show_stylemiiu_options();

                }

            }

        }

        SettingsPopup::process_ui();
    }

    bool
    check_is_first_boot() {
        return cfg.is_first_boot;
    }

    void
    run_boot_integrity_check() {
        if (!bootIntegrityCheckPending)
            return;

        SettingsPopup::open(SettingsPopup::OpenState::force_integrity);

        bootIntegrityCheckPending = false;
    }

    void
    run_first_boot_check() {
        if (!cfg.is_first_boot)
            return;

        SettingsPopup::open(SettingsPopup::OpenState::force_integrity);

        cfg.is_first_boot = false;
    }

} // namespace SettingsScreen
