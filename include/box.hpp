#ifndef _BOX_HPP_
#define _BOX_HPP_

#include <string_view>

#include "terminal_display.hpp"

class TermBox : public TerminalDisplay
{
public:
    void DrawBox(int x, int y, int w, int h, const std::string_view title);
    void DrawExitConfirm(const int seloption);

    void DrawSearchBox(const std::string&              query,
                       const std::string&              text,
                       const std::vector<std::string>& results,
                       const size_t                    selected,
                       size_t&                         scroll_offset,
                       const size_t                    cursor_x,
                       const bool                      is_search_tab);

    void DrawInputBox(int                win_x,
                      int                win_y,
                      int                win_w,
                      int                win_h,
                      const std::string& prompt,
                      const std::string& input,
                      const size_t       cursor_pos);

    void DrawInputBox(const std::string& prompt, const std::string& input, const size_t cursor_pos);
};

extern TermBox termbox;

#endif  // !_BOX_HPP_
