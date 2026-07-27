/*
 * Themiify - A theme manager for the Nintendo Wii U
 * Copyright (C) 2026 Fangal-Airbag
 * Copyright (C) 2026 AlphaCraft9658
 * Copyright (C) 2026 Daniel K. O. <dkosmari>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "HomeScreen.h"
#include "SettingsScreen.h"
#include "SettingsPopup.h"
#include "../IconsFontAwesome4.h"
#include "../ImageLoader.h"
#include "../NavBar.h"
#include "../PluginManager.h"
#include "../ThemeManager.h"
#include "../tracer.hpp"
#include "../utils.h"

#include <iostream>
#include <filesystem>
#include <optional>
#include <string>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include <imgui.h>
#include <imgui_raii.h>

#include <mocha/mocha.h>

using std::cout;
using std::cerr;
using std::endl;
using namespace std::literals;

// Define this to help seeing the padding and spacing values for windows.
// #define DEBUG_BG_COLOR

// Enable to inject stylemiiu missing state
// #define DEBUG_INJECT_STYLEMIIU_MISSING

namespace HomeScreen {

    using ThemeManager::Theme;

    SDL_Renderer *home_renderer = nullptr;

    SDL_Texture *themiify_logo = nullptr;

    bool isFirstBoot = true;
    bool styleMiiUExists = true;
    bool queueStyleMiiUPrompt = false;

    void initialize(SDL_Renderer *renderer) {
        TRACE_FUNC;

        home_renderer = renderer;

        themiify_logo = IMG_LoadTexture(renderer, "fs:/vol/content/ui/themiify-logo.png");

        styleMiiUExists = PluginManager::IsInstalled();
        if (!styleMiiUExists)
            queueStyleMiiUPrompt = true;
#ifdef DEBUG_INJECT_STYLEMIIU_MISSING
        queueStyleMiiUPrompt = true;
#endif

        cout << "styleMiiUExists: " << styleMiiUExists << endl;

        isFirstBoot = SettingsScreen::check_is_first_boot();
    }

    void finalize() {
        TRACE_FUNC;

        if (themiify_logo) {
            SDL_DestroyTexture(themiify_logo);
            themiify_logo = nullptr;
        }
    }

    void show_current_theme() {
        using namespace ImGui::RAII;
        {
            Font font{nullptr, 35};
            ImGui::Text("Your current theme:");
        }

        // const ImVec2 img_size = {640, 360};
        // const ImVec2 img_size = {560, 315};
        const ImVec2 img_size = {512, 288};

        if (Child theme_frame{"theme_frame",
                              {0, 0},
                              ImGuiChildFlags_NavFlattened |
                              ImGuiChildFlags_FrameStyle |
                              ImGuiChildFlags_AutoResizeY,
                              ImGuiWindowFlags_NoSavedSettings}) {

            const bool refreshing_themes = ThemeManager::IsRefreshingThemes();
            auto cfg = PluginManager::GetConfig();
            SDL_Texture* img = nullptr;
            std::string text;
            std::string subtext;
            if (!cfg) {
                // No valid plugin config
                img = ImageLoader::get("ui/theme-placeholder-no-theme.png");
                text = "Could not read StyleMiiU config.";
            } else {
                // We have valid plugin config.
                if (cfg->shuffleThemes) {
                    img = ImageLoader::get("ui/theme-placeholder-random.png");
                    text = "Theme shuffling enabled.";
                } else {
                    // Not shuffling, so expect zero or one theme.
                    if (cfg->enabledThemes.empty()) {
                        img = ImageLoader::get("ui/theme-placeholder-no-theme.png");
                        text = "No theme set.";
                    } else {
                        // Should have exactly one theme, but it might not be loaded yet.
                        auto theme = ThemeManager::GetCurrentTheme();
                        if (theme) {
                            if (theme->previews.empty())
                                img = ImageLoader::get("ui/theme-placeholder-no-preview.png");
                            else
                                img = ImageLoader::get(theme->previews.front());
                            text = theme->metadata.themeName;
                            if (theme->metadata.themeAuthor)
                                subtext = "by " + *theme->metadata.themeAuthor;
                        } else {
                            // No theme info found, maybe it's still loading?
                            if (refreshing_themes) {
                                img = ImageLoader::get("ui/theme-placeholder-loading.png");
                                text = "Loading theme info...";
                            } else {
                                // Theme just doesn't exist.
                                img = ImageLoader::get("ui/load-error-image.png");
                                text = "Theme not found.";
                            }
                        }
                    }
                }
            }

            if (img) {
                StyleVar no_border{ImGuiStyleVar_ImageBorderSize, 0};
                ImGui::Image((ImTextureID)img, img_size);
                ImGui::SameLine();
            }

            {
                Group right_group;
                {
                    Font font_guard{nullptr, 30};
                    ImGui::TextWrapped(text);
                }
                if (!subtext.empty())
                    ImGui::TextWrapped(subtext);
            }
        }
    }

    void show_credits() {
        using namespace ImGui::RAII;
        {
            Font font_guard{nullptr, 45};
            ImGui::Text("Credits:");
        }

        ImGui::Spacing();

        ImGui::Text(ICON_FA_CODE " Developers:");
        {
            Indent _;
            ImGui::BulletText("Fangal-Airbag");
            ImGui::BulletText("AlphaCraft9658");
            ImGui::BulletText("Daniel K. O. (dkosmari)");
        }

        ImGui::Spacing();

        ImGui::Text(ICON_FA_PAINT_BRUSH " UI Design:");
        {
            Indent _;
            ImGui::BulletText("Perrohuevo");
            ImGui::BulletText("dewgong");
            ImGui::BulletText("Daniel K. O. (dkosmari)");
        }

        ImGui::Spacing();

        ImGui::Text(ICON_FA_FONT " Fonts:");
        {
            Indent _;
            ImGui::BulletText("Wii U System Font");
            ImGui::BulletText("FontAwesome");
        }

        ImGui::Spacing();

        ImGui::Text(ICON_FA_MUSIC " Music:");
        {
            Indent one;
            ImGui::BulletText("OMORI OST - 029 Good For Health, Bad for Imagination");
            {
                Indent two;
                ImGui::TextLink("https://www.youtube.com/watch?v=XeK_I0XQW6U");
            }
        }

        ImGui::Spacing();

        ImGui::Text(ICON_FA_GITHUB " GitHub:");
        {
            Indent _;
            ImGui::Bullet();
            ImGui::TextLink("https://github.com/ThemeCafe/Themiify");
        }

        ImGui::Spacing();

        ImGui::Text(ICON_FA_STAR " Special thanks:");
        {
            Indent _;
            ImGui::BulletText("Juanen100 for the StyleMiiU Aroma Plugin!");
            ImGui::BulletText("The Theme Café Discord mods, devs and founders!");
            ImGui::BulletText("Gatto for the incredible Theme Café docs!");
            ImGui::BulletText("Migush and the whole Themezer team!");
            ImGui::BulletText("All the amazing Wii U theme creators!");
            ImGui::BulletText("And many more!");
        }
    }

    void process_ui() {
        using namespace ImGui::RAII;

        const auto &style = ImGui::GetStyle();
#ifdef DEBUG_BG_COLOR
        StyleColor blue_bg{ImGuiCol_ChildBg, {0.0, 0.5, 0.0, 1.0}};
#endif
        Child home_content{"HomeContent", {0, 0}, ImGuiChildFlags_AlwaysUseWindowPadding};
        if (!home_content)
            return;

        if (themiify_logo) {

            ImGui::Image((ImTextureID)themiify_logo, {520, 124});

            ImGui::SameLine();

            {
                Font font{nullptr, 25};
                ImGui::Text("v"s + THEMIIFY_VERSION);
            }
        }

        {
            Font font{nullptr, 55};
            // Cute lil thing cause why not?
            isFirstBoot ? ImGui::Text("Welcome!") : ImGui::Text("Welcome back!");
        }

        if (queueStyleMiiUPrompt) {
            SettingsPopup::open(SettingsPopup::OpenState::stylemiiu);
            queueStyleMiiUPrompt = false;
        }

        if (styleMiiUExists) {
            // For checking integrity at boot
            SettingsScreen::run_first_boot_check();
            SettingsScreen::run_boot_integrity_check();
        }

        SettingsPopup::process_ui();

#ifdef DEBUG_BG_COLOR
        StyleColor brown_bg{ImGuiCol_ChildBg, {0.3, 0.3, 0.0, 1.0}};
#endif
        if (Child scrollable_content{"scrollable_content"}) {
            show_current_theme();

            if (ImGui::Button("Download Themes"))
                NavBar::set_current_tab(NavBar::Tab::themezer);

            ImGui::SameLine();

            {
                // align next button to the right
                auto available = ImGui::GetContentRegionAvail();
                std::string label = "Manage Installed Themes";
                ImVec2 button_size = ImGui::CalcTextSize(label) + 2 * style.FramePadding;
                ImGui::SetCursorPosX(ImGui::GetCursorPosX() + available.x - button_size.x);
                if (ImGui::Button(label, button_size))
                    NavBar::set_current_tab(NavBar::Tab::manage_themes);
            }

            ImGui::Separator();

            show_credits();
        }
    }

} // namespace HomeScreen
