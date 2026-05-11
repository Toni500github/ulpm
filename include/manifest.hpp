#pragma once
#include <memory>

#include "language_backend.hpp"
#include "manifest_settings.hpp"
#include "util.hpp"

class Manifest
{
public:
    Manifest();

    // Non-copyable
    Manifest(const Manifest&)            = delete;
    Manifest& operator=(const Manifest&) = delete;

    manifest_settings_t&       settings() { return m_settings; }
    const manifest_settings_t& settings() const { return m_settings; }
    LanguageBackend*           backend() { return m_backend.get(); }
    rapidjson::Document&       doc() { return m_doc; }

    // Rebuild and write ulpm.json from current m_settings + backend state.
    void save();

    bool empty() const { return m_doc.ObjectEmpty(); }
    void setBackend(std::unique_ptr<LanguageBackend> b) { m_backend = std::move(b); }

private:
    FileHandler                      m_file;
    rapidjson::Document              m_doc;
    manifest_settings_t              m_settings;
    std::unique_ptr<LanguageBackend> m_backend;

    rapidjson::Document m_config_doc;
    void                load_common_fields();
};
