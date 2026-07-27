/*
 * Themiify - A theme manager for the Nintendo Wii U
 * Copyright (C) 2026 Fangal-Airbag
 * Copyright (C) 2026 AlphaCraft9658
 * Copyright (C) 2026 Daniel K. O. <dkosmari>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <array>
#include <functional>
#include <optional>
#include <string>
#include <vector>

namespace ThemezerAPI {

    struct PageInfo {
        unsigned page;
        unsigned pageCount;
        unsigned limit;
        unsigned itemCount;
    }; // struct PageInfo


    struct WiiuInstallThemeLookup {
        std::string createdAt;
        std::string downloadUrl;
        std::string name;
        std::string quickId;
        std::string uuid;
    }; // struct WiiuInstallThemeLookup


    struct WiiuThemeSmall {
        std::string uuid;
        std::string hexId;
        std::string name;
        std::string slug;
        std::string updatedAt;
        struct {
            std::string username;
        } creator;
        struct {
            std::string tinyUrl;  // 320x180
            std::string thumbUrl; // 426x240
        } collagePreview;
        unsigned downloadCount;
        std::string downloadUrl;
    }; // struct WiiuThemeSmall

    using WiiuThemeSmallVec = std::vector<WiiuThemeSmall>;


    struct ThemeTag {
        std::string name;
    }; // struct ThemeTag


    struct ImageSizesHd {
        std::string tinyUrl;    // 320x180
        std::string thumbUrl;   // 426x240
        std::string sdUrl;      // 640x360
        std::string hdUrl;      // 1280x720
    }; // struct ImageSizesHd


    struct WiiuThemeFull {

        std::string uuid;
        std::string hexId;
        std::string quickId;
        std::string slug;

        std::string name;
        std::string createdAt;
        std::string updatedAt;

        struct {
            std::string username;
            std::optional<std::string> avatarUrl;
        } creator;

        std::optional<std::string> bgmPreviewUrl; // Ogg Vorbis

        ImageSizesHd collagePreview;
        ImageSizesHd launcherScreenshot;
        ImageSizesHd waraWaraPlazaScreenshot;

        unsigned downloadCount;

        std::string downloadUrl;

        std::vector<ThemeTag> tags;

        std::optional<std::string> description;
    }; // struct WiiuThemeFull


    enum class ItemSort {
        COLOR_SIMILARITY,
        CREATED,
        DOWNLOADS,
        RISING,
        SAVES,
        TRENDING,
        UPDATED,
    };

    inline constexpr
    std::array ItemSortList = {
        ItemSort::COLOR_SIMILARITY,
        ItemSort::CREATED,
        ItemSort::DOWNLOADS,
        ItemSort::RISING,
        ItemSort::SAVES,
        ItemSort::TRENDING,
        ItemSort::UPDATED,
    };


    enum class SortOrder {
        ASC,
        DESC,
    };

    inline constexpr
    std::array SortOrderList = {
        SortOrder::ASC,
        SortOrder::DESC,
    };


    void
    initialize(const std::string& user_agent);

    void
    finalize();

    void
    process();


    /// Return true when there's a pending API call.
    bool
    is_busy();


    namespace wiiu {

        using ThemesSignature = void (const WiiuThemeSmallVec& themes,
                                      const PageInfo& page_info);
        using ThemesFunction = std::move_only_function<ThemesSignature>;

        struct PaginationInput {
            unsigned    limit = 20;
            unsigned    page = 1;
        }; // struct PaginationInput

        struct ThemesSpec {
            PaginationInput paginationArgs = {};
            ItemSort    sort  = ItemSort::CREATED;
            SortOrder   order = SortOrder::ASC;
            std::string query = "";
        }; // struct ThemesSpec

        void
        themes(const ThemesSpec& spec,
               ThemesFunction callback);



        using ThemeSignature = void(const WiiuThemeFull& theme);
        using ThemeFunction = std::move_only_function<ThemeSignature>;

        void
        theme(const std::string& hexId,
              ThemeFunction callback);


        using LookupByQuickIdSignature = void (const WiiuInstallThemeLookup& theme);
        using LookupByQuickIdFunction = std::move_only_function<LookupByQuickIdSignature>;

        void
        lookupByQuickId(const std::string& quickId,
                        LookupByQuickIdFunction callback);

    } // namespace wiiu

} // namespace ThemezerAPI
