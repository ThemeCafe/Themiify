/*
 * Themiify - A theme manager for the Nintendo Wii U
 * Copyright (C) 2026 Fangal-Airbag
 * Copyright (C) 2026 AlphaCraft9658
 * Copyright (C) 2026 Daniel K. O. <dkosmari>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "SettingsPopup.h"

#include "../App.h"
#include "../IconsFontAwesome4.h"
#include "../UI.h"
#include "../utils.h"
#include "../thread_safe.hpp"

#include <coreinit/systeminfo.h>
#include <sysapp/title.h>

#include <atomic>
#include <fstream>
#include <iostream>
#include <mutex>
#include <string>
#include <thread>

#include <imgui.h>
#include <imgui_raii.h>

#include <zlib.h>

// Enable to inject cache error state.
// #define DEBUG_INJECT_CACHE_ERROR

// Enable to ninject dump error state
// #define DEBUG_INJECT_DUMP_ERROR

// Enable to inject integrity error state.
// #define DEBUG_INJECT_INTEGRITY_ERROR

using std::cout;
using std::endl;
using namespace std::literals;

namespace SettingsPopup {

    namespace {

        /*-------*/
        /* Types */
        /*-------*/

        enum class State {
            hidden,
            stylemiiu_error,
            integrity_confirmation,
            checking_integrity,
            integrity_success,
            integrity_error,
            dump_confirmation,
            dumping,
            dump_completed,
            dump_error,
            cache_confirmation,
            clearing_cache,
            cache_cleared,
            cache_clear_error,
        };

        enum class Region {
            USA,
            EUR,
            JPN,
            UNK,
        };

        struct FileIntegrityInfo {
            std::filesystem::path relative_path;
            uint32_t expected_crc;
        };

        /*-----------*/
        /* Constants */
        /*-----------*/

        const std::string popup_id = "SettingsPopup"s;

        const std::array<std::filesystem::path, 13> all_message_szs_locations{
            "UsEnglish/Message/AllMessage.szs",
            "UsFrench/Message/AllMessage.szs",
            "UsPortuguese/Message/AllMessage.szs",
            "UsSpanish/Message/AllMessage.szs",
            "EuDutch/Message/AllMessage.szs",
            "EuEnglish/Message/AllMessage.szs",
            "EuFrench/Message/AllMessage.szs",
            "EuGerman/Message/AllMessage.szs",
            "EuItalian/Message/AllMessage.szs",
            "EuPortuguese/Message/AllMessage.szs",
            "EuRussian/Message/AllMessage.szs",
            "EuSpanish/Message/AllMessage.szs",
            "JpJapanese/Message/AllMessage.szs"
        };

        const std::array<FileIntegrityInfo, 16> integrity_files{{
#ifndef DEBUG_INJECT_INTEGRITY_ERROR
            {"Common/Package/Men2.pack",               0x946CD8A2},
            {"Common/Package/Men.pack",                0xB9A4343A},
            {"Common/Sound/Men/cafe_barista_men.bfsar",0xC9C16521},
#else
            {"Common/Package/Men2.pack",               0x00000000},
            {"Common/Package/Men.pack",                0x00000000},
            {"Common/Sound/Men/cafe_barista_men.bfsar",0x00000000},
#endif

            {"UsEnglish/Message/AllMessage.szs",       0x9C91A249},
            {"UsFrench/Message/AllMessage.szs",        0xF80483EE},
            {"UsPortuguese/Message/AllMessage.szs",    0x82F3CB76},
            {"UsSpanish/Message/AllMessage.szs",       0xAFA41B10},

            {"EuDutch/Message/AllMessage.szs",         0xA3662453},
            {"EuEnglish/Message/AllMessage.szs",       0x15DB5A6D},
            {"EuFrench/Message/AllMessage.szs",        0x2690B327},
            {"EuGerman/Message/AllMessage.szs",        0xF6FD1ADA},
            {"EuItalian/Message/AllMessage.szs",       0xDAF77D8C},
            {"EuPortuguese/Message/AllMessage.szs",    0xE0DC3860},
            {"EuRussian/Message/AllMessage.szs",       0x0BDED99E},
            {"EuSpanish/Message/AllMessage.szs",       0x89C4AA89},

            {"JpJapanese/Message/AllMessage.szs",      0xEEB51547}
        }};

        /*-----------*/
        /* Variables */
        /*-----------*/

        State state;

        Region region;

        bool popup_queued;

        bool dump_allmessage = false;
        bool delete_thumbnails = false;

        std::filesystem::path menu_content_path;

        std::jthread worker_thread;
        std::atomic_bool worker_done = false;
        std::atomic_bool worker_success = false;

        std::vector<std::filesystem::path> full_all_message_paths;
        std::vector<std::filesystem::path> cache_all_message_paths;

        thread_safe<std::vector<std::filesystem::path>> safe_modified_files;

        /*-----------------------*/
        /* Function declarations */
        /*-----------------------*/

        void
        action_check_integrity();

        void
        action_clear_cache();

        void
        action_close();

        void
        action_dump_files();

        uint32_t
        CalculateCRC32(const std::filesystem::path& path);

        void
        CreateCacheFile(std::ifstream &sourceFile,
                        const std::filesystem::path& outputPath);

        std::filesystem::path
        GetMenuContentPath();

        void
        show_cache_clear_error();

        void
        show_cache_cleared();

        void
        show_cache_confirmation();

        void
        show_checking_integrity();

        void
        show_clearing_cache();

        void
        show_dump_completed();

        void
        show_dump_confirmation();

        void
        show_dump_error();

        void
        show_dumping();

        void
        show_integrity_confirmation();

        void
        show_integrity_error();

        void
        show_integrity_success();

        void
        show_stylemiiu_error();

        void
        start_worker(auto&& func);

        /*----------------------*/
        /* Function definitions */
        /*----------------------*/

        void
        action_close() {
            UI::CloseCurrentPopup();
            state = State::hidden;
        }

        void
        action_check_integrity() {
            state = State::checking_integrity;
            start_worker([] {
                safe_modified_files.lock()->clear();
                bool result = true;
                for (const auto& entry : integrity_files) {
                    auto full_path = menu_content_path / entry.relative_path;

                    if (!exists(full_path))
                        continue;

                    uint32_t crc = CalculateCRC32(full_path);

                    if (crc != entry.expected_crc) {
                        result = false;
                        auto modified_files = safe_modified_files.lock();
                        modified_files->push_back(entry.relative_path);
                    }
                }
                return result;
            });
        }

        void
        action_clear_cache() {
            state = State::clearing_cache;
            start_worker([] {
                if (delete_thumbnails)
                    DeletePath(THEMIIFY_THUMBNAILS);

                DeletePath(THEMIIFY_ROOT / "cache/Common");

                for (const auto& path : all_message_szs_locations) {
                    DeletePath(THEMIIFY_ROOT / "cache" / path);
                }

                if (delete_thumbnails && exists(THEMIIFY_THUMBNAILS))
                    return false;

                if (exists(THEMIIFY_ROOT / "cache/Common"))
                    return false;

                for (const auto& path : all_message_szs_locations) {
                    if (exists(THEMIIFY_ROOT / "cache" / path))
                        return false;
                }

                return true;
            });
        }

        void
        action_dump_files() {
            state = State::dumping;
            start_worker([] {
                bool result = true;

                auto dump_one = [](const std::filesystem::path& src,
                                   const std::filesystem::path& dst) {
                    std::ifstream file(src, std::ios::binary | std::ios::ate);
                    create_directories(dst.parent_path());
                    CreateCacheFile(file, dst);
                    return exists(dst) && file_size(dst) > 0;
                };

                result &= dump_one(
                    menu_content_path / MEN_PATH,
                    THEMIIFY_ROOT / "cache" / MEN_PATH
                );

                result &= dump_one(
                    menu_content_path / MEN2_PATH,
                    THEMIIFY_ROOT / "cache" / MEN2_PATH
                );

                result &= dump_one(
                    menu_content_path / CAFE_BARISTA_MEN_PATH,
                    THEMIIFY_ROOT / "cache" / CAFE_BARISTA_MEN_PATH
                );

                if (dump_allmessage) {
                    for (size_t i = 0; i < full_all_message_paths.size(); ++i) {
                        result &= dump_one(
                            full_all_message_paths.at(i),
                            cache_all_message_paths.at(i)
                        );
                    }
                }

                return result;
            });
        }

        uint32_t
        CalculateCRC32(const std::filesystem::path& path) {
            // TODO: use filebuf instead of fstream
            std::ifstream file(path, std::ios::binary);
            if (!file)
                throw std::runtime_error("Failed to open file");

            uint32_t crc = crc32(0L, Z_NULL, 0);

            std::vector<char> buffer(64 * 1024);

            while (file) {
                file.read(buffer.data(), buffer.size());

                std::streamsize bytes_read = file.gcount();
                if (bytes_read > 0) {
                    crc = crc32(
                        crc,
                        reinterpret_cast<const Bytef*>(buffer.data()),
                        static_cast<uInt>(bytes_read)
                    );
                }
            }
            return crc;
        }

        void
        CreateCacheFile(std::ifstream &sourceFile,
                        const std::filesystem::path& outputPath) {
            if (!sourceFile.is_open()) {
                cout << "Invalid or unopened source file" << endl;
                return;
            }

            std::size_t sourceSize = static_cast<std::size_t>(sourceFile.tellg());
            sourceFile.seekg(0, std::ios::beg);

            std::vector<unsigned char> buffer(sourceSize);
            sourceFile.read(reinterpret_cast<char*>(buffer.data()), sourceSize);

            if (!sourceFile) {
                cout << "Error reading source file to create cache file." << endl;
                return;
            }

            std::ofstream outputFile(outputPath, std::ios::binary | std::ios::trunc);
            if (!outputFile.is_open()) {
                cout << "Failed to open output file for writing: " << outputPath << endl;
                return;
            }

            outputFile.write(reinterpret_cast<const char*>(buffer.data()), buffer.size());

            if (!outputFile) {
                cout << "Error writing cache file to " << outputPath << endl;
                return;
            }

            cout << "Successfully cached file to: " << outputPath << endl;
        }

        std::filesystem::path
        GetMenuContentPath() {
            uint64_t menuTitleID = _SYSGetSystemApplicationTitleId(SYSTEM_APP_ID_WII_U_MENU);

            switch (menuTitleID) {
                case WII_U_MENU_USA_TID:
                    region = Region::USA;
                    break;
                case WII_U_MENU_EUR_TID:
                    region = Region::EUR;
                    break;
                case WII_U_MENU_JPN_TID:
                    region = Region::JPN;
                    break;
                default:
                    region = Region::UNK;
                    break;
            }

            uint32_t menuIDParentDir = (uint32_t)(menuTitleID >> 32);
            uint32_t menuIDChildDir = (uint32_t)menuTitleID;

            char splitMenuID[18];
            std::snprintf(splitMenuID,
                          sizeof splitMenuID,
                          "%08x/%08x",
                          menuIDParentDir,
                          menuIDChildDir);

            return std::filesystem::path{"storage_mlc:/sys/title"} / splitMenuID / "content";
        }

        void
        show_cache_clear_error() {
            UI::Title("Error Clearing Cache");

            ImGui::Text("One or more files could not be removed from the cache");

            UI::ButtonHBox buttons;
            buttons.add(ICON_FA_TIMES " Close", true, action_close);
            buttons.show();
        }

        void
        show_cache_cleared() {
            UI::Title("Cache Cleared");

            ImGui::Text("The Themiify cache has been succesfully cleared.");

            UI::ButtonHBox buttons;
            buttons.add(ICON_FA_TIMES " Close", true, action_close);
            buttons.show();
        }

        void
        show_cache_confirmation() {
            UI::Title("Clear Cache Confirmation");

            ImGui::Text("Would you like to delete your Themiify cache located at:\n"
                        "SD:/themiify/cache ?\n"
                        "\n"
                        "Doing so will delete all dumped Wii U Menu files");

            UI::Checkbox("Delete theme thumbnails as well?", delete_thumbnails);

            UI::ButtonHBox buttons;
            buttons.add(ICON_FA_TIMES " Cancel", action_close);
            buttons.add(ICON_FA_TRASH " Clear Cache", true, action_clear_cache);
            buttons.show();
        }

        void
        show_checking_integrity() {
            UI::Title("Checking...");

            ImGui::Text("Please wait. Do not turn off your Wii U.");

            if (worker_done) {
                worker_thread = {};
                state = worker_success
                    ? State::integrity_success
                    : State::integrity_error;
            }
        }

        void
        show_clearing_cache() {
            UI::Title("Clearing Cache...");

            ImGui::Text("Please wait.");

            if (worker_done) {
                worker_thread = {};
#ifndef DEBUG_INJECT_CACHE_ERROR
                state = worker_success
                    ? State::cache_cleared
                    : State::cache_clear_error;
#else
                state = State::cache_clear_error;
#endif
            }
        }

        void
        show_dump_completed() {
            UI::Title("Dump Completed");

            ImGui::Text("The dump has been sucessfully completed\n"
                        "\n"
                        "You can find your dumped files at:\n"
                        "SD:/themiify/cache");

            UI::ButtonHBox buttons;
            buttons.add(ICON_FA_TIMES " Close", true, action_close);
            buttons.show();
        }

        void
        show_dump_confirmation() {
            UI::Title("Confirm Dump Menu Files");

            ImGui::Text(
                "Would you like to dump the most common Wii U Menu files\n"
                "used in theme creation to your SD Card?\n"
                "\n"
                "The files dumped are: Men.pack, Men2.pack & cafe_barista_men.bfsar\n"
                "\n"
                "Note: By installing any theme via Themiify, this will be done automatically."
            );

            UI::Checkbox("Dump AllMessage.szs for all languages.\n"
                            "Consult the Theme Café docs for more info on these files.",
                            dump_allmessage);

            UI::ButtonHBox buttons;
            buttons.add(ICON_FA_TIMES " Cancel", action_close);
            buttons.add(ICON_FA_DOWNLOAD " Dump Files", true, action_dump_files);
            buttons.show();
        }

        void
        show_dump_error() {
            UI::Title("Dump Error");

            ImGui::Text("One or more files failed to dump correctly.");

            UI::ButtonHBox buttons;
            buttons.add(ICON_FA_TIMES " Close", true, action_close);
            buttons.show();
        }

        void
        show_dumping() {
            UI::Title("Dumping...");

            ImGui::Text("Please wait. Do not turn off your Wii U.");

            if (worker_done) {
                worker_thread = {};
                state = worker_success
                    ? State::dump_completed
                    : State::dump_error;
#ifdef DEBUG_INJECT_DUMP_ERROR
                state = State::dump_error;
#endif
            }
        }

        void
        show_integrity_confirmation() {
            UI::Title("Confirm Check Menu Integrity");

            ImGui::Text(
                "Would you like to check the integrity of your Wii U Menu's\n"
                "files on your NAND to verify whether they have been modified?\n"
                "\n"
                "If your files have been modified, Themiify will always fail to install themes\n"
                "until you either restore clean files to your NAND, or place clean files\n"
                "in SD:/themiify/cache\n"
                "\n"
                "Please check the Theme Café Docs for more info."
            );

            UI::ButtonHBox buttons;
            buttons.add(ICON_FA_TIMES " Cancel", action_close);
            buttons.add(ICON_FA_SHIELD " Check Integrity", true, action_check_integrity);
            buttons.show();
        }

        void
        show_integrity_error() {
            using namespace ImGui::RAII;

            UI::Title("Integrity Check Error!");

            ImGui::Text("The following files appear to be modified: ");

            {
                Indent one;
                auto modified_files = safe_modified_files.c_lock();
                for (auto &file : *modified_files) {
                    ImGui::Text(file.string());
                }
            }

            ImGui::TextWrapped("Please consult the Theme Café docs for steps to "
                               "restore your original files.");

            UI::ButtonHBox buttons;
            buttons.add(ICON_FA_TIMES " Close", action_close);
            buttons.show();
        }

        void
        show_integrity_success() {
            UI::Title("Integrity Checked");

            ImGui::Text("All your menu files are clean and ready for use with Themiify!");

            UI::ButtonHBox buttons;
            buttons.add(ICON_FA_TIMES " Close", true, action_close);
            buttons.show();
        }

        void
        show_stylemiiu_error() {
            UI::Title("StyleMiiU Not Found!");

            ImGui::Text("The StyleMiiU plugin could not be found!\n"
                        "\n"
                        "For your installed themes to work in the Wii U Menu you will need to install\n"
                        "this plugin from either the Homebrew App Store or from:\n");
            ImGui::TextLink("https://github.com/ThemeCafe/Themiify/releases/latest");


            UI::ButtonHBox buttons;
            buttons.add(ICON_FA_SIGN_OUT " Quit Themiify", true, App::quit);
            buttons.show();
        }

        void
        start_worker(auto&& func) {
            OSEnableHomeButtonMenu(FALSE);

            worker_done = false;
            worker_success = false;

            worker_thread = std::jthread{
                [func = std::forward<decltype(func)>(func)]() mutable {
                    bool success = false;

                    try {
                        success = func();
                    }
                    catch (const std::exception& e) {
                        cout << "Settings worker error: " << e.what() << endl;
                        success = false;
                    }
                    catch (...) {
                        cout << "Settings worker unknown error" << endl;
                        success = false;
                    }

                    worker_success = success;
                    worker_done = true;

                    OSEnableHomeButtonMenu(TRUE);
                }
            };
        }

    } // namespace

    /*------------------*/
    /* Public functions */
    /*------------------*/

    void
    open(OpenState openState) {
        menu_content_path = GetMenuContentPath();

        full_all_message_paths.clear();
        cache_all_message_paths.clear();
        safe_modified_files.lock()->clear();

        for (const auto& path : all_message_szs_locations) {
            switch (region) {
                case Region::USA:
                    if (path.string().starts_with("Us")) {
                        full_all_message_paths.push_back(menu_content_path / path);
                        cache_all_message_paths.push_back(THEMIIFY_ROOT / "cache" / path);
                    }

                    break;
                case Region::EUR:
                    if (path.string().starts_with("Eu")) {
                        full_all_message_paths.push_back(menu_content_path / path);
                        cache_all_message_paths.push_back(THEMIIFY_ROOT / "cache" / path);
                    }

                    break;
                case Region::JPN:
                    if (path.string().starts_with("Jp")) {
                        full_all_message_paths.push_back(menu_content_path / path);
                        cache_all_message_paths.push_back(THEMIIFY_ROOT / "cache" / path);
                    }

                    break;
                default:
                    break;
            }
        }

        popup_queued = true;

        switch (openState) {
            case OpenState::stylemiiu:
                state = State::stylemiiu_error;
                break;
            case OpenState::integrity:
                state = State::integrity_confirmation;
                break;
            case OpenState::force_integrity:
                action_check_integrity();
                break;
            case OpenState::dump:
                state = State::dump_confirmation;
                break;
            case OpenState::cache:
                state = State::cache_confirmation;
                break;
            default:
                state = State::hidden;
                break;
        }

    }

    void
    process_ui() {
        using namespace ImGui::RAII;
        if (state == State::hidden)
            return;

        if (popup_queued) {
            UI::OpenPopup(popup_id);
            popup_queued = false;
        }

        auto center = ImGui::GetMainViewport()->GetCenter();
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, {0.5f, 0.5f});

        PopupModal popup{popup_id, nullptr,
                         ImGuiWindowFlags_NoSavedSettings |
                         ImGuiWindowFlags_AlwaysAutoResize |
                         ImGuiWindowFlags_NoMove |
                         ImGuiWindowFlags_NoScrollbar |
                         ImGuiWindowFlags_NoCollapse |
                         ImGuiWindowFlags_NoTitleBar};

        if (!popup) {
            // ImGui closed the popup
            UI::PlaySFXPopupClose();
            state = State::hidden;
            return;
        }

        switch (state) {
            case State::stylemiiu_error:
                show_stylemiiu_error();
                break;

            case State::integrity_confirmation:
                show_integrity_confirmation();
                break;

            case State::checking_integrity:
                show_checking_integrity();
                break;

            case State::integrity_success:
                show_integrity_success();
                break;

            case State::integrity_error:
                show_integrity_error();
                break;

            case State::dump_confirmation:
                show_dump_confirmation();
                break;

            case State::dumping:
                show_dumping();
                break;

            case State::dump_completed:
                show_dump_completed();
                break;

            case State::dump_error:
                show_dump_error();
                break;

            case State::cache_confirmation:
                show_cache_confirmation();
                break;

            case State::clearing_cache:
                show_clearing_cache();
                break;

            case State::cache_cleared:
                show_cache_cleared();
                break;

            case State::cache_clear_error:
                show_cache_clear_error();
                break;

            default:
                ;
        }
    }

} // namespace SettingsPopup
