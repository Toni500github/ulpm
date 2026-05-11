#define RAPIDJSON_HAS_STDSTRING 1
#include "backends/rust_backend.hpp"

#include <filesystem>
#include <sstream>
#include <utility>

#include "rapidjson/document.h"
#include "toml++/toml.hpp"
#include "util.hpp"

namespace fs = std::filesystem;

toml::table& RustBackend::ensure_table(toml::table& parent, const std::string_view key)
{
    if (auto* n = parent[key].node(); n && n->as_table())
        return *n->as_table();
    auto [it, _] = parent.insert(key, toml::table{});
    return *it->second.as_table();
}

void RustBackend::write_toml(const toml::table& tbl, const char* path)
{
    std::stringstream ss;
    ss << tbl;
    output_to_file(path, ss.str(), true);
}

void RustBackend::load(const rapidjson::Document& doc)
{
    if (!doc.HasMember("rust") || !doc["rust"].IsObject())
        return;

    const auto& obj = doc["rust"];
    if (obj.HasMember("edition") && obj["edition"].IsString())
        m_rust_edition = obj["edition"].GetString();
}

void RustBackend::save(rapidjson::Document& doc) const
{
    rapidjson::Document::AllocatorType& alloc = doc.GetAllocator();
    rapidjson::Value                    obj(rapidjson::kObjectType);

    obj.AddMember("edition", rapidjson::Value(m_rust_edition, alloc), alloc);
    doc.AddMember("rust", std::move(obj), alloc);
}

void RustBackend::promptInit(manifest_settings_t& common)
{
    common.package_manager = "cargo";  // only option, no need to ask
    m_rust_edition = draw_entry_menu("Choose a Rust edition", { "2024", "2021", "2018", "2015" }, m_rust_edition);
}

void RustBackend::generateFiles(const manifest_settings_t& s)
{
    info("Initializing cargo project...");

    fs::create_directory("src");
    output_to_file("src/main.rs", "fn main() {\n    println!(\"Hello, World!\");\n}");
    output_to_file("Cargo.toml", "[package]\n\n[dependencies]\n");

    toml::table  tbl = toml::parse_file("Cargo.toml");
    toml::table& pkg = ensure_table(tbl, "package");
    pkg.insert_or_assign("name", s.project_name);
    pkg.insert_or_assign("version", s.project_version);
    pkg.insert_or_assign("description", s.project_description);
    pkg.insert_or_assign("license", s.license);
    pkg.insert_or_assign("edition", m_rust_edition);
    ensure_table(tbl, "dependencies");
    write_toml(tbl, "Cargo.toml");
}

bool RustBackend::syncPkgManifest(const manifest_settings_t& /*common*/, const manifest_update_t& upd)
{
    output_to_file("Cargo.toml", "[package]\n\n[dependencies]\n");

    toml::table tbl;
    try
    {
        tbl = toml::parse_file("Cargo.toml");
    }
    catch (const toml::parse_error& err)
    {
        die("Parsing Cargo.toml failed:\n"
            "{}\n"
            "\t(error occurred at line {} column {})",
            err.description(),
            err.source().begin.line,
            err.source().begin.column);
    }

    toml::table& pkg   = ensure_table(tbl, "package");
    bool         dirty = false;
    auto         apply = [&](const std::optional<std::string>& val, const char* key) {
        if (!val)
            return;
        pkg.insert_or_assign(key, *val);
        dirty = true;
    };

    // Common fields Cargo.toml mirrors
    apply(upd.project_name, "name");
    apply(upd.project_version, "version");
    apply(upd.project_description, "description");
    apply(upd.license, "license");

    if (upd.rust_edition)
    {
        m_rust_edition = *upd.rust_edition;
        pkg.insert_or_assign("edition", *upd.rust_edition);
        dirty = true;
    }

    if (dirty)
        write_toml(tbl, "Cargo.toml");
    return dirty;
}
