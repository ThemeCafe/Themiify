/*
 * Themiify - A theme manager for the Nintendo Wii U
 * Copyright (C) 2026 Fangal-Airbag
 * Copyright (C) 2026 AlphaCraft9658
 * Copyright (C) 2026 Daniel K. O. <dkosmari>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cassert>
#include <iostream>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include <imgui.h>
#include <imgui_raii.h>

#include <SDL_image.h>

#include "ThemezerScreen.h"

#include "../DownloadManager.h"
#include "../IconsFontAwesome4.h"
#include "../ImageLoader.h"
#include "../ThemeManager.h"
#include "../ThemezerAPI.h"
#include "../tracer.hpp"
#include "../UI.h"
#include "../utils.h"
#include "DownloadThemePopup.h"
#include "InstallThemePopup.h"
#include "QRCodePopup.h"
#include "ThemeDetailsPopup.h"

// Define this to help seeing the padding and spacing values for windows.
// #define DEBUG_BG_COLOR

using ThemezerAPI::PageInfo;
using ThemezerAPI::WiiuThemeSmall;
using ThemezerAPI::WiiuThemeSmallVec;
using ThemezerAPI::WiiuThemeFull;
using ThemezerAPI::SortOrder;
using ThemezerAPI::ItemSort;

using std::cout;
using std::endl;
using namespace std::literals;

namespace ThemezerScreen {
    bool first_fetch = true;
    uint32_t page = 0;

    ItemSort sort = ItemSort::CREATED;
    SortOrder order = SortOrder::DESC;

    std::string query;
    std::string quick_id_query;

    bool scroll_to_top = false;

    std::optional<PageInfo> page_info;
    std::optional<WiiuThemeSmallVec> themes;

    SDL_Texture* themezer_logo = nullptr;

    std::string sort_to_label(ItemSort arg) {
        switch (arg) {
            case ItemSort::CREATED:   return "Created";
            case ItemSort::DOWNLOADS: return "Downloads";
            case ItemSort::SAVES:     return "Saves";
            case ItemSort::UPDATED:   return "Updated";
            default: throw std::logic_error{"invalid"};
        }
    }

    std::string order_to_label(SortOrder arg) {
        switch (arg) {
            case SortOrder::ASC:  return ICON_FA_SORT_AMOUNT_ASC;
            case SortOrder::DESC: return ICON_FA_SORT_AMOUNT_DESC;
            default: throw std::logic_error{"invalid"};
        }
    }

    WiiuThemeSmall full_to_small(const WiiuThemeFull& full) {
        WiiuThemeSmall small;

        small.uuid = full.uuid;
        small.hexId = full.hexId;
        small.name = full.name;
        small.slug = full.slug;
        small.creator.username = full.creator.username;
        small.collagePreview.tinyUrl = full.collagePreview.tinyUrl;
        small.collagePreview.thumbUrl = full.collagePreview.thumbUrl;
        small.downloadCount = full.downloadCount;
        small.downloadUrl = full.downloadUrl;

        return small;
    }

    void
    fetch_theme_by_quick_id(const std::string& quick_id) {
        // Note: start a download imediately from the response, don't show anything.
        ThemezerAPI::wiiu::lookupByQuickId(
            quick_id,
            [](const ThemezerAPI::WiiuInstallThemeLookup& theme)
            {
                DownloadThemePopup::open(theme);
            });
    }

    void fetch_page(unsigned new_page) {
        if (!new_page)
            return;

        // TODO: should have an error handler too.
        ThemezerAPI::wiiu::themes({
                .paginationArgs = {
                    .limit = 21,
                    .page = new_page,
                },
                .sort = sort,
                .order = order,
                .query = query,
            },
            [](const WiiuThemeSmallVec& new_themes,
               const PageInfo& new_page_info)
            {
                page = new_page_info.page;
                page_info = new_page_info;
                themes = new_themes;

                scroll_to_top = true;
            });
    }

    void initialize(SDL_Renderer* renderer) {
        TRACE_FUNC;

        themezer_logo = IMG_LoadTexture(renderer, "fs:/vol/content/ui/themezer-logo.png");

        first_fetch = true;
    }

    void finalize() {
        TRACE_FUNC;

        if (themezer_logo) {
            SDL_DestroyTexture(themezer_logo);
            themezer_logo = nullptr;
        }
    }

    static void text_limited(float width, const std::string& text) {
        // WORKAROUND: prevent tooltip.
        auto& io = ImGui::GetIO();
        auto old_mouse_pos = io.MousePos;
        ImGui::TextAligned(0.0f, width, text);
        io.MousePos = old_mouse_pos;
    }

    void show(const WiiuThemeSmall& theme,
              const ImVec2& inner_size,
              const ImVec2& padding) {
        // NOTE: to create a complex button, we create a button with no text, then overlap
        // the contents.
        using namespace ImGui::RAII;

        ID id{theme.hexId};

        const auto& style = ImGui::GetStyle();
        const ImVec2 outer_size = inner_size + 2 * padding;

        // Put everything inside a child window so we can bail out when not visibile.
        Child container{"container", outer_size,
                        ImGuiChildFlags_NavFlattened};
        if (!container)
            return;

        ImVec2 start_pos = padding;

        ImGui::SetCursorPos({0, 0});
        if (UI::Button("##button", outer_size)) {
            ThemeDetailsPopup::open_themezer(theme);
        }

        // NOTE: when hovered or activated, change the text color to the window bg color.
        std::optional<StyleColor> dark_text;
        if (ImGui::IsItemHovered() || ImGui::IsItemActive()) {
            const auto& colors = style.Colors;
            dark_text.emplace(ImGuiCol_Text, colors[ImGuiCol_WindowBg]);
        }

        ImGui::SetCursorPos(start_pos);
        Group grp;

        {
            StyleVar no_border{ImGuiStyleVar_ImageBorderSize, 0};
            auto img = ImageLoader::get(theme.collagePreview.tinyUrl);
            ImVec2 img_size = {inner_size.x, inner_size.x * 9.0f / 16.0f};
            ImGui::Image((ImTextureID)img, img_size);
        }


        {
            // Show theme name to the left, status glyph to the right.
            Font font{nullptr, 24};

            std::string status_label;
            ImVec4 status_color;
            if (auto itheme = ThemeManager::GetThemeByID(theme.hexId)) {
                // If version mismatch OR it's a legacy theme, suggest an update.
                if (itheme->metadata.themeVersion != theme.updatedAt
                    ||
                    !itheme->legacyMetadataPath.empty()) {
                    status_label = ICON_FA_REFRESH;
                    status_color = { 1.0f, 0.7f, 0.0f, 1.0f };
                } else {
                    status_label = ICON_FA_CHECK;
                    status_color = { 0.0f, 1.0f, 0.3f, 1.0f };
                }
            }

            float name_width = inner_size.x;
            if (!status_label.empty())
                name_width -= style.ItemSpacing.x + ImGui::CalcTextSize(status_label).x;

            text_limited(name_width, theme.name);

            if (!status_label.empty()) {
                ImGui::SameLine();

                std::optional<StyleColor> status_glyph_color;
                if (!dark_text)
                    status_glyph_color.emplace(ImGuiCol_Text, status_color);
                ImGui::Text(status_label);
            }
        }

        {
            // Show author to the left, downloads to the right.
            Font font{nullptr, 18};

            std::string downloads_label = ICON_FA_DOWNLOAD " "
                + std::to_string(theme.downloadCount);
            float downloads_width = ImGui::CalcTextSize(downloads_label).x;

            float author_width = inner_size.x - downloads_width - style.ItemSpacing.x;
            text_limited(author_width, "by " + theme.creator.username);

            ImGui::SameLine();

            ImGui::Text(downloads_label);
        }
    }

    void show_toolbar() {
        using namespace ImGui::RAII;

        const auto& style = ImGui::GetStyle();

        const bool themezer_busy = ThemezerAPI::is_busy();

        SDL_WiiUSetSWKBDKeyboardMode(SDL_WIIU_SWKBD_KEYBOARD_MODE_RESTRICTED);
        SDL_WiiUSetSWKBDOKLabel("Search");
        SDL_WiiUSetSWKBDHighlightInitialText(SDL_TRUE);

        /*-------------------------------------------------------------------------------.
        | Layout:                                                                        |
        |                                                                                |
        | [SEARCH] [QUICKID] [QR] [SORT] [REVERSE] [NAV-PREV] [NAV-PAGE] [NAV-NEXT]      |
        |                                                                                |
        | Only SEARCH is stretched, so we need to calculate the width of everything that |
        | comes after.                                                                   |
        `-------------------------------------------------------------------------------*/

        const std::string quick_id_label = "QuickID";
        const auto quick_id_size = ImGui::CalcTextSize(quick_id_label) + 2 * style.FramePadding;

        const std::string qr_label = ICON_FA_QRCODE;
        const auto qr_size = ImGui::CalcTextSize(qr_label) + 2 * style.FramePadding;

        const float sort_width = 220;

        const std::string reverse_label = order_to_label(order);
        const auto reverse_size = ImGui::CalcTextSize(reverse_label) + 2 * style.FramePadding;

        const std::string nav_prev_label = ICON_FA_CHEVRON_LEFT;
        const auto nav_prev_size = ImGui::CalcTextSize(nav_prev_label) + 2 * style.FramePadding;

        const std::string nav_next_label = ICON_FA_CHEVRON_RIGHT;
        const auto nav_next_size = ImGui::CalcTextSize(nav_next_label) + 2 * style.FramePadding;

        std::string nav_page_label;
        if (page_info) {
            nav_page_label =
                "Page "s
                + std::to_string(page_info->page)
                + "/"s
                + std::to_string(page_info->pageCount);
        }
        const float nav_page_width = ImGui::CalcTextSize(nav_page_label).x;

        const float space = style.ItemSpacing.x;
        const float search_width =
            ImGui::GetContentRegionAvail().x
            - space
            - quick_id_size.x
            - space
            - qr_size.x
            - space
            - sort_width
            - space
            - reverse_size.x
            - space
            - nav_prev_size.x
            - space
            - nav_page_width
            - space
            - nav_next_size.x;

        {
            Disabled disable_if{themezer_busy};

            ImGui::SetNextItemWidth(search_width);
            if (ImGui::InputTextWithHint("##network_search"s, "Search..."s, query)) {
                cout << "Searching: " << query << endl;
                fetch_page(1);
            }
        }

        ImGui::SameLine();

        {
            Disabled disable_if{themezer_busy};

            ImGui::SetNextItemWidth(quick_id_size.x);
            if (ImGui::InputTextWithHint("##quick_id_search"s, quick_id_label, quick_id_query)) {
                if (!quick_id_query.empty()) {
                    cout << "Searching QuickId: " << quick_id_query << endl;
                    quick_id_query = as_upper_case(quick_id_query);
                    fetch_theme_by_quick_id(quick_id_query);
                }
            }
        }

        ImGui::SameLine();

        if (UI::Button(qr_label, qr_size))
            QRCodePopup::open();

        ImGui::SameLine();

        {
            Disabled disable_if{themezer_busy};

            ImGui::SetNextItemWidth(sort_width);
            if (Combo sort_combo{"##sort_combo"s, sort_to_label(sort)}) {
                for (auto new_sort : ThemezerAPI::ItemSortList) {
                    if (UI::Selectable(sort_to_label(new_sort),
                                       new_sort == sort)) {
                        sort = new_sort;
                        fetch_page(1);
                    }
                }
            }
            if (ImGui::IsItemClicked())
                UI::PlaySFXClick();
        }

        ImGui::SameLine();

        {
            Disabled disable_if{themezer_busy};
            if (UI::Button(reverse_label, reverse_size)) {
                order = order == SortOrder::ASC ? SortOrder::DESC : SortOrder::ASC;
                fetch_page(1);
            }
        }

        ImGui::SameLine();

        // Navigation controls
        {
            Disabled disabled_if{themezer_busy};

            {
                bool first_page = true;
                if (page_info)
                    first_page = page_info->page == 1;

                Disabled disable_if{first_page};

                if (UI::Button(nav_prev_label, nav_prev_size))
                    fetch_page(page - 1);
            }

            ImGui::SameLine();

            ImGui::Text(nav_page_label);

            ImGui::SameLine();

            {
                bool last_page = true;
                if (page_info)
                    last_page = page_info->page == page_info->pageCount;

                Disabled disable_if{last_page};

                if (UI::Button(nav_next_label, nav_next_size))
                    fetch_page(page + 1);
            }
        }
    }

    void process_ui() {
        using namespace ImGui::RAII;

        const bool themezer_busy = ThemezerAPI::is_busy();
        {
            // NOTE: use a scope to contain all the temporary style changes, so they don't
            // leak into the popups at the bottom.
#ifdef DEBUG_BG_COLOR
            StyleColor green_bg{ImGuiCol_ChildBg, {0.0, 0.5, 0.0, 1.0}};
#endif
            StyleVar padding{ImGuiStyleVar_WindowPadding, {6, 6}};
            if (Child themezer_content{"ThemezerContent",
                                       {0, 0},
                                       ImGuiChildFlags_NavFlattened |
                                       ImGuiChildFlags_AlwaysUseWindowPadding}) {
                // Title
                if (themezer_logo) {
                    ImGui::Image((ImTextureID)themezer_logo, {300, 86});
                }

                show_toolbar();

                // Themes List
                {
                    Disabled disable_if{themezer_busy};

#ifdef DEBUG_BG_COLOR
                    StyleColor brown_bg{ImGuiCol_ChildBg, {0.3, 0.3, 0.0, 1.0}};
#endif
                    if (Child theme_list{"ThemeGrid"}) {

                        if (scroll_to_top) {
                            scroll_to_top = false;
                            ImGui::SetScrollY(0);
                        }

                        const ImVec2 inner_size = {320, 260};
                        const ImVec2 padding = {12, 12};

                        if (!themes) {
                            ImGui::Text("Waiting for Themezer to respond...");
                        }
                        else {
                            const ImVec2 grid_start_pos = ImGui::GetCursorPos();
                            const ImVec2 outer_size = inner_size + 2 * padding;
                            const ImVec2 spacing = {15, 15};
                            for (auto [idx, theme] : *themes | std::views::enumerate) {
                                ImVec2 grid_pos = { float(idx % 3), float(idx / 3) };
                                ImVec2 pos = grid_pos * (outer_size + spacing);
                                ImGui::SetCursorPos(grid_start_pos + pos);
                                show(theme, inner_size, padding);
                            }
                        }
                    }
                }
            }
        }

        DownloadThemePopup::process_ui();
        InstallThemePopup::process_ui();
        QRCodePopup::process_ui();
        ThemeDetailsPopup::process_ui();

        if (first_fetch) {
            fetch_page(1);
            first_fetch = false;
        }
    }
}
