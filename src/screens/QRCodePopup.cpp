/*
 * Themiify - A theme manager for the Nintendo Wii U
 * Copyright (C) 2026 Fangal-Airbag
 * Copyright (C) 2026 AlphaCraft9658
 * Copyright (C) 2026 Daniel K. O. <dkosmari>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "QRCodePopup.h"

#include "../Camera.h"
#include "../tracer.hpp"
#include "../UI.h"
#include "DownloadThemePopup.h"

#include <iostream>
#include <cstring>

#include <SDL.h>

#include <imgui.h>
#include <imgui_stdlib.h>
#include <imgui_raii.h>

#include <quirc.h>

using std::cout;
using std::cerr;
using std::endl;
using namespace std::literals;

namespace QRCodePopup {

    namespace {

        /*-------*/
        /* Types */
        /*-------*/

        enum class State {
            hidden,
            queued,
            visible,
        };

        /*-----------*/
        /* Constants */
        /*-----------*/

        const std::string popup_id = "QRCodePopup"s;
        const unsigned scan_frame_every = 4;

        /*-----------*/
        /* Variables */
        /*-----------*/

        State state;

        quirc* qr = nullptr;
        unsigned scan_frame_counter;

        /*-----------------------*/
        /* Function declarations */
        /*-----------------------*/

        void
        scan_code();

        void
        start_scan();

        /*----------------------*/
        /* Function definitions */
        /*----------------------*/

        void
        scan_code() {
            if (!qr)
                return;

            const uint8_t* cam_img = Camera::get_grayscale_buffer();

            if (!cam_img)
                return;

            int w = 0;
            int h = 0;

            uint8_t* qr_img = quirc_begin(qr, &w, &h);

            if (!qr_img)
                return;

            for (int y = 0; y < Camera::get_height(); y++) {
                std::memcpy(
                    qr_img + y * w,
                    cam_img + y * Camera::get_pitch(),
                    Camera::get_width()
                );
            }

            quirc_end(qr);

            int count = quirc_count(qr);
            for (int i = 0; i < count; i++) {
                quirc_code code {};
                quirc_data data {};

                quirc_extract(qr, i, &code);

                auto err = quirc_decode(&code, &data);
                if (err == QUIRC_SUCCESS) {
                    std::string payload(reinterpret_cast<const char*>(data.payload),
                                        data.payload_len);

                    cout << "QR payload: \"" << payload << "\"" << endl;
                    UI::PlaySFXQRScan();

                    if (payload.starts_with("http://") ||
                        payload.starts_with("https://")) {
                        UI::CloseCurrentPopup(true);
                        DownloadThemePopup::open(payload);
                    } else {
                        // TODO: report error, only URLs can be scanned
                        cerr << "Only http and https urls are supported." << endl;
                    }
                    break;
                } else {
                    cout << "quirc: " << quirc_strerror(err) << '\n';
                }
            }
        }

        void
        start_scan() {
            TRACE_FUNC;

            if (qr)
                return;

            qr = quirc_new();

            if (!qr) {
                cerr << "quirc_new failed" << endl;
                return;
            }

            if (quirc_resize(qr, Camera::get_width(), Camera::get_height()) < 0) {
                cerr << "quirc_resize failed" << endl;
                quirc_destroy(qr);
                qr = nullptr;
            }
        }

    } // namespace

    /*------------------*/
    /* Public functions */
    /*------------------*/

    void
    initialize() {
        TRACE_FUNC;

        state = State::hidden;
    }

    void
    finalize() {
        TRACE_FUNC;
    }

    void
    open() {
        state = State::queued;
        start_scan();
        scan_frame_counter = 0;
    }

    void
    process_ui() {
        using namespace ImGui::RAII;

        if (state == State::hidden) {
            return;
        }

        if (state == State::queued) {
            UI::OpenPopup(popup_id);
            state = State::visible;
        }

        auto center = ImGui::GetMainViewport()->GetCenter();
        auto *viewport = ImGui::GetMainViewport();

        ImGui::SetNextWindowSize({viewport->Size.x * 0.85f, 0.0f}, ImGuiCond_Always);
        ImGui::SetNextWindowPos(center, ImGuiCond_Always, {0.5f, 0.5f});

        Popup popup{
            popup_id,
            ImGuiWindowFlags_NoSavedSettings |
            ImGuiWindowFlags_AlwaysAutoResize |
            ImGuiWindowFlags_NoMove
        };

        if (!popup) {
            // ImGui closed the popup
            UI::PlaySFXPopupClose();
            state = State::hidden;
            return;
        }

        switch (state) {
            case State::visible: {
                ImGui::Text("Scan Theme QR Code");
                ImGui::Separator();

                ImGui::Text(
                    "Place a Themezer QR Code in front of the camera and wait for the\n"
                    "code to be scanned automatically."
                );

                ImGui::Spacing();

                Camera::update_texture();

                // Don't scan every single frame if it feels too heavy.
                if (++scan_frame_counter > scan_frame_every) {
                    scan_frame_counter = 0;
                    scan_code();
                }

                SDL_Texture* camera_texture = Camera::get_texture();

                if (camera_texture) {
                    float image_height = 400;
                    ImVec2 image_size = {
                        image_height * 4 / 3,
                        image_height
                    };

                    float x = (ImGui::GetContentRegionAvail().x - image_size.x) / 2;
                    if (x > 0.0f)
                        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + x);

                    ImGui::Image((ImTextureID)camera_texture, image_size);
                }

                {
                    Font font{nullptr, 25};
                    ImGui::TextWrapped("Tip: If the QR code is too bright, lower the brightness of the device it's displayed on until it is clear " \
                                        "enough to be seen on the camera preview.");
                }

                break;
            }

            default:
                ;
        }
    }

} // namespace QRCodePopup
