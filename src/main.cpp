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

#include <cstdlib>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "backend_registry.hpp"
#include "backends/js_backend.hpp"
#include "backends/rust_backend.hpp"
#include "box.hpp"
#include "fmt/base.h"
#include "fmt/compile.h"
#include "getopt_port/getopt.h"
#include "manifest.hpp"
#include "manifest_settings.hpp"
#include "operations.hpp"
#include "switch_fnv1a.hpp"
#include "terminal_display.hpp"
#include "texts.hpp"

#if (!__has_include("version.h"))
#  error "version.h not found, please generate it with ./scripts/generateVersion.sh"
#else
#  include "version.h"
#endif

BackendRegistry g_registry;
TerminalDisplay display;
TermBox         termbox;

enum class Op
{
    None,
    Init,
    Set,
    External
};

static const std::unordered_map<std::string_view, Op> k_op_map = {
    { "init", Op::Init },
    { "set", Op::Set },
};

struct parse_result_t
{
    Op                op = Op::None;
    std::string       cmd;
    cmd_options_t     opts;
    manifest_update_t update;
};

void version()
{
    fmt::print(
        "ulpm {} built from branch '{}' at {} commit '{}' ({}).\n"
        "Date: {}\n"
        "Tag: {}\n",
        VERSION,
        GIT_BRANCH,
        GIT_DIRTY,
        GIT_COMMIT_HASH,
        GIT_COMMIT_MESSAGE,
        GIT_COMMIT_DATE,
        GIT_TAG);

    // if only everyone would not return error when querying the program version :(
    std::exit(EXIT_SUCCESS);
}

[[noreturn]] void help(const std::string_view help_msg, int invalid_opt = false)
{
    fmt::print(FMT_COMPILE("{}"), help_msg);

    std::exit(invalid_opt);
}

static void parse_manifest_fields(int                    argc,
                                  char*                  argv[],
                                  const bool             allow_yes,
                                  const std::string_view help_msg,
                                  cmd_options_t&         opts,
                                  manifest_update_t&     upd)
{
    // clang-format off
    const struct option long_opts[] = {
        {"force",               no_argument,       nullptr, 'f'},
        {"yes",                 no_argument,       nullptr, 'y'},
        {"help",                no_argument,       nullptr, 'h'},
        // Common
        {"project_name",        required_argument, nullptr, "project_name"_fnv1a16},
        {"project_description", required_argument, nullptr, "project_description"_fnv1a16},
        {"project_version",     required_argument, nullptr, "project_version"_fnv1a16},
        {"author",              required_argument, nullptr, "author"_fnv1a16},
        {"license",             required_argument, nullptr, "license"_fnv1a16},
        {"language",            required_argument, nullptr, "language"_fnv1a16},
        {"package_manager",     required_argument, nullptr, "package_manager"_fnv1a16},
        // JS-specific
        {"js_main_src",         required_argument, nullptr, "js_main_src"_fnv1a16},
        // Rust-specific
        {"rust_edition",        required_argument, nullptr, "rust_edition"_fnv1a16},
        {0, 0, 0, 0}
    };
    // clang-format on

    int opt;
    while ((opt = getopt_long(argc, argv, allow_yes ? "+fyh" : "+fh", long_opts, nullptr)) != -1)
    {
        switch (opt)
        {
            case 'h': help(help_msg, EXIT_SUCCESS);
            case '?': help(help_msg, EXIT_FAILURE);
            case 'f': opts.init_force = true; break;
            case 'y':
                if (allow_yes)
                    opts.init_yes = true;
                break;

            case "project_name"_fnv1a16:        upd.project_name = optarg; break;
            case "project_description"_fnv1a16: upd.project_description = optarg; break;
            case "project_version"_fnv1a16:     upd.project_version = optarg; break;
            case "author"_fnv1a16:              upd.author = optarg; break;
            case "license"_fnv1a16:             upd.license = optarg; break;
            case "language"_fnv1a16:            upd.language = optarg; break;
            case "package_manager"_fnv1a16:     upd.package_manager = optarg; break;
            case "js_main_src"_fnv1a16:         upd.js_main_src = optarg; break;
            case "rust_edition"_fnv1a16:        upd.rust_edition = optarg; break;
        }
    }
}

static void parse_run_args(int argc, char* argv[], std::vector<std::string>& out_args)
{
    const struct option long_opts[] = { { "help", no_argument, nullptr, 'h' }, { 0, 0, 0, 0 } };
    int                 opt;
    while ((opt = getopt_long(argc, argv, "+h", long_opts, nullptr)) != -1)
    {
        switch (opt)
        {
            case 'h': help(ulpm_help_run, EXIT_SUCCESS);
            case '?': help(ulpm_help_run, EXIT_FAILURE);
        }
    }
    for (int i = optind; i < argc; ++i)
        out_args.emplace_back(argv[i]);
}

static std::optional<parse_result_t> parseargs(int argc, char* argv[])
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

    if (argc < 2)
    {
        fmt::println("Please run ulpm through a terminal interface like cmd or msys2.");
        system("pause");
        return std::nullopt;
    }

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
        }
    }

    if (optind >= argc)
        help(ulpm_help, EXIT_FAILURE);  // no subcommand

    parse_result_t res;
    res.cmd = argv[optind];

    if (auto it = k_op_map.find(res.cmd); it != k_op_map.end())
        res.op = it->second;
    else
        res.op = Op::External;

    int    sub_argc = argc - optind - 1;
    char** sub_argv = argv + optind + 1;
    optind          = 0;  // reset for subcommand parsing

    switch (res.op)
    {
        case Op::Init:     parse_manifest_fields(sub_argc, sub_argv, true, ulpm_help_set, res.opts, res.update); break;
        case Op::Set:      parse_manifest_fields(sub_argc, sub_argv, false, ulpm_help_init, res.opts, res.update); break;
        case Op::External: parse_run_args(sub_argc, sub_argv, res.opts.arguments); break;
        default:           help(ulpm_help, EXIT_FAILURE); break;
    }

    return res;
}

static void register_backends()
{
    g_registry.registerBackend("javascript", [] { return std::make_unique<JsBackend>(); });
    g_registry.registerBackend("rust", [] { return std::make_unique<RustBackend>(); });
}

int main(int argc, char* argv[])
{
    std::optional<parse_result_t> parsed = parseargs(argc, argv);
    if (!parsed)
        return EXIT_FAILURE;

    setlocale(LC_ALL, "");
    register_backends();

    if (parsed->opts.init_yes)
    {
        if (!parsed->update.project_version)
            parsed->update.project_version = "0.0.1";
    }

    Manifest manifest;
    switch (parsed->op)
    {
        case Op::Init:     op_init(manifest, parsed->opts, parsed->update); break;
        case Op::Set:      op_set(manifest, parsed->update); break;
        case Op::External: op_run(manifest, parsed->cmd, parsed->opts); break;

        default: break;
    }

    return EXIT_SUCCESS;
}
