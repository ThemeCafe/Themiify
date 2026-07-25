/*
 * Themiify - A theme manager for the Nintendo Wii U
 * Copyright (C) 2026 Fangal-Airbag
 * Copyright (C) 2026 AlphaCraft9658
 * Copyright (C) 2026 Daniel K. O. <dkosmari>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <format>
#include <fstream>
#include <future>
#include <iostream>
#include <map>
#include <ranges>
#include <stdexcept>
#include <thread>
#include <unordered_map>
#include <unordered_set>
#include <utility>

#include <glaze/exceptions/json_exceptions.hpp>
#include <glaze/json.hpp>
#include <hips.hpp>
#include <mocha/mocha.h>

#include <sys/iosupport.h>      // GetDeviceOpTab
#include <coreinit/systeminfo.h>
#include <sysapp/title.h>

#include "ThemeManager.h"

#include "PluginManager.h"
#include "thread_safe.hpp"
#include "tracer.hpp"
#include "utils.h"
#include "async_queue.hpp"
#include "Zip.h"

using std::cout;
using std::cerr;
using std::endl;
using namespace std::literals;

// Note: to avoid errors, we rename "metadata" to "Metadata" by customizing the glaze
// metadata.
namespace {
    struct MetadataJson {
        ThemeManager::Metadata metadata;
    };
}

// The built-in glz::pascal_case transformer will make the first letter uppercase.
template<>
struct glz::meta<MetadataJson> : glz::pascal_case {};

namespace ThemeManager {

    namespace {

        /*------------------*/
        /* Type definitions */
        /*------------------*/

        // RAII type to control Home Button Menu state.
        struct HomeButtonMenuDisabler {

            HomeButtonMenuDisabler()
                noexcept;

            ~HomeButtonMenuDisabler()
                noexcept;

        }; // struct HomeButtonMenuDisabler

        // RAII type to clean up theme installation on error.
        struct InstallCleaner {

            InstallCleaner(const std::filesystem::path& theme_path);

            ~InstallCleaner()
                noexcept;

            void
            disable();

        private:

            const std::filesystem::path theme_path;
            bool disabled = false;

        }; // struct InstallCleaner

        struct InstallContext {
            const std::filesystem::path utheme_path;
            ConstMetadataPtr metadata;
            const bool enable_theme;
            ProgressFunction progress_func;
            SuccessFunction success_func;
            ErrorFunction error_func;
            std::filesystem::path theme_path;

            InstallContext(const std::filesystem::path& utheme_path,
                           const ConstMetadataPtr& metadata,
                           bool enable_theme,
                           ProgressFunction progress_func,
                           SuccessFunction success_func,
                           ErrorFunction error_func);
        }; // struct InstallContext

        struct LegacyMetadataJson {
            struct {
                std::string themeName;
                std::string themeAuthor;
                std::string themeID;
                std::string themeIDPath;
                std::string themeVersion;
                std::filesystem::path themeInstallPath;
            } ThemeData;
        };

        /*--------------*/
        /* Type aliases */
        /*--------------*/

        using InstallContextPtr = std::shared_ptr<InstallContext>;

        using Task = std::future<void>;

        using ThemeIDMap = std::unordered_map<std::string, ConstThemePtr>;

        // NOTE: for theme map, we store only the folder name of the path.
        using ThemesMap = std::map<std::filesystem::path, ConstThemePtr, IgnoreCaseLess>;

        // NOTE: for .utheme map, we store the full path.
        using UThemesMap = std::map<std::filesystem::path, ConstMetadataPtr, IgnoreCaseLess>;

        /*-----------*/
        /* Constants */
        /*-----------*/

        const std::unordered_map<std::string, std::filesystem::path> regionLangMap = {
            {"UsEn", "UsEnglish/Message/AllMessage.szs"},
            {"UsFr", "UsFrench/Message/AllMessage.szs"},
            {"UsPt", "UsPortuguese/Message/AllMessage.szs"},
            {"UsEs", "UsSpanish/Message/AllMessage.szs"},
            {"EuNl", "EuDutch/Message/AllMessage.szs"},
            {"EuEn", "EuEnglish/Message/AllMessage.szs"},
            {"EuFr", "EuFrench/Message/AllMessage.szs"},
            {"EuDe", "EuGerman/Message/AllMessage.szs"},
            {"EuIt", "EuItalian/Message/AllMessage.szs"},
            {"EuPt", "EuPortuguese/Message/AllMessage.szs"},
            {"EuRu", "EuRussian/Message/AllMessage.szs"},
            {"EuEs", "EuSpanish/Message/AllMessage.szs"},
            {"JpJa", "JpJapanese/Message/AllMessage.szs"}
        };

        const std::filesystem::path THEMES_ROOT = SD_ROOT / "wiiu/themes";

        /*-----------*/
        /* Variables */
        /*-----------*/

        thread_safe<ThemeIDMap> safe_theme_id_map;
        thread_safe<ThemesMap> safe_themes_map;
        thread_safe<UThemesMap> safe_uthemes_map;

        std::atomic_bool refresh_themes_thread_busy = false;
        std::atomic_bool refresh_uthemes_thread_busy = false;

        std::jthread refresh_themes_thread;
        std::jthread refresh_uthemes_thread;
        std::jthread install_thread;
        std::jthread uninstall_thread;

        async_queue<Task> pending_tasks;

        /*-----------------------*/
        /* Function declarations */
        /*-----------------------*/

        template<typename F,
                 typename... Args>
        void
        AddTask(F&& func,
                Args&& ...args);

        std::filesystem::path
        CalcLegacyThumbnailPath(const std::string& themeID);

        void
        CreateCacheFile(std::ifstream &sourceFile,
                        const std::filesystem::path &outputPath);

        void
        ExecTasks();

        std::optional<std::string>
        GetHexId(const ConstThemePtr& theme);

        std::filesystem::path
        GetMenuContentPath();

        void
        InstallThread(std::stop_token stopper,
                      InstallContextPtr ctx)
            noexcept;

        ConstThemePtr
        ReadInstalledTheme(const std::filesystem::path &themePath);

        ConstThemePtr
        ReadInstalledThemeLegacy(const std::filesystem::path &themePath);

        std::filesystem::path
        SanitizeElement(const std::filesystem::path& input);

        void
        TaskCompleteInstall(const InstallContextPtr& ctx);

        void
        TaskPrintError(const std::string& msg);

        void
        TaskReportInstallError(const InstallContextPtr& ctx,
                               const std::string& msg);

        void
        TaskReportInstallProgress(const InstallContextPtr& ctx,
                                  const std::string& msg);

        void
        UninstallThread(ConstThemePtr theme);

        /*----------------------*/
        /* Function definitions */
        /*----------------------*/

        template<typename F,
                 typename... Args>
        void
        AddTask(F&& func,
                Args&& ...args) {
            pending_tasks.push(std::async(std::launch::deferred,
                                          std::forward<F>(func),
                                          std::forward<Args>(args)...));
        }

        std::filesystem::path
        CalcLegacyThumbnailPath(const std::string& themeID) {
            std::string filename = themeID;
            // Simply delete the ':' from the ID.
            std::erase(filename, ':');
            filename += ".webp";
            return THEMIIFY_THUMBNAILS / filename;
        }

        void
        CreateCacheFile(std::ifstream &sourceFile,
                        const std::filesystem::path &outputPath) {
            if (!sourceFile.is_open()) {
                cerr << "Invalid or unopened source file" << endl;
                return;
            }

            std::size_t sourceSize = static_cast<std::size_t>(sourceFile.tellg());
            sourceFile.seekg(0, std::ios::beg);

            std::vector<unsigned char> buffer(sourceSize);
            sourceFile.read(reinterpret_cast<char*>(buffer.data()), sourceSize);

            if (!sourceFile) {
                cerr << "Error reading source file to create cache file." << endl;
                return;
            }

            std::ofstream outputFile(outputPath, std::ios::binary | std::ios::trunc);
            if (!outputFile.is_open()) {
                cerr << "Failed to open output file for writing: " << outputPath << endl;
                return;
            }

            outputFile.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());

            if (!outputFile) {
                cerr << "Error writing cache file to " << outputPath << endl;
                return;
            }

            cout << "Successfully cached file to: " << outputPath << endl;
        }

        void
        ExecTasks() {
            while (auto task = pending_tasks.try_pop())
                try {
                    task->get();
                }
                catch (std::exception& e) {
                    cerr << "ERROR in ExecTasks(): " << e.what() << endl;
                }
        }

        std::optional<std::string>
        GetHexId(const ConstThemePtr& theme) {
            if (!theme->metadata.themeID)
                return {};
            static const std::string prefix = "Themezer:";
            if (!theme->metadata.themeID->starts_with(prefix))
                return {};
            std::string result = theme->metadata.themeID->substr(prefix.size());
            if (result.empty())
                return {};
            return result;
        }

        std::filesystem::path
        GetMenuContentPath() {
            uint64_t menuTitleID = _SYSGetSystemApplicationTitleId(SYSTEM_APP_ID_WII_U_MENU);

            uint32_t menuIDParentDir = menuTitleID >> 32;
            uint32_t menuIDChildDir = menuTitleID;

            char splitMenuID[18];
            std::snprintf(splitMenuID, sizeof splitMenuID,
                          "%08x/%08x",
                          menuIDParentDir,
                          menuIDChildDir);

            return "storage_mlc:/sys/title" / std::filesystem::path{splitMenuID} / "content";
        }

        HomeButtonMenuDisabler::HomeButtonMenuDisabler()
            noexcept
        {
            OSEnableHomeButtonMenu(false);
        }

        HomeButtonMenuDisabler::~HomeButtonMenuDisabler()
            noexcept
        {
            OSEnableHomeButtonMenu(true);
        }

        InstallCleaner::InstallCleaner(const std::filesystem::path& theme_path) :
            theme_path{theme_path}
        {}

        InstallCleaner::~InstallCleaner()
            noexcept
        {
            if (!disabled) {
                cerr << "Deleting partially installed theme: " << theme_path << endl;
                DeletePath(theme_path);
            }
        }

        void
        InstallCleaner::disable()
        {
            disabled = true;
        }

        InstallContext::InstallContext(const std::filesystem::path& utheme_path,
                                       const ConstMetadataPtr& metadata,
                                       bool enable_theme,
                                       ProgressFunction progress_func,
                                       SuccessFunction success_func,
                                       ErrorFunction error_func) :
            utheme_path{utheme_path},
            metadata{metadata},
            enable_theme{enable_theme},
            progress_func{std::move(progress_func)},
            success_func{std::move(success_func)},
            error_func{std::move(error_func)}
        {}

        void
        InstallThread(std::stop_token stopper,
                      InstallContextPtr ctx)
            noexcept
        try {
            if (!ctx->metadata) {
                ctx->metadata = ReadUThemeMetadata(ctx->utheme_path);
                if (!ctx->metadata)
                    throw std::runtime_error{"could not get utheme metadata"};
            }

            ctx->theme_path = CalcThemePath(*ctx->metadata);
            const auto& themePath = ctx->theme_path;

            // Helper lambda
            auto reportProgress =
                [&ctx]<typename... Args>(std::format_string<Args...> fmt,
                                         Args&&... args)
            {
                std::string msg = std::format(fmt, std::forward<Args>(args)...);
                AddTask(TaskReportInstallProgress, ctx, std::move(msg));
            };

            // Helper lambda
            auto throwIfStopped = [&stopper] {
                if (stopper.stop_requested())
                    throw std::runtime_error{"Installation canceled."};
            };

            HomeButtonMenuDisabler hbm_disabler;
            InstallCleaner cleaner{themePath};

            auto menuContentPath = GetMenuContentPath();

            throwIfStopped();

            Zip::Archive uthemeArchive{ctx->utheme_path, ZIP_RDONLY};

            throwIfStopped();

            reportProgress("Installing {}...", ctx->metadata->themeName);
            reportProgress("Installing theme to: \"{}\"", themePath.string());

            const std::string AllMessage_ = "AllMessage_";
            const std::uint64_t numEntries = uthemeArchive.get_num_entries(ZIP_FL_UNCHANGED);
            for (std::uint64_t i = 0; i < numEntries; ++i) {

                throwIfStopped();

                std::filesystem::path entryName = uthemeArchive.get_name(i, ZIP_FL_ENC_RAW);
                std::filesystem::path menuFilePath;
                std::string entryStem = entryName.stem().string();

                if (entryName == "Men.bps") {
                    menuFilePath = MEN_PATH;
                }
                else if (entryName == "Men2.bps") {
                    menuFilePath = MEN2_PATH;
                }
                else if (entryName == "cafe_barista_men.bps") {
                    menuFilePath = CAFE_BARISTA_MEN_PATH;
                }
                else if (entryStem.starts_with(AllMessage_) &&
                         as_lower_case(entryName.extension()) == ".bps") {
                    std::string regionLangStr = entryStem.substr(AllMessage_.size(), 4);
                    auto it = regionLangMap.find(regionLangStr);
                    if (it == regionLangMap.end()) {
                        reportProgress("Unknown AllMessage Region and Language: \"{}\"",
                                       regionLangStr);
                        continue;
                    }

                    menuFilePath = it->second;
                }

                if (!menuFilePath.empty()) {
                    // Found a known patch.
                    reportProgress("Handling patch for \"{}\"", menuFilePath.string());

                    auto menuPath = menuContentPath / menuFilePath;
                    auto cachePath = THEMIIFY_ROOT / "cache" / menuFilePath;
                    auto patchPath = entryName;
                    auto outputPath = themePath / "content" / menuFilePath;

                    create_directories(cachePath.parent_path());

                    auto patchFile = uthemeArchive.fopen(i);

                    zip_stat_t patchStatData = uthemeArchive.stat(i);

                    throwIfStopped();

                    auto patchSize = patchStatData.size;

                    std::ifstream inputFile(cachePath, std::ios::binary | std::ios::ate);
                    if (!inputFile.is_open()) {
                        reportProgress("Cache does not exist for \"{}\"",
                                       menuPath.string());

                        inputFile.clear();
                        inputFile.open(menuPath, std::ios::binary | std::ios::ate);

                        if (!inputFile.is_open()) {
                            inputFile.clear();
                            // NOTE: don't stop installation, just report and continue
                            reportProgress("Could not open source file for \"{}\"",
                                           patchPath.string());
                            continue;
                        }
                        reportProgress("Creating cache for \"{}\"", menuPath.string());
                        CreateCacheFile(inputFile, cachePath);
                    }
                    else {
                        reportProgress("Found \"{}\" in cache at \"{}\"",
                                       menuFilePath.string(),
                                       cachePath.string());
                    }

                    throwIfStopped();

                    auto inputSize = inputFile.tellg();
                    // if (inputSize != inputFile.tellg())
                    //     throw std::runtime_error{"Input file is too large."};
                    inputFile.seekg(0, std::ios::beg);

                    throwIfStopped();

                    std::vector<uint8_t> patchData(patchSize);
                    std::vector<uint8_t> inputData(inputSize);

                    if (patchFile.read(std::span{patchData}) != patchData.size())
                        throw std::runtime_error{"Could not read the whole patch"};

                    patchFile.close();

                    throwIfStopped();

                    inputFile.read(reinterpret_cast<char*>(inputData.data()), inputData.size());
                    if (std::cmp_not_equal(inputFile.gcount(), inputData.size()))
                        throw std::runtime_error{"Could not read the whole input"};
                    inputFile.close();

                    throwIfStopped();

                    auto [bytes, result] = Hips::patch(
                        inputData.data(),
                        inputData.size(),
                        patchData.data(),
                        patchData.size(),
                        Hips::PatchType::BPS
                    );

                    reportProgress("Patch processed");

                    throwIfStopped();

                    if (result == Hips::Result::Success) {
                        create_directories(outputPath.parent_path());

                        throwIfStopped();

                        std::ofstream outputFile(outputPath, std::ios::binary);
                        outputFile.write(reinterpret_cast<char*>(bytes.data()), bytes.size());
                        outputFile.close();

                        reportProgress("File written to \"{}\"", outputPath.string());
                    }
                    else {
                        throw std::runtime_error{"Patch failed: " + to_string(result)};
                    }
                }
                else {
                    // Not a known patch: if it's not a .bps file, just copy it verbatim
                    // to themePath.
                    if (as_lower_case(entryName.extension()) != ".bps") {
                        reportProgress("Copying file: \"{}\"", entryName.string());
                        auto patchFile = uthemeArchive.fopen(i);
                        auto outputPath = themePath / entryName;
                        if (outputPath.has_parent_path())
                            create_directories(outputPath.parent_path());
                        std::filebuf output;
                        if (!output.open(outputPath,
                                         std::ios::out | std::ios::binary | std::ios::trunc))
                            throw std::runtime_error{"Could not create output file: \""s
                                                     + outputPath.string() + "\""s};
                        std::vector<char> buf(65536);
                        std::uint64_t total = 0;
                        while (auto read = patchFile.read(std::span{buf})) {
                            output.sputn(buf.data(), read);
                            total += read;
                        }
                        output.close();
                        patchFile.close();
                        reportProgress("File written to \"{}\" ({} bytes)",
                                       outputPath.string(),
                                       total);
                    }
                }
            }

            uthemeArchive.close();

            throwIfStopped();

            if (auto theme = ReadInstalledTheme(themePath)) {
                if (auto hexId = GetHexId(theme))
                    (*safe_theme_id_map.lock())[*hexId] = theme;
                (*safe_themes_map.lock())[themePath.filename()] = std::move(theme);
            } else
                throw std::runtime_error{"Could not read metadata from installed theme!"};

            cleaner.disable();

            AddTask(TaskCompleteInstall, std::move(ctx));
        }
        catch (std::exception& e) {
            std::string msg = e.what();
            AddTask(TaskReportInstallError, std::move(ctx), std::move(msg));
        }

        ConstThemePtr
        ReadInstalledTheme(const std::filesystem::path &themePath) {
            static const std::array image_extensions{".webp", ".jpg", ".png"};
            try {
                auto theme = std::make_shared<Theme>();
                theme->path = themePath;
                try {
                    for (auto& entry
                             : std::filesystem::recursive_directory_iterator{theme->path}) {
                        if (entry.is_regular_file())
                            theme->files.push_back(entry.path());
                    }
                    std::ranges::sort(theme->files, IgnoreCaseLess{});
                }
                catch (std::exception &e) {
                    cerr << "Error listing files inside theme. SD card may be corrupted." << endl;
                }

                static const std::array image_names{
                    "preview-collage",
                    "preview-launcher",
                    "preview-warawara"
                };
                for (auto name : image_names) {
                    auto preview_base = theme->path / name;
                    for (auto ext : image_extensions) {
                        auto preview = preview_base;
                        preview += ext;
                        if (exists(preview)) {
                            theme->previews.push_back(preview);
                            break;
                        }
                    }
                }
                MetadataJson metaJson;
                glz::ex::read_file_json(metaJson,
                                        (theme->path / "metadata.json").string(),
                                        std::string{});
                theme->metadata = std::move(metaJson.metadata);

                // If there was no preview image on the theme folder, look for a cached thumbnail.
                if (theme->path.empty() && theme->metadata.themeID) {
                    auto preview = CalcLegacyThumbnailPath(*theme->metadata.themeID);
                    if (exists(preview))
                        theme->previews.push_back(preview);
                }

                return theme;
            }
            catch (std::exception& e) {
                // Fallback: try to read legacy metadata.
                return ReadInstalledThemeLegacy(themePath);
            }
        }

        ConstThemePtr
        ReadInstalledThemeLegacy(const std::filesystem::path &themePath) {
            // Fallback: find a json (in SD:/themiify/installed/) that matches this theme.
            try {
                auto theme = std::make_shared<Theme>();
                theme->path = themePath;
                for (auto entry
                         : std::filesystem::directory_iterator{THEMIIFY_INSTALLED_THEMES}) {
                    if (!entry.is_regular_file())
                        continue;
                    if (as_lower_case(entry.path().extension()) != ".json")
                        continue;
                    try {
                        LegacyMetadataJson legacyMetaJson;
                        glz::ex::read_file_json(legacyMetaJson,
                                                entry.path().string(),
                                                std::string{});
                        auto& leg_meta = legacyMetaJson.ThemeData;
                        if (leg_meta.themeID.empty()) // not a Themezer theme, skip it
                            continue;
                        if (exists(themePath) && exists(leg_meta.themeInstallPath)
                            && equivalent(themePath, leg_meta.themeInstallPath)) {
                            theme->metadata.themeID = leg_meta.themeID;
                            theme->metadata.themeName = leg_meta.themeName;
                            theme->metadata.themeAuthor = leg_meta.themeAuthor;
                            theme->metadata.themeVersion = leg_meta.themeVersion;
                            theme->legacyMetadataPath = entry.path();
                            auto preview = CalcLegacyThumbnailPath(*theme->metadata.themeID);
                            if (exists(preview))
                                theme->previews.push_back(preview);
                            return theme;
                        }
                    }
                    catch (std::exception& e3) {
                        cout << "Failed to parse " << entry.path()
                             << ": " << e3.what() << endl;
                    }
                }
                // No matching metadata found, just use the folder name as the name.
                theme->metadata.themeName = themePath.filename();
                return theme;
            }
            catch (std::exception& e2) {
                cerr << "ERROR: while looking for themiify metadata: " << e2.what() << endl;
                return {};
            }
        }

        std::filesystem::path
        SanitizeElement(const std::filesystem::path& input) {
            std::u32string output_str;
            std::u32string input_str = input.u32string();
            for (auto c : input_str) {
                switch (c) {
                    case U'/':
                    case U'\\':
                    case U'<':
                    case U'>':
                    case U':':
                    case U'"':
                    case U'|':
                    case U'?':
                    case U'*':
                    case 127: // "DEL"
                        output_str += U'_';
                        break;
                    default:
                        // NOTE: the Wii U needs filenames encoded in Shift-JIS, but wut
                        // does not implement any conversion. So we'll enforce ASCII-only
                        // for now.
                        if (c >= 32 && c < 127)
                            output_str += c;
                        else
                            output_str += U'_';
                }
            }
            // Do not allow it to end with '.'
            while (output_str.ends_with(U'.'))
                output_str.pop_back();
            return output_str;
        }

        // Called from the main thread.
        void
        TaskCompleteInstall(const InstallContextPtr& ctx) {
            cout << "TaskCompleteInstall: " << ctx->metadata->themeName << endl;
            if (ctx->enable_theme)
                PluginManager::Enable(ctx->theme_path);
            if (ctx->success_func)
                ctx->success_func();
        }

        // Called from the main thread.
        void
        TaskPrintError(const std::string& msg) {
            cerr << "ERROR: " << msg << endl;
        }

        // Called from the main thread.
        void
        TaskReportInstallError(const InstallContextPtr& ctx,
                               const std::string& msg) {
            cerr << "TaskReportInstallError: " << msg << endl;
            if (ctx->error_func)
                ctx->error_func(msg);
        }

        // Called from the main thread.
        void
        TaskReportInstallProgress(const InstallContextPtr& ctx,
                                  const std::string& msg) {
            cout << "TaskReportInstallProgress: " << msg << endl;
            if (ctx->progress_func)
                ctx->progress_func(msg);
        }

        void
        UninstallThread(ConstThemePtr theme) {
            try {
                if (auto hexId = GetHexId(theme))
                    safe_theme_id_map.lock()
                        ->erase(*hexId);

                safe_themes_map.lock()
                    ->erase(theme->path.filename());

                // If there's a cached thumbnail, delete it.
                if (theme->metadata.themeID) {
                    auto thumbnail = CalcLegacyThumbnailPath(*theme->metadata.themeID);
                    if (exists(thumbnail))
                        DeletePath(thumbnail);
                }

                // If there's legacy metadata, delete it.
                auto& leg_meta = theme->legacyMetadataPath;
                if (!leg_meta.empty()) {
                    if (exists(leg_meta))
                        DeletePath(leg_meta);
                }

                // Finally, delete the theme path with all its contents.
                DeletePath(theme->path);
            }
            catch (std::exception& e) {
                std::string msg = "Uninstall error: "s + e.what();
                AddTask(TaskPrintError, std::move(msg));
            }
        }

    } // namespace

    /*------------------*/
    /* Public functions */
    /*------------------*/

    void initialize() {
        TRACE_FUNC;

        create_directories(THEMES_ROOT);

        RefreshInstalledThemes();

        RefreshUThemes();

        install_thread = {};
        uninstall_thread = {};
    }

    void finalize() {
        TRACE_FUNC;

        refresh_themes_thread = {};
        refresh_uthemes_thread = {};

        install_thread = {};
        uninstall_thread = {};
    }

    void process() {
        ExecTasks();
    }

    ConstMetadataPtr
    ReadUThemeMetadata(const std::filesystem::path &uthemePath)
    try {
        Zip::Archive uthemeArchive{uthemePath, ZIP_RDONLY};
        auto metadataStatData = uthemeArchive.stat("metadata.json", ZIP_FL_NOCASE);
        std::string buffer(metadataStatData.size, '\0');
        auto themeMetadataFile = uthemeArchive.fopen("metadata.json", ZIP_FL_NOCASE);
        if (themeMetadataFile.read(std::span{buffer}) != buffer.size())
            throw std::runtime_error{"could not read all metadata"};
        themeMetadataFile.close();
        uthemeArchive.close();
        MetadataJson json;
        glz::ex::read_json(json, buffer);
        return std::make_shared<Metadata>(std::move(json.metadata));
    }
    catch (std::exception& e) {
        cerr << "ERROR: " << e.what() << endl;
        return {};
    }

    void
    Install(const std::filesystem::path& utheme,
            const ConstMetadataPtr& metadata,
            bool enable_theme,
            ProgressFunction progress_func,
            SuccessFunction success_func,
            ErrorFunction error_func) {
        auto ctx = std::make_shared<InstallContext>(utheme,
                                                    metadata,
                                                    enable_theme,
                                                    std::move(progress_func),
                                                    std::move(success_func),
                                                    std::move(error_func));
        install_thread = std::jthread{InstallThread, std::move(ctx)};
    }

    void
    CancelInstall() {
        install_thread = {};
    }

    // NOTE: This takes a copy and not a reference, to keep the theme object alive while
    // it's being erased.
    void
    Uninstall(ConstThemePtr theme) {
        PluginManager::Disable(theme->path);
        uninstall_thread = std::jthread{UninstallThread, std::move(theme)};
    }

    std::filesystem::path
    CalcThemePath(const Metadata& meta) {
        std::string foldername = meta.themeName;
        if (meta.themeID) {
            // Strip the ":" from the themeID
            std::string clean_theme_id = *meta.themeID;
            std::erase(clean_theme_id, ':');
            foldername += " (" + clean_theme_id + ")";
        }
        return THEMES_ROOT / SanitizeElement(foldername);
    }

    std::filesystem::path
    CalcUThemePath() {
        auto now = std::chrono::system_clock::now();
        auto today = std::chrono::floor<std::chrono::days>(now);
        std::chrono::year_month_day date = today;
        std::chrono::hh_mm_ss time{now - today};
        std::string filename = std::format("download-{}-{}.utheme", date, time);
        return THEMES_ROOT / SanitizeElement(filename);
    }

    std::filesystem::path
    CalcUThemePath(const std::string& url) {
        if (as_lower_case(url).ends_with(".utheme")) {
            // Assume the URL contains a .utheme file name.
            auto last_slash = url.find('/');
            if (last_slash != std::string::npos) {
                auto filename = url.substr(last_slash + 1);
                return THEMES_ROOT / SanitizeElement(filename);
            }
        }
        return CalcUThemePath(); // generate filename using date and time
    }

    std::filesystem::path
    CalcUThemePath(const std::string& name,
                   const std::string& id) {
        std::string filename = join({name, id}, "-");
        if (filename.empty())
            return CalcUThemePath(); // generate filename using date and time
        filename += ".utheme";
        return THEMES_ROOT / SanitizeElement(filename);
    }

    void
    RefreshInstalledThemes() {
        refresh_themes_thread = {}; // ensure any current thread is stopped
        refresh_themes_thread_busy = true;
        refresh_themes_thread = std::jthread{
            [](std::stop_token stopper)
            {
                try {
                    safe_theme_id_map.lock()->clear();
                    safe_themes_map.lock()->clear();
                    if (stopper.stop_requested())
                        throw std::runtime_error{"thread stopped"};

                    for (auto& entry : std::filesystem::directory_iterator{THEMES_ROOT}) {
                        if (stopper.stop_requested())
                            throw std::runtime_error{"thread stopped"};
                        if (!entry.is_directory())
                            continue;
                        if (auto theme = ReadInstalledTheme(entry.path())) {
                            if (auto hexId = GetHexId(theme))
                                safe_theme_id_map.lock()->emplace(*hexId, theme);
                            safe_themes_map.lock()->emplace(
                                entry.path().filename(),
                                std::move(theme)
                            );
                        }
                    }
                }
                catch (std::exception& e) {
                    // NOTE: print the error on the main thread
                    std::string msg ="RefreshInstalledThemes() failed: "s + e.what();
                    AddTask(TaskPrintError, std::move(msg));
                }
                refresh_themes_thread_busy = false;
            }
        };
    }

    bool
    IsRefreshingThemes() {
        return refresh_themes_thread_busy;
    }

    ConstThemePtr
    GetThemeByID(const std::string& hexId) {
        auto theme_id_map = safe_theme_id_map.lock();
        auto it = theme_id_map->find(hexId);
        if (it == theme_id_map->end())
            return {};
        return it->second;
    }

    void
    ForEachInstalledTheme(const ThemeFunction& func) {
        auto themes = safe_themes_map.lock();
        for (auto [index, path_theme] : *themes | std::views::enumerate)
            func(index, path_theme.second);
    }

    ConstThemePtr
    GetCurrentTheme()
        noexcept {
        auto name = PluginManager::GetCurrentTheme();
        if (name.empty())
            return {};
        auto themes = safe_themes_map.lock();
        auto it = themes->find(name);
        if (it == themes->end())
            return {};
        return it->second;
    }

    void
    RefreshUThemes() {
        refresh_uthemes_thread = {}; // ensure any current thread is stopped
        refresh_uthemes_thread_busy = true;
        refresh_uthemes_thread = std::jthread{
            [](std::stop_token stopper)
            {
                try {
                    safe_uthemes_map.lock()->clear();
                    if (stopper.stop_requested())
                        throw std::runtime_error{"thread stopped"};

                    for (auto& entry : std::filesystem::directory_iterator{THEMES_ROOT}) {
                        if (stopper.stop_requested())
                            throw std::runtime_error{"thread stopped"};
                        if (!entry.is_regular_file())
                            continue;
                        if (as_lower_case(entry.path().extension()) != ".utheme")
                            continue;
                        safe_uthemes_map.lock()->emplace(
                            entry.path(), // NOTE: use full path
                            ReadUThemeMetadata(entry.path())
                        );
                    }
                }
                catch (std::exception& e) {
                    // NOTE: print the error on the main thread
                    std::string msg ="RefreshUThemes() failed: "s + e.what();
                    AddTask(TaskPrintError, std::move(msg));
                }
                refresh_uthemes_thread_busy = false;
            }
        };
    }

    bool
    IsRefreshingUThemes() {
        return refresh_uthemes_thread_busy;
    }

    void
    ForEachUTheme(const MetadataFunction& func) {
        auto uthemes = safe_uthemes_map.lock();
        for (auto [index, path_meta] : *uthemes | std::views::enumerate)
            func(index, path_meta.first, path_meta.second);
    }

} // namespace ThemeManager
