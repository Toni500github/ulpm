#include "util.hpp"

#include <ncurses.h>

#include <algorithm>
#include <cctype>
#include <string>
#include <unordered_map>
#include <vector>

// src/box.cpp
void draw_search_box(const std::string& query, const std::string& text, const std::vector<std::string>& results,
                     const size_t selected, size_t& scroll_offset, const size_t cursor_x, const bool is_search_tab);

bool hasStart(const std::string_view fullString, const std::string_view start)
{
    if (start.length() > fullString.length())
        return false;
    return (fullString.substr(0, start.size()) == start);
}

int str_to_enum(const std::unordered_map<std::string, int>& map, const std::string_view name)
{
    if (auto it = map.find(name.data()); it != map.end())
        return it->second;
    return -1;
}

static void remove_entries(std::vector<std::string>& results_value, const std::string& query)
{
    auto new_end = std::remove_if(results_value.begin(), results_value.end(),
                                  [&](const std::string& s) { return !hasStart(s, query); });

    results_value.erase(new_end, results_value.end());
}

const size_t SEARCH_TITLE_LEN = 2 + 8;  // 2 for box border, 8 for "Search: "
std::string  draw_menu(const std::vector<std::string>& entries, const std::string& text)
{
    initscr();
    noecho();
    cbreak();              // Enable immediate character input
    keypad(stdscr, TRUE);  // Enable arrow keys

    std::vector<std::string> results(entries);
    if (entries.empty())
        return "";

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
                    remove_entries(results, query);
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

                remove_entries(results, query);
            }
        }
        else
        {
            // go up
            if (ch == KEY_DOWN || ch == KEY_RIGHT || tolower(ch) == 'j')
            {
                if (selected < results.size() - 1)
                {
                    ++selected;
                    if (selected >= scroll_offset + max_visible)
                        ++scroll_offset;
                }
            }
            // go down
            else if (ch == KEY_UP || ch == KEY_LEFT || tolower(ch) == 'k')
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
                return results[selected];
            }
        }

        draw_search_box(query, text, (query.empty() ? entries : results), selected, scroll_offset, cursor_x,
                        is_search_tab);

        curs_set(is_search_tab);
    }

    endwin();
    return UNKNOWN;
}
