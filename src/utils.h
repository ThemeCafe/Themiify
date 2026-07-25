/*
 * Themiify - A theme manager for the Nintendo Wii U
 * Copyright (C) 2026 Fangal-Airbag
 * Copyright (C) 2026 AlphaCraft9658
 * Copyright (C) 2026 Daniel K. O. <dkosmari>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#pragma once

#include <cstdint>
#include <string>
#include <optional>
#include <filesystem>
#include <vector>

#include <imgui.h>

#ifndef THEMIIFY_VERSION
#define THEMIIFY_VERSION "?.?"
#endif


inline const std::filesystem::path SD_ROOT = "fs:/vol/external01";

inline const std::filesystem::path THEMIIFY_ROOT = SD_ROOT / "themiify";
inline const std::filesystem::path THEMIIFY_INSTALLED_THEMES = THEMIIFY_ROOT / "installed";
inline const std::filesystem::path THEMIIFY_THUMBNAILS = THEMIIFY_ROOT / "cache/thumbnails";

inline const std::filesystem::path MEN_PATH = "Common/Package/Men.pack";
inline const std::filesystem::path MEN2_PATH = "Common/Package/Men2.pack";
inline const std::filesystem::path CAFE_BARISTA_MEN_PATH = "Common/Sound/Men/cafe_barista_men.bfsar";

inline const std::uint64_t WII_U_MENU_JPN_TID = 0x00050010'10040000;
inline const std::uint64_t WII_U_MENU_USA_TID = 0x00050010'10040100;
inline const std::uint64_t WII_U_MENU_EUR_TID = 0x00050010'10040200;

void DeletePath(const std::filesystem::path& target);

std::string
as_lower_case(const std::string& input);

std::string
as_upper_case(const std::string& input);

struct IgnoreCaseLess {

    bool
    operator ()(const std::string& a,
                const std::string& b)
        const noexcept;

    bool
    operator ()(const std::filesystem::path& a,
                const std::filesystem::path& b)
        const noexcept;

}; // struct IgnoreCaseLess

[[nodiscard]]
std::vector<std::string>
split(const std::string& input,
      const std::vector<std::string>& separators = {","},
      bool compress = true,
      std::size_t max_tokens = 0);

[[nodiscard]]
std::vector<std::string>
split(const std::string& input,
      const std::string& separator = ",",
      bool compress = true,
      std::size_t max_tokens = 0);

[[nodiscard]]
std::string
join(const std::vector<std::string>& tokens,
     const std::string& separator = "",
     bool compress = true);
