/*
 * Themiify - A theme manager for the Nintendo Wii U
 * Copyright (C) 2026 Fangal-Airbag
 * Copyright (C) 2026 AlphaCraft9658
 * Copyright (C) 2026 Daniel K. O. <dkosmari>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include "../ThemezerAPI.h"

namespace DownloadThemePopup {

    void open(const ThemezerAPI::WiiuThemeSmall &theme_data);

    void open(const std::string& url);

    void open(const ThemezerAPI::WiiuInstallThemeLookup &theme);

    void process_ui();

} // namespace DownloadThemePopup
