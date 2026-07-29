/*
 * Themiify - A theme manager for the Nintendo Wii U
 * Copyright (C) 2026 Fangal-Airbag
 * Copyright (C) 2026 AlphaCraft9658
 * Copyright (C) 2026 Daniel K. O. <dkosmari>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <cmath>
#include <iostream>
#include <optional>
#include <ranges>
#include <unordered_set>

#include <SDL.h>
#include <SDL_image.h>

#include <imgui.h>
#include <imgui_raii.h>

#include "ManageThemesScreen.h"

#include "../IconsFontAwesome4.h"
#include "../ImageLoader.h"
#include "../PluginManager.h"
#include "../ThemeManager.h"
#include "../ThemezerAPI.h"
#include "../tracer.hpp"
#include "../UI.h"
#include "../utils.h"
#include "DeleteThemePopup.h"
#include "DownloadThemePopup.h"
#include "InstallThemePopup.h"
#include "QRCodePopup.h"
#include "ThemeDetailsPopup.h"

// Define this to help seeing the padding and spacing values for windows.
// #define DEBUG_BG_COLOR

using std::cout;
using std::endl;
using namespace std::literals;

namespace ManageThemesScreen {

    namespace {

        /*-------*/
        /* Types */
        /*-------*/

        enum class Tab {
            installed,
            uthemes,
        };

        /*-----------*/
        /* Variables */
        /*-----------*/

        // NOTE: keep a copy of the themes so we can easily filter and reorder them.
        std::vector<ThemeManager::ConstThemePtr> installed_themes;
        std::vector<std::size_t> visible_indexes;

        SDL_Renderer *manage_renderer;

        std::string search;

        // NOTE: used to track when the tab swich happens.
        Tab current_tab = Tab::installed;

        bool update_check_queued;
        bool update_check_performed;

        std::unordered_set<std::string> themes_with_update;

        /*-----------------------*/
        /* Function declarations */
        /*-----------------------*/

        ImVec2
        calc_text_size(float font_size, const std::string& text);

        void
        check_updates();

        bool
        has_update(const ThemeManager::ConstThemePtr& theme);

        bool
        is_from_themezer(const ThemeManager::ConstThemePtr& theme);

        void
        show_installed_theme(const ThemeManager::ConstThemePtr& theme,
                             const ImVec2& inner_size,
                             const ImVec2& padding);

        void
        show_tab_install_local();

        void
        show_tab_manage_installed();

        void
        show_utheme(const std::filesystem::path& utheme,
                    const ThemeManager::ConstMetadataPtr& metadata);

        int
        similarity_score(const std::string& haystack_,
                         const std::string& needle_);

        void
        switch_tab(Tab tab);

        void
        text_limited(float width,
                     const std::string& text);

        /*----------------------*/
        /* Function definitions */
        /*----------------------*/

        ImVec2
        calc_text_size(float font_size, const std::string& text)
        {
            ImGui::RAII::Font font{nullptr, font_size};
            return ImGui::CalcTextSize(text);
        }

        void
        check_updates()
        {
            ThemezerAPI::wiiu::CheckUpdatesSpec spec;
            ThemeManager::ForEachInstalledTheme(
                [&spec](const ThemeManager::ConstThemePtr& theme)
                {
                    if (is_from_themezer(theme)
                        && theme->metadata.themeVersion
                        && !theme->metadata.themeVersion->empty())
                        spec.items.emplace_back(*theme->metadata.themeID,
                                                *theme->metadata.themeVersion);
                }
            );
            if (spec.items.empty())
                return;

            ThemezerAPI::wiiu::checkUpdates(
                spec,
                [](const ThemezerAPI::WiiuBaseVec& themes)
                {
                    update_check_performed = true;
                    std::unordered_set<std::string> new_themes_with_update;
                    for (auto& theme : themes)
                        new_themes_with_update.insert(theme.hexId);
                    themes_with_update = std::move(new_themes_with_update);
                }
            );
        }

        bool
        has_update(const ThemeManager::ConstThemePtr& theme)
        {
            if (!theme->metadata.themeID)
                return false;
            static const std::string prefix = "Themezer:";
            if (!theme->metadata.themeID->starts_with(prefix))
                return false;
            return themes_with_update.contains(theme->metadata.themeID->substr(prefix.size()));
        }

        bool
        is_from_themezer(const ThemeManager::ConstThemePtr& theme)
        {
            return theme->metadata.themeID
                && theme->metadata.themeID->starts_with("Themezer:");
        }

        void
        show_installed_theme(const ThemeManager::ConstThemePtr& theme,
                             const ImVec2& inner_size,
                             const ImVec2& padding) {
            // NOTE: to create a complex button, we create a button with no text, then overlap
            // the contents.
            using namespace ImGui::RAII;

            ID id{theme->path.string()};

            const auto& style = ImGui::GetStyle();
            const ImVec2 outer_size = inner_size + 2 * padding;

            // Put everything inside a child window so we can bail out when not visibile.
            Child container{"container", outer_size,
                            ImGuiChildFlags_NavFlattened};
            if (!container)
                return;

            const ImVec2 start_pos = padding;

            bool open_popup = false;
            ImGui::SetCursorPos({0, 0});
            if (UI::Button("##button", outer_size)) {
                // NOTE: delay opening the popup, gotta check if the user clicked on the
                // "enabled" icon.
                open_popup = true;
            }

            // NOTE: when hovered or activated, make text dark.
            bool invert_colors = ImGui::IsItemHovered() || ImGui::IsItemActive();
            const auto& colors = style.Colors;
            const auto light_color = colors[ImGuiCol_ButtonActive];
            const auto dark_color = colors[ImGuiCol_WindowBg];
            std::optional<StyleColor> dark_text;
            if (invert_colors)
                dark_text.emplace(ImGuiCol_Text, dark_color);

            ImGui::SetCursorPos(start_pos);
            Group grp;

            {
                const ImVec2 img_size = {inner_size.x, inner_size.x * 9.0f / 16.0f};
                StyleVar no_border{ImGuiStyleVar_ImageBorderSize, 0};
                auto img = !theme->previews.empty()
                    ? ImageLoader::get(theme->previews.front())
                    : ImageLoader::get("ui/theme-placeholder-no-preview.png");
                ImGui::Image((ImTextureID)img, img_size);
            }

            auto cfg = PluginManager::GetConfig();
            bool is_shuffling = cfg && cfg->shuffleThemes;
            bool is_enabled = PluginManager::IsEnabled(theme->path);

            // Icon to show "enabled" state.
            const std::string enabled_label = is_shuffling
                ? (is_enabled ? ICON_FA_CHECK_CIRCLE_O : ICON_FA_CIRCLE_O)
                : (is_enabled ? ICON_FA_STAR : ICON_FA_STAR_O);
            const float enabled_font_size = 56;
            const ImVec2 enabled_size = calc_text_size(enabled_font_size, enabled_label);

            // Icon to show status.
            const bool found_themezer = is_from_themezer(theme) && update_check_performed;
            const bool found_update = has_update(theme);
            const std::string status_label = found_update
                ? UI::update_icon
                : (found_themezer
                   ? UI::installed_icon
                   : ""s);
            const float status_font_size = 40;
            const ImVec2 status_size = calc_text_size(status_font_size, status_label);

            {
                Font font{nullptr, 24};
                // Make sure to limit the name width, so it doesn't get covered by the "status"
                // icon.
                float name_width = inner_size.x - status_size.x - style.ItemSpacing.x;
                text_limited(name_width, theme->metadata.themeName);
            }

            if (theme->metadata.themeAuthor) {
                Font font{nullptr, 18};
                // Make sure to limit the author width, so it doesn't get covered by the
                // "status" icon.
                float author_width = inner_size.x - status_size.x - style.ItemSpacing.x;
                text_limited(author_width, "by " + *theme->metadata.themeAuthor);
            }

            // Show "status" icon on the bottom right.
            if (!status_label.empty()) {
                Font status_font{nullptr, status_font_size};
                StyleColor update_color{ImGuiCol_Text,
                                        invert_colors
                                        ? dark_color
                                        : (found_update
                                           ? UI::update_color
                                           : UI::installed_color)};
                ImGui::SetCursorPos(start_pos + inner_size - status_size);
                ImGui::Text(status_label);
            }

            // Show "enabled" icon on top right.
            {
                Font enabled_font{nullptr, enabled_font_size};

                const ImVec2 text_offset = { 0, -5 };
                const ImVec2 bg_padding = { 4, 0 };
                const ImVec2 bg_offset = { 0, 5 }; // Fix text misalignment.
                const ImVec2 text_pos = {
                    start_pos.x
                    + inner_size.x
                    - enabled_size.x
                    - bg_padding.x
                    + text_offset.x,
                    start_pos.y
                    + bg_padding.y
                    + text_offset.y
                };
                ImGui::SetCursorPos(text_pos);
                const ImVec2 screen_text_pos = ImGui::GetCursorScreenPos();
                auto draw = ImGui::GetWindowDrawList();
                auto box_min = screen_text_pos
                    - bg_padding
                    + bg_offset;
                auto box_max = screen_text_pos + enabled_size
                    + bg_padding
                    + bg_offset;
                const auto bg_color = ImGui::GetColorU32(
                    invert_colors ? light_color : ImVec4{0.0f, 0.0f, 0.0f, 0.75f});
                auto diff = box_max - box_min;
                float radius = std::fmax(diff.x, diff.y) / 2;
                draw->AddCircleFilled((box_max + box_min) / 2, radius, bg_color);

                {
                    // Show text
                    StyleColor enabled_color{ImGuiCol_Text,
                                             invert_colors ? dark_color : UI::enabled_color};
                    ImGui::Text(enabled_label);
                }
                if (open_popup && ImGui::IsItemHovered()) {
                    open_popup = false;
                    if (is_enabled)
                        PluginManager::Disable(theme->path);
                    else
                        PluginManager::Enable(theme->path);
                }

            }

            if (open_popup)
                ThemeDetailsPopup::open_local(theme);
        }

        void show_tab_install_local() {
            using namespace ImGui::RAII;

            const auto& style = ImGui::GetStyle();

            /*-----------------------------------------------------------------------.
            | Toolbar:                                                               |
            |                                                                        |
            | [INFO-TEXT] [QR] [REFRESH]                                             |
            |                                                                        |
            | INFO-TEXT is stretched, so we need to calculate the width of the rest. |
            `-----------------------------------------------------------------------*/

            const std::string qr_label = ICON_FA_QRCODE;
            const auto qr_size = ImGui::CalcTextSize(qr_label)
                + 2 * style.FramePadding;

            const std::string refresh_label = ICON_FA_REFRESH;
            const auto refresh_size = ImGui::CalcTextSize(refresh_label)
                + 2 * style.FramePadding;

            const float space = style.ItemSpacing.x;

            const float info_width =
                ImGui::GetContentRegionAvail().x
                - space
                - qr_size.x
                - space
                - refresh_size.x;

            ImGui::AlignTextToFramePadding();
            ImGui::TextAligned(0.0f,
                               info_width,
                               "Install .utheme files from SD:/wiiu/themes");

            ImGui::SameLine();

            if (UI::Button(qr_label, qr_size))
                QRCodePopup::open();

            ImGui::SameLine();

            const bool refreshing = ThemeManager::IsRefreshingUThemes();

            {
                Disabled if_refreshing{refreshing};
                if (UI::Button(refresh_label, refresh_size))
                    ThemeManager::RefreshUThemes();
            }

            ImGui::Separator();

            // To keep the refresh button visible, put the uthemes in another child.
            if (Child uthemes_list{"uthemes_list"}) {
                Disabled if_refreshing{refreshing};
                ThemeManager::ForEachUTheme(
                    [](const std::filesystem::path& utheme,
                       const ThemeManager::ConstMetadataPtr& meta)
                    {
                        show_utheme(utheme, meta);
                    }
                );
            }
        }

        void show_tab_manage_installed() {
            using namespace ImGui::RAII;

            const auto &style = ImGui::GetStyle();

            SDL_WiiUSetSWKBDKeyboardMode(SDL_WIIU_SWKBD_KEYBOARD_MODE_FULL);
            SDL_WiiUSetSWKBDOKLabel("Search");
            SDL_WiiUSetSWKBDHighlightInitialText(SDL_TRUE);

            /*------------------------------------------------------------------------.
            | Toolbar:                                                                |
            |                                                                         |
            | [SEARCH] [CHECK-UPDATES] [SHUFFLE] [ENABLE-ALL] [DISABLE-ALL] [REFRESH] |
            |                                                                         |
            | SEARCH is stretched, so we need to calculate the width of the rest.     |
            `------------------------------------------------------------------------*/

            const std::string check_updates_label = UI::update_icon + " Check updates";
            const auto check_updates_size = ImGui::CalcTextSize(check_updates_label)
                + 2 * style.FramePadding;

            const std::string shuffle_label = "Shuffle";
            const float checkbox_square_width = ImGui::GetFrameHeight();
            const float shuffle_width = ImGui::CalcTextSize(shuffle_label).x
                + checkbox_square_width
                + style.ItemInnerSpacing.x;

            const std::string enable_all_label = ICON_FA_CHECK_SQUARE_O;
            const auto enable_all_size = ImGui::CalcTextSize(enable_all_label)
                + 2 * style.FramePadding;

            const std::string disable_all_label = ICON_FA_SQUARE_O;
            const auto disable_all_size = ImGui::CalcTextSize(disable_all_label)
                + 2 * style.FramePadding;

            const std::string refresh_label = ICON_FA_REFRESH;
            const auto refresh_size = ImGui::CalcTextSize(refresh_label)
                + 2 * style.FramePadding;

            const float space = style.ItemSpacing.x;

            const float search_width =
                ImGui::GetContentRegionAvail().x
                - space
                - check_updates_size.x
                - space
                - shuffle_width
                - space
                - enable_all_size.x
                - space
                - disable_all_size.x
                - space
                - refresh_size.x;

            ImGui::SetNextItemWidth(search_width);
            ImGui::InputTextWithHint("##local_search"s, "Search..."s, search);

            visible_indexes.clear();
            installed_themes.clear();
            ThemeManager::ForEachInstalledTheme(
                [](const ThemeManager::ConstThemePtr& theme)
                {
                    installed_themes.push_back(theme);
                }
            );
            for (auto [idx, theme] : installed_themes | std::views::enumerate) {
                int score = similarity_score(theme->metadata.themeName, search);
                if (search.empty() || score >= 0)
                    visible_indexes.push_back(idx);
            }

            if (!search.empty()) {
                std::ranges::sort(
                    visible_indexes,
                    [&](std::size_t a, std::size_t b) {
                        const auto& ta = *installed_themes[a];
                        const auto& tb = *installed_themes[b];
                        auto sa = similarity_score(ta.metadata.themeName, search);
                        auto sb = similarity_score(tb.metadata.themeName, search);
                        return sa > sb;
                    }
                );
            }

            ImGui::SameLine();

            const bool refreshing = ThemeManager::IsRefreshingThemes();

            {
                Disabled if_refreshing_or_busy{refreshing || ThemezerAPI::is_busy()};
                if (UI::Button(check_updates_label, check_updates_size))
                    request_update_check();
            }

            ImGui::SameLine();

            {
                Disabled if_refreshing{refreshing};
                if (UI::Button(refresh_label, refresh_size))
                    ThemeManager::RefreshInstalledThemes();
            }

            ImGui::SameLine();

            auto cfg = PluginManager::GetConfig();
            if (cfg) {
                bool is_shuffling = cfg->shuffleThemes;
                if (UI::Checkbox(shuffle_label, is_shuffling))
                    PluginManager::ToggleShuffling();

                if (is_shuffling) {
                    ImGui::SameLine();
                    if (UI::Button(enable_all_label, enable_all_size)) {
                        ThemeManager::ForEachInstalledTheme(
                            [](const ThemeManager::ConstThemePtr& theme)
                            {
                                PluginManager::Enable(theme->path);
                            }
                        );
                    }
                    ImGui::SameLine();
                    if (UI::Button(disable_all_label, disable_all_size)) {
                        ThemeManager::ForEachInstalledTheme(
                            [](const ThemeManager::ConstThemePtr& theme)
                            {
                                PluginManager::Disable(theme->path);
                            }
                        );
                    }
                }
            }

            // To keep the search widget visible, put the search results inside another child.
            if (Child search_results{"ThemeGrid"}) {
                Disabled if_refreshing{refreshing};

                const ImVec2 grid_start_pos = ImGui::GetCursorPos();
                const ImVec2 inner_size = {320, 260};
                const ImVec2 padding = {12, 12};
                const ImVec2 outer_size = inner_size + 2 * padding;
                const ImVec2 spacing = {15, 15};

                for (auto [counter, index] : visible_indexes | std::views::enumerate) {
                    const auto& theme = installed_themes[index];
                    ImVec2 grid_pos = { float(counter % 3), float(counter / 3) };
                    ImVec2 pos = grid_pos * (outer_size + spacing);
                    ImGui::SetCursorPos(grid_start_pos + pos);
                    show_installed_theme(theme,
                                         inner_size,
                                         padding);
                }
            }
        }

        void
        show_utheme(const std::filesystem::path& utheme,
                    const ThemeManager::ConstMetadataPtr& metadata) {
            using namespace ImGui::RAII;

            Child theme_frame{utheme.string(),
                              {0, 0},
                              ImGuiChildFlags_NavFlattened |
                              ImGuiChildFlags_AutoResizeY |
                              ImGuiChildFlags_FrameStyle,
                              ImGuiWindowFlags_NoSavedSettings};

            if (!theme_frame)
                return;

            // TODO: use a vertical button box

            const auto &style = ImGui::GetStyle();

            const std::string install_label = ICON_FA_COGS " Install";
            auto install_size = ImGui::CalcTextSize(install_label);
            const std::string delete_label = ICON_FA_TRASH " Delete";
            auto delete_size = ImGui::CalcTextSize(delete_label);

            // Use a common button size, make it prettier when it lines up.
            const auto button_size = UI::max(install_size, delete_size) + 2 * style.FramePadding;

            const float filename_width =
                ImGui::GetContentRegionAvail().x
                - style.ItemSpacing.x
                - button_size.x;
            ImGui::AlignTextToFramePadding();
            ImGui::TextAligned(0, filename_width, utheme.filename().string());

            ImGui::SameLine();

            if (UI::Button(install_label, button_size))
                InstallThemePopup::open(utheme, metadata, false, true);


            const float name_width =
                ImGui::GetContentRegionAvail().x
                - style.ItemSpacing.x
                - button_size.x;
            ImGui::AlignTextToFramePadding();
            const std::string name_text = metadata ? metadata->themeName : "<NO METADATA>";
            ImGui::TextAligned(0, name_width, name_text);

            ImGui::SameLine();

            if (UI::Button(delete_label, button_size)) {
                DeletePath(utheme);
                ThemeManager::RefreshUThemes();
            }
        }

        int
        similarity_score(const std::string& haystack_,
                         const std::string& needle_) {
            auto haystack = as_lower_case(haystack_);
            auto needle = as_lower_case(needle_);

            if (needle.empty())
                return 0;

            if (needle == haystack)
                return 10000;

            if (haystack.starts_with(needle))
                return 8000 - static_cast<int>(haystack.size());

            if (haystack.contains(needle))
                return 6000 - static_cast<int>(haystack.find(needle));

            int score = 0;
            std::size_t pos = 0;

            for (char c : needle) {
                pos = haystack.find(c, pos);
                if (pos == std::string::npos)
                    return -1;

                score += 10;
                ++pos;
            }

            return score;
        }

        void
        switch_tab(Tab tab)
        {
            if (tab != current_tab) {
                UI::PlaySFXTabSwitch();
                current_tab = tab;
            }
        }

        void
        text_limited(float width,
                     const std::string& text) {
            // WORKAROUND: prevent tooltip.
            auto& io = ImGui::GetIO();
            auto old_mouse_pos = io.MousePos;
            ImGui::TextAligned(0.0f, width, text);
            io.MousePos = old_mouse_pos;
        }

    } // namespace

    /*------------------*/
    /* Public functions */
    /*------------------*/

    void
    initialize(SDL_Renderer *renderer) {
        TRACE_FUNC;
        manage_renderer = renderer;
        installed_themes.clear();
        update_check_performed = false;
        request_update_check();
    }

    void
    finalize() {
        TRACE_FUNC;
        installed_themes.clear();
    }

    void
    process_ui() {
        // NOTE: don't check for updates until the installed themes refresh is completed.
        if (update_check_queued && !ThemeManager::IsRefreshingThemes()) {
            update_check_queued = false;
            check_updates();
        }

        {
            // NOTE: use a scope to contain all the temporary style changes, so they don't leak
            // into the popups at the bottom.
            using namespace ImGui::RAII;

#ifdef DEBUG_BG_COLOR
            StyleColor green_bg{ImGuiCol_ChildBg, {0.0, 0.5, 0.0, 1.0}};
#endif
            StyleVar padding{ImGuiStyleVar_WindowPadding, {6, 6}};
            if (Child manage_content{"ManageThemesContent",
                                     {0, 0},
                                     ImGuiChildFlags_NavFlattened |
                                     ImGuiChildFlags_AlwaysUseWindowPadding}) {

                if (TabBar tab_bar{"tab_bar"}) {

                    if (TabItem manage_installed{"Manage Installed Themes"}) {
                        switch_tab(Tab::installed);
                        show_tab_manage_installed();
                    }

                    if (TabItem install_local{"Install Local Themes"}) {
                        switch_tab(Tab::uthemes);
                        show_tab_install_local();
                    }

                }

            }
        }

        DeleteThemePopup::process_ui();
        DownloadThemePopup::process_ui();
        InstallThemePopup::process_ui();
        QRCodePopup::process_ui();
        ThemeDetailsPopup::process_ui();
    }

    void
    request_update_check() {
        update_check_queued = true;
    }

} // namespace ManageThemesScreen
