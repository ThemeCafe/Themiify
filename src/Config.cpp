/*
 * Themiify - A theme manager for the Nintendo Wii U
 * Copyright (C) 2026 Fangal-Airbag
 * Copyright (C) 2026 AlphaCraft9658
 * Copyright (C) 2026 Daniel K. O. <dkosmari>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <filesystem>
#include <iostream>

#include <glaze/exceptions/json_exceptions.hpp>
#include <glaze/json.hpp>

#include <SDL_mixer.h>

#include "Config.h"

#include "tracer.hpp"
#include "utils.h"

using std::cout;
using std::cerr;
using std::endl;
using namespace std::literals;

namespace Config {

    namespace {

        /*-----------*/
        /* Constants */
        /*-----------*/

        // Don't error out when unknown fields are found, to allow users to downgrade.
        constexpr glz::opts read_opts = { .error_on_unknown_keys = false };

        constexpr glz::opts write_opts = { .prettify = true };

        const std::filesystem::path settings_path = THEMIIFY_ROOT / "settings.json";

    } // namespace

    /*------------------*/
    /* Public variables */
    /*------------------*/

    Cfg cfg;

    /*-----------------------------*/
    /* Public function definitions */
    /*-----------------------------*/

    void
    initialize()
    {
        TRACE_FUNC;
        load();
    }

    void
    finalize()
    {
        TRACE_FUNC;

        save();
    }

    void
    load() {
        TRACE_FUNC;
        try {
            glz::ex::read_file_json<read_opts>(cfg,
                                               settings_path.string(),
                                               std::string{});
        }
        catch (std::exception& e) {
            cerr << "ERROR loading settings: " << e.what() << endl;
        }
    }

    void
    save() {
        TRACE_FUNC;
        try {
            create_directories(THEMIIFY_ROOT);
            glz::ex::write_file_json<write_opts>(cfg,
                                                 settings_path.string(),
                                                 std::string{});
        }
        catch (std::exception& e) {
            cerr << "ERROR saving settings: " << e.what() << endl;
        }
    }

} // namespace Config
