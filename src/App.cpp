/*
 * Themiify - A theme manager for the Nintendo Wii U
 * Copyright (C) 2026 Fangal-Airbag
 * Copyright (C) 2026 AlphaCraft9658
 * Copyright (C) 2026 Daniel K. O. <dkosmari>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "App.h"

#include "Camera.h"
#include "ContentPanel.h"
#include "DownloadManager.h"
#include "ImageLoader.h"
#include "NavBar.h"
#include "PluginManager.h"
#include "ThemeManager.h"
#include "ThemezerAPI.h"
#include "timer.hpp"
#include "utils.h"
#include "screens/QRCodePopup.h"
#include "screens/ConfirmExitPopup.h"

#include <chrono>
#include <fstream>
#include <iostream>
#include <span>
#include <vector>

#include <coreinit/memory.h>
#include <coreinit/debug.h>
#include <proc_ui/procui.h>

#include <mocha/mocha.h>

#include <SDL2/SDL.h>
#include <SDL2/SDL_syswm.h>
#include <SDL2/SDL_image.h>
#include <SDL2/SDL_mixer.h>

#include <imgui.h>
#include <imgui_internal.h>
#include <imgui_raii.h>
#include <imgui_stdlib.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>
#ifdef IMGUI_ENABLE_FREETYPE
#include <imgui_freetype.h>
#endif

#include <curl/curl.h>

// Define this to help seeing the padding and spacing values for windows.
// #define DEBUG_BG_COLOR

// Enable to get access to the style editor.
// #define ENABLE_STYLE_EDITOR

// Enable to track slow render times.
// #define MEASURE_RENDER_TIME

using std::cout;
using std::cerr;
using std::endl;
using namespace std::literals;

namespace App {
    SDL_Window *window;
    SDL_Renderer *renderer;

    std::vector<SDL_GameController *> controllers;
    std::vector<char> bgMusicData;

    Mix_Music *bgMusic;

    bool isRunning;

    static uint32_t procCallbackAcquire(void *) {
        if (Camera::is_initialized())
            Camera::open();

        return 0;
    }

    void load_imgui_theme() {
        ImVec4* colors = ImGui::GetStyle().Colors;
        colors[ImGuiCol_Text]                   = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_TextDisabled]           = ImVec4(0.50f, 0.50f, 0.50f, 1.00f);
        colors[ImGuiCol_WindowBg]               = ImVec4(0.00f, 0.00f, 0.00f, 1.00f);
        colors[ImGuiCol_ChildBg]                = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_PopupBg]                = ImVec4(0.08f, 0.08f, 0.08f, 0.94f);
        colors[ImGuiCol_Border]                 = ImVec4(0.43f, 0.50f, 0.50f, 0.50f);
        colors[ImGuiCol_BorderShadow]           = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_FrameBg]                = ImVec4(0.28f, 0.61f, 0.64f, 0.54f);
        colors[ImGuiCol_FrameBgHovered]         = ImVec4(0.26f, 0.92f, 0.98f, 0.40f);
        colors[ImGuiCol_FrameBgActive]          = ImVec4(0.26f, 0.92f, 0.98f, 0.67f);
        colors[ImGuiCol_TitleBg]                = ImVec4(0.04f, 0.04f, 0.04f, 1.00f);
        colors[ImGuiCol_TitleBgActive]          = ImVec4(0.16f, 0.45f, 0.48f, 1.00f);
        colors[ImGuiCol_TitleBgCollapsed]       = ImVec4(0.00f, 0.00f, 0.00f, 0.51f);
        colors[ImGuiCol_MenuBarBg]              = ImVec4(0.14f, 0.14f, 0.14f, 1.00f);
        colors[ImGuiCol_ScrollbarBg]            = ImVec4(0.02f, 0.02f, 0.02f, 0.53f);
        colors[ImGuiCol_ScrollbarGrab]          = ImVec4(0.31f, 0.31f, 0.31f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabHovered]   = ImVec4(0.41f, 0.41f, 0.41f, 1.00f);
        colors[ImGuiCol_ScrollbarGrabActive]    = ImVec4(0.51f, 0.51f, 0.51f, 1.00f);
        colors[ImGuiCol_CheckMark]              = ImVec4(0.26f, 0.92f, 0.98f, 1.00f);
        colors[ImGuiCol_CheckboxSelectedBg]     = ImVec4(0.22f, 0.76f, 0.80f, 0.45f);
        colors[ImGuiCol_SliderGrab]             = ImVec4(0.24f, 0.83f, 0.88f, 1.00f);
        colors[ImGuiCol_SliderGrabActive]       = ImVec4(0.26f, 0.92f, 0.98f, 1.00f);
        colors[ImGuiCol_Button]                 = ImVec4(0.20f, 0.73f, 0.78f, 0.40f);
        colors[ImGuiCol_ButtonHovered]          = ImVec4(0.26f, 0.92f, 0.98f, 1.00f);
        colors[ImGuiCol_ButtonActive]           = ImVec4(0.06f, 0.90f, 0.98f, 1.00f);
        colors[ImGuiCol_Header]                 = ImVec4(0.26f, 0.92f, 0.98f, 0.31f);
        colors[ImGuiCol_HeaderHovered]          = ImVec4(0.26f, 0.92f, 0.98f, 0.80f);
        colors[ImGuiCol_HeaderActive]           = ImVec4(0.26f, 0.92f, 0.98f, 1.00f);
        colors[ImGuiCol_Separator]              = ImVec4(0.43f, 0.50f, 0.50f, 0.50f);
        colors[ImGuiCol_SeparatorHovered]       = ImVec4(0.10f, 0.70f, 0.75f, 0.78f);
        colors[ImGuiCol_SeparatorActive]        = ImVec4(0.10f, 0.70f, 0.75f, 1.00f);
        colors[ImGuiCol_ResizeGrip]             = ImVec4(0.26f, 0.92f, 0.98f, 0.20f);
        colors[ImGuiCol_ResizeGripHovered]      = ImVec4(0.26f, 0.92f, 0.98f, 0.67f);
        colors[ImGuiCol_ResizeGripActive]       = ImVec4(0.26f, 0.92f, 0.98f, 0.95f);
        colors[ImGuiCol_InputTextCursor]        = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_TabHovered]             = ImVec4(0.26f, 0.92f, 0.98f, 0.80f);
        colors[ImGuiCol_Tab]                    = ImVec4(0.18f, 0.55f, 0.58f, 0.86f);
        colors[ImGuiCol_TabSelected]            = ImVec4(0.20f, 0.64f, 0.68f, 1.00f);
        colors[ImGuiCol_TabSelectedOverline]    = ImVec4(0.26f, 0.92f, 0.98f, 1.00f);
        colors[ImGuiCol_TabDimmed]              = ImVec4(0.07f, 0.14f, 0.15f, 0.97f);
        colors[ImGuiCol_TabDimmedSelected]      = ImVec4(0.14f, 0.40f, 0.42f, 1.00f);
        colors[ImGuiCol_TabDimmedSelectedOverline]  = ImVec4(0.50f, 0.50f, 0.50f, 0.00f);
        colors[ImGuiCol_PlotLines]              = ImVec4(0.61f, 0.61f, 0.61f, 1.00f);
        colors[ImGuiCol_PlotLinesHovered]       = ImVec4(1.00f, 0.43f, 0.35f, 1.00f);
        colors[ImGuiCol_PlotHistogram]          = ImVec4(0.90f, 0.70f, 0.00f, 1.00f);
        colors[ImGuiCol_PlotHistogramHovered]   = ImVec4(1.00f, 0.60f, 0.00f, 1.00f);
        colors[ImGuiCol_TableHeaderBg]          = ImVec4(0.19f, 0.20f, 0.20f, 1.00f);
        colors[ImGuiCol_TableBorderStrong]      = ImVec4(0.31f, 0.35f, 0.35f, 1.00f);
        colors[ImGuiCol_TableBorderLight]       = ImVec4(0.23f, 0.25f, 0.25f, 1.00f);
        colors[ImGuiCol_TableRowBg]             = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_TableRowBgAlt]          = ImVec4(1.00f, 1.00f, 1.00f, 0.06f);
        colors[ImGuiCol_TextLink]               = ImVec4(0.26f, 0.59f, 0.98f, 1.00f);
        colors[ImGuiCol_TextSelectedBg]         = ImVec4(0.26f, 0.59f, 0.98f, 0.35f);
        colors[ImGuiCol_TreeLines]              = ImVec4(0.43f, 0.50f, 0.50f, 0.50f);
        colors[ImGuiCol_DragDropTarget]         = ImVec4(1.00f, 1.00f, 0.00f, 0.90f);
        colors[ImGuiCol_DragDropTargetBg]       = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);
        colors[ImGuiCol_UnsavedMarker]          = ImVec4(1.00f, 1.00f, 1.00f, 1.00f);
        colors[ImGuiCol_NavCursor]              = ImVec4(0.26f, 0.92f, 0.98f, 1.00f);
        colors[ImGuiCol_NavWindowingHighlight]  = ImVec4(1.00f, 1.00f, 1.00f, 0.70f);
        colors[ImGuiCol_NavWindowingDimBg]      = ImVec4(0.80f, 0.80f, 0.80f, 0.20f);
        colors[ImGuiCol_ModalWindowDimBg]       = ImVec4(0.80f, 0.80f, 0.80f, 0.35f);
    }

    void initialize_imgui() {
        IMGUI_CHECKVERSION();
        ImGui::CreateContext();

#ifndef ENABLE_STYLE_EDITOR
        ImGuiContext& g = *ImGui::GetCurrentContext();
        g.ConfigNavWindowingWithGamepad = false;
        g.ConfigNavWindowingKeyNext = 0;
        g.ConfigNavWindowingKeyPrev = 0;
#endif

        ImGuiIO &io = ImGui::GetIO();
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;
        io.ConfigFlags |= ImGuiConfigFlags_NavEnableGamepad;

#ifdef IMGUI_ENABLE_FREETYPE
        io.Fonts->FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_LoadColor;
        io.Fonts->FontLoaderFlags |= ImGuiFreeTypeLoaderFlags_Bitmap;
#endif

        io.ConfigDragScroll = true;
        io.ConfigWindowsMoveFromTitleBarOnly = true;
        io.MouseDragThreshold = 25;

        io.ConfigInputTrickleEventQueue = false;

        auto &style = ImGui::GetStyle();
        style.ScaleAllSizes(3);

        style.FramePadding = {20, 15};
        style.ItemSpacing = {24, 18};

        style.FrameRounding = 12.0f;
        style.ChildRounding = 0.0f;
        style.GrabRounding = 12.0f;
        style.PopupRounding = 12.0f;

        load_imgui_theme();

#ifdef DEBUG_BG_COLOR
        auto &colors = style.Colors;
        colors[ImGuiCol_WindowBg] = {0.5, 0.0f, 0.0f, 1.0f};
#endif

        style.FontSizeBase = 30;

        ImFontConfig fontConfig;
        fontConfig.Flags |= ImFontFlags_NoLoadError;
        fontConfig.EllipsisChar = U'…';
#ifdef IMGUI_ENABLE_FREETYPE
        // WORKAROUND: the freetype backend seems to misalign fonts merged with FontAwesome
        fontConfig.GlyphOffset.y = -style.FontSizeBase * (1.0f / 8.0f);
#endif
        void *fontData = nullptr;
        uint32_t fontSize = 0;
        OSGetSharedData(OS_SHAREDDATATYPE_FONT_STANDARD, 0, &fontData, &fontSize);

        io.Fonts->AddFontFromMemoryTTF(fontData, fontSize, style.FontSizeBase, &fontConfig);
        fontConfig.MergeMode = true;
        io.Fonts->AddFontFromFileTTF("fs:/vol/content/fonts/fontawesome-webfont.ttf", style.FontSizeBase, &fontConfig);
        io.Fonts->AddFontFromFileTTF("fs:/vol/content/fonts/InterVariable.ttf", style.FontSizeBase, &fontConfig);
        io.Fonts->AddFontFromFileTTF("fs:/vol/content/fonts/Symbola.ttf", style.FontSizeBase, &fontConfig);

        ImGui_ImplSDL2_InitForSDLRenderer(window, renderer);
        ImGui_ImplSDLRenderer2_Init(renderer);
    }

    void initialize() {
        std::filesystem::create_directories(THEMIIFY_ROOT);

        MochaUtilsStatus res;
        if ((res = Mocha_InitLibrary()) != MOCHA_RESULT_SUCCESS) {
            OSFatal("FATAL ERROR:\nCould not initialize Mocha.\n\nPlease make sure you are running on the latest version of Aroma");
        }

        if ((res = Mocha_MountFS("storage_mlc", nullptr, "/vol/storage_mlc01")) != MOCHA_RESULT_SUCCESS) {
            OSFatal("FATAL ERROR:\nCould not mount storage_mlc.\n\nPlease make sure you are running on the latest version of Aroma");
        }

        curl_global_init(CURL_GLOBAL_DEFAULT);

        ThemezerAPI::initialize(user_agent);

        SDL_Init(SDL_INIT_VIDEO | SDL_INIT_EVENTS | SDL_INIT_GAMECONTROLLER);
        IMG_Init(IMG_INIT_JPG | IMG_INIT_PNG | IMG_INIT_WEBP);
        Mix_Init(MIX_INIT_MP3 | MIX_INIT_OGG);

        Mix_OpenAudioDevice(48000, MIX_DEFAULT_FORMAT, 2, 4096, NULL, SDL_AUDIO_ALLOW_ANY_CHANGE);

        SDL_EventState(SDL_SYSWMEVENT, SDL_ENABLE);

        window = SDL_CreateWindow("Themiify", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, 1280, 720, SDL_WINDOW_SHOWN);
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);

        cout << "Hello world from Themiify!" << endl;

        initialize_imgui();

        Camera::initialize(renderer);
        Camera::open();

        QRCodePopup::initialize();

        PluginManager::initialize();
        ThemeManager::initialize();

        DownloadManager::initialize(user_agent);
        ImageLoader::initialize(renderer);
        NavBar::initialize(renderer);
        ContentPanel::initialize(renderer);

        // Register proc_ui callback for camera
        ProcUIRegisterCallback(PROCUI_CALLBACK_ACQUIRE, &procCallbackAcquire, nullptr, 1);

        // Start playing bg music, AFTER settings are initialized (through the ContentPanel).
        bgMusicData = load_file(THEMIIFY_ROOT / "bgm.mp3");
        if (bgMusicData.empty())
            bgMusicData = load_file(THEMIIFY_ROOT / "bgm.ogg");
        if (bgMusicData.empty())
            bgMusicData = load_file(THEMIIFY_ROOT / "bgm.opus");
        if (bgMusicData.empty())
            bgMusicData = load_file("fs:/vol/content/sound/bgm.mp3");
        if (!bgMusicData.empty()) {
            auto rwops = SDL_RWFromConstMem(bgMusicData.data(), bgMusicData.size());
            bgMusic = Mix_LoadMUS_RW(rwops, 1);
            if (bgMusic)
                Mix_PlayMusic(bgMusic, -1);
            else
                cerr << "Failed to load bgm: " << SDL_GetError() << endl;
        } else {
            cerr << "No bgm found!" << endl;
        }
    }

    void finalize() {

        Mix_PlayMusic(nullptr, 0);
        if (bgMusic) {
            Mix_FreeMusic(bgMusic);
            bgMusic = nullptr;
        }
        bgMusicData.clear();

        NavBar::finalize();
        ContentPanel::finalize();
        ThemeManager::finalize();
        PluginManager::finalize();
        ImageLoader::finalize();
        DownloadManager::finalize();

        QRCodePopup::finalize();

        Camera::close();
        Camera::shutdown();

        Mix_Quit();

        SDL_DestroyRenderer(renderer);
        SDL_DestroyWindow(window);

        SDL_Quit();

        ThemezerAPI::finalize();

        curl_global_cleanup();

        Mocha_UnmountFS("storage_mlc");
        Mocha_DeInitLibrary();
    }

    bool run() {
        isRunning = true;

        while (isRunning) {
            try {
                ThemezerAPI::process();
            }
            catch (std::exception& e) {
                cerr << "ERROR in ThemezerAPI::process(): " << e.what() << endl;
            }

            try {
                DownloadManager::process();
            }
            catch (std::exception& e) {
                cerr << "ERROR in DownloadManager::process(): " << e.what() << endl;
            }

            try {
                ImageLoader::process();
            }
            catch (std::exception& e) {
                cerr << "ERROR in ImageLoader::process(): " << e.what() << endl;
            }

            try {
                ThemeManager::process();
            }
            catch (std::exception& e) {
                cerr << "ERROR in ThemeManager::process(): " << e.what() << endl;
            }

            SDL_Event e;
            while(SDL_PollEvent(&e)) {
                ImGui_ImplSDL2_ProcessEvent(&e);
                switch (e.type) {
                    case SDL_QUIT: {
                        cout << "Quitting Themiify!" << endl;
                        isRunning = false;
                        break;
                    }
                    case SDL_CONTROLLERDEVICEADDED: {
                        SDL_GameController *controller = SDL_GameControllerOpen(e.cdevice.which);
                        if (controller) {
                            controllers.push_back(controller);
                        }
                        break;
                    }
                    case SDL_CONTROLLERDEVICEREMOVED: {
                        SDL_GameController *controller = SDL_GameControllerFromInstanceID(e.cdevice.which);
                        for (auto it = controllers.begin(); it != controllers.end(); ++it) {
                            if (*it == controller) {
                                SDL_GameControllerClose(*it);
                                controllers.erase(it);
                                break;
                            }
                        }
                        break;
                    }
                    default:
                        break;
                }
            }

            ImGui_ImplSDLRenderer2_NewFrame();
            ImGui_ImplSDL2_NewFrame();

            ImGui::NewFrame();

            {
                using namespace ImGui::RAII;
                auto viewport = ImGui::GetMainViewport();
                ImGui::SetNextWindowPos(viewport->Pos, ImGuiCond_Always);
                ImGui::SetNextWindowSize(viewport->Size, ImGuiCond_Always);
                // NOTE: on the toplevel window, we disable padding, rounding, border,
                // but we don't want this to propagate to the rest of the UI code.
                const auto &style = ImGui::GetStyle();
                auto orig_padding  = style.WindowPadding;
                auto orig_border   = style.WindowBorderSize;
                auto orig_rounding = style.WindowRounding;
                StyleVar no_padding{ImGuiStyleVar_WindowPadding, {0, 0}};
                StyleVar no_border{ImGuiStyleVar_WindowBorderSize, 0};
                StyleVar no_rounding{ImGuiStyleVar_WindowRounding, 0};
                if (Window main_window{"Themiify",
                                       nullptr,
                                       ImGuiWindowFlags_NoTitleBar |
                                       ImGuiWindowFlags_NoMove |
                                       ImGuiWindowFlags_NoSavedSettings |
                                       ImGuiWindowFlags_NoResize}) {
                    // Restore the window properties.
                    StyleVar restore_padding{ImGuiStyleVar_WindowPadding, orig_padding};
                    StyleVar restore_border{ImGuiStyleVar_WindowBorderSize, orig_border};
                    StyleVar restore_rounding{ImGuiStyleVar_WindowRounding, orig_rounding};
                    NavBar::process_ui();
                    ImGui::SameLine(0, 6); // NOTE: override ItemSpacing
                    {
#ifdef MEASURE_RENDER_TIME
                        TimerReporter slow_content{std::cout,
                                                   "ContentPanel::process_ui()",
                                                   33ms};
#endif
                        ContentPanel::process_ui(NavBar::get_current_tab());
                    }

                    ConfirmExitPopup::process_ui();
                }
            }

#ifdef ENABLE_STYLE_EDITOR
            ImGui::ShowStyleEditor();
#endif

            ImGui::EndFrame();

            ImGui::Render();

            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderClear(renderer);

            ImGui_ImplSDLRenderer2_RenderDrawData(ImGui::GetDrawData(), renderer);

            // Wii U clip fix
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 0);
            SDL_RenderDrawPoint(renderer, 0, 0);

            SDL_RenderPresent(renderer);
        }

        return isRunning;
    }

    void quit() {
        isRunning = false;
    }
}
