#pragma once
#include <functional>
#include <map>
#include <memory>
#include <string>
#include <vector>

#include "language_backend.hpp"

class BackendRegistry
{
public:
    using Factory = std::function<std::unique_ptr<LanguageBackend>()>;

    // Instantiates once to read the name, then stores the factory.
    void registerBackend(const char* name, Factory factory) { m_factories.emplace(name, std::move(factory)); }

    std::unique_ptr<LanguageBackend> create(const std::string& lang) const
    {
        auto it = m_factories.find(lang);
        return it != m_factories.end() ? it->second() : nullptr;
    }

    std::vector<std::string> languageNames() const
    {
        std::vector<std::string> out;
        out.reserve(m_factories.size());
        for (const auto& [k, _] : m_factories)
            out.push_back(k);
        return out;
    }

private:
    std::map<std::string, Factory> m_factories;
};

extern BackendRegistry g_registry;
