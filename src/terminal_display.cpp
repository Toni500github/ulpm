#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <cstdlib>

#define TB_IMPL 1
#include "settings.hpp"
#include "terminal_display.hpp"
#include "utf8.h"

// utf8len requires const utf8_int8_t* (aka char8_t* in C++20), but
// std::string::c_str() returns const char*. This helper silences the
// conversion at a single place rather than scattering casts everywhere.
size_t utf8_len(const std::string& s)
{
    return utf8len(reinterpret_cast<const utf8_int8_t*>(s.c_str()));
}

static void enable_ansi_colors()
{
#ifdef _WIN32
    HANDLE hOut = GetStdHandle(STD_OUTPUT_HANDLE);
    if (hOut == INVALID_HANDLE_VALUE)
        return;

    DWORD mode = 0;
    if (!GetConsoleMode(hOut, &mode))
        return;

    mode |= ENABLE_VIRTUAL_TERMINAL_PROCESSING;
    SetConsoleMode(hOut, mode);
#endif
}

TerminalDisplay::~TerminalDisplay()
{
    clearDisplay();
    tb_shutdown();
}

bool TerminalDisplay::begin()
{
    enable_ansi_colors();
    if (tb_init() < 0)
        return false;

    updateDims();
    tb_hide_cursor();
    return true;
}

void TerminalDisplay::updateDims()
{
    m_width  = tb_width();
    m_height = tb_height();

    m_cursor_x = std::clamp(m_cursor_x, 0, std::max(0, m_width - 1));
    m_cursor_y = std::clamp(m_cursor_y, 0, std::max(0, m_height - 1));
}

void TerminalDisplay::clearDisplay()
{
    updateDims();
    resetColors();
    tb_clear();
    m_cursor_x = 0;
    m_cursor_y = 0;
}

void TerminalDisplay::display()
{
    updateDims();
    tb_present();
}

void TerminalDisplay::resetColors()
{
    m_fg_col = TB_DEFAULT;
    m_bg_col = TB_DEFAULT;
}

void TerminalDisplay::setTextColor(const uintattr_t col)
{
    m_fg_col = col;
}

void TerminalDisplay::setTextBgColor(const uintattr_t col)
{
    m_bg_col = col;
}

void TerminalDisplay::setCursor(const int x, const int y)
{
    m_cursor_x = std::clamp(x, 0, m_width - 1);
    m_cursor_y = std::clamp(y, 0, m_height - 1);
}

void TerminalDisplay::drawPixel(int x, int y, uint32_t ch)
{
    if (x < 0 || x >= m_width || y < 0 || y >= m_height)
        return;

    tb_set_cell(x, y, ch, m_fg_col, m_bg_col);
}

void TerminalDisplay::drawLine(int x0, int y0, int x1, int y1, uint32_t ch)
{
    // Bresenham's line algorithm
    int dx  = abs(x1 - x0);
    int dy  = abs(y1 - y0);
    int sx  = (x0 < x1) ? 1 : -1;
    int sy  = (y0 < y1) ? 1 : -1;
    int err = dx - dy;

    while (true)
    {
        drawPixel(x0, y0, ch);

        if (x0 == x1 && y0 == y1)
            break;

        int e2 = 2 * err;
        if (e2 > -dy)
        {
            err -= dy;
            x0 += sx;
        }
        if (e2 < dx)
        {
            err += dx;
            y0 += sy;
        }
    }
}

void TerminalDisplay::drawCircle(int center_x, int center_y, int radius, uint32_t ch)
{
    // Midpoint circle algorithm
    int x   = radius;
    int y   = 0;
    int err = 0;

    while (x >= y)
    {
        drawPixel(center_x + x, center_y + y, ch);
        drawPixel(center_x + y, center_y + x, ch);
        drawPixel(center_x - y, center_y + x, ch);
        drawPixel(center_x - x, center_y + y, ch);
        drawPixel(center_x - x, center_y - y, ch);
        drawPixel(center_x - y, center_y - x, ch);
        drawPixel(center_x + y, center_y - x, ch);
        drawPixel(center_x + x, center_y - y, ch);

        if (err <= 0)
        {
            y += 1;
            err += 2 * y + 1;
        }
        if (err > 0)
        {
            x -= 1;
            err -= 2 * x + 1;
        }
    }
}

void TerminalDisplay::drawRect(int x, int y, int width, int height, uint32_t ch)
{
    // Top and bottom horizontal lines
    drawLine(x, y, x + width - 1, y, ch);
    drawLine(x, y + height - 1, x + width - 1, y + height - 1, ch);

    // Left and right vertical lines
    drawLine(x, y, x, y + height - 1, ch);
    drawLine(x + width - 1, y, x + width - 1, y + height - 1, ch);
}

void TerminalDisplay::drawFilledRect(int x, int y, int width, int height, uint32_t ch)
{
    for (int row = y; row < y + height; ++row)
        for (int col = x; col < x + width; ++col)
            drawPixel(col, row, ch);
}
