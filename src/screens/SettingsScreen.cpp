/*
 * Themiify - A theme manager for the Nintendo Wii U
 * Copyright (C) 2026 Fangal-Airbag
 * Copyright (C) 2026 AlphaCraft9658
 * Copyright (C) 2026 Daniel K. O. <dkosmari>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "SettingsScreen.h"
#include "SettingsPopup.h"
#include "../NavBar.h"
#include "../PluginManager.h"
#include "../ThemeManager.h"
#include "../utils.h"
#include "../tracer.hpp"

#include <iostream>

#include <SDL2/SDL_mixer.h>

#include <imgui.h>
#include <imgui_raii.h>

#include <glaze/exceptions/json_exceptions.hpp>
#include <glaze/json.hpp>

// Define this to help seeing the padding and spacing values for windows.
// #define DEBUG_BG_COLOR

using std::cout;
using std::cerr;
using std::endl;

namespace SettingsScreen {

    namespace {

        /*-------*/
        /* Types */
        /*-------*/

        struct Settings {
            bool is_first_boot = true;
            bool check_integrity_at_boot = false;
            int music_volume = 75;
        };

        /*-----------*/
        /* Constants */
        /*-----------*/

        // Don't error out when unknown fields are found, to allow users to downgrade.
        constexpr glz::opts read_opts = { .error_on_unknown_keys = false };

        constexpr glz::opts write_opts = { .prettify = true };

        const std::filesystem::path settings_path = THEMIIFY_ROOT / "settings.json";

        /*-----------*/
        /* Variables */
        /*-----------*/

        bool bootIntegrityCheckPending;

        Settings settings;

        /*-----------------------*/
        /* Function declarations */
        /*-----------------------*/

        void
        load_settings();

        void
        save_settings();

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
        load_settings() {
            TRACE_FUNC;
            try {
                glz::ex::read_file_json<read_opts>(settings,
                                                   settings_path.string(),
                                                   std::string{});
            }
            catch (std::exception& e) {
                cerr << "ERROR loading settings: " << e.what() << endl;
            }
        }

        void
        save_settings() {
            TRACE_FUNC;
            try {
                create_directories(THEMIIFY_ROOT);
                glz::ex::write_file_json<write_opts>(settings,
                                                     settings_path.string(),
                                                     std::string{});
            }
            catch (std::exception& e) {
                cerr << "ERROR saving settings: " << e.what() << endl;
            }
        }

        void
        show_sound_options() {
            ImGui::RAII::Indent _;

            ImGui::Text("Background music volume level:");
            if (ImGui::Slider("##volume", settings.music_volume, 0, 100, "%d%%")) {
                int mix_volume = (settings.music_volume * MIX_MAX_VOLUME) / 100;
                Mix_VolumeMusic(mix_volume);
            }
        }

        void
        show_special_files_options() {
            ImGui::RAII::Indent _;

            if (ImGui::Button("Check integrity of Wii U Menu files"))
                SettingsPopup::open(SettingsPopup::OpenState::integrity);

            ImGui::SameLine();

            ImGui::Checkbox("Check at every boot", settings.check_integrity_at_boot);

            if (ImGui::Button("Dump Wii U Menu files"))
                SettingsPopup::open(SettingsPopup::OpenState::dump);

            if (ImGui::Button("Clear Themiify cache"))
                SettingsPopup::open(SettingsPopup::OpenState::cache);
        }

        void
        show_stylemiiu_options() {
            using namespace ImGui::RAII;

            Indent _;

            if (auto cfg = PluginManager::GetConfig()) {

                ImGui::Checkbox("Enable plugin", cfg->themeManagerEnabled);
                ImGui::SetItemTooltip("Set \"themeManagerEnabled\"");

                bool shuffle_value = cfg->shuffleThemes;
                if (ImGui::Checkbox("Shuffle themes", shuffle_value))
                    PluginManager::ToggleShuffling();
                ImGui::SetItemTooltip("Set \"suffleThemes\""); // NOTE: typo

                ImGui::Checkbox("Mash up themes", cfg->mashupThemes);
                ImGui::SetItemTooltip("Set \"mashupThemes\"");

                ImGui::Checkbox("Show notifications", cfg->showNotification);
                ImGui::SetItemTooltip("Set \"showNotification\"");

                if (ImGui::CollapsingHeader("Enabled themes:")) {
                    Indent _2;
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
                            [](std::size_t, const ThemeManager::ConstThemePtr& theme)
                            {
                                bool enabled = PluginManager::IsEnabled(theme->path);
                                if (ImGui::Checkbox("##" + theme->path.filename().string(),
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

                if (ImGui::Button("Manage installed themes..."))
                    NavBar::set_current_tab(NavBar::Tab::manage_themes);
                ImGui::SetItemTooltip("Set \"enabledThemes\"");

            } else {
                ImGui::TextWrapped("Could not parse StyleMiiU configuration.");
            }

            if (ImGui::Button("Delete Style Mii U configuration")) {
                PluginManager::DeleteConfig();
            }
        }

    } // namespace

    /*------------------*/
    /* Public functions */
    /*------------------*/

    void
    initialize(SDL_Renderer * /*renderer*/) {
        TRACE_FUNC;

        create_directories(THEMIIFY_ROOT);

        load_settings();

        bootIntegrityCheckPending = settings.check_integrity_at_boot;
        int mix_volume = (settings.music_volume * MIX_MAX_VOLUME) / 100;
        Mix_VolumeMusic(mix_volume);
    }

    void
    finalize() {
        TRACE_FUNC;

        save_settings();
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
                    if (ImGui::CollapsingHeader("Special files"))
                        show_special_files_options();
                    if (ImGui::CollapsingHeader("Sound options"))
                        show_sound_options();
                    if (ImGui::CollapsingHeader("StyleMiiU options"))
                        show_stylemiiu_options();
                }

            }

        }

        SettingsPopup::process_ui();
    }

    bool
    check_is_first_boot() {
        return settings.is_first_boot;
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
        if (!settings.is_first_boot)
            return;

        SettingsPopup::open(SettingsPopup::OpenState::force_integrity);

        settings.is_first_boot = false;
        save_settings();

        settings.is_first_boot = false;
    }

} // namespace SettingsScreen
