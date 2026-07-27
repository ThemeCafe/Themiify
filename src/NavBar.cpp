/*
 * Themiify - A theme manager for the Nintendo Wii U
 * Copyright (C) 2026 Fangal-Airbag
 * Copyright (C) 2026 AlphaCraft9658
 * Copyright (C) 2026 Daniel K. O. <dkosmari>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <iostream>

#include "NavBar.h"

#include "App.h"
#include "screens/ConfirmExitPopup.h"
#include "tracer.hpp"
#include "utils.h"

#include <SDL_image.h>

#include <imgui.h>
#include <imgui_stdlib.h>
#include <imgui_raii.h>

// Define this to help seeing the padding and spacing values for windows.
// #define DEBUG_BG_COLOR

using std::cout;
using std::endl;

namespace NavBar {
    SDL_Texture *logo_tex;

    SDL_Texture *home_button_normal_tex;
    SDL_Texture *home_button_active_tex;

    SDL_Texture *manage_themes_button_normal_tex;
    SDL_Texture *manage_themes_button_active_tex;

    SDL_Texture *settings_button_normal_tex;
    SDL_Texture *settings_button_active_tex;

    SDL_Texture *themezer_button_normal_tex;
    SDL_Texture *themezer_button_active_tex;

    SDL_Texture *exit_button_normal_tex;
    SDL_Texture *exit_button_active_tex;

    Tab current_tab = Tab::home;

    void initialize(SDL_Renderer *renderer) {
        TRACE_FUNC;

        logo_tex = IMG_LoadTexture(renderer, "fs:/vol/content/ui/logo.png");

        home_button_normal_tex = IMG_LoadTexture(renderer, "fs:/vol/content/ui/home-button-normal.png");
        home_button_active_tex = IMG_LoadTexture(renderer, "fs:/vol/content/ui/home-button-active.png");

        manage_themes_button_normal_tex = IMG_LoadTexture(renderer, "fs:/vol/content/ui/manage-themes-button-normal.png");
        manage_themes_button_active_tex = IMG_LoadTexture(renderer, "fs:/vol/content/ui/manage-themes-button-active.png");

        settings_button_normal_tex = IMG_LoadTexture(renderer, "fs:/vol/content/ui/settings-button-normal.png");
        settings_button_active_tex = IMG_LoadTexture(renderer, "fs:/vol/content/ui/settings-button-active.png");

        themezer_button_normal_tex = IMG_LoadTexture(renderer, "fs:/vol/content/ui/themezer-button-normal.png");
        themezer_button_active_tex = IMG_LoadTexture(renderer, "fs:/vol/content/ui/themezer-button-active.png");

        exit_button_normal_tex = IMG_LoadTexture(renderer, "fs:/vol/content/ui/exit-button-normal.png");
        exit_button_active_tex = IMG_LoadTexture(renderer, "fs:/vol/content/ui/exit-button-active.png");
    }

    void finalize() {
        TRACE_FUNC;

        SDL_DestroyTexture(logo_tex);

        SDL_DestroyTexture(home_button_normal_tex);
        SDL_DestroyTexture(home_button_active_tex);

        SDL_DestroyTexture(manage_themes_button_normal_tex);
        SDL_DestroyTexture(manage_themes_button_active_tex);

        SDL_DestroyTexture(settings_button_normal_tex);
        SDL_DestroyTexture(settings_button_active_tex);

        SDL_DestroyTexture(themezer_button_normal_tex);
        SDL_DestroyTexture(themezer_button_active_tex);

        SDL_DestroyTexture(exit_button_normal_tex);
        SDL_DestroyTexture(exit_button_active_tex);
    }

    void process_ui() {
        using namespace ImGui::RAII;

        StyleVar no_child_border{ImGuiStyleVar_ChildBorderSize, 0.0f};
        StyleVar item_spacing{ImGuiStyleVar_ItemSpacing, {0.0f, 12.0f}};
        // auto viewport = ImGui::GetMainViewport();

#ifdef DEBUG_BG_COLOR
        StyleColor navbar_bg{ImGuiCol_ChildBg, {0.5f, 0.0f, 0.0f, 1.0f}};
#endif
        const float navbar_width = 160 - 8;

        Child nav_bar{"NavBar", {navbar_width, 0},
                      ImGuiChildFlags_NavFlattened,
                      ImGuiWindowFlags_NoScrollbar};
        if (!nav_bar)
            return;

        StyleVar no_frame_border{ImGuiStyleVar_FrameBorderSize, 0};
        StyleVar no_image_border{ImGuiStyleVar_ImageBorderSize, 0};
        StyleVar no_frame_rounding{ImGuiStyleVar_FrameRounding, 0};
        StyleVar no_frame_padding{ImGuiStyleVar_FramePadding, {0, 0}};

        ImGui::Image(logo_tex, {152.4f, 138.0f});

        const auto &style = ImGui::GetStyle();
        const auto available = ImGui::GetContentRegionAvail();
        const int num_buttons = 5;
        const ImVec2 button_size = {148, 96};
        const float total_empty =
            available.y
            - num_buttons * button_size.y
            - style.SeparatorSize;
        const float spacing = total_empty / num_buttons;

        StyleVar button_spacing{ImGuiStyleVar_ItemSpacing, {0, spacing}};

#ifdef DEBUG_BG_COLOR
        StyleColor buttons_box_bg{ImGuiCol_ChildBg, {0.0f, 0.5f, 0.0f, 1.0f}};
#endif
        if (Child buttons_box{"ButtonsBox", {navbar_width, 0}, ImGuiChildFlags_NavFlattened}) {
            if (current_tab == Tab::home) {
                ImGui::ImageButton("home_button_active", home_button_active_tex, button_size);
            }
            else {
                // TODO: Implement the App::ImageButton overload in the App namespace to
                // add the sound effect and rumble when clicked
                if (ImGui::ImageButton("home_button_normal", home_button_normal_tex, button_size)) {
                    current_tab = Tab::home;
                }
            }

            if (current_tab == Tab::manage_themes) {
                ImGui::ImageButton("installed_button_active", manage_themes_button_active_tex, button_size);
            }
            else {
                if (ImGui::ImageButton("installed_button_normal", manage_themes_button_normal_tex, button_size)) {
                    current_tab = Tab::manage_themes;
                }
            }

            if (current_tab == Tab::themezer) {
                ImGui::ImageButton("themezer_button_active", themezer_button_active_tex, button_size);
            }
            else {
                if (ImGui::ImageButton("themezer_button_normal", themezer_button_normal_tex, button_size)) {
                    current_tab = Tab::themezer;
                }
            }

            if (current_tab == Tab::settings) {
                ImGui::ImageButton("settings_button_active", settings_button_active_tex, button_size);
            }
            else {
                if (ImGui::ImageButton("settings_button_normal", settings_button_normal_tex, button_size)) {
                    current_tab = Tab::settings;
                }
            }

            ImGui::Separator();

            if (ImGui::ImageButton("exit_button_normal", exit_button_normal_tex, button_size)) {
                ConfirmExitPopup::open();
            }
        }
    }

    Tab get_current_tab() {
        return current_tab;
    }

    void set_current_tab(Tab tab) {
        current_tab = tab;
    }
}
