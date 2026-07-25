/*
 * Themiify - A theme manager for the Nintendo Wii U
 * Copyright (C) 2026 Fangal-Airbag
 * Copyright (C) 2026 AlphaCraft9658
 * Copyright (C) 2026 Daniel K. O. <dkosmari>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <SDL2/SDL.h>
#include <string>

namespace ThemezerScreen {
    void initialize(SDL_Renderer *renderer);

    void finalize();

    void process_ui();

    void fetch_theme_by_quick_id(const std::string& quick_id);
}
