#include <ncurses.h>
#include <wchar.h>

#include <sstream>
#include <string>
#include <vector>

static std::vector<std::string> wrap_text(const std::string& text, const size_t max_width)
{
    std::vector<std::string> lines;
    std::stringstream        ss(text);
    std::string              line;

    while (std::getline(ss, line, '\n'))
    {
        while (line.length() > max_width)
        {
            lines.push_back(line.substr(0, max_width));
            line = line.substr(max_width);
        }

        lines.push_back(line);
    }

    return lines;
}

// omfg too many args
void draw_search_box(const std::string& query, const std::string& text, const std::vector<std::string>& results, const size_t selected,
                     size_t& scroll_offset, const size_t cursor_x, const bool is_search_tab)
{
    erase();
    box(stdscr, 0, 0);

    int maxy, maxx;
    getmaxyx(stdscr, maxy, maxx);

    // header
    attron(A_BOLD);
    mvprintw(1, 2, "Search: %s", query.c_str());
    mvprintw(3, 4, "%s", text.c_str());
    attroff(A_BOLD);

    // First ensure selected item is visible
    size_t lines_above = 0;
    if (selected < scroll_offset)
    {
        scroll_offset = selected;
    }
    else
    {
        // Calculate lines needed to show selected item
        size_t needed_lines = 5;  // header + spacing (1)
        for (size_t i = scroll_offset; i <= selected && i < results.size(); i++)
        {
            const auto& wrapped = wrap_text(results[i], maxx - 11);
            needed_lines += wrapped.size() + 1;
            if (needed_lines > static_cast<size_t>(maxy - 1))
            {
                scroll_offset = i;
                break;
            }
            if (i == selected)
            {
                lines_above = needed_lines - (1 + wrapped.size());
            }
        }
    }

    // Draw visible items
    size_t row = 2;
    for (size_t i = scroll_offset; i < results.size(); ++i)
    {
        const bool  is_selected = (i == selected);
        const auto& wrapped     = wrap_text(results[i], maxx - 11);

        // Check space for this item
        if (row + 1 + wrapped.size() >= static_cast<size_t>(maxy - 1))
            break;

        // Draw item
        ++row;
        for (const std::string& line : wrapped)
        {
            if (is_selected && !is_search_tab)
                attron(A_REVERSE);
            mvprintw(++row, 6, "%s", line.c_str());
            if (is_selected && !is_search_tab)
                attroff(A_REVERSE);
        }
    }

    if (is_search_tab)
        move(1, cursor_x);
    else
        // Calculate cursor row based on lines above selected item
        move(3 + lines_above, 6);

    refresh();
}
