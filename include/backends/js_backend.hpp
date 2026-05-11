#pragma once
#include <unordered_map>

#include "language_backend.hpp"
#include "manifest_settings.hpp"

class JsBackend final : public LanguageBackend
{
public:
    std::string_view         name() const override { return "javascript"; }
    std::vector<std::string> packageManagers() const override { return { "npm", "yarn", "pnpm" }; }

    void load(const rapidjson::Document& doc) override;
    void save(rapidjson::Document& doc) const override;
    void promptInit(manifest_settings_t& common) override;
    void generateFiles(const manifest_settings_t& common) override;
    bool syncPkgManifest(const manifest_settings_t& common, const manifest_update_t& upd) override;

private:
    std::string m_js_main_src     = "src/main.js";
    std::string m_js_runtime_bin  = "node";
    std::string m_js_runtime_name = "Node.js";

    const std::unordered_map<std::string, std::string> m_runtimes{
        { "Node.js", "node" }, { "Bun", "bun" }, { "Deno", "deno" }, { "QuickJS", "qjs" }, { "V8", "d8" },
    };
};
