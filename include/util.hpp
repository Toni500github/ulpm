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

#include <cstdlib>
#include <unordered_map>
#include <vector>

#include "fmt/color.h"
#include "fmt/core.h"

#define UNKNOWN "(unknown)"

#define BOLD_COLOR(x) (fmt::emphasis::bold | fmt::fg(fmt::rgb(x)))

#if DEBUG
inline bool debug_print = true;
#else
inline bool debug_print = false;
#endif

template <typename... Args>
void error(const std::string_view fmt, Args&&... args) noexcept
{
    fmt::print(stderr, BOLD_COLOR(fmt::color::red), "ERROR: {}\033[0m\n",
               fmt::format(fmt::runtime(fmt), std::forward<Args>(args)...));
}

template <typename... Args>
void die(const std::string_view fmt, Args&&... args) noexcept
{
    fmt::print(stderr, BOLD_COLOR(fmt::color::red), "FATAL: {}\033[0m\n",
               fmt::format(fmt::runtime(fmt), std::forward<Args>(args)...));
    std::exit(1);
}

template <typename... Args>
void debug(const std::string_view fmt, Args&&... args) noexcept
{
    if (debug_print)
        fmt::print(BOLD_COLOR((fmt::color::hot_pink)), "[DEBUG]:\033[0m {}\n",
                   fmt::format(fmt::runtime(fmt), std::forward<Args>(args)...));
}

template <typename... Args>
void warn(const std::string_view fmt, Args&&... args) noexcept
{
    fmt::print(stderr, BOLD_COLOR((fmt::color::yellow)), "WARNING: {}\033[0m\n",
               fmt::format(fmt::runtime(fmt), std::forward<Args>(args)...));
}

template <typename... Args>
void info(const std::string_view fmt, Args&&... args) noexcept
{
    fmt::print(BOLD_COLOR((fmt::color::cyan)), "INFO: {}\033[0m\n",
               fmt::format(fmt::runtime(fmt), std::forward<Args>(args)...));
}

#undef BOLD_COLOR

struct cmd_options_t
{
    bool                     install_force = false;
    bool                     init_force    = false;
    bool                     init_yes      = false;
    std::vector<std::string> arguments;
};

bool        hasStart(const std::string_view fullString, const std::string_view start);
int         str_to_enum(const std::unordered_map<std::string, int>& map, const std::string_view name);
std::string draw_entry_menu(const std::vector<std::string>& entries, const std::string& text,
                            const std::string& default_option);
std::string draw_input_menu(const std::string& prompt, const std::string& default_option);
