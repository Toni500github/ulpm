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

#include <sstream>

#define RAPIDJSON_HAS_STDSTRING 1

#include <unistd.h>

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <iterator>
#include <string>
#include <unordered_map>
#include <vector>

#include "box.hpp"
#include "fmt/os.h"
#include "rapidjson/error/en.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"
#include "rapidjson/prettywriter.h"
#include "utf8.h"
#include "util.hpp"

constexpr size_t SEARCH_TITLE_LEN = 2 + 8;  // 2 for box border, 8 for "Search: "

static std::string codepoint_to_utf8(uint32_t cp)
{
    char         buf[5] = {};
    utf8_int8_t* end    = utf8catcodepoint(reinterpret_cast<utf8_int8_t*>(buf), cp, sizeof(buf) - 1);
    *end                = '\0';
    return buf;
}

bool hasStart(const std::string_view fullString, const std::string_view start)
{
    if (start.length() > fullString.length())
        return false;
    return (fullString.substr(0, start.size()) == start);
}

std::vector<std::string> split(const std::string_view text, const char delim)
{
    std::string              line;
    std::vector<std::string> vec;
    std::stringstream        ss(text.data());
    while (std::getline(ss, line, delim))
        vec.push_back(line);
    return vec;
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
    auto new_end = std::remove_if(
        results_value.begin(), results_value.end(), [&](const std::string& s) { return !hasStart(s, query); });

    results_value.erase(new_end, results_value.end());
    return results_value.empty() ? std::string::npos : std::distance(results_value.begin(), new_end) - 1;
}

std::string draw_entry_menu(const std::string&              prompt,
                            const std::vector<std::string>& entries,
                            const std::string&              default_option)
{
    std::vector<std::string> results(entries);
    if (entries.empty())
    {
        tb_shutdown();
        return "";
    }

    std::string     query         = default_option;
    struct tb_event ev            = {};
    size_t          selected      = 0;
    size_t          scroll_offset = 0;
    size_t          cursor_x      = SEARCH_TITLE_LEN + query.length();
    bool            is_search_tab = true;

    if (!default_option.empty())
    {
        selected = remove_entries(results, query);
        if (selected == std::string::npos)
            selected = 0;
        else
            is_search_tab = false;
    }

    // yeah magic numbers buuuu
    termbox.DrawSearchBox(query, prompt, results, selected, scroll_offset, cursor_x, is_search_tab);

    bool exit          = false;
    bool exit_selected = false;
    while (tb_poll_event(&ev) == TB_OK)
    {
        if (ev.type != TB_EVENT_KEY)
            continue;

        const uint32_t ch  = ev.ch;   // Unicode codepoint (printable input)
        const uint16_t key = ev.key;  // Special key (TB_KEY_*)

        if (key == TB_KEY_TAB)
        {
            is_search_tab = !is_search_tab;
        }
        else if (is_search_tab && key != TB_KEY_ESC && !exit)
        {
            bool erased = false;
            if (key == TB_KEY_BACKSPACE || key == TB_KEY_BACKSPACE2)
            {
                if (!query.empty() && cursor_x > SEARCH_TITLE_LEN)
                {
                    query.erase(--cursor_x - SEARCH_TITLE_LEN, 1);
                    erased  = true;
                    results = entries;
                    remove_entries(results, query);
                }
            }
            else if (key == TB_KEY_DELETE)
            {
                if (cursor_x < SEARCH_TITLE_LEN + query.size())
                    query.erase(cursor_x - SEARCH_TITLE_LEN, 1);
            }
            else if (key == TB_KEY_ARROW_LEFT)
            {
                if (cursor_x > SEARCH_TITLE_LEN)
                    --cursor_x;
            }
            else if (key == TB_KEY_ARROW_RIGHT)
            {
                if (cursor_x < SEARCH_TITLE_LEN + query.length())
                    ++cursor_x;
            }
            else if (key == TB_KEY_ARROW_DOWN || key == TB_KEY_ENTER)
            {
                is_search_tab = false;
            }
            else if (key == TB_KEY_HOME)
            {
                cursor_x = SEARCH_TITLE_LEN;
            }
            else if (key == TB_KEY_END)
            {
                cursor_x = SEARCH_TITLE_LEN + query.length();
            }
            else if (ch != 0)  // Printable character
            {
                if (!erased)
                {
                    query.insert(cursor_x - SEARCH_TITLE_LEN, codepoint_to_utf8(ch));
                    ++cursor_x;
                }
                erased = false;

                selected      = 0;
                scroll_offset = 0;
                remove_entries(results, query);
            }
        }
        else
        {
            // go up
            if (key == TB_KEY_ARROW_DOWN || key == TB_KEY_ARROW_RIGHT ||
                (ch != 0 && tolower(static_cast<int>(ch)) == 'j'))
            {
                if (exit)
                    exit_selected = false;
                else if (selected < results.size() - 1)
                    ++selected;
            }
            // go down
            else if (key == TB_KEY_ARROW_UP || key == TB_KEY_ARROW_LEFT ||
                     (ch != 0 && tolower(static_cast<int>(ch)) == 'k'))
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
            else if (key == TB_KEY_ESC && !exit)
            {
                exit = true;
            }
            // operation delete and choose "yes"
            else if (exit && key == TB_KEY_ENTER && exit_selected)
            {
                tb_shutdown();
                warn("Balling out. All changes are lost");
                std::exit(1);
            }
            // operation delete and pressed 'q' or "no"
            else if (exit && key != TB_KEY_ESC && (!exit_selected || (ch != 0 && ch == 'q')))
            {
                exit = false;
            }
            // pressed an item
            else if (key == TB_KEY_ENTER && !results.empty())
            {
                tb_shutdown();
                return results[selected];
            }
        }

        if (exit)
        {
            termbox.DrawExitConfirm(exit_selected);
        }
        else
        {
            termbox.DrawSearchBox(
                query, prompt, (query.empty() ? entries : results), selected, scroll_offset, cursor_x, is_search_tab);            
        }
    }

    tb_shutdown();
    return UNKNOWN;
}

std::string draw_input_menu(const std::string& prompt, const std::string& default_option)
{
    const size_t    INPUT_TITLE_LEN = prompt.length() + 1;
    std::string     input           = default_option;
    struct tb_event ev              = {};
    size_t          cursor_x        = INPUT_TITLE_LEN;

    termbox.DrawInputBox(prompt, input, cursor_x - INPUT_TITLE_LEN);

    bool exit          = false;
    bool exit_selected = false;
    while (tb_poll_event(&ev) == TB_OK)
    {
        if (ev.type != TB_EVENT_KEY)
            continue;

        const uint32_t ch  = ev.ch;
        const uint16_t key = ev.key;

        if (key == TB_KEY_ESC)  // ESC to exit
        {
            exit = true;
        }
        else if (key == TB_KEY_ENTER)  // Enter to submit
        {
            if (exit && exit_selected)
            {
                tb_shutdown();
                warn("Balling out. All changes are lost");
                std::exit(1);
            }
            // operation delete and pressed 'q' or "no"
            else if (exit && (!exit_selected || (ch != 0 && ch == 'q')))
            {
                exit = false;
            }
            else
            {
                tb_shutdown();
                return input;
            }
        }
        else if (key == TB_KEY_BACKSPACE || key == TB_KEY_BACKSPACE2)
        {
            if (!input.empty() && cursor_x > INPUT_TITLE_LEN)
                input.erase(--cursor_x - INPUT_TITLE_LEN, 1);
        }
        else if (key == TB_KEY_ARROW_LEFT)
        {
            if (exit)
                exit_selected = true;
            else if (cursor_x > INPUT_TITLE_LEN)
                --cursor_x;
        }
        else if (key == TB_KEY_ARROW_RIGHT)
        {
            if (exit)
                exit_selected = false;
            else if (cursor_x < INPUT_TITLE_LEN + input.size())
                ++cursor_x;
        }
        else if (key == TB_KEY_DELETE)
        {
            if (cursor_x < INPUT_TITLE_LEN + input.size())
                input.erase(cursor_x - INPUT_TITLE_LEN, 1);
        }
        else if (key == TB_KEY_HOME)
        {
            cursor_x = INPUT_TITLE_LEN;
        }
        else if (key == TB_KEY_END)
        {
            cursor_x = INPUT_TITLE_LEN + input.size();
        }
        else if (ch != 0)  // Printable character
        {
            input.insert(cursor_x - INPUT_TITLE_LEN, codepoint_to_utf8(ch));
            ++cursor_x;
        }

        if (exit)
            termbox.DrawExitConfirm(exit_selected);
        else
            termbox.DrawInputBox(prompt, input, cursor_x - INPUT_TITLE_LEN);
    }

    tb_shutdown();
    return "";
}

namespace JsonUtils
{

std::vector<std::string> vec_from_members(const rapidjson::Value& obj)
{
    std::vector<std::string> keys;
    if (!obj.IsObject())
        return keys;
    keys.reserve(obj.MemberCount());
    for (auto const& item : obj.GetObject())
        keys.emplace_back(item.name.GetString());
    return keys;
}

std::vector<std::string> vec_from_array(const rapidjson::Value& array)
{
    std::vector<std::string> keys;
    if (!array.IsArray())
        return keys;
    keys.reserve(array.Size());
    for (auto const& e : array.GetArray())
        if (e.IsString())
            keys.emplace_back(e.GetString());
    return keys;
}

void write_to_json(std::FILE* file, const rapidjson::Document& doc)
{
    // seek back to the beginning to overwrite
    fseek(file, 0, SEEK_SET);

    char                                                writeBuffer[UINT16_MAX] = { 0 };
    rapidjson::FileWriteStream                          writeStream(file, writeBuffer, sizeof(writeBuffer));
    rapidjson::PrettyWriter<rapidjson::FileWriteStream> fileWriter(writeStream);
    fileWriter.SetFormatOptions(rapidjson::kFormatSingleLineArray);  // Disable newlines between array elements
    doc.Accept(fileWriter);

    ftruncate(fileno(file), ftell(file));
    fflush(file);
}

void autogen_empty_json(const std::string_view name)
{
    auto f = fmt::output_file(name.data(), fmt::file::CREATE | fmt::file::WRONLY | fmt::file::TRUNC);
    f.print("{{}}");
    f.close();
}

void populate_doc(std::FILE* file, rapidjson::Document& doc)
{
    if (!file)
    {
        perror("fopen");
        return;
    }

    char                      buf[UINT16_MAX] = { 0 };
    rapidjson::FileReadStream stream(file, buf, sizeof(buf));

    if (doc.ParseStream(stream).HasParseError())
    {
        fclose(file);
        die("Failed to parse json file: {} At offset {}",
            rapidjson::GetParseError_En(doc.GetParseError()),
            doc.GetErrorOffset());
    };
}

void update_json_field(rapidjson::Document& pkg_doc, const std::string& field, const std::string& value)
{
    rapidjson::Document::AllocatorType& allocator = pkg_doc.GetAllocator();

    if (pkg_doc.HasMember(field))
    {
        debug("changing {} from '{}' to '{}'", field, pkg_doc[field].GetString(), value);
        pkg_doc[field].SetString(value.c_str(), value.length(), allocator);
    }
    else
    {
        debug("adding field '{}' with value '{}'", field, value);
        pkg_doc.AddMember(rapidjson::Value(field.c_str(), field.length(), allocator),
                          rapidjson::Value(value.c_str(), value.length(), allocator),
                          allocator);
    }
}

std::string find_value_from_obj_array(const rapidjson::Value& array, const std::string& name, const std::string& value)
{
    if (!array.IsArray())
        return {};

    for (const auto& obj : array.GetArray())
        if (obj.HasMember("name") && obj["name"].IsString() && name == obj["name"].GetString() &&
            obj.HasMember(value) && obj[value].IsString())
            return obj[value].GetString();
    return {};
}

std::vector<std::string> vec_from_obj_array(const rapidjson::Value& array, const std::string& name)
{
    if (!array.IsArray())
        return {};

    std::vector<std::string> ret;

    for (const auto& obj : array.GetArray())
        if (obj.HasMember(name) && obj[name].IsString())
            ret.push_back(obj[name].GetString());
    return ret;
}

}  // namespace JsonUtils
