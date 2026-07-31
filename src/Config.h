/*
 * Themiify - A theme manager for the Nintendo Wii U
 * Copyright (C) 2026 Fangal-Airbag
 * Copyright (C) 2026 AlphaCraft9658
 * Copyright (C) 2026 Daniel K. O. <dkosmari>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

namespace Config {

    /*--------------*/
    /* Public types */
    /*--------------*/

    struct Cfg {
        bool check_integrity_at_boot = false;
        bool is_first_boot = true;
        int  music_volume = 75;
        int  sfx_volume = 75;
        bool check_themezer_updates_at_boot = false;
    }; // struct Cfg

    /*------------------*/
    /* Public variables */
    /*------------------*/

    extern Cfg cfg;

    /*------------------------------*/
    /* Public function declarations */
    /*------------------------------*/

    void
    initialize();

    void
    finalize();

    void
    load();

    void
    save();

} // namespace Config
