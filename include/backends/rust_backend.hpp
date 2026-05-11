#pragma once

#include "language_backend.hpp"
#include "toml++/toml.hpp"

class RustBackend final : public LanguageBackend
{
public:
    std::string_view         name() const override { return "rust"; }
    std::vector<std::string> packageManagers() const override { return { "cargo" }; }

    void load(const rapidjson::Document& doc) override;
    void save(rapidjson::Document& doc) const override;
    void promptInit(manifest_settings_t& common) override;
    void generateFiles(const manifest_settings_t& common) override;
    bool syncPkgManifest(const manifest_settings_t& common, const manifest_update_t& upd) override;

private:
    std::string m_rust_edition = "2024";

    static toml::table& ensure_table(toml::table& parent, std::string_view key);
    static void         write_toml(const toml::table& tbl, const char* path);
};
