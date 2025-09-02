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

#include <ncurses.h>

#include <cstdlib>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "fmt/base.h"
#include "fmt/compile.h"
#include "settings.hpp"
#include "switch_fnv1a.hpp"
#include "texts.hpp"

#if (!__has_include("version.h"))
#error "version.h not found, please generate it with ./scripts/generateVersion.sh"
#else
#include "version.h"
#endif

#include "getopt_port/getopt.h"

enum OPs
{
    NONE,
    INSTALL,
    INIT,
    RUN,
    BUILD,
    SET,
} op = NONE;

static std::string                                     cmd;
static const std::unordered_map<std::string_view, OPs> map{
    { "install", INSTALL }, { "build", BUILD }, { "init", INIT }, { "run", RUN }, { "set", SET }
};

struct cmd_options_t cmd_options;

OPs str_to_enum(const std::string_view name)
{
    if (auto it = map.find(name); it != map.end())
        return it->second;
    return NONE;
}

void version()
{
    fmt::print(
        "ulpm {} built from branch '{}' at {} commit '{}' ({}).\n"
        "Date: {}\n"
        "Tag: {}\n",
        VERSION, GIT_BRANCH, GIT_DIRTY, GIT_COMMIT_HASH, GIT_COMMIT_MESSAGE, GIT_COMMIT_DATE, GIT_TAG);

    // if only everyone would not return error when querying the program version :(
    std::exit(EXIT_SUCCESS);
}

void help(const std::string_view help_msg, int invalid_opt = false)
{
    fmt::print(FMT_COMPILE("{}"), help_msg);

    std::exit(invalid_opt);
}

bool parse_run_args(int argc, char* argv[])
{
    // clang-format off
    const struct option long_opts[] = {
        {"force", no_argument, nullptr, 'f'},
        {"help",  no_argument, nullptr, 'h'},
        {0, 0, 0, 0}
    };
    // clang-format on

    int opt;
    while ((opt = getopt_long(argc, argv, "+fh", long_opts, nullptr)) != -1)
    {
        switch (opt)
        {
            case 'h': help(ulpm_help_run, EXIT_SUCCESS); break;
            case '?': help(ulpm_help_run, EXIT_FAILURE); break;
        }
    }

    for (int i = optind; i < argc; ++i)
        cmd_options.arguments.emplace_back(argv[i]);

    return true;
}

bool parse_init_args(int argc, char* argv[])
{
    // clang-format off
    const struct option long_opts[] = {
        {"force", no_argument, nullptr, 'f'},
        {"yes",   no_argument, nullptr, 'y'},
        {"help",  no_argument, nullptr, 'h'},

        {"language",            required_argument, nullptr, "language"_fnv1a16},
        {"package_manager",     required_argument, nullptr, "package_manager"_fnv1a16},
        {"project_name",        required_argument, nullptr, "project_name"_fnv1a16},
        {"license",             required_argument, nullptr, "license"_fnv1a16},
        {"project_description", required_argument, nullptr, "project_description"_fnv1a16},
	{"project_version",	required_argument, nullptr, "project_version"_fnv1a16},
        {"author",              required_argument, nullptr, "author"_fnv1a16},
        {0, 0, 0, 0}
    };
    // clang-format on

    int opt;
    while ((opt = getopt_long(argc, argv, "+fyh", long_opts, nullptr)) != -1)
    {
        switch (opt)
        {
            case 0:   break;
            case '?': help(ulpm_help_init, EXIT_FAILURE); break;
            case 'h': help(ulpm_help_init, EXIT_SUCCESS); break;

            case 'f': cmd_options.init_force = true; break;
            case 'y': cmd_options.init_yes = true; break;

            case "language"_fnv1a16:            Settings::manifest_defaults.language = optarg; break;
            case "package_manager"_fnv1a16:     Settings::manifest_defaults.package_manager = optarg; break;
            case "license"_fnv1a16:             Settings::manifest_defaults.license = optarg; break;
            case "project_name"_fnv1a16:        Settings::manifest_defaults.project_name = optarg; break;
            case "project_description"_fnv1a16: Settings::manifest_defaults.project_description = optarg; break;
            case "project_version"_fnv1a16:     Settings::manifest_defaults.project_version = optarg; break;
            case "author"_fnv1a16:              Settings::manifest_defaults.author = optarg; break;
        }
    }

    return true;
}

bool parse_set_args(int argc, char* argv[])
{
    // clang-format off
    const struct option long_opts[] = {
        {"force", no_argument, nullptr, 'f'},
        {"help",  no_argument, nullptr, 'h'},

        {"language",            required_argument, nullptr, "language"_fnv1a16},
        {"package_manager",     required_argument, nullptr, "package_manager"_fnv1a16},
        {"project_name",        required_argument, nullptr, "project_name"_fnv1a16},
        {"license",             required_argument, nullptr, "license"_fnv1a16},
	{"project_version",     required_argument, nullptr, "project_version"_fnv1a16},
        {"project_description", required_argument, nullptr, "project_description"_fnv1a16},
        {"author",              required_argument, nullptr, "author"_fnv1a16},
        {0, 0, 0, 0}
    };
    // clang-format on

    int opt;
    while ((opt = getopt_long(argc, argv, "+fh", long_opts, nullptr)) != -1)
    {
        switch (opt)
        {
            case 0:   break;
            case '?': help(ulpm_help_set, EXIT_FAILURE); break;
            case 'h': help(ulpm_help_set, EXIT_SUCCESS); break;

            case 'f': cmd_options.init_force = true; break;

            case "language"_fnv1a16:            Settings::manifest_defaults.language = optarg; break;
            case "package_manager"_fnv1a16:     Settings::manifest_defaults.package_manager = optarg; break;
            case "license"_fnv1a16:             Settings::manifest_defaults.license = optarg; break;
            case "project_name"_fnv1a16:        Settings::manifest_defaults.project_name = optarg; break;
            case "project_description"_fnv1a16: Settings::manifest_defaults.project_description = optarg; break;
            case "project_version"_fnv1a16:     Settings::manifest_defaults.project_version = optarg; break;
            case "author"_fnv1a16:              Settings::manifest_defaults.author = optarg; break;
        }
    }

    return true;
}

bool parse_general_command_args(int argc, char* argv[])
{
    // clang-format off
    const struct option long_opts[] = {
        {"help",  no_argument, nullptr, 'h'},
        {0, 0, 0, 0}
    };
    // clang-format on

    int opt;
    while ((opt = getopt_long(argc, argv, "+h", long_opts, nullptr)) != -1)
    {
        switch (opt)
        {
            case 'h': help(ulpm_help, EXIT_SUCCESS); break;
            case '?': help(ulpm_help, EXIT_FAILURE); break;
        }
    }

    for (int i = optind; i < argc; ++i)
        cmd_options.arguments.emplace_back(argv[i]);

    return true;
}

static bool parseargs(int argc, char* argv[])
{
    // clang-format off
    int opt = 0;
    int option_index = 0;
    const char *optstring = "+Vh";
    static const struct option opts[] = {
        {"version", no_argument, 0, 'V'},
        {"help",    no_argument, 0, 'h'},
        {0,0,0,0}
    };

    // clang-format on
    optind = 1;
    while ((opt = getopt_long(argc, argv, optstring, opts, &option_index)) != -1)
    {
        switch (opt)
        {
            case 0:   break;
            case '?': help(ulpm_help, EXIT_FAILURE); break;

            case 'V': version(); break;
            case 'h': help(ulpm_help, EXIT_SUCCESS); break;
            default:  return false;
        }
    }

    if (optind >= argc)
        help(ulpm_help, EXIT_FAILURE);  // no subcommand

    cmd             = argv[optind];
    int    sub_argc = argc - optind - 1;
    char** sub_argv = argv + optind + 1;

    op = str_to_enum(cmd);
    switch (op)
    {
        case RUN:  optind = 0; return parse_run_args(sub_argc, sub_argv);
        case INIT: optind = 0; return parse_init_args(sub_argc, sub_argv);
        case SET:  optind = 0; return parse_set_args(sub_argc, sub_argv);
        default:   optind = 0; return parse_general_command_args(sub_argc, sub_argv);
    }

    return true;
}

int main(int argc, char* argv[])
{
    if (!parseargs(argc, argv))
        return -1;

    setlocale(LC_ALL, "");
    if (cmd_options.init_yes)
    {
        if (Settings::manifest_defaults.project_version.empty())
            Settings::manifest_defaults.project_version = "0.0.1";
        if (Settings::manifest_defaults.js_main_src.empty())
            Settings::manifest_defaults.js_main_src = "src/main.js";
        if (Settings::manifest_defaults.js_runtime.empty())
            Settings::manifest_defaults.js_runtime = "node";
    }
    Settings::Manifest man;
    if (op == INIT)
    {
        if (!cmd_options.init_yes)
        {
            initscr();
            noecho();
            cbreak();              // Enable immediate character input
            keypad(stdscr, TRUE);  // Enable arrow keys
            curs_set(1);           // Show cursor
        }
        man.init_project(cmd_options);
    }
    else if (op == SET)
    {
        man.set_project_settings(cmd_options);
    }
    else if (op == RUN || op == INSTALL || op == BUILD)
    {
        man.validate_manifest();
        man.run_cmd(cmd, cmd_options.arguments);
    }

    return 0;
}
