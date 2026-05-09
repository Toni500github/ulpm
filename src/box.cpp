#include "box.hpp"

#include <sstream>
#include <string>
#include <string_view>
#include <vector>

static constexpr uint32_t BOX_ULCORNER = U'┌';
static constexpr uint32_t BOX_URCORNER = U'┐';
static constexpr uint32_t BOX_LLCORNER = U'└';
static constexpr uint32_t BOX_LRCORNER = U'┘';
static constexpr uint32_t BOX_HLINE    = U'─';
static constexpr uint32_t BOX_VLINE    = U'│';

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

void TermBox::DrawBox(int x, int y, int width, int height, const std::string_view title)
{
    resetColors();
    for (int row = y; row < y + height; ++row)
        for (int col = x; col < x + width; ++col)
            drawPixel(col, row, U' ');

    for (int col = x + 1; col < x + width - 1; ++col)
    {
        drawPixel(col, y, BOX_HLINE);
        drawPixel(col, y + height - 1, BOX_HLINE);
    }

    for (int row = y + 1; row < y + height - 1; ++row)
    {
        drawPixel(x, row, BOX_VLINE);
        drawPixel(x + width - 1, row, BOX_VLINE);
    }

    drawPixel(x, y, BOX_ULCORNER);
    drawPixel(x + width - 1, y, BOX_URCORNER);
    drawPixel(x, y + height - 1, BOX_LLCORNER);
    drawPixel(x + width - 1, y + height - 1, BOX_LRCORNER);

    setCursor(x + 4, y);
    print("{}", title);
}

void TermBox::DrawExitConfirm(const int seloption)
{
    const int width  = 60;
    const int height = 6;
    const int box_x  = (getWidth() - width) / 2;
    const int box_y  = (getHeight() - height) / 2;

    DrawBox(box_x, box_y, width, height, "Confirm exit");

    setCursor(box_x + 2, box_y + 1);
    print("Are you sure you want to exit? All changes will be lost.");

    // yes option (column 20 relative to box)
    if (seloption)
        setTextBgColor(TB_REVERSE);
    setCursor(box_x + 20, box_y + 4);
    print("yes");
    resetColors();

    // no option (column 34 relative to box)
    if (!seloption)
        setTextBgColor(TB_REVERSE);
    setCursor(box_x + 34, box_y + 4);
    print("no");
    resetColors();

    display();
}

void TermBox::DrawSearchBox(const std::string&              query,
                            const std::string&              text,
                            const std::vector<std::string>& results,
                            const size_t                    selected,
                            size_t&                         scroll_offset,
                            const size_t                    cursor_x,
                            const bool                      is_search_tab)
{
    clearDisplay();

    const int maxx = getWidth();
    const int maxy = getHeight();

    // Outer border
    drawRect(0, 0, maxx, maxy, BOX_VLINE);  // placeholder char; we redraw properly
    // Redraw border with correct box-drawing characters
    drawPixel(0, 0, BOX_ULCORNER);
    drawPixel(maxx - 1, 0, BOX_URCORNER);
    drawPixel(0, maxy - 1, BOX_LLCORNER);
    drawPixel(maxx - 1, maxy - 1, BOX_LRCORNER);
    for (int c = 1; c < maxx - 1; ++c)
    {
        drawPixel(c, 0, BOX_HLINE);
        drawPixel(c, maxy - 1, BOX_HLINE);
    }
    for (int r = 1; r < maxy - 1; ++r)
    {
        drawPixel(0, r, BOX_VLINE);
        drawPixel(maxx - 1, r, BOX_VLINE);
    }

    // Header
    setCursor(2, 1);
    setTextColor(TB_BOLD);
    print("Search: {}", query);
    resetColors();

    setCursor(4, 3);
    print("{}", text);

    // Ensure selected item is visible
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
            const auto& wrapped = wrap_text(results[i], static_cast<size_t>(maxx) - 11);
            needed_lines += wrapped.size() + 1;
            if (needed_lines > static_cast<size_t>(maxy - 2))
            {
                scroll_offset = i;
                break;
            }
        }
    }

    // Draw visible items
    size_t row = 2;
    for (size_t i = scroll_offset; i < results.size(); ++i)
    {
        const bool  is_selected = (i == selected);
        const auto& wrapped     = wrap_text(results[i], static_cast<size_t>(maxx) - 11);

        // Check space for this item
        if (row + 1 + wrapped.size() >= size_t(maxy - 2))
            break;

        // Draw item
        ++row;
        for (const std::string& line : wrapped)
        {
            if (is_selected && !is_search_tab)
                setTextColor(TB_REVERSE);

            setCursor(6, ++row);
            print("{}", line);

            if (is_selected && !is_search_tab)
                resetColors();
        }
    }

    // Position cursor
    if (is_search_tab)
        showCursor(static_cast<int>(cursor_x), 1);
    else
        hideCursor();

    display();
}

void TermBox::DrawInputBox(int                win_x,
                           int                win_y,
                           int                win_w,
                           int                win_h,
                           const std::string& prompt,
                           const std::string& input,
                           const size_t       cursor_pos)
{
    // Clear and draw box
    for (int row = win_y; row < win_y + win_h; ++row)
        for (int col = win_x; col < win_x + win_w; ++col)
            drawPixel(col, row, U' ');

    DrawBox(win_x, win_y, win_w, win_h, "");

    // Prompt
    setCursor(win_x + 2, win_y + 1);
    print("{}:", prompt);

    // Input field
    const int input_start_x   = win_x + 2 + static_cast<int>(prompt.length()) + 1;
    const int available_width = win_x + win_w - input_start_x - 2;

    setTextColor(TB_REVERSE);
    std::string field = input.substr(0, available_width);
    field.resize(available_width, ' ');
    setCursor(input_start_x, win_y + 1);
    print("{}", field);
    resetColors();

    // Instructions
    setCursor(win_x + 2, win_y + 3);
    print("Enter: Submit");
    setCursor(win_x + 2, win_y + 4);
    print("ESC: Exit");

    // Position cursor within the input field
    const size_t display_cursor_pos = std::min(cursor_pos, static_cast<size_t>(available_width - 1));
    showCursor(input_start_x + static_cast<int>(display_cursor_pos), win_y + 1);

    display();
}

void TermBox::DrawInputBox(const std::string& prompt, const std::string& input, const size_t cursor_pos)
{
    const int width  = getWidth() - 4;  // leave a 2-col margin each side
    const int height = 7;               // same as the windowed version
    const int win_x  = (getWidth() - width) / 2;
    const int win_y  = (getHeight() - height) / 2;

    DrawInputBox(win_x, win_y, width, height, prompt, input, cursor_pos);
}
