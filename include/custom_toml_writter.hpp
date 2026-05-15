#pragma once

#define TOML_HEADER_ONLY 0
#include <sstream>
#include <string>
#include <vector>

#include "toml++/toml.hpp"

class OrderedToml
{
public:
    template <typename T>
    void set(const std::string& key, T&& val)
    {
        if (m_data.insert_or_assign(key, std::forward<T>(val)).second)
            m_order.push_back(key);
    }

    const toml::table& table() const { return m_data; }
    toml::table&       table() { return m_data; }

    std::string serialize() const
    {
        std::stringstream ss;
        for (const std::string& key : m_order)
        {
            const toml::node* node = m_data.get(key);
            if (!node)
                continue;

            if (const toml::table* sub = node->as_table())
            {
                ss << "[" << key << "]\n";
                for (auto&& [k, v] : *sub)
                    ss << k.str() << " = " << format_value(v) << "\n";
                ss << "\n";
            }
            else
            {
                ss << key << " = " << format_value(*node) << "\n";
            }
        }
        return ss.str();
    }

private:
    toml::table              m_data;
    std::vector<std::string> m_order;

    static std::string format_value(const toml::node& node)
    {
        std::stringstream ss;
        toml::toml_formatter formatter{ node,
                                        toml::format_flags::none | toml::format_flags::allow_unicode_strings |
                                            toml::format_flags::allow_multi_line_strings |
                                            toml::format_flags::quote_dates_and_times };

        ss << formatter;
        return ss.str();
    }
};
