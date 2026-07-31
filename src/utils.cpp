/*
 * Themiify - A theme manager for the Nintendo Wii U
 * Copyright (C) 2026 Fangal-Airbag
 * Copyright (C) 2026 AlphaCraft9658
 * Copyright (C) 2026 Daniel K. O. <dkosmari>
 *
 * SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <cctype>
#include <cstdio>
#include <fstream>
#include <iostream>
#include <locale>
#include <ranges>
#include <string_view>
#include <tuple>

#include "utils.h"

using std::cout;
using std::cerr;
using std::endl;

void DeletePath(const std::filesystem::path& target) {
    if (target.empty()) {
        cerr << "attempting to delete empty path!" << endl;
        return;
    }
    if (!exists(target))
        return;

#if 1
    if (is_directory(target)) {
        for (auto& entry : std::filesystem::directory_iterator(target)) {
            DeletePath(entry.path());
            cout << "Successfully deleted " << entry.path() << endl;
        }
    }

    cout << "Deleting " << target << "..." << endl;
    try {
        remove(target);
    }
    catch (const std::exception& e) {
        cerr << "Error deleting: " << e.what() << endl;
    }
#else
    // BUG: this should work, but it doesn't. Probably a bug in wut.
    try {
        remove_all(target);
    }
    catch (std::exception& e) {
        cerr << "Error deleting " << target << ": " << e.what() << endl;
    }
#endif
}

std::string
as_lower_case(const std::string& input)
{
    auto& c_locale = std::locale::classic();
    std::string output = input;
    for (char &c : output)
        c = std::tolower(c, c_locale);
    return output;
}

std::string
as_upper_case(const std::string& input)
{
    auto& c_locale = std::locale::classic();
    std::string output = input;
    for (char &c : output)
        c = std::toupper(c, c_locale);
    return output;
}

bool
IgnoreCaseLess::operator ()(const std::string& a,
                            const std::string& b)
    const noexcept
{
    auto& c_locale = std::locale::classic();
    for (auto [ca, cb] : std::views::zip(a, b)) {
        auto low_ca = std::tolower(ca, c_locale);
        auto low_cb = std::tolower(cb, c_locale);
        if (low_ca < low_cb)
            return true;
        if (low_ca > low_cb)
            return false;
    }
    if (a.size() < b.size())
        return true;
    return false;
}

bool
IgnoreCaseLess::operator ()(const std::filesystem::path& a,
                            const std::filesystem::path& b)
    const noexcept
{
    return operator()(a.string(), b.string());
}

// Used by the split() functions.
namespace {

    std::tuple<std::string::size_type,
               std::vector<std::string>::size_type>
    find_first_of(const std::string& haystack,
                  const std::vector<std::string>& needles,
                  std::string::size_type start = 0)
    {
        std::string::size_type result_pos = std::string::npos;
        std::vector<std::string>::size_type result_index = 0;
        for (std::vector<std::string>::size_type i = 0; i < needles.size(); ++i) {
            auto pos = haystack.find(needles[i], start);
            if (pos < result_pos) {
                result_pos = pos;
                result_index = i;
            }
        }
        return { result_pos, result_index };
    }

} // namespace

std::vector<std::string>
split(const std::string& input,
      const std::vector<std::string>& separators,
      bool compress,
      std::size_t max_tokens)
{
    std::vector<std::string> result;

    using size_type = std::string::size_type;
    auto [sep_start, sep_index] = find_first_of(input, separators);
    size_type tok_start = 0;

    // Loop until no more separators are found.
    while (sep_start != std::string::npos) {
        if (!compress || sep_start > tok_start) {
            // If this token reaches the maximum allowed, stop the looop
            if (max_tokens && result.size() + 1 == max_tokens)
                break;
            result.push_back(input.substr(tok_start, sep_start - tok_start));
        }
        tok_start = sep_start + separators[sep_index].size();
        if (tok_start >= input.size())
            break;
        std::tie(sep_start, sep_index) = find_first_of(input, separators, tok_start);
    }
    // The remainder of the string is the last token, unless (compress && empty)
    if (!compress || tok_start < input.size())
        result.push_back(input.substr(tok_start));
    return result;
}

std::vector<std::string>
split(const std::string& input,
      const std::string& separator,
      bool compress,
      std::size_t max_tokens)
{
    return split(input, std::vector{separator}, compress, max_tokens);
}

std::string
join(const std::vector<std::string>& tokens,
     const std::string& separator,
     bool compress)
{
    if (tokens.empty())
        return "";

    // Optimize allocation for result: calculate the final capacity.
    std::string::size_type size = 0;
    std::string result;

    if (compress) {

        // skip empty tokens, as if they don't exist
        std::string::size_type n = 0; // how many non-empty tokens
        for (const auto& token : tokens) {
            if (token.empty())
                continue;
            size += token.size();
            ++n;
        }
        size += (n - 1) * separator.size();

        result.reserve(size);
        for (const auto& token : tokens) {
            if (token.empty())
                continue;
            if (!result.empty())
                result += separator;
            result += token;
        }

    } else {

        // not compressing, will treat empty strings as valid tokens
        for (const auto& token : tokens)
            size += token.size();
        size += (tokens.size() - 1) * separator.size();

        result.reserve(size);
        for (const auto& token: tokens) {
            if (!result.empty())
                result += separator;
            result += token;
        }

    }

    return result;
}

std::vector<char>
load_file(const std::filesystem::path& filename) {
    std::vector<char> result;
    if (!exists(filename))
        return {};
    result.reserve(file_size(filename));
    std::filebuf file;
    if (file.open(filename,
                  std::ios::in | std::ios::binary)) {
        char buf[4096];
        std::streamsize read;
        while ((read = file.sgetn(buf, sizeof buf)) > 0)
            result.append_range(std::span(buf, read));
    }
    return result;
}
