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
#include "util.hpp"

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
} op = NONE;

static const std::unordered_map<std::string_view, OPs> map{
    { "init", INIT },
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

void help(int invalid_opt = false)
{
    fmt::print(FMT_COMPILE("{}"), cufetchpm_help);

    std::exit(invalid_opt);
}

void help_install(int invalid_opt = false)
{
    fmt::print(FMT_COMPILE("{}"), cufetchpm_help_install);

    std::exit(invalid_opt);
}

bool parse_install_args(int argc, char* argv[])
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
            case 'f': cmd_options.install_force = true; break;
            case 'h': help_install(EXIT_SUCCESS); break;
            case '?': help_install(EXIT_FAILURE); break;
        }
    }

    for (int i = optind; i < argc; ++i)
        cmd_options.arguments.emplace_back(argv[i]);

    if (cmd_options.arguments.empty())
        die("install: no repositories/paths given");

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
            case '?': help_install(EXIT_FAILURE); break;

            case 'h': help_install(EXIT_SUCCESS); break;
            case 'f': cmd_options.install_force = true; break;
            case 'y': cmd_options.init_yes = true; break;

            case "language"_fnv1a16:            Settings::manifest_defaults.language = optarg; break;
            case "package_manager"_fnv1a16:     Settings::manifest_defaults.prefered_pm = optarg; break;
            case "license"_fnv1a16:             Settings::manifest_defaults.license = optarg; break;
            case "project_name"_fnv1a16:        Settings::manifest_defaults.project_name = optarg; break;
            case "project_description"_fnv1a16: Settings::manifest_defaults.project_description = optarg; break;
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
            case 'h': help(EXIT_SUCCESS); break;
            case '?': help_install(EXIT_FAILURE); break;
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
            case '?': help(EXIT_FAILURE); break;

            case 'V': version(); break;
            case 'h': help(); break;
            default:  return false;
        }
    }

    if (optind >= argc)
        help(EXIT_FAILURE);  // no subcommand

    std::string_view cmd      = argv[optind];
    int              sub_argc = argc - optind - 1;
    char**           sub_argv = argv + optind + 1;

    op = str_to_enum(cmd);
    switch (op)
    {
        case INSTALL: optind = 0; return parse_install_args(sub_argc, sub_argv);
        case INIT:    optind = 0; return parse_init_args(sub_argc, sub_argv);
        default:      optind = 0; return parse_general_command_args(sub_argc, sub_argv);
    }

    return true;
}

int main(int argc, char* argv[])
{
    if (!parseargs(argc, argv))
        return -1;

    setlocale(LC_ALL, "");
    if (op == INIT)
    {
        Settings::Manifest man;
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

    return 0;
}
