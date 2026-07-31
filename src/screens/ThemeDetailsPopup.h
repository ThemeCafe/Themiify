/*
 * Themiify - A theme manager for the Nintendo Wii U
 * Copyright (C) 2026 Fangal-Airbag
 * Copyright (C) 2026 AlphaCraft9658
 * Copyright (C) 2026 Daniel K. O. <dkosmari>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <filesystem>
#include <string>
#include <SDL.h>
#include "../ThemeManager.h"
#include "../ThemezerAPI.h"

namespace ThemeDetailsPopup {
    void open_themezer(const ThemezerAPI::WiiuThemeSmall &small_theme);

    void open_local(ThemeManager::ConstThemePtr theme);

    void process_ui();
}
