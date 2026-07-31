/*
 * Themiify - A theme manager for the Nintendo Wii U
 * Copyright (C) 2026 Fangal-Airbag
 * Copyright (C) 2026 AlphaCraft9658
 * Copyright (C) 2026 Daniel K. O. <dkosmari>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <future>
#include <iostream>
#include <queue>
#include <ranges>
#include <stdexcept>
#include <utility>

#include <glaze/json/generic.hpp>
#include <glaze/json/read.hpp>
#include <glaze/json/write.hpp>
#include <glaze/exceptions/json_exceptions.hpp>

#include "ThemezerAPI.h"
#include "graphql.h"
#include "tracer.hpp"

using std::cout;
using std::cerr;
using std::endl;
using namespace std::literals;

/*-----------------*/
/* Glaze glue code */
/*-----------------*/

template<>
struct glz::meta<ThemezerAPI::ItemSort> {
    using enum ThemezerAPI::ItemSort;
    static constexpr auto value = enumerate(
        COLOR_SIMILARITY,
        CREATED,
        DOWNLOADS,
        RISING,
        SAVES,
        TRENDING,
        UPDATED
    );
};

template<>
struct glz::meta<ThemezerAPI::SortOrder> {
    using enum ThemezerAPI::SortOrder;
    static constexpr auto value = enumerate(ASC, DESC);
};


namespace ThemezerAPI {

    namespace {

        /*-------*/
        /* Types */
        /*-------*/

        using Task = std::future<void>;

        /*-----------*/
        /* Constants */
        /*-----------*/

        const std::string url = "https://api.themezer.net/graphql";

        /*-----------*/
        /* Variables */
        /*-----------*/

        bool api_call_in_progress;

        std::queue<Task> pending_tasks;

        /*-----------------------*/
        /* Function declarations */
        /*-----------------------*/

        template<typename F,
                 typename... Args>
        void
        add_task(F&& func,
                 Args&&... args);

        void
        common_errors_handler(const graphql::generic& errors);

        void
        common_exception_handler(const std::exception& error);

        void
        dispatch_one_task()
            noexcept;

        void
        finish_api_call();

        void
        real_checkUpdates(const wiiu::CheckUpdatesSpec& spec,
                          wiiu::CheckUpdatesResponseFunction callback);

        void
        real_lookupByQuickId(const std::string& quickId,
                             wiiu::LookupByQuickIdResponseFunction callback);

        void
        real_theme(const std::string& hexId,
                   wiiu::ThemeResponseFunction callback);

        void
        real_themes(const wiiu::ThemesSpec& spec,
                    wiiu::ThemesResponseFunction callback);

        void
        start_api_call();

        /*----------------------*/
        /* Function definitions */
        /*----------------------*/

        template<typename F,
                 typename... Args>
        void
        add_task(F&& func,
                 Args&&... args)
        {
            pending_tasks.push(std::async(std::launch::deferred,
                                          std::forward<F>(func),
                                          std::forward<Args>(args)...));
        }

        void
        common_errors_handler(const graphql::generic& errors)
        {
            finish_api_call();

            auto msg = glz::write<glz::opts{.prettify = true}>(errors)
                             .value_or("error")
                             .c_str();
            cout << "ERROR: graphql returned errors:\n" << msg << endl;
        }

        void
        common_exception_handler(const std::exception& error)
        {
            finish_api_call();
            cerr << "ERROR: " << error.what() << endl;
        }

        void
        dispatch_one_task()
            noexcept
        {
            if (api_call_in_progress)
                return;

            if (!pending_tasks.empty()) {
                try {
                    pending_tasks.front().get();
                }
                catch (std::exception& e) {
                    cerr << "ERROR in task: " << e.what() << endl;
                }
                pending_tasks.pop();
            }
        }

        void
        finish_api_call()
        {
            api_call_in_progress = false;
        }

        void
        real_checkUpdates(const wiiu::CheckUpdatesSpec& spec,
                          wiiu::CheckUpdatesResponseFunction callback)
        {
            TRACE_FUNC;

            static const std::string query = R"(
query CheckUpdates($items: [WiiuCheckUpdatesItemInput!]!) {
  wiiu {
    checkUpdates(items: $items) {
      hexId
    }
  }
}
)";

            std::string variables_json;
            if (auto error = glz::write_json(spec, variables_json))
                throw std::runtime_error{"glz::write_json() failed: "
                                         + glz::format_error(error)};

            auto data_handler = [callback = std::move(callback)](const graphql::generic& data)
                mutable
            {
                finish_api_call();
                auto& gen_wiiu = data.at("wiiu");
                auto& gen_checkUpdates = gen_wiiu.at("checkUpdates")
                    .get<graphql::generic::array_t>();

                WiiuBaseVec themes;
                themes.reserve(gen_checkUpdates.size());

                for (auto& item : gen_checkUpdates)
                    themes.push_back(glz::ex::read_json<WiiuBase>(item));

                if (callback)
                    callback(themes);
            };

            start_api_call();
            graphql::get_async(url,
                               query,
                               variables_json,
                               std::move(data_handler),
                               common_errors_handler,
                               common_exception_handler);
        }

        void
        real_lookupByQuickId(const std::string& quickId,
                             wiiu::LookupByQuickIdResponseFunction callback)
        {
            TRACE_FUNC;

            static const std::string query = R"(
query LookupByQuickId($quickId: String!) {
  wiiu {
    lookupByQuickId(quickId: $quickId) {
      downloadUrl
      createdAt
      name
      quickId
      uuid
    }
  }
}
)";

            graphql::generic variables;
            variables["quickId"] = quickId;

            auto data_handler =
                [callback = std::move(callback)](const graphql::generic& data)
                mutable
                {
                    TRACE_FUNC;

                    finish_api_call();

                    auto& gen_wiiu = data.at("wiiu");
                    auto& gen_lookupByQuickId = gen_wiiu.at("lookupByQuickId");

                    if (gen_lookupByQuickId.is_null())
                        throw std::runtime_error{"no theme found"};

                    auto result = glz::ex::read_json<WiiuInstallThemeLookup>(gen_lookupByQuickId);

                    if (callback)
                        callback(result);
                };

            start_api_call();
            graphql::get_async(url,
                               query,
                               variables,
                               std::move(data_handler),
                               common_errors_handler,
                               common_exception_handler);
        }

        void
        real_theme(const std::string& hexId,
                   wiiu::ThemeResponseFunction callback)
        {
            TRACE_FUNC;

            static const std::string query = R"(
query Theme($hexId: String!) {
  wiiu {
    theme(hexId: $hexId) {
      uuid
      hexId
      quickId
      slug
      name
      createdAt
      updatedAt
      creator {
        username
        avatarUrl
      }
      bgmPreviewUrl
      collagePreview {
        tinyUrl
        thumbUrl
        sdUrl
        hdUrl
      }
      launcherScreenshot {
        tinyUrl
        thumbUrl
        sdUrl
        hdUrl
      }
      waraWaraPlazaScreenshot {
        tinyUrl
        thumbUrl
        sdUrl
        hdUrl
      }
      downloadCount
      downloadUrl
      tags {
        name
      }
      description
    }
  }
}
)";

            graphql::generic variables;
            variables["hexId"] = hexId;

            auto data_handler = [callback = std::move(callback)](const graphql::generic& data) mutable
            {
                TRACE_FUNC;

                finish_api_call();

                auto& gen_wiiu = data.at("wiiu");
                auto& gen_theme = gen_wiiu.at("theme");

                if (gen_theme.is_null())
                    throw std::runtime_error{"no theme found"};

                auto result = glz::ex::read_json<WiiuThemeFull>(gen_theme);

                if (callback)
                    callback(result);
            };

            start_api_call();
            graphql::get_async(url,
                               query,
                               variables,
                               std::move(data_handler),
                               common_errors_handler,
                               common_exception_handler);
        }

        void
        real_themes(const wiiu::ThemesSpec& spec,
                    wiiu::ThemesResponseFunction callback)
        {
            TRACE_FUNC;

            static const std::string query = R"(
query Themes($order: SortOrder, $paginationArgs: PaginationInput, $query: String, $sort: ItemSort) {
  wiiu {
    themes(order: $order, paginationArgs: $paginationArgs, query: $query, sort: $sort) {
      pageInfo {
        itemCount
        limit
        page
        pageCount
      }
      nodes {
        uuid
        hexId
        name
        updatedAt
        slug
        creator {
          username
        }
        collagePreview {
          tinyUrl
          thumbUrl
        }
        downloadCount
        downloadUrl
      }
    }
  }
}
)";

            std::string variables_json;
            if (auto error = glz::write_json(spec, variables_json))
                throw std::runtime_error{"glz::write_json() failed: "
                                         + glz::format_error(error)};

            auto data_handler = [callback = std::move(callback)](const graphql::generic& data)
                mutable
            {
                finish_api_call();

                auto& gen_wiiu = data.at("wiiu");
                auto& gen_themes = gen_wiiu.at("themes");
                auto& gen_nodes = gen_themes.at("nodes").get<graphql::generic::array_t>();
                auto& gen_pageInfo = gen_themes.at("pageInfo");

                WiiuThemeSmallVec themes;
                themes.reserve(gen_nodes.size());
                for (auto node : gen_nodes)
                    themes.push_back(glz::ex::read_json<WiiuThemeSmall>(node));

                auto pageInfo = glz::ex::read_json<PageInfo>(gen_pageInfo);

                if (callback)
                    callback(themes, pageInfo);
            };

            start_api_call();

            graphql::get_async(url,
                               query,
                               variables_json,
                               std::move(data_handler),
                               common_errors_handler,
                               common_exception_handler);
        }

        void
        start_api_call()
        {
            if (api_call_in_progress)
                throw std::logic_error{"BUG: should never start an API call while busy!"};
            api_call_in_progress = true;
        }

    } // namespace

    /*------------------*/
    /* Public functions */
    /*------------------*/

    void
    initialize(const std::string& user_agent)
    {
        TRACE_FUNC;

        graphql::initialize(user_agent);

        api_call_in_progress = false;
    }

    void
    finalize()
    {
        TRACE_FUNC;

        graphql::finalize();
        api_call_in_progress = false;
    }

    bool
    is_busy()
    {
        return api_call_in_progress || !pending_tasks.empty();
    }

    void
    process()
    {
        graphql::process();
        dispatch_one_task();
    }

    void
    wiiu::themes(const ThemesSpec& spec,
                      ThemesResponseFunction callback)
    {
        add_task(real_themes, spec, std::move(callback));
    }

    void
    wiiu::theme(const std::string& hexId,
                     ThemeResponseFunction callback)
    {
        add_task(real_theme, hexId, std::move(callback));
    }


    void
    wiiu::lookupByQuickId(const std::string& quickId,
                          LookupByQuickIdResponseFunction callback)
    {
        add_task(real_lookupByQuickId, quickId, std::move(callback));
    }


    void
    wiiu::checkUpdates(const CheckUpdatesSpec& spec,
                       CheckUpdatesResponseFunction callback)
    {
        add_task(real_checkUpdates, spec, std::move(callback));
    }

} // namespace ThemezerAPI
