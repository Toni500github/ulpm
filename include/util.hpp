/*
 * Copyright 2025 Toni500git
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
 * following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following
 * disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
 * following disclaimer in the documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote
 * products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#pragma once

#include <rapidjson/document.h>

#include <cstdlib>
#include <iostream>
#include <unordered_map>
#include <vector>

#include "fmt/base.h"
#include "fmt/format.h"

#define UNKNOWN "(unknown)"

/* Write error message and exit if EOF (or CTRL-D most of the time)
 * @param cin The std::cin used for getting the input
 */
void ctrl_d_handler(const std::istream& cin);

#if DEBUG
inline bool debug_print = true;
#else
inline bool debug_print = false;
#endif

template <typename... Args>
void error(const std::string_view fmt, Args&&... args) noexcept
{
    fmt::print(stderr, "ulpm: \033[1;31mERROR: {}\033[0m\n",
               fmt::format(fmt::runtime(fmt), std::forward<Args>(args)...));
}

template <typename... Args>
void die(const std::string_view fmt, Args&&... args) noexcept
{
    fmt::print(stderr, "ulpm: \033[1;31mFATAL: {}\033[0m\n",
               fmt::format(fmt::runtime(fmt), std::forward<Args>(args)...));
    std::exit(1);
}

template <typename... Args>
void debug(const std::string_view fmt, Args&&... args) noexcept
{
    if (debug_print)
        fmt::print("\033[1;35m[DEBUG]:\033[0m {}\n", fmt::format(fmt::runtime(fmt), std::forward<Args>(args)...));
}

template <typename... Args>
void warn(const std::string_view fmt, Args&&... args) noexcept
{
    fmt::print(stderr, "\033[1;33m==> {}\033[0m\n", fmt::format(fmt::runtime(fmt), std::forward<Args>(args)...));
}

template <typename... Args>
void info(const std::string_view fmt, Args&&... args) noexcept
{
    fmt::print("\033[1;36m==> {}\033[0m\n", fmt::format(fmt::runtime(fmt), std::forward<Args>(args)...));
}

template <typename... Args>
void warn_stat(const std::string_view fmt, Args&&... args) noexcept
{
    fmt::print(stderr, "ulpm: \033[1;33mWARNING: {}\033[0m\n",
               fmt::format(fmt::runtime(fmt), std::forward<Args>(args)...));
}

template <typename... Args>
void info_stat(const std::string_view fmt, Args&&... args) noexcept
{
    fmt::print("ulpm: \033[1;36mINFO: {}\033[0m\n", fmt::format(fmt::runtime(fmt), std::forward<Args>(args)...));
}

/** Ask the user a yes or no question.
 * @param def The default result
 * @param fmt The format string
 * @param args Arguments in the format
 * @returns the result, y = true, n = false, only returns def if the result is def
 */
template <typename... Args>
bool askUserYorN(bool def, const std::string_view fmt, Args&&... args)
{
    const std::string& inputs_str = fmt::format(" [{}]: ", def ? "Y/n" : "y/N");
    std::string        result;
    fmt::print(fmt::runtime(fmt), std::forward<Args>(args)...);
    fmt::print("{}", inputs_str);

    while (std::getline(std::cin, result) && (result.length() > 1))
        warn("Please answear y or n {}", inputs_str);

    ctrl_d_handler(std::cin);

    if (result.empty())
        return def;

    if (def ? std::tolower(result[0]) != 'n' : std::tolower(result[0]) != 'y')
        return def;

    return !def;
}

bool        hasStart(const std::string_view fullString, const std::string_view start);
int         str_to_enum(const std::unordered_map<std::string, int>& map, const std::string_view name);
std::string draw_entry_menu(const std::string& prompt, const std::vector<std::string>& entries,
                            const std::string& default_option);
std::string draw_input_menu(const std::string& prompt, const std::string& default_option);

namespace JsonUtils
{

std::vector<std::string> vec_from_members(const rapidjson::Value& obj);
std::vector<std::string> vec_from_array(const rapidjson::Value& array);
void                     write_to_json(std::FILE* file, const rapidjson::Document& doc);
void                     populate_doc(std::FILE* file, rapidjson::Document& doc);
void                     autogen_empty_json(const std::string_view name);
void update_json_field(rapidjson::Document& pkg_doc, const std::string& field, const std::string& value);

}  // namespace JsonUtils
