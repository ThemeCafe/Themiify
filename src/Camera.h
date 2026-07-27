/*
 * Themiify - A theme manager for the Nintendo Wii U
 * Copyright (C) 2026 Fangal-Airbag
 * Copyright (C) 2026 AlphaCraft9658
 * Copyright (C) 2026 Daniel K. O. <dkosmari>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <SDL.h>
#include <camera/camera.h>

namespace Camera {
    enum class FlipMode {
        none,
        horizontal,
        vertical,
        both
    };

    bool initialize(SDL_Renderer* renderer, FlipMode flipMode = FlipMode::horizontal);
    void shutdown();

    bool open();
    void close();

    void update_texture();

    SDL_Texture* get_texture();
    bool is_initialized();
    bool is_open();

    void cameraEventHandler(CAMEventData* eventData);

    const uint8_t* get_grayscale_buffer();
    int get_width();
    int get_height();
    int get_pitch();
}
