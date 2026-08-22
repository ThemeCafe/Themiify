/*
 * Themiify - A theme manager for the Nintendo Wii U
 * Copyright (C) 2026 Fangal-Airbag
 * Copyright (C) 2026 AlphaCraft9658
 * Copyright (C) 2026 Daniel K. O. <dkosmari>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <cassert>
#include <cstddef>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <list>
#include <memory>
#include <optional>
#include <ranges>
#include <stdexcept>
#include <string>
#include <vector>

#include <curl/curl.h>

#include "DownloadManager.h"
#include "screens/DownloadThemePopup.h"
#include "utils.h"
#include "tracer.hpp"

using std::cout;
using std::cerr;
using std::endl;
using namespace std::literals;

namespace DownloadManager {

    std::string user_agent;

    struct Download {
        std::shared_ptr<Info> info;

        success_function_t success_func;
        failure_function_t failure_func;

        CURL* easy = nullptr;

        std::filebuf file;

        std::array<char, CURL_ERROR_SIZE> error_buffer;

        bool content_started = false;

        bool done = false;

        Download(Download&&) = delete;

        Download(std::shared_ptr<Info> info_,
                 success_function_t success_func_,
                 failure_function_t failure_func_)
            : info{std::move(info_)},
              success_func{std::move(success_func_)},
              failure_func{std::move(failure_func_)}
        {
            create_directories(info->filename.parent_path());

            if (!file.open(info->filename, std::ios::out | std::ios::binary | std::ios::trunc))
                throw std::runtime_error{"could not open "s + info->filename.string()};

            error_buffer[0] = '\0';
            setup_easy();
        }

        ~Download()
        {
            if (easy)
                curl_easy_cleanup(easy);
        }

        void setup_easy()
        {
            easy = curl_easy_init();
            if (!easy)
                throw std::runtime_error{"curl_easy_init() failed"};

            if (!user_agent.empty())
                curl_easy_setopt(easy, CURLOPT_USERAGENT, user_agent.c_str());

            curl_easy_setopt(easy, CURLOPT_VERBOSE, 1L);
            curl_easy_setopt(easy, CURLOPT_URL, info->url.c_str());
            curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 1L);
            curl_easy_setopt(easy, CURLOPT_AUTOREFERER, 1L);
            curl_easy_setopt(easy, CURLOPT_SSL_VERIFYPEER, 1L);
            curl_easy_setopt(easy, CURLOPT_ACCEPT_ENCODING, "");
            curl_easy_setopt(easy, CURLOPT_TRANSFER_ENCODING, 1L);
            curl_easy_setopt(easy, CURLOPT_BUFFERSIZE, 65536L);
            curl_easy_setopt(easy, CURLOPT_TCP_NODELAY, 0L);
            curl_easy_setopt(easy, CURLOPT_FAILONERROR, 1L);
            curl_easy_setopt(easy, CURLOPT_ERRORBUFFER, error_buffer.data());

            curl_easy_setopt(easy, CURLOPT_WRITEDATA, this);
            curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, write_callback);

            curl_easy_setopt(easy, CURLOPT_NOPROGRESS, 0L);
            curl_easy_setopt(easy, CURLOPT_XFERINFODATA, this);
            curl_easy_setopt(easy, CURLOPT_XFERINFOFUNCTION, progress_callback);
        }

        static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata)
        {
            auto* self = static_cast<Download*>(userdata);
            const size_t total = size * nmemb;

            self->content_started = true;

            return self->file.sputn(ptr, total);
        }

         static int progress_callback(void* userdata,
                                     curl_off_t dltotal,
                                     curl_off_t dlnow,
                                     curl_off_t,
                                     curl_off_t)
        {
            auto* self = static_cast<Download*>(userdata);

            if (self->content_started && dltotal)
                self->info->progress = float(dlnow) / float(dltotal);

            curl_off_t speed = 0;

            if (self->easy)
                curl_easy_getinfo(self->easy, CURLINFO_SPEED_DOWNLOAD_T, &speed);

            self->info->speed = static_cast<std::uint64_t>(speed);

            return 0;
        }

        bool owns(CURL* easy_) const
        {
            return easy_ == easy;
        }

        void mark_done()
        {
            done = true;
        }

        bool is_done() const
        {
            return done;
        }

        void finish()
        {
            file.close();
            if (success_func)
                success_func(*info);
        }

        void finish(const std::exception& e) noexcept
        try {
            file.close();
            if (failure_func)
                failure_func(e);
        }
        catch (...) {
        }
    };

    struct Resources {
        CURLM* multi = nullptr;

        std::vector<std::shared_ptr<const Info>> infos;
        std::list<Download> downloads;

        Resources()
        {
            TRACE_FUNC;

            multi = curl_multi_init();
            if (!multi)
                throw std::runtime_error{"curl_multi_init() failed"};

            curl_multi_setopt(multi, CURLMOPT_MAXCONNECTS, 5L);
            curl_multi_setopt(multi, CURLMOPT_MAX_TOTAL_CONNECTIONS, 5L);
            curl_multi_setopt(multi, CURLMOPT_MAX_HOST_CONNECTIONS, 1L);
        }

        ~Resources() noexcept
        {
            for (auto& d : downloads)
                if (d.easy)
                    curl_multi_remove_handle(multi, d.easy);

            if (multi) {
                curl_multi_cleanup(multi);
                multi = nullptr;
            }
        }

        void process()
        {
            int running = 0;
            curl_multi_perform(multi, &running);

            int msgs_left = 0;
            while (auto* msg = curl_multi_info_read(multi, &msgs_left)) {
                if (msg->msg != CURLMSG_DONE)
                    continue;

                // Find the Download that corresponds to the handle
                auto completed = std::ranges::find_if(
                    downloads,
                    [msg](const Download& d) {
                        return d.owns(msg->easy_handle);
                    }
                );

                try {
                    if (completed == downloads.end())
                        throw std::logic_error{"BUG: transfer not found"};

                    if (msg->data.result != CURLE_OK) {
                        std::string url = completed->info->url;
                        throw std::runtime_error{
                            curl_easy_strerror(msg->data.result) + " ["s + url + "]"s
                        };
                    }

                    completed->mark_done();
                    curl_multi_remove_handle(multi, completed->easy);
                    completed->finish();
                    downloads.erase(completed);
                }
                catch (std::exception& e) {
                    cerr << "DownloadManager::Resources::process(): ERROR: "
                         << e.what()
                         << endl;

                    if (completed != downloads.end())
                        completed->finish(e);

                    if (completed != downloads.end()) {
                        if (completed->easy)
                            curl_multi_remove_handle(multi, completed->easy);

                        downloads.erase(completed);
                    }
                }
            }
        }

        const std::vector<std::shared_ptr<const Info>>& get_infos() const
        {
            return infos;
        }

        bool add(const std::string& url,
                 const std::filesystem::path& filename,
                 success_function_t success_func,
                 failure_function_t failure_func)
        {
            for (auto& entry : infos) {
                if (entry->url == url)
                    return false;
            }
            auto info = std::make_shared<Info>(
                url,
                filename,
                0.0f,
                0,
                State::queued
            );

            infos.push_back(info);

            downloads.emplace_back(
                std::move(info),
                std::move(success_func),
                std::move(failure_func)
            );

            auto& download = downloads.back();

            curl_multi_add_handle(multi, download.easy);

            cout << "Added download:"
                 << "\n    " << url
                 << "\n    " << filename
                 << endl;

            return true;
        }
    };

    std::optional<Resources> res;

    void initialize(const std::string& new_user_agent)
    {
        TRACE_FUNC;

        curl_global_init(CURL_GLOBAL_DEFAULT);

        user_agent = new_user_agent;

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
        if (res)
            res->process();
    }

    void pause_all()
    {
        TRACE_FUNC;
    }

    void resume_all()
    {
        TRACE_FUNC;
    }

    void cancel_all()
    {
        TRACE_FUNC;
    }

    void clear_all()
    {
        res->infos.clear();
    }

    bool add(const std::string& url,
             const std::filesystem::path& filename,
             success_function_t success_func,
             failure_function_t failure_func)
    {
        TRACE_FUNC;
        assert(res);

        return res->add(url,
                        filename,
                        std::move(success_func),
                        std::move(failure_func));
    }

    void pause(const std::string& url)
    {
        TRACE_FUNC;
        assert(res);

        cout << "Pausing " << url << endl;
        // TODO
    }

    void cancel(const std::string& url)
    {
        TRACE_FUNC;
        assert(res);

        cout << "Canceling " << url << endl;
        // TODO
    }

    std::shared_ptr<const Info>
    get_info(const std::string& url)
    {
        assert(res);
        for (auto& info : res->infos)
            if (info->url == url)
                return info;
        return {};
    }

}
