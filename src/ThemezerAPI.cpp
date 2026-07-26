/*
 * Themiify - A theme manager for the Nintendo Wii U
 * Copyright (C) 2026 Fangal-Airbag
 * Copyright (C) 2026 AlphaCraft9658
 * Copyright (C) 2026 Daniel K. O. <dkosmari>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <iostream>
#include <ranges>
#include <stdexcept>
#include <utility>

#include <glaze/json/generic.hpp>
#include <glaze/json/read.hpp>
#include <glaze/json/write.hpp>

#include "ThemezerAPI.h"
#include "graphql.h"
#include "tracer.hpp"

using std::cout;
using std::cerr;
using std::endl;
using namespace std::literals;

namespace glz {

    template<>
    struct meta<ThemezerAPI::ItemSort> {
        using enum ThemezerAPI::ItemSort;
        static constexpr auto value = enumerate(CREATED,
                                                DOWNLOADS,
                                                SAVES,
                                                UPDATED);
    };

    template<>
    struct meta<ThemezerAPI::SortOrder> {
        using enum ThemezerAPI::SortOrder;
        static constexpr auto value = enumerate(ASC, DESC);
    };

}

namespace ThemezerAPI {

    const std::string url = "https://api.themezer.net/graphql";

    bool busy;

    void initialize(const std::string& user_agent)
    {
        TRACE_FUNC;

        graphql::initialize(user_agent);
        busy = false;
    }

    void finalize()
    {
        TRACE_FUNC;

        graphql::finalize();
        busy = true;
    }

    bool is_busy()
    {
        return busy;
    }

    void process()
    {
        graphql::process();
    }

    namespace {

        void common_errors_handler(const graphql::generic& errors)
        {
            busy = false;

            auto msg = glz::write<glz::opts{.prettify = true}>(errors)
                             .value_or("error")
                             .c_str();
            cout << "ERROR: graphql returned errors:\n" << msg << endl;
        }

        void common_exception_handler(const std::exception& error)
        {
            busy = false;

            cerr << "ERROR: " << error.what() << endl;
        }

    } // namespace

    void wiiu::themes(const ThemesSpec& spec,
                      ThemesFunction callback)
    {
        TRACE_FUNC;

        if (busy) {
            cerr << "ERROR: ThemezerAPI is busy" << endl;
            return;
        }

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

        auto data_handler = [callback = std::move(callback)](const graphql::generic& data) mutable
        {
            busy = false;

            auto& wiiu_obj = data.at("wiiu");
            auto& themes_obj = wiiu_obj.at("themes");

            auto& nodes = themes_obj.at("nodes").get<graphql::generic::array_t>();
            WiiuThemeSmallVec themes(nodes.size());

            for (auto [theme, node] : std::views::zip(themes, nodes)) {
                if (auto error = glz::read_json(theme, node))
                    throw std::runtime_error{"glz::read_json() failed: "
                                             + glz::format_error(error, node)};
            }

            auto& pageInfo_obj = themes_obj.at("pageInfo");
            PageInfo pageInfo;
            if (auto error = glz::read_json(pageInfo, pageInfo_obj))
                throw std::runtime_error{"glz::read_json() failed: "
                                         + glz::format_error(error, pageInfo_obj)};

            if (callback)
                callback(themes, pageInfo);
        };

        busy = true;

        graphql::get_async(url,
                           query,
                           variables_json,
                           std::move(data_handler),
                           common_errors_handler,
                           common_exception_handler);
    }

    void wiiu::theme(const std::string& hexId,
                     ThemeFunction callback)
    {
        TRACE_FUNC;

        if (busy) {
            cerr << "ERROR: ThemezerAPI is busy" << endl;
            return;
        }

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

            busy = false;

            auto& wiiu_obj = data.at("wiiu");
            auto& theme_obj = wiiu_obj.at("theme");

            if (theme_obj.is_null())
                throw std::runtime_error{"no theme found"};

            auto result = glz::read_json<WiiuThemeFull>(theme_obj);
            if (!result)
                throw std::runtime_error{"glz::read_json() failed: "s
                                         + glz::format_error(result.error(), theme_obj)};

            if (callback)
                callback(*result);
        };

        busy = true;

        graphql::get_async(url,
                           query,
                           variables,
                           std::move(data_handler),
                           common_errors_handler,
                           common_exception_handler);
    }


    void
    wiiu::lookupByQuickId(const std::string& quickId,
                          LookupByQuickIdFunction callback)
    {
        TRACE_FUNC;
        if (busy) {
            cerr << "ERROR: ThemezerAPI is busy" << endl;
            return;
        }

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

                busy = false;

                auto& wiiu_obj = data.at("wiiu");
                auto& lookup_obj = wiiu_obj.at("lookupByQuickId");

                if (lookup_obj.is_null())
                    throw std::runtime_error{"no theme found"};

                auto result = glz::read_json<WiiuInstallThemeLookup>(lookup_obj);
                if (!result)
                    throw std::runtime_error{"glz::read_json() failed: "s
                                             + glz::format_error(result.error(), lookup_obj)};

                if (callback)
                    callback(*result);
            };

        busy = true;

        graphql::get_async(url,
                           query,
                           variables,
                           std::move(data_handler),
                           common_errors_handler,
                           common_exception_handler);
    }

} // namespace ThemezerAPI
