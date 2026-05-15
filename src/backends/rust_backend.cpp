#include "custom_toml_writter.hpp"
#define RAPIDJSON_HAS_STDSTRING 1
#include <filesystem>
#include <utility>

#include "backends/rust_backend.hpp"
#include "rapidjson/document.h"
#include "toml++/toml.hpp"
#include "util.hpp"

namespace fs = std::filesystem;

void RustBackend::write_toml(const OrderedToml& tbl, const char* path)
{
    output_to_file(path, tbl.serialize(), true);
}

void RustBackend::load(const rapidjson::Document& doc)
{
    if (!doc.HasMember("rust") || !doc["rust"].IsObject())
        return;

    const rapidjson::Value& obj = doc["rust"];
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
    OrderedToml cargo;

    fs::create_directory("src");
    output_to_file("src/main.rs", "fn main() {\n    println!(\"Hello, World!\");\n}");

    toml::table pkg;
    pkg.insert_or_assign("name", s.project_name);
    pkg.insert_or_assign("version", s.project_version);
    pkg.insert_or_assign("description", s.project_description);
    pkg.insert_or_assign("license", s.license);
    pkg.insert_or_assign("edition", m_rust_edition);

    cargo.set("package", std::move(pkg));
    cargo.set("dependencies", toml::table{});

    write_toml(cargo, "Cargo.toml");
}

bool RustBackend::syncPkgManifest(const manifest_settings_t& /*common*/, const manifest_update_t& upd)
{
    OrderedToml cargo;
    try
    {
        cargo.table() = toml::parse_file("Cargo.toml");
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

    toml::table pkg;
    if (auto* n = cargo.table()["package"].node(); n && n->as_table())
        pkg = *n->as_table();

    bool dirty = false;
    auto apply = [&](const std::optional<std::string>& val, const char* key) {
        if (!val)
            return;
        pkg.insert_or_assign(key, *val);
        dirty = true;
    };

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
    {
        cargo.set("package", std::move(pkg));
        write_toml(cargo, "Cargo.toml");
    }
    return dirty;
}
