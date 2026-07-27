/*
 * Themiify - A theme manager for the Nintendo Wii U
 * Copyright (C) 2026 Fangal-Airbag
 * Copyright (C) 2026 AlphaCraft9658
 * Copyright (C) 2026 Daniel K. O. <dkosmari>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <iostream>
#include <utility>

#include <SDL2/SDL.h>

#include <imgui.h>
#include <imgui_stdlib.h>
#include <imgui_raii.h>

#include "ThemeDetailsPopup.h"

#include "../App.h"
#include "../DownloadManager.h"
#include "../IconsFontAwesome4.h"
#include "../ImageLoader.h"
#include "../NavBar.h"
#include "../PluginManager.h"
#include "../ThemezerAPI.h"
#include "../tracer.hpp"
#include "DeleteThemePopup.h"
#include "DownloadThemePopup.h"
#include "HomeScreen.h"
#include "ManageThemesScreen.h"
#include "ThemePreviewPopup.h"

using std::cout;
using std::endl;
using namespace std::literals;

using ThemezerAPI::WiiuThemeFull;
using ThemezerAPI::WiiuThemeSmall;

namespace ThemeDetailsPopup {
    enum class State {
        hidden,
        waiting_themezer,
        ready_themezer,
        ready_local,
        error,
    };

    bool popup_queued;
    State state;
    std::string hexId;
    std::string error;
    WiiuThemeFull fullTheme;
    WiiuThemeSmall smallTheme;
    ThemeManager::ConstThemePtr installedTheme;
    const std::string popup_id = "ThemeDetailsPopup"s;

    void open_themezer(const WiiuThemeSmall &small_theme) {
        popup_queued = true;
        hexId = small_theme.hexId;
        state = State::hidden;
        error.clear();
        fullTheme = {};
        smallTheme = small_theme;
        ThemezerAPI::wiiu::theme(hexId,
                                 [](const WiiuThemeFull& t)
                                 {
                                    cout << "Got theme!" << endl;
                                    fullTheme = t;
                                    state = State::ready_themezer;
                                 });
        state = State::waiting_themezer;
    }

    void open_local(ThemeManager::ConstThemePtr installed_theme) {
        popup_queued = true;
        installedTheme = std::move(installed_theme);
        state = State::ready_local;
    }

    void show_label_text(const std::string& label,
                         const std::string& text)
    {
        const auto colors = ImGui::GetStyle().Colors;
        {
            ImGui::RAII::StyleColor label_color{ImGuiCol_Text, colors[ImGuiCol_HeaderActive]};
            ImGui::Text(label);
        }
        ImGui::SameLine();
        ImGui::TextWrapped(text);
    }

    void show_local() {
        using namespace ImGui::RAII;
        const auto &style = ImGui::GetStyle();

        // Split the window into two child windows: left and right
        const float total_width = ImGui::GetContentRegionAvail().x;
        float right_width = 300;
        float left_width = total_width - right_width - style.ItemSpacing.x;

        if (Child left{"left", {left_width, 0}, ImGuiChildFlags_NavFlattened}) {
            {
                Font name_font{nullptr, 50};
                ImGui::TextWrapped(installedTheme->metadata.themeName);
            }
            if (installedTheme->metadata.themeAuthor)
                ImGui::TextWrapped("by " + *installedTheme->metadata.themeAuthor);

            // Put content in a scrollable child window.
            if (Child content{"content", {0, 0},
                              ImGuiChildFlags_NavFlattened
                              | ImGuiChildFlags_AlwaysUseWindowPadding}) {
                // Always scroll to the top when the popup first appears.
                if (ImGui::IsWindowAppearing())
                    ImGui::SetScrollY(0);

                if (!installedTheme->previews.empty()) {
                    ImVec2 preview_size {640, 360};
                    // Center the image.
                    auto available = ImGui::GetContentRegionAvail();
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (available.x - preview_size.x)
                                         / 2);
                    auto img = ImageLoader::get(installedTheme->previews.front());
                    StyleVar no_padding{ImGuiStyleVar_FramePadding, {0, 0}};
                    if (ImGui::ImageButton("preview", (ImTextureID)img, preview_size))
                        ThemePreviewPopup::open(installedTheme->path,
                                                installedTheme->previews);
                }

                if (!installedTheme->files.empty()) {
                    ImGui::SeparatorText("Files:");
                    Font file_font{nullptr, 20};
                    const std::string prefix = installedTheme->path.string() + "/";
                    for (const auto& filename : installedTheme->files) {
                        std::string label = filename;
                        if (label.starts_with(prefix))
                            label.erase(0, prefix.size());
                        ImGui::Bullet();
                        ImGui::SameLine();
                        ImGui::TextAligned(0, -1, label);
                    }
                }
            }
        }

        ImGui::SameLine();

        if (Child right{"right", {right_width, 0}, ImGuiChildFlags_NavFlattened}) {
            {
                // Calculate how much space is needed for the buttons at the bottom.
                const int num_buttons = 4;
                const float buttons_height = num_buttons * ImGui::GetFrameHeightWithSpacing();

                // Put content in a scrollable child window.
                if (Child content{"content", {0 , -buttons_height},
                                  ImGuiChildFlags_NavFlattened
                                  | ImGuiChildFlags_AlwaysUseWindowPadding}) {
                    if (installedTheme->metadata.themeVersion) {
                        Font small{nullptr, 24};
                        show_label_text(ICON_FA_CALENDAR_CHECK_O,
                                        installedTheme->metadata.themeVersion->substr(0, 10));
                        ImGui::SetItemTooltip("Theme version.");
                    }
                }

                auto cfg = PluginManager::GetConfig();
                bool is_enabled = PluginManager::IsEnabled(installedTheme->path);

                if (cfg && cfg->shuffleThemes) {
                    if (ImGui::Checkbox("Enabled", is_enabled)) {
                        if (is_enabled)
                            PluginManager::Enable(installedTheme->path);
                        else
                            PluginManager::Disable(installedTheme->path);
                    }
                } else {
                    // Disable button if there's no config, or theme is already enabled.
                    Disabled disabled_if{!cfg || is_enabled};
                    if (ImGui::Button(ICON_FA_STAR " Apply", {-1, 0})) {
                        PluginManager::Enable(installedTheme->path);
                        ImGui::CloseCurrentPopup();
                    }
                }
                ImGui::SetItemDefaultFocus();

                {
                    Disabled if_no_previews{installedTheme->previews.empty()};
                    if (ImGui::Button(ICON_FA_EYE " Preview", {-1, 0}))
                        ThemePreviewPopup::open(installedTheme->path,
                                                installedTheme->previews);
                }

                if (ImGui::Button(ICON_FA_TRASH " Delete", {-1, 0})) {
                    DeleteThemePopup::open(installedTheme);
                }

                if (ImGui::Button(ICON_FA_TIMES " Close", {-1, 0}))
                    ImGui::CloseCurrentPopup();
            }
        }
    }

    void show_themezer() {
        using namespace ImGui::RAII;
        const auto &style = ImGui::GetStyle();

        // Split the window into two child windows: left and right
        const float total_width = ImGui::GetContentRegionAvail().x;
        float right_width = 300;
        float left_width = total_width - right_width - style.ItemSpacing.x;

        if (Child left{"left", {left_width, 0},
                       ImGuiChildFlags_NavFlattened}) {
            {
                Font name_font{nullptr, 50};
                ImGui::TextWrapped(fullTheme.name);
            }
            ImGui::TextWrapped("by " + fullTheme.creator.username);

            // Put content in a scrollable child window.
            if (Child content{"content",
                              {0, 0},
                              ImGuiChildFlags_NavFlattened
                              | ImGuiChildFlags_AlwaysUseWindowPadding}) {
                // Always scroll to the top when the popup first appears.
                if (ImGui::IsWindowAppearing())
                    ImGui::SetScrollY(0);

                if (!fullTheme.collagePreview.hdUrl.empty()) {
                    ImVec2 preview_size = {640, 360};
                    // Center the image.
                    auto available = ImGui::GetContentRegionAvail();
                    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (available.x - preview_size.x) / 2);
                    auto img = ImageLoader::get(fullTheme.collagePreview.sdUrl);
                    StyleVar no_padding{ImGuiStyleVar_FramePadding, {0, 0}};
                    if (ImGui::ImageButton("preview",
                                           (ImTextureID)img,
                                           preview_size))
                        ThemePreviewPopup::open(fullTheme.hexId,
                                                std::vector<std::string>{
                                                    fullTheme.collagePreview.hdUrl,
                                                    fullTheme.launcherScreenshot.hdUrl,
                                                    fullTheme.waraWaraPlazaScreenshot.hdUrl
                                                });
                }

                if (fullTheme.description) {
                    Font small{nullptr, 24};
                    ImGui::TextWrapped(*fullTheme.description);
                }
            }

        }

        ImGui::SameLine();

        if (Child right{"right", {right_width, 0},
                        ImGuiChildFlags_NavFlattened}) {

            // Calculate how much space is needed for the buttons at the bottom.
            const int num_buttons = 3;
            const float buttons_height = num_buttons * ImGui::GetFrameHeightWithSpacing();

            // Put content in a scrollable child window.
            if (Child content{"content", {0, -buttons_height},
                              ImGuiChildFlags_NavFlattened |
                              ImGuiChildFlags_AlwaysUseWindowPadding}) {
                // Always scroll to the top when the popup first appears.
                if (ImGui::IsWindowAppearing())
                    ImGui::SetScrollY(0);

                Font small{nullptr, 24};
                StyleVar spacing{ImGuiStyleVar_ItemSpacing, {12, 10}};
                show_label_text("QuickID:", fullTheme.quickId);
                show_label_text(ICON_FA_CALENDAR_PLUS_O, fullTheme.createdAt.substr(0, 10));
                ImGui::SetItemTooltip("Creation date");
                show_label_text(ICON_FA_CALENDAR_CHECK_O, fullTheme.updatedAt.substr(0, 10));
                ImGui::SetItemTooltip("Update date");
                show_label_text(ICON_FA_DOWNLOAD, std::to_string(fullTheme.downloadCount));
                ImGui::SetItemTooltip("Number of downloads");
                if (!fullTheme.tags.empty()) {
                    ImGui::Separator();
                    for (auto& tag : fullTheme.tags)
                        show_label_text(ICON_FA_TAG, tag.name);
                }
            }

            if (ImGui::Button(ICON_FA_DOWNLOAD " Download", {-1, 0}))
                DownloadThemePopup::open(smallTheme);
            ImGui::SetItemDefaultFocus();

            if (ImGui::Button(ICON_FA_EYE " Preview", {-1, 0}))
                ThemePreviewPopup::open(fullTheme.hexId,
                                        std::vector<std::string>{
                                            fullTheme.collagePreview.hdUrl,
                                            fullTheme.launcherScreenshot.hdUrl,
                                            fullTheme.waraWaraPlazaScreenshot.hdUrl
                                        });

            if (ImGui::Button(ICON_FA_TIMES " Close", {-1, 0}))
                ImGui::CloseCurrentPopup();
        }
    }

    void process_ui() {
        using namespace ImGui::RAII;

        if (state == State::hidden)
            return;

        if (popup_queued) {
            ImGui::OpenPopup(popup_id);
            popup_queued = false;
        }

        auto viewport = ImGui::GetMainViewport();
        auto center = viewport->GetCenter();
        ImGui::SetNextWindowSize({1140, 680}, ImGuiCond_Always);
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, {0.5f, 0.5f});
        Popup popup{popup_id,
                    ImGuiWindowFlags_NoSavedSettings |
                    ImGuiWindowFlags_NoMove};

        if (!popup) {
            state = State::hidden;
            return;
        }

        switch (state) {
            case State::waiting_themezer:
                ImGui::Text("Fetching theme details...");
                break;

            case State::error:
                ImGui::FormatTextWrapped("Error: {}", error);
                break;

            case State::ready_themezer:
                show_themezer();
                break;

            case State::ready_local:
                show_local();
                break;

            default:
                ;
        }

        ThemePreviewPopup::process_ui();
    }
}
