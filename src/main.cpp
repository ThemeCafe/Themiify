/*
 * Themiify - A theme manager for the Nintendo Wii U
 * Copyright (C) 2026 Fangal-Airbag
 * Copyright (C) 2026 AlphaCraft9658
 * Copyright (C) 2026 Daniel K. O. <dkosmari>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include "App.h"

#include <wiiu-stdout.h>

int main()
{
#ifdef __WIIU__
    wiiu_init_stdout();
#endif

    App::initialize();
    App::run();
    App::finalize();

    return 0;
}
