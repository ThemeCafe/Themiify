/*
 * Themiify - A theme manager for the Nintendo Wii U
 * Copyright (C) 2026 Fangal-Airbag
 * Copyright (C) 2026 AlphaCraft9658
 * Copyright (C) 2026 Daniel K. O. <dkosmari>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <iostream>
#include <optional>
#include <ranges>

#include <SDL2/SDL.h>
#include <SDL2/SDL_image.h>

#include <imgui.h>
#include <imgui_raii.h>

#include "ManageThemesScreen.h"

#include "../IconsFontAwesome4.h"
#include "../ImageLoader.h"
#include "../PluginManager.h"
#include "../ThemeManager.h"
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

        /*-----------*/
        /* Variables */
        /*-----------*/

        // NOTE: keep a copy of the themes so we can easily filter and reorder them.
        std::vector<ThemeManager::ConstThemePtr> installed_themes;

        SDL_Renderer *manage_renderer;

        std::string search;

        /*-----------------------*/
        /* Function declarations */
        /*-----------------------*/

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
        text_limited(float width,
                     const std::string& text);

        /*----------------------*/
        /* Function definitions */
        /*----------------------*/

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

            bool clicked = false;
            ImGui::SetCursorPos({0, 0});
            if (ImGui::Button("##button", outer_size)) {
                // NOTE: delay opening the popup, gotta check if the user clicked on the star.
                clicked = true;
            }

            // NOTE: when hovered or activated, make text light.
            std::optional<StyleColor> dark_text;
            if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
                const auto& colors = style.Colors;
                auto dark_color = colors[ImGuiCol_WindowBg];
                dark_text.emplace(ImGuiCol_Text, dark_color);
            }

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

            // NOTE: Measure size for the "enabled" icon, but don't place it yet, to not mess
            // with the cursor position.

            const std::string enabled_label = is_shuffling
                ? (is_enabled ? ICON_FA_CHECK_CIRCLE_O : ICON_FA_CIRCLE_O)
                : (is_enabled ? ICON_FA_STAR : ICON_FA_STAR_O);

            const float enabled_font_size = 48;
            ImVec2 enabled_size;
            {
                Font enabled_font{nullptr, enabled_font_size};
                enabled_size = ImGui::CalcTextSize(enabled_label);
            }

            {
                Font font{nullptr, 24};
                // Make sure to limit the name width, so it doesn't get covered by the enabled
                // icon.
                float name_width = inner_size.x - enabled_size.x - style.ItemSpacing.x;
                text_limited(name_width, theme->metadata.themeName);
            }

            if (theme->metadata.themeAuthor) {
                Font font{nullptr, 18};
                // Make sure to limit the author width, so it doesn't get covered by the
                // "enabled" icon.
                float author_width = inner_size.x - enabled_size.x - style.ItemSpacing.x;
                text_limited(author_width, "by " + *theme->metadata.themeAuthor);
            }

            // Put active marker on bottom right.
            {
                Font enabled_font{nullptr, enabled_font_size};
                StyleColor enabled_color{ImGuiCol_Text, {1.0f, 0.9f, 0.0f, 1.0f}};
                ImGui::SetCursorPos(inner_size - enabled_size);
                ImGui::Text(enabled_label);
                if (clicked && ImGui::IsItemHovered()) {
                    clicked = false; // cancel the click
                    if (is_enabled)
                        PluginManager::Disable(theme->path);
                    else
                        PluginManager::Enable(theme->path);
                }
            }

            if (clicked)
                ThemeDetailsPopup::open_local(theme);
        }

        void show_tab_install_local() {
            using namespace ImGui::RAII;

            const auto& style = ImGui::GetStyle();

            /*---------------------------------------------------------------------------.
            | Toolbar:                                                                   |
            |                                                                            |
            | [INFO-TEXT] [QR] [REFRESH]                                                 |
            |                                                                            |
            | The INFO-TEXT is stretched, so we need to calculate the width of the rest. |
            `---------------------------------------------------------------------------*/

            const std::string qr_label = ICON_FA_QRCODE;
            const auto qr_size = ImGui::CalcTextSize(qr_label) + 2 * style.FramePadding;
            const std::string refresh_label = ICON_FA_REFRESH;
            const auto refresh_size = ImGui::CalcTextSize(refresh_label) + 2 * style.FramePadding;

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

            if (ImGui::Button(qr_label, qr_size))
                QRCodePopup::open();

            ImGui::SameLine();

            const bool refreshing = ThemeManager::IsRefreshingUThemes();

            {
                Disabled if_refreshing{refreshing};
                if (ImGui::Button(refresh_label))
                    ThemeManager::RefreshUThemes();
            }

            ImGui::Separator();

            // To keep the refresh button visible, put the uthemes in another child.
            if (Child uthemes_list{"uthemes_list"}) {
                Disabled if_refreshing{refreshing};
                ThemeManager::ForEachUTheme(
                    [](std::size_t,
                       const std::filesystem::path& utheme,
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

            SDL_WiiUSetSWKBDHintText("Write some search terms...");
            SDL_WiiUSetSWKBDOKLabel("Search");
            SDL_WiiUSetSWKBDHighlightInitialText(SDL_TRUE);

            // Let search widget expand, leaving space for the buttons.

            const std::string refresh_label = ICON_FA_REFRESH;
            const float refresh_width =
                ImGui::CalcTextSize(refresh_label).x +
                2 * style.FramePadding.x;

            const std::string shuffle_label = "Shuffle";
            const float checkbox_square_size = ImGui::GetFrameHeight();
            const float shuffle_width =
                checkbox_square_size +
                style.ItemInnerSpacing.x +
                ImGui::CalcTextSize(shuffle_label).x;

            const std::string enable_all_label = ICON_FA_CHECK_SQUARE_O " All";
            const float enable_all_width =
                ImGui::CalcTextSize(enable_all_label).x +
                2 * style.FramePadding.x;

            const std::string disable_all_label = ICON_FA_SQUARE_O " All";
            const float disable_all_width =
                ImGui::CalcTextSize(disable_all_label).x +
                2 * style.FramePadding.x;

            const float buttons_width =
                refresh_width +
                style.ItemSpacing.x +
                shuffle_width +
                style.ItemSpacing.x +
                enable_all_width +
                style.ItemSpacing.x +
                disable_all_width;

            auto available = ImGui::GetContentRegionAvail();
            ImGui::SetNextItemWidth(available.x - style.ItemSpacing.x - buttons_width);
            ImGui::InputTextWithHint("##local_search"s, "Search..."s, search);

            std::vector<std::size_t> visible_indexes;

            installed_themes.clear();
            ThemeManager::ForEachInstalledTheme(
                [](std::size_t /*index*/,
                   const ThemeManager::ConstThemePtr& theme)
                {
                    installed_themes.push_back(theme);
                }
            );
            for (std::size_t i = 0; i < installed_themes.size(); ++i) {
                int score = similarity_score(installed_themes[i]->metadata.themeName,
                                             search);

                if (search.empty() || score >= 0)
                    visible_indexes.push_back(i);
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
                Disabled if_refreshing{refreshing};
                if (ImGui::Button(refresh_label))
                    ThemeManager::RefreshInstalledThemes();
            }

            ImGui::SameLine();

            auto cfg = PluginManager::GetConfig();
            if (cfg) {
                bool is_shuffling = cfg->shuffleThemes;
                if (ImGui::Checkbox(shuffle_label, is_shuffling))
                    PluginManager::ToggleShuffling();

                if (is_shuffling) {
                    ImGui::SameLine();
                    if (ImGui::Button(enable_all_label)) {
                        ThemeManager::ForEachInstalledTheme(
                            [](std::size_t,
                               const ThemeManager::ConstThemePtr& theme)
                            {
                                PluginManager::Enable(theme->path);
                            }
                        );
                    }
                    ImGui::SameLine();
                    if (ImGui::Button(disable_all_label)) {
                        ThemeManager::ForEachInstalledTheme(
                            [](std::size_t,
                               const ThemeManager::ConstThemePtr& theme)
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

            if (ImGui::Button(install_label, button_size))
                InstallThemePopup::open(utheme, metadata, false, true);


            const float name_width =
                ImGui::GetContentRegionAvail().x
                - style.ItemSpacing.x
                - button_size.x;
            ImGui::AlignTextToFramePadding();
            const std::string name_text = metadata ? metadata->themeName : "<NO METADATA>";
            ImGui::TextAligned(0, name_width, name_text);

            ImGui::SameLine();

            if (ImGui::Button(delete_label, button_size)) {
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
    }

    void
    finalize() {
        TRACE_FUNC;
        installed_themes.clear();
    }

    void
    process_ui() {
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
                        show_tab_manage_installed();
                    }

                    if (TabItem install_local{"Install Local Themes"}) {
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

} // namespace ManageThemesScreen
