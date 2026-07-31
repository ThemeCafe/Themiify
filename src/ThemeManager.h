/*
 * Themiify - A theme manager for the Nintendo Wii U
 * Copyright (C) 2026 Fangal-Airbag
 * Copyright (C) 2026 AlphaCraft9658
 * Copyright (C) 2026 Daniel K. O. <dkosmari>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <exception>
#include <filesystem>
#include <functional>
#include <optional>
#include <string>
#include <vector>
#include <memory>

namespace ThemeManager {

    /*------------------*/
    /* Type definitions */
    /*------------------*/

    struct Metadata {
        std::optional<std::string> themeID;
        std::string                themeName;
        std::optional<std::string> themeAuthor;
        std::optional<std::string> themeVersion;
    };

    struct Theme {
        Metadata                           metadata;
        std::filesystem::path              path;
        std::vector<std::filesystem::path> previews;
        std::filesystem::path              legacyMetadataPath;
        std::vector<std::filesystem::path> files;
    };

    /*--------------*/
    /* Type aliases */
    /*--------------*/

    using MetadataPtr = std::shared_ptr<Metadata>;
    using ConstMetadataPtr = std::shared_ptr<const Metadata>;

    using ThemePtr = std::shared_ptr<Theme>;
    using ConstThemePtr = std::shared_ptr<const Theme>;

    using ProgressCallbackSignature = void (const std::string &msg);
    using ProgressFunction = std::move_only_function<ProgressCallbackSignature>;

    using SuccessCallbackSignature = void ();
    using SuccessFunction = std::move_only_function<SuccessCallbackSignature>;

    using ErrorCallbackSignature = void (const std::string &msg);
    using ErrorFunction = std::function<ErrorCallbackSignature>;

    using ThemeCallbackSignature = void (const ConstThemePtr& theme);
    using ThemeFunction = std::function<ThemeCallbackSignature>;

    using MetadataCallbackSignature = void(const std::filesystem::path& utheme,
                                           const ConstMetadataPtr& meta);
    using MetadataFunction = std::function<MetadataCallbackSignature>;

    /*-----------------------*/
    /* Function declarations */
    /*-----------------------*/

    void initialize();

    void finalize();

    void process();

    ConstMetadataPtr
    ReadUThemeMetadata(const std::filesystem::path &uthemePath);

    void
    Install(const std::filesystem::path& utheme,
            const ConstMetadataPtr& metadata,
            bool enable_theme,
            ProgressFunction progress_func,
            SuccessFunction success_func,
            ErrorFunction error_func);

    void
    CancelInstall();

    void
    Uninstall(ConstThemePtr theme);

    std::filesystem::path
    CalcThemePath(const Metadata& meta);

    std::filesystem::path
    CalcUThemePath();

    std::filesystem::path
    CalcUThemePath(const std::string& url);

    std::filesystem::path
    CalcUThemePath(const std::string& name,
                   const std::string& id);

    void
    RefreshInstalledThemes();

    bool
    IsRefreshingThemes();

    ConstThemePtr
    GetThemeByID(const std::string& hexId);

    void
    ForEachInstalledTheme(const ThemeFunction& func);

    ConstThemePtr
    GetCurrentTheme()
        noexcept;

    void
    RefreshUThemes();

    bool
    IsRefreshingUThemes();

    void
    ForEachUTheme(const MetadataFunction& func);

} // namespace ThemeManager
