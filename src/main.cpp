#include <ncurses.h>

#include <algorithm>
#include <cstdlib>
#include <string_view>
#include <unordered_map>
#include <vector>

#include "commands.hpp"
#include "fmt/base.h"
#include "fmt/compile.h"
#include "texts.hpp"
#include "util.hpp"

#if (!__has_include("version.h"))
#error "version.h not found, please generate it with ./scripts/generateVersion.sh"
#else
#include "version.h"
#endif

#include "getopt_port/getopt.h"

// src/box.cpp
void draw_search_box(const std::string& query, const std::string& text, const std::vector<std::string>& results, const size_t selected,
                     size_t& scroll_offset, const size_t cursor_x, const bool is_search_tab);

enum OPs
{
    NONE,
    INSTALL,
    INIT,
} op = NONE;

static const std::unordered_map<std::string_view, OPs> map{
    { "install", INSTALL },
    { "init", INIT },
};

OPs str_to_enum(const std::string_view name)
{
    if (auto it = map.find(name); it != map.end())
        return it->second;
    return NONE;
}

void version()
{
    fmt::print(
        "cufetchpm {} built from branch '{}' at {} commit '{}' ({}).\n"
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
        default:      optind = 0; return parse_general_command_args(sub_argc, sub_argv);
    }

    return true;
}

static void removeEntries(std::vector<std::string>& results_value, const std::string& query)
{
    auto new_end = std::remove_if(results_value.begin(), results_value.end(),
                                  [&](const std::string& s) { return !hasStart(s, query); });

    results_value.erase(new_end, results_value.end());
}

const size_t SEARCH_TITLE_LEN = 2 + 8;  // 2 for box border, 8 for "Search: "
int search_algo(const std::vector<std::string>& entries, const std::string& text)
{
    initscr();
    noecho();
    cbreak();              // Enable immediate character input
    keypad(stdscr, TRUE);  // Enable arrow keys

    std::vector<std::string> results(entries);
    if (entries.empty())
        return 1;

    std::string query;
    int         ch            = 0;
    size_t      selected      = 0;
    size_t      scroll_offset = 0;
    size_t      cursor_x      = SEARCH_TITLE_LEN;
    bool        is_search_tab = true;

    // yeah magic numbers buuuu
    const int max_visible = ((getmaxy(stdscr) - 3) / 2) * 0.80f;
    draw_search_box(query, text, entries, selected, scroll_offset, cursor_x, is_search_tab);
    move(1, cursor_x);

    while ((ch = getch()) != ERR)
    {
        if (ch == 27)  // ESC
            break;

        if (ch == '\t')
        {
            is_search_tab = !is_search_tab;
        }
        else if (is_search_tab)
        {
            bool erased = false;
            if (ch == KEY_BACKSPACE || ch == 127)
            {
                if (!query.empty())
                {
                    // decrease then pass
                    if (cursor_x > SEARCH_TITLE_LEN)
                        query.erase(--cursor_x - SEARCH_TITLE_LEN, 1);

                    erased  = true;
                    results = entries;
                    removeEntries(results, query);
                }
            }
            else if (ch == KEY_LEFT)
            {
                if (cursor_x > SEARCH_TITLE_LEN)
                    --cursor_x;
            }
            else if (ch == KEY_RIGHT)
            {
                if (cursor_x < SEARCH_TITLE_LEN + query.size())
                    ++cursor_x;
            }
            else if (ch == KEY_DOWN || ch == '\n')
            {
                is_search_tab = false;
            }
            else if (!(ch >= KEY_UP && ch <= KEY_MAX))
            {
                // pass then increase
                if (!erased)
                    query.insert(cursor_x++ - SEARCH_TITLE_LEN, 1, ch);
                erased = false;

                selected      = 0;
                scroll_offset = 0;

                removeEntries(results, query);
            }
        }
        else
        {
            // go up
            if (ch == KEY_DOWN || ch == KEY_RIGHT)
            {
                if (selected < results.size() - 1)
                {
                    ++selected;
                    if (selected >= scroll_offset + max_visible)
                        ++scroll_offset;
                }
            }
            // go down
            else if (ch == KEY_UP || ch == KEY_LEFT)
            {
                if (selected == 0)
                {
                    is_search_tab = true;
                }
                else
                {
                    --selected;
                    if (selected < scroll_offset)
                        --scroll_offset;
                }
            }
            // pressed an item
            else if (ch == '\n' && !results.empty())
            {
                endwin();
                fmt::println("selected {}", results[selected]);
                return 0;
            }
        }

        draw_search_box(query, text, (query.empty() ? entries : results), selected, scroll_offset, cursor_x, is_search_tab);

        curs_set(is_search_tab);
    }

    endwin();
    return 0;
}

int main(int argc, char* argv[])
{
    if (!parseargs(argc, argv))
        return -1;

    switch (op)
    {
        case INSTALL:
        case INIT:    return search_algo({"npm", "yarn", "node"}, "Choose the preferred package manager");
        default:      warn("uh?");
    }

    return 0;
}
