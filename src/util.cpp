#include "util.hpp"

#include <ncurses.h>

#include <algorithm>
#include <cctype>
#include <iterator>
#include <string>
#include <unordered_map>
#include <vector>

// src/box.cpp
void draw_search_box(const std::string& query, const std::string& text, const std::vector<std::string>& results,
                     const size_t selected, size_t& scroll_offset, const size_t cursor_x, const bool is_search_tab);
void draw_input_box(WINDOW* win, const std::string& prompt, const std::string& input, const size_t cursor_pos);
void draw_exit_confirm(const int seloption);

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

void ctrl_d_handler(const std::istream& cin)
{
    if (cin.eof())
        die("Exiting due to CTRL-D or EOF");
}

static size_t remove_entries(std::vector<std::string>& results_value, const std::string& query)
{
    auto new_end = std::remove_if(results_value.begin(), results_value.end(),
                                  [&](const std::string& s) { return !hasStart(s, query); });

    results_value.erase(new_end, results_value.end());
    return results_value.empty() ? std::string::npos : std::distance(results_value.begin(), new_end) - 1;
}

const size_t SEARCH_TITLE_LEN = 2 + 8;  // 2 for box border, 8 for "Search: "
std::string  draw_entry_menu(const std::string& prompt, const std::vector<std::string>& entries,
                             const std::string& default_option)
{
    std::vector<std::string> results(entries);
    if (entries.empty())
    {
        endwin();
        return "";
    }

    std::string query         = default_option;
    int         ch            = 0;
    size_t      selected      = 0;
    size_t      scroll_offset = 0;
    size_t      cursor_x      = SEARCH_TITLE_LEN + query.length();
    bool        is_search_tab = true;

    if (!default_option.empty())
    {
        selected = remove_entries(results, query);
        if (selected == std::string::npos)
        {
            selected = 0;
        }
        else
        {
            is_search_tab = false;
            curs_set(0);
        }
    }

    // yeah magic numbers buuuu
    const int max_visible = static_cast<int>((getmaxy(stdscr) - 3) / 2) * 0.80f;
    draw_search_box(query, prompt, results, selected, scroll_offset, cursor_x, is_search_tab);
    move(1, cursor_x);

    bool exit          = false;
    bool exit_selected = false;
    while ((ch = getch()) != ERR)
    {
        if (ch == '\t')
        {
            is_search_tab = !is_search_tab;
        }
        else if (is_search_tab && ch != 27 && !exit)
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
            else if (ch == KEY_DC)  // Delete key
            {
                if (cursor_x < SEARCH_TITLE_LEN + query.size())
                    query.erase(cursor_x - SEARCH_TITLE_LEN, 1);
            }
            else if (ch == KEY_LEFT)
            {
                if (cursor_x > SEARCH_TITLE_LEN)
                    --cursor_x;
            }
            else if (ch == KEY_RIGHT)
            {
                if (cursor_x < SEARCH_TITLE_LEN + query.length())
                    ++cursor_x;
            }
            else if (ch == KEY_DOWN || ch == '\n')
            {
                is_search_tab = false;
            }
            else if (ch == KEY_HOME)  // Move to beginning
            {
                cursor_x = 0;
            }
            else if (ch == KEY_END)  // Move to end
            {
                cursor_x = query.length();
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
                if (exit)
                {
                    exit_selected = false;
                }
                else if (selected < results.size() - 1)
                {
                    ++selected;
                    if (selected >= scroll_offset + max_visible)
                        ++scroll_offset;
                }
            }
            // go down
            else if (ch == KEY_UP || ch == KEY_LEFT || tolower(ch) == 'k')
            {
                if (exit)
                {
                    exit_selected = true;
                }
                else if (selected == 0)
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
            // ESC pressed
            else if (ch == 27 && !exit)
            {
                exit = true;
            }
            // operation delete and choose "yes"
            else if (exit && ch == '\n' && exit_selected)
            {
                endwin();
                warn("Balling out. All changes are lost");
                std::exit(1);
            }
            // operation delete and pressed 'q' or "no"
            else if (exit && ch != 27 && (!exit_selected || ch == 'q'))
            {
                exit = false;
            }
            // pressed an item
            else if (ch == '\n' && !results.empty())
            {
                endwin();
                return results[selected];
            }
        }

        if (exit)
        {
            curs_set(false);
            draw_exit_confirm(exit_selected);
        }
        else
        {
            draw_search_box(query, prompt, (query.empty() ? entries : results), selected, scroll_offset, cursor_x,
                            is_search_tab);
            curs_set(is_search_tab);
        }
    }

    endwin();
    return UNKNOWN;
}

std::string draw_input_menu(const std::string& prompt, const std::string& default_option)
{
    const size_t INPUT_TITLE_LEN = prompt.length() + 1;
    std::string  input           = default_option;
    int          ch              = 0;
    size_t       cursor_x        = INPUT_TITLE_LEN;

    draw_input_box(stdscr, prompt, input, cursor_x - INPUT_TITLE_LEN);
    bool exit          = false;
    bool exit_selected = false;
    while ((ch = getch()) != ERR)
    {
        if (ch == 27)  // ESC to exit
        {
            exit = true;
        }
        else if (ch == '\n' || ch == KEY_ENTER)  // Enter to submit
        {
            if (exit && exit_selected)
            {
                endwin();
                warn("Balling out. All changes are lost");
                std::exit(1);
            }
            // operation delete and pressed 'q' or "no"
            else if (exit && ch != 27 && (!exit_selected || ch == 'q'))
            {
                exit = false;
            }
            else
            {
                endwin();
                return input;
            }
        }
        else if (ch == KEY_BACKSPACE || ch == 127)
        {
            if (!input.empty() && cursor_x > INPUT_TITLE_LEN)
                input.erase(--cursor_x - INPUT_TITLE_LEN, 1);
        }
        else if (ch == KEY_LEFT)
        {
            if (exit)
                exit_selected = true;
            else if (cursor_x > INPUT_TITLE_LEN)
                --cursor_x;
        }
        else if (ch == KEY_RIGHT)
        {
            if (exit)
                exit_selected = false;
            else if (cursor_x < INPUT_TITLE_LEN + input.size())
                ++cursor_x;
        }
        else if (ch == KEY_DC)  // Delete key
        {
            if (cursor_x < INPUT_TITLE_LEN + input.size())
                input.erase(cursor_x - INPUT_TITLE_LEN, 1);
        }
        else if (ch == KEY_HOME)
        {
            cursor_x = INPUT_TITLE_LEN;
        }
        else if (ch == KEY_END)
        {
            cursor_x = INPUT_TITLE_LEN + input.size();
        }
        else if (!(ch >= KEY_UP && ch <= KEY_MAX))  // Printable characters
        {
            input.insert(cursor_x++ - INPUT_TITLE_LEN, 1, ch);
        }

        if (exit)
        {
            curs_set(false);
            draw_exit_confirm(exit_selected);
        }
        else
        {
            curs_set(true);
            draw_input_box(stdscr, prompt, input, cursor_x - INPUT_TITLE_LEN);
        }
    }

    endwin();
    return "";
}
