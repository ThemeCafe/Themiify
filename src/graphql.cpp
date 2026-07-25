/*
 * Themiify - A theme manager for the Nintendo Wii U
 * Copyright (C) 2026 Fangal-Airbag
 * Copyright (C) 2026 AlphaCraft9658
 * Copyright (C) 2026 Daniel K. O. <dkosmari>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cassert>
#include <iostream>
#include <optional>
#include <span>
#include <map>
#include <stdexcept>
#include <memory>
#include <string>

#include <curl/curl.h>

#include <glaze/json.hpp>

#include "graphql.h"
#include "byte_stream.hpp"
#include "tracer.hpp"

using std::cout;
using std::cerr;
using std::endl;
using namespace std::literals;

// TODO: use glz::lazy_json instead of glz::generic.

namespace graphql {

    std::string user_agent;

    static size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata)
    {
        auto* stream = static_cast<byte_stream*>(userdata);
        std::span<const char> data(ptr, size * nmemb);
        return stream->write(data);
    }

    struct easy_handle {
        CURL* handle = nullptr;
        curl_slist* headers = nullptr;
        std::string post_data;

        easy_handle(const std::string& url)
        {
            handle = curl_easy_init();
            if (!handle)
                throw std::runtime_error{"curl_easy_init() failed"};

            headers = curl_slist_append(headers, "Accept: application/json");
            headers = curl_slist_append(headers, "Content-Type: application/json");

            curl_easy_setopt(handle, CURLOPT_VERBOSE, 1L);
            curl_easy_setopt(handle, CURLOPT_URL, url.c_str());
            curl_easy_setopt(handle, CURLOPT_HTTPHEADER, headers);
            curl_easy_setopt(handle, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(handle, CURLOPT_AUTOREFERER, 1L);
            curl_easy_setopt(handle, CURLOPT_SSL_VERIFYPEER, 1L);
            curl_easy_setopt(handle, CURLOPT_ACCEPT_ENCODING, "");
            curl_easy_setopt(handle, CURLOPT_TRANSFER_ENCODING, 1L);
            curl_easy_setopt(handle, CURLOPT_BUFFERSIZE, 65536L);
            curl_easy_setopt(handle, CURLOPT_TCP_NODELAY, 0L);
            curl_easy_setopt(handle, CURLOPT_FAILONERROR, 1L);

            if (!user_agent.empty())
                curl_easy_setopt(handle, CURLOPT_USERAGENT, user_agent.c_str());
        }

        ~easy_handle()
        {
            if (headers)
                curl_slist_free_all(headers);

            if (handle)
                curl_easy_cleanup(handle);
        }

        easy_handle(easy_handle&&) = delete;
        easy_handle& operator=(easy_handle&&) = delete;
    };

    struct request {
        status current_status = status::pending;
        easy_handle easy;
        data_function_t data_func;
        errors_function_t errors_func;
        exception_function_t exception_func;
        byte_stream response;

        request(request&&) = delete;

        request(const std::string& url,
                const std::string& query,
                const glz::generic& variables,
                data_function_t data_func,
                errors_function_t errors_func,
                exception_function_t exception_func)
            : easy{url},
              data_func{std::move(data_func)},
              errors_func{std::move(errors_func)},
              exception_func{std::move(exception_func)}
        {
            glz::generic post;
            post["query"] = query;

            if (!variables.empty())
                post["variables"] = variables;

            auto post_str = glz::write_json(post);
            if (!post_str)
                throw std::runtime_error{"glz::write_json() failed: "
                                         + glz::format_error(post_str.error())};

            easy.post_data = *post_str;

            curl_easy_setopt(easy.handle, CURLOPT_POST, 1L);
            curl_easy_setopt(easy.handle,
                             CURLOPT_POSTFIELDSIZE_LARGE,
                             curl_off_t(easy.post_data.size()));
            curl_easy_setopt(easy.handle,
                             CURLOPT_POSTFIELDS,
                             easy.post_data.c_str());

            curl_easy_setopt(easy.handle, CURLOPT_WRITEFUNCTION, write_cb);
            curl_easy_setopt(easy.handle, CURLOPT_WRITEDATA, &response);
        }

        CURL* get_id() const noexcept
        {
            return easy.handle;
        }

        void finish() noexcept
        try {
            current_status = status::finished;

            char* content_type = nullptr;
            curl_easy_getinfo(easy.handle, CURLINFO_CONTENT_TYPE, &content_type);

            std::string ct = content_type ? content_type : "";
            auto response_str = response.read_str();

            if (!ct.starts_with("application/json"))
                throw std::runtime_error{"Content-Type should be application/json, but got \""s
                                         + ct + "\"\ncontent:\n"s + response_str};

            glz::generic parsed;
            auto error = glz::read_json(parsed, response_str);

            if (error)
                throw std::runtime_error{"glz::read_json() failed: "
                                         + glz::format_error(error, response_str)};

            if (parsed.contains("data"))
                on_data(parsed.at("data"));

            if (parsed.contains("errors"))
                on_errors(parsed.at("errors"));
        }
        catch (std::exception& e) {
            on_exception(e);
        }

        void on_data(const glz::generic& data) noexcept
        try {
            if (data_func)
                data_func(data);
        }
        catch (std::exception& e) {
            cerr << "ERROR: request::on_data(): " << e.what() << endl;
        }
        catch (...) {
            cerr << "ERROR: request::on_data() caught an exception!" << endl;
        }

        void on_errors(const glz::generic& errors) noexcept
        try {
            if (errors_func)
                errors_func(errors);
        }
        catch (std::exception& e) {
            cerr << "ERROR: request::on_errors(): " << e.what() << endl;
        }
        catch (...) {
            cerr << "ERROR: request::on_errors() caught an exception!" << endl;
        }

        void on_exception(const std::exception& ex) noexcept
        try {
            if (exception_func)
                exception_func(ex);
        }
        catch (std::exception& e) {
            cerr << "ERROR: request::on_exception(): " << e.what() << endl;
        }
        catch (...) {
            cerr << "ERROR: request::on_exception() caught an exception" << endl;
        }
    };

    struct resources {
        CURLM* multi = nullptr;
        std::map<CURL*, std::shared_ptr<request>> requests;

        resources()
        {
            multi = curl_multi_init();
            if (!multi)
                throw std::runtime_error{"curl_multi_init() failed"};

            curl_multi_setopt(multi, CURLMOPT_MAX_TOTAL_CONNECTIONS, 5L);
            curl_multi_setopt(multi, CURLMOPT_MAXCONNECTS, 5L);
        }

        ~resources() noexcept
        {
            for (auto& [key, val] : requests) {
                if (val)
                    curl_multi_remove_handle(multi, val->easy.handle);
            }

            if (multi)
                curl_multi_cleanup(multi);
        }

        void add(std::shared_ptr<request> req)
        {
            assert(req);

            curl_multi_add_handle(multi, req->easy.handle);
            requests.emplace(req->get_id(), std::move(req));
        }

        void remove(std::shared_ptr<request>& req)
        {
            assert(req);

            curl_multi_remove_handle(multi, req->easy.handle);
            requests.erase(req->get_id());
        }

        void process()
        {
            int running = 0;
            curl_multi_perform(multi, &running);

            int msgs_left = 0;
            while (auto* msg = curl_multi_info_read(multi, &msgs_left)) {
                if (msg->msg != CURLMSG_DONE)
                    continue;

                CURL* id = msg->easy_handle;
                auto it = requests.find(id);

                if (it == requests.end()) {
                    cerr << "BUG: finished an unknown handle!" << endl;
                    continue;
                }

                auto req = std::move(it->second);
                remove(req);

                if (msg->data.result != CURLE_OK) {
                    req->on_exception(std::runtime_error{
                        curl_easy_strerror(msg->data.result)
                    });
                }
                else {
                    req->finish();
                }
            }
        }
    };

    std::optional<resources> res;

    token::token(std::shared_ptr<request> req) noexcept
        : req{std::move(req)}
    {}

    token::~token() noexcept
    {
        detach();
    }

    status token::get_status() const noexcept
    {
        if (!req)
            return status::invalid;

        return req->current_status;
    }

    void token::cancel()
    {
        if (req && req->current_status != status::finished) {
            req->current_status = status::canceled;
            res->remove(req);
        }
    }

    void token::detach() noexcept
    {
        req.reset();
    }

    bool token::is_pending() const noexcept
    {
        if (!req)
            return false;

        return req->current_status == status::pending;
    }

    void initialize(const std::string& user_agent_)
    {
        TRACE_FUNC;

        curl_global_init(CURL_GLOBAL_DEFAULT);

        user_agent = user_agent_;
        res.emplace();
    }

    void finalize()
    {
        TRACE_FUNC;

        res.reset();
        curl_global_cleanup();
    }

    void process()
    {
        assert(res);
        res->process();
    }

    token get_async(const std::string& url,
                    const std::string& query,
                    const glz::generic& variables,
                    data_function_t data_func,
                    errors_function_t errors_func,
                    exception_function_t exception_func)
    {
        auto req = std::make_shared<request>(
            url,
            query,
            variables,
            std::move(data_func),
            std::move(errors_func),
            std::move(exception_func)
        );

        res->add(req);

        return token{std::move(req)};
    }

    token get_async(const std::string& url,
                    const std::string& query,
                    const std::string& variables_json,
                    data_function_t data_func,
                    errors_function_t errors_func,
                    exception_function_t exception_func)
    {
        glz::generic variables;

        if (auto error = glz::read_json(variables, variables_json))
            throw std::runtime_error{"glz::read_json() failed: "
                                     + glz::format_error(error, variables_json)};

        return get_async(
            url,
            query,
            variables,
            std::move(data_func),
            std::move(errors_func),
            std::move(exception_func)
        );
    }

    glz::generic get_sync(const std::string& url,
                          const std::string& query,
                          const glz::generic& variables)
    {
        TRACE_FUNC;

        easy_handle easy{url};

        glz::generic post;
        post["query"] = query;

        if (!variables.empty())
            post["variables"] = variables;

        auto post_json = glz::write_json(post);
        if (!post_json)
            throw std::runtime_error{"glz::write_json() failed"};

        easy.post_data = *post_json;

        byte_stream response;

        cout << "post: " << easy.post_data << endl;
        curl_easy_setopt(easy.handle, CURLOPT_POST, 1L);
        curl_easy_setopt(easy.handle,
                         CURLOPT_POSTFIELDSIZE_LARGE,
                         curl_off_t(easy.post_data.size()));
        curl_easy_setopt(easy.handle,
                         CURLOPT_POSTFIELDS,
                         easy.post_data.c_str());
        curl_easy_setopt(easy.handle, CURLOPT_WRITEFUNCTION, write_cb);
        curl_easy_setopt(easy.handle, CURLOPT_WRITEDATA, &response);

        CURLcode code = curl_easy_perform(easy.handle);
        if (code != CURLE_OK)
            throw std::runtime_error{curl_easy_strerror(code)};

        char* content_type = nullptr;
        curl_easy_getinfo(easy.handle, CURLINFO_CONTENT_TYPE, &content_type);

        std::string ct = content_type ? content_type : "";
        if (!ct.starts_with("application/json"))
            throw std::runtime_error{"Content-Type should be application/json, but got \""s
                                     + ct + "\""s};

        auto response_str = response.read_str();

        glz::generic result;
        auto error = glz::read_json(result, response_str);

        if (error)
            throw std::runtime_error{"glz::read_json() failed: "
                                     + glz::format_error(error, response_str)};

        return result;
    }

    glz::generic get_sync(const std::string& url,
                          const std::string& query,
                          const std::string& variables_json)
    {
        glz::generic variables;

        if (auto error = glz::read_json(variables, variables_json))
            throw std::runtime_error{"glz::read_json() failed: "
                                     + glz::format_error(error, variables_json)};

        return get_sync(url, query, variables);
    }

}
