#ifndef _TERMINAL_DISPLAY_HPP_
#define _TERMINAL_DISPLAY_HPP_

#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

#include "fmt/format.h"

#define TB_OPT_ATTR_W 32
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wmissing-field-initializers"
#ifdef _WIN32
#  include "termbox2_win.h"
#else
#  include "termbox2.h"
#endif
#pragma GCC diagnostic pop

#include "util.hpp"

size_t utf8_len(const std::string& s);

// A similair clone of Adafruit_SSD130 for terminals
class TerminalDisplay
{
public:
    TerminalDisplay() : m_width(0), m_height(0), m_cursor_x(0), m_cursor_y(0), m_fg_col(0), m_bg_col(0) {}
    ~TerminalDisplay();

    bool begin();
    void clearDisplay();
    void setCursor(const int x, const int y);
    void setTextColor(const uintattr_t hex);
    void setTextBgColor(const uintattr_t hex);
    void resetColors();
    void updateDims();
    void drawLine(int x0, int y0, int x1, int y1, uint32_t ch);
    void drawCircle(int center_x, int center_y, int radius, uint32_t ch);
    void drawRect(int x, int y, int width, int height, uint32_t ch);
    void drawFilledRect(int x, int y, int width, int height, uint32_t ch);
    void drawPixel(int x, int y, uint32_t ch);
    void display();

    template <typename... Args>
    void print(const std::string_view fmt, Args&&... args)
    {
        const std::string&              text       = fmt::vformat(fmt, fmt::make_format_args(args...));
        const std::vector<std::string>& text_lines = split(text, '\n');

        int max_width = 0;
        for (const std::string& line : text_lines)
        {
            tb_print(m_cursor_x, m_cursor_y++, m_fg_col, m_bg_col, line.c_str());
            max_width = std::max<int>(max_width, utf8_len(line));
        }

        m_cursor_x += max_width;
        if (m_cursor_x >= m_width)
        {
            m_cursor_x = 0;
            m_cursor_y++;
            if (m_cursor_y >= m_height)
                m_cursor_y = m_height - 1;
        }
    }

    template <typename... Args>
    void centerText(int y, const std::string_view fmt, Args... args)
    {
        const std::string&              text       = fmt::vformat(fmt, fmt::make_format_args(args...));
        const std::vector<std::string>& text_lines = split(text, '\n');

        int max_width = 0;
        for (const std::string& line : text_lines)
            max_width = std::max<int>(max_width, utf8_len(line));

        int current_y = y;
        for (const std::string& line : text_lines)
        {
            int x = (m_width - static_cast<int>(utf8_len(line))) / 2;
            x     = std::max(0, x);

            tb_print(x, current_y++, m_fg_col, m_bg_col, line.c_str());
            setCursor(x, current_y);
        }
    }

    int getWidth() const { return m_width; }
    int getHeight() const { return m_height; }
    int getCursorX() const { return m_cursor_x; }
    int getCursorY() const { return m_cursor_y; }

    int pctX(float p) const { return static_cast<int>(m_width * p); }
    int pctY(float p) const { return static_cast<int>(m_height * p); }

    void hideCursor() { tb_hide_cursor(); }
    void showCursor(int cx = 0, int cy = 0) { tb_set_cursor(cx, cy); }
    void showCursor() { tb_set_cursor(m_cursor_x, m_cursor_y); }

private:
    int        m_width, m_height;
    int        m_cursor_x, m_cursor_y;
    uintattr_t m_fg_col, m_bg_col;
};

extern TerminalDisplay display;

#endif
