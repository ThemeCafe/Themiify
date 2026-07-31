/*
 * Themiify - A theme manager for the Nintendo Wii U
 * Copyright (C) 2026 Fangal-Airbag
 * Copyright (C) 2026 AlphaCraft9658
 * Copyright (C) 2026 Daniel K. O. <dkosmari>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <functional>
#include <memory>
#include <string>

#include <curl/curl.h>

#include <glaze/json/generic_fwd.hpp>

namespace graphql {

    using generic = glz::generic_u64;

    using data_function_sig = void (const generic& data);
    using data_function_t = std::move_only_function<data_function_sig>;

    using errors_function_sig = void (const generic& errors);
    using errors_function_t = std::move_only_function<errors_function_sig>;

    using exception_function_sig = void (const std::exception& error);
    using exception_function_t = std::move_only_function<exception_function_sig>;


    struct request;


    enum class status {
        invalid,
        pending,
        finished,
        canceled,
    };


    class token {

        std::shared_ptr<request> req;

    public:

        constexpr
        token()
            noexcept = default;

        explicit
        token(std::shared_ptr<request> req)
            noexcept;

        ~token()
            noexcept;


        status
        get_status()
            const noexcept;

        void
        cancel();

        void
        detach()
            noexcept;

        bool
        is_pending()
            const noexcept;

    }; // class token


    void
    initialize(const std::string& user_agent = "");

    void
    finalize();


    void
    process();


    token
    get_async(const std::string& url,
              const std::string& query,
              const generic& variables,
              data_function_t data_func,
              errors_function_t errors_func,
              exception_function_t exception_func = {});

    token
    get_async(const std::string& url,
              const std::string& query,
              const std::string& variables_json,
              data_function_t data_func,
              errors_function_t errors_func,
              exception_function_t exception_func = {});


    generic
    get_sync(const std::string& url,
             const std::string& query,
             const generic& variables);

    generic
    get_sync(const std::string& url,
             const std::string& query,
             const std::string& variables_json);

} // graphql
