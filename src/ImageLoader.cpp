/*
 * Themiify - A theme manager for the Nintendo Wii U
 * Copyright (C) 2026 Fangal-Airbag
 * Copyright (C) 2026 AlphaCraft9658
 * Copyright (C) 2026 Daniel K. O. <dkosmari>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <algorithm>
#include <atomic>
#include <cassert>
#include <chrono>
#include <cmath>
#include <concepts>
#include <filesystem>
#include <functional>
#include <iostream>
#include <memory>
#include <optional>
#include <print>
#include <queue>
#include <ranges>
#include <span>
#include <string>
#include <syncstream>
#include <thread>
#include <unordered_map>
#include <vector>

#include <curl/curl.h>
#include <SDL_image.h>
#include <imgui_stdlib.h>
#include <imgui_impl_sdl2.h>
#include <imgui_impl_sdlrenderer2.h>

#include "ImageLoader.h"

#include "App.h"
#include "async_queue.hpp"
#include "timer.hpp"
#include "tracer.hpp"

// TODO: downloads should be done in the foreground thread, in process().

using std::endl;
using namespace std::literals;

#define COUT (resources->sync_cout)
#define CERR (resources->sync_cerr)

#define TCOUT (resources->sync_tcout)
#define TCERR (resources->sync_tcerr)

// #define MEASURE_IMG_LOAD
// #define MEASURE_CREATE_TEXTURE
#define MEASURE_GET

namespace ImageLoader {

    struct Resources {
        std::osyncstream sync_cout{std::cout};
        std::osyncstream sync_cerr{std::cerr};

        std::osyncstream sync_tcout{std::cout};
        std::osyncstream sync_tcerr{std::cerr};

    };

    std::optional<Resources> resources;

    std::string user_agent = App::user_agent;

    const std::filesystem::path content_prefix = "fs:/vol/content";

    const std::size_t max_cache_size = 64;

    using Timestamp = std::uint64_t;
    Timestamp timestamp = 0;

    SDL_Renderer *renderer = nullptr;
    SDL_Texture *load_error_image = nullptr;
    SDL_Texture *loading_image = nullptr;

    CURLM *multi = nullptr;

    enum class LoadState : int {
        unloaded,
        requested,
        loading,
        loaded,
        converted,
        error,
    };

    std::string to_string(LoadState st);

    struct CacheEntry : std::enable_shared_from_this<CacheEntry> {
        std::atomic<LoadState> state{LoadState::requested};
        Timestamp last_use = 0;

        SDL_Surface *img = nullptr;
        SDL_Texture *tex = nullptr;

        std::string location;

        CURL *easy = nullptr;
        curl_slist *headers = nullptr;

        std::optional<std::vector<char>> raw_buf;

        CacheEntry(Timestamp now,
                   const std::string& location);

        ~CacheEntry() noexcept;

        void prepare_download();
        void cleanup_download();

        static std::shared_ptr<CacheEntry> get_entry(CURL* handle);
    }; // struct CacheEntry

    using CacheEntryPtr = std::shared_ptr<CacheEntry>;
    using Cache = std::unordered_map<std::string, CacheEntryPtr>;
    Cache cache;

    async_queue<CacheEntryPtr> requests_queue;
    std::jthread worker_thread;

    async_queue<CacheEntryPtr> convert_queue;

    CacheEntry::CacheEntry(Timestamp now,
                           const std::string& location) :
        last_use{now},
        location{location}
    {}

    CacheEntry::~CacheEntry()
        noexcept
    {
        if (multi && easy)
            curl_multi_remove_handle(multi, easy);

        if (headers) {
            curl_slist_free_all(headers);
            headers = nullptr;
        }

        if (easy) {
            curl_easy_cleanup(easy);
            easy = nullptr;
        }

        if (tex) {
            SDL_DestroyTexture(tex);
            tex = nullptr;
        }

        if (img) {
            SDL_FreeSurface(img);
            img = nullptr;
        }
    }

    static size_t write_cb(char* ptr, size_t size, size_t nmemb, void* userdata)
    {
        auto* entry = static_cast<CacheEntry*>(userdata);
        const std::size_t total = size * nmemb;

        if (!entry->raw_buf)
            entry->raw_buf.emplace();

        entry->raw_buf->insert(entry->raw_buf->end(), ptr, ptr + total);

        return total;
    }

    void
    CacheEntry::prepare_download()
    {
        easy = curl_easy_init();
        if (!easy)
            throw std::runtime_error{"curl_easy_init() failed"};

        curl_easy_setopt(easy, CURLOPT_PRIVATE, this);

        headers = curl_slist_append(headers, "Accept: image/*");
        curl_easy_setopt(easy, CURLOPT_VERBOSE, 0L);
        curl_easy_setopt(easy, CURLOPT_URL, location.data());
        curl_easy_setopt(easy, CURLOPT_HTTPHEADER, headers);
        curl_easy_setopt(easy, CURLOPT_SSL_VERIFYPEER, 1L);
        curl_easy_setopt(easy, CURLOPT_FOLLOWLOCATION, 1L);
        curl_easy_setopt(easy, CURLOPT_AUTOREFERER, 1L);
        curl_easy_setopt(easy, CURLOPT_ACCEPT_ENCODING, "");
        curl_easy_setopt(easy, CURLOPT_TRANSFER_ENCODING, 1L);
        curl_easy_setopt(easy, CURLOPT_BUFFERSIZE, 65536L);
        curl_easy_setopt(easy, CURLOPT_TCP_NODELAY, 0L);
        curl_easy_setopt(easy, CURLOPT_FAILONERROR, 1L);
        curl_easy_setopt(easy, CURLOPT_WRITEFUNCTION, write_cb);
        curl_easy_setopt(easy, CURLOPT_WRITEDATA, this);

        if (!user_agent.empty())
            curl_easy_setopt(easy, CURLOPT_USERAGENT, user_agent.data());

        curl_multi_add_handle(multi, easy);
    }

    void
    CacheEntry::cleanup_download()
    {
        curl_multi_remove_handle(multi, easy);

        if (headers) {
            curl_slist_free_all(headers);
            headers = nullptr;
        }

        if (easy) {
            curl_easy_cleanup(easy);
            easy = nullptr;
        }
        raw_buf.reset();
    }

    CacheEntryPtr
    CacheEntry::get_entry(CURL *handle)
    {
        if (!handle)
            return {};
        CacheEntry* self = nullptr;
        auto e = curl_easy_getinfo(handle, CURLINFO_PRIVATE, &self);
        if (e != CURLE_OK)
            return {};
        return self->shared_from_this();
    }

    void background_loader(std::stop_token token);

    void initialize(SDL_Renderer* rend)
    {
        resources.emplace();
        COUT.rdbuf()->set_emit_on_sync(true);
        CERR.rdbuf()->set_emit_on_sync(true);
        TCOUT.rdbuf()->set_emit_on_sync(true);
        TCERR.rdbuf()->set_emit_on_sync(true);

        curl_global_init(CURL_GLOBAL_DEFAULT);

        renderer = rend;

        loading_image = IMG_LoadTexture(
            renderer,
            "fs:/vol/content/ui/theme-placeholder-loading.png");

        load_error_image = IMG_LoadTexture(
            renderer,
            "fs:/vol/content/ui/load-error-image.png");

        if (loading_image)
            SDL_SetTextureBlendMode(loading_image, SDL_BLENDMODE_BLEND);

        if (load_error_image)
            SDL_SetTextureBlendMode(load_error_image, SDL_BLENDMODE_BLEND);

        requests_queue.reset();

        COUT << "ImageLoader: launching worker thread." << endl;
        worker_thread = std::jthread{background_loader};

        assert((std::atomic<LoadState>{}.is_lock_free()));
    }

    void finalize()
    {
        COUT << "Stopping convert_queue" << endl;
        convert_queue.stop();

        COUT << "Stopping requests_queue" << endl;
        requests_queue.stop();

        COUT << "Destroying thread" << endl;
        worker_thread = {};
        COUT << "Thread destroyed" << endl;

        convert_queue.clear();
        requests_queue.clear();

        COUT << "Clearing cache" << endl;
        cache.clear();

        COUT << "Destroying predefined icons" << endl;

        if (loading_image) {
            SDL_DestroyTexture(loading_image);
            loading_image = nullptr;
        }

        if (load_error_image) {
            SDL_DestroyTexture(load_error_image);
            load_error_image = nullptr;
        }

        curl_global_cleanup();

        resources.reset();
    }

    SDL_Texture* get(const std::string& location)
    {
#ifdef MEASURE_GET
        TimerReporter get_timer{COUT, "ImageLoader::get()", 5ms};
#endif

        CacheEntryPtr entry;
        auto it = cache.find(location);
        if (it != cache.end())
            entry = it->second;

        try {
            if (entry) {
                entry->last_use = timestamp;

                switch (entry->state.load()) {
                    case LoadState::converted:
                        return entry->tex;

                    case LoadState::error:
                        return load_error_image;

                    case LoadState::requested:
                    case LoadState::loading:
                    case LoadState::loaded:
                        return loading_image;

                    case LoadState::unloaded:
                        entry->state = LoadState::requested;
                        requests_queue.push(entry);
                        return loading_image;

                    default:
                        throw std::logic_error{
                            "invalid entry state: " + to_string(entry->state.load())
                        };
                }
            } else {
                // entry not found, so prepare a request
                entry = std::make_shared<CacheEntry>(timestamp, location);
                cache.emplace(location, entry);
                requests_queue.push(std::move(entry));
                return loading_image;
            }
        }
        catch (std::exception& e) {
            CERR << "ERROR: ImageLoader::get(): " << e.what() << endl;
            return load_error_image;
        }
    }

    /*
     * NOTE: GX2 only supports a few texture formats, so this allows converting the image
     * to a supported format early.
     */
    static SDL_Surface* optimized(SDL_Surface* input)
    {
        if (!input)
            return nullptr;
        switch (input->format->format) {
            // Supported formats, see SDL_render_wiiu.h from the SDL2 port.
            case SDL_PIXELFORMAT_ARGB4444:
            case SDL_PIXELFORMAT_RGBA4444:
            case SDL_PIXELFORMAT_ABGR4444:
            case SDL_PIXELFORMAT_BGRA4444:
            case SDL_PIXELFORMAT_ARGB1555:
            case SDL_PIXELFORMAT_ABGR1555:
            case SDL_PIXELFORMAT_RGBA5551:
            case SDL_PIXELFORMAT_BGRA5551:
            case SDL_PIXELFORMAT_RGB565:
            case SDL_PIXELFORMAT_BGR565:
            case SDL_PIXELFORMAT_RGBX8888:
            case SDL_PIXELFORMAT_RGBA8888:
            case SDL_PIXELFORMAT_ARGB8888:
            case SDL_PIXELFORMAT_BGRX8888:
            case SDL_PIXELFORMAT_BGRA8888:
            case SDL_PIXELFORMAT_ABGR8888:
            case SDL_PIXELFORMAT_ARGB2101010:
                return input;
            default:
                auto output = SDL_ConvertSurfaceFormat(input, SDL_PIXELFORMAT_RGBA32, 0);
                SDL_FreeSurface(input);
                return output;
        }
    }

    void process_one_request(CacheEntryPtr& entry)
    {
        if (!entry)
            return;
        LoadState expected = LoadState::requested;
        if (!entry->state.compare_exchange_strong(expected, LoadState::loading)) {
            TCERR << "ERROR: ImageLoader::process_one_request() wrong cache entry state: "
                  << to_string(expected)
                  << endl;
            return;
        }

        try {
            if (entry->location.starts_with("http://") ||
                entry->location.starts_with("https://")) {
                entry->prepare_download();
            }
            else {
                // Assume everything else is a local path.
                std::filesystem::path img_path;
                if (entry->location.starts_with("ui/"))
                    img_path = content_prefix / entry->location;
                else
                    img_path = entry->location;

#ifdef MEASURE_IMG_LOAD
                Timer timer{"IMG_Load()"};
                timer.start();
#endif
                entry->img = optimized(IMG_Load(img_path.c_str()));
#ifdef MEASURE_IMG_LOAD
                timer.stop();
                timer.print(TCOUT) << endl;
#endif
                if (!entry->img)
                    throw std::runtime_error{IMG_GetError()};

                entry->state = LoadState::loaded;
                convert_queue.push(entry);
            }
        }
        catch (std::exception& e) {
            std::println(TCERR,
                         "ERROR: ImageLoader::process_one_request(): location=\"{}\", exception={}",
                         entry->location,
                         e.what());

            entry->state = LoadState::error;
        }
    }

    template<std::ranges::random_access_range R,
             typename Comp = std::ranges::less,
             class Proj = std::identity>
    requires std::sortable<std::ranges::iterator_t<R>, Comp, Proj>
    void sift_down_heap(R&& r, Comp comp = {}, Proj proj = {})
    {
        if (std::empty(r))
            return;

        const auto size = std::size(r);
        std::ranges::range_size_t<R> cur_idx = 0;

        for (;;) {
            auto left_idx = 2u * cur_idx + 1u;

            if (left_idx >= size)
                break;

            auto next_idx = left_idx;
            auto right_idx = 2u * cur_idx + 2u;

            if (right_idx < size &&
                std::invoke(comp,
                            std::invoke(proj, r[left_idx]),
                            std::invoke(proj, r[right_idx]))) {
                next_idx = right_idx;
            }

            if (!std::invoke(comp,
                             std::invoke(proj, r[cur_idx]),
                             std::invoke(proj, r[next_idx]))) {
                break;
            }

            std::swap(r[cur_idx], r[next_idx]);
            cur_idx = next_idx;
        }
    }

    Timestamp& by_last_use(const Cache::iterator& it) noexcept
    {
        return it->second->last_use;
    }

    struct CompareLastUse {
        bool
        operator ()(const Cache::iterator& a, const Cache::iterator& b)
            const noexcept
        {
            return by_last_use(a) < by_last_use(b);
        }
    };

    void trim_cache()
    {
        if (cache.size() <= max_cache_size)
            return;

        std::size_t excess = cache.size() - max_cache_size;

#if 1
        std::vector<Cache::iterator> to_remove(excess);
        auto to_remove_end = to_remove.begin();

        for (auto it = cache.begin(); it != cache.end(); ++it) {
            if (to_remove_end != to_remove.end()) {
                *to_remove_end++ = it;
                std::ranges::push_heap(to_remove.begin(),
                                       to_remove_end,
                                       {},
                                       by_last_use);
            }
            else {
                if (by_last_use(it) < by_last_use(to_remove.front())) {
                    to_remove.front() = it;
                    sift_down_heap(to_remove, {}, by_last_use);
                }
            }
        }
        for (auto it : to_remove)
            cache.erase(it);
#else
        std::priority_queue<Cache::iterator,
                            std::vector<Cache::iterator>,
                            CompareLastUse> to_remove;
        for (auto it = cache.begin(); it != cache.end(); ++it) {
            to_remove.push(it);
            if (to_remove.size() > excess)
                to_remove.pop();
        }
        while (!to_remove.empty()) {
            cache.erase(to_remove.top());
            to_remove.pop();
        }
#endif
    }

    void process()
    {
        trim_cache();
        ++timestamp;

        // Convert exactly one surface into a texture.
        if (auto entry = convert_queue.try_pop()) {
            if ((*entry)->state == LoadState::loaded) {
#ifdef MEASURE_CREATE_TEXTURE
                Timer timer{"SDL_CreateTextureFromSurface()"};
                timer.start();
#endif
                (*entry)->tex = SDL_CreateTextureFromSurface(renderer, (*entry)->img);
#ifdef MEASURE_CREATE_TEXTURE
                timer.stop();
                timer.print(COUT) << endl;
#endif
                if ((*entry)->tex)
                    SDL_SetTextureBlendMode((*entry)->tex, SDL_BLENDMODE_BLEND);

                SDL_FreeSurface((*entry)->img);
                (*entry)->img = nullptr;
                (*entry)->state = LoadState::converted;
            }
        }
    }

    void handle_finished_downloads()
    {
        int msgs_left = 0;

        while (auto* msg = curl_multi_info_read(multi, &msgs_left)) {
            if (msg->msg != CURLMSG_DONE)
                continue;

            CURL* easy = msg->easy_handle;

            auto entry = CacheEntry::get_entry(easy);
            if (!entry) {
                TCERR << "ERROR: ImageLoader::handle_finished_downloads(): no entry" << endl;
                curl_multi_remove_handle(multi, easy);
                curl_easy_cleanup(easy);
                continue;
            }

            try {
                if (msg->data.result != CURLE_OK) {
                    throw std::runtime_error{
                        "[" + entry->location + "] " +
                        curl_easy_strerror(msg->data.result)
                    };
                }

                char* content_type = nullptr;
                curl_easy_getinfo(easy, CURLINFO_CONTENT_TYPE, &content_type);

                std::string ct = content_type ? content_type : "";
                if (!ct.starts_with("image/")) {
                    throw std::runtime_error{
                        "Content-Type should be image/* but got \"" + ct + "\""
                    };
                }

                if (!entry->raw_buf || entry->raw_buf->empty())
                    throw std::runtime_error{"empty download"};

                SDL_RWops* rw = SDL_RWFromConstMem(entry->raw_buf->data(),
                                                   static_cast<int>(entry->raw_buf->size()));
                if (!rw)
                    throw std::runtime_error{SDL_GetError()};

#ifdef MEASURE_IMG_LOAD
                Timer timer{"IMG_Load_RW()"};
                timer.start();
#endif
                entry->img = optimized(IMG_Load_RW(rw, 1));
#ifdef MEASURE_IMG_LOAD
                timer.stop();
                timer.print(TCOUT) << endl;
#endif
                if (!entry->img)
                    throw std::runtime_error{IMG_GetError()};

                entry->state = LoadState::loaded;
                convert_queue.push(entry);
            }
            catch (std::exception& e) {
                TCERR << "ERROR: ImageLoader::handle_finished_downloads(): " << e.what() << endl;
                entry->state = LoadState::error;
            }

            entry->cleanup_download();
        }
    }

    void background_loader(std::stop_token stopper)
    {
        try {
            multi = curl_multi_init();
            if (!multi)
                throw std::runtime_error{"curl_multi_init() failed"};

            curl_multi_setopt(multi, CURLMOPT_MAX_TOTAL_CONNECTIONS, 10L);
            curl_multi_setopt(multi, CURLMOPT_MAXCONNECTS, 10L);

            while (!stopper.stop_requested()) {
                auto entry = requests_queue.try_pop();

                if (entry) {
                    process_one_request(*entry);
                }
                else if (entry.error() == async_queue_error::stop) {
                    break;
                }
                else if (entry.error() == async_queue_error::locked) {
                    TCOUT << "WARNING: requests_queue was locked" << endl;
                }

                int running = 0;
                curl_multi_perform(multi, &running);

                handle_finished_downloads();

                std::this_thread::sleep_for(50ms);
            }
        }
        catch (std::exception& e) {
            TCERR << "ERROR: ImageLoader::worker_func(): " << e.what() << endl;
        }

        if (multi) {
            curl_multi_cleanup(multi);
            multi = nullptr;
        }
    }

    std::string to_string(LoadState st)
    {
        switch (st) {
            case LoadState::unloaded:
                return "unloaded";
            case LoadState::requested:
                return "requested";
            case LoadState::loading:
                return "loading";
            case LoadState::loaded:
                return "loaded";
            case LoadState::error:
                return "error";
            default:
                return "unknown (" + std::to_string(static_cast<int>(st)) + ")";
        }
    }
}
