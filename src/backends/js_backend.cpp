#include "backends/js_backend.hpp"

#include <filesystem>
#include <vector>

#include "rapidjson/document.h"
#include "util.hpp"

namespace fs = std::filesystem;
using namespace JsonUtils;

void JsBackend::load(const rapidjson::Document& doc)
{
    if (!doc.HasMember("javascript") || !doc["javascript"].IsObject())
        return;

    const rapidjson::Value& js = doc["javascript"];
    if (js.HasMember("main_src") && js["main_src"].IsString())
        m_js_main_src = js["main_src"].GetString();
    if (js.HasMember("runtime") && js["runtime"].IsObject())
    {
        m_js_runtime_name = js["runtime"]["name"].GetString();
        m_js_runtime_bin  = js["runtime"]["bin"].GetString();
    }
}

void JsBackend::save(rapidjson::Document& doc) const
{
    rapidjson::Document::AllocatorType& alloc = doc.GetAllocator();
    rapidjson::Value                    obj(rapidjson::kObjectType);
    rapidjson::Value                    obj_runtime(rapidjson::kObjectType);

    update_json_field(obj_runtime, "name", m_js_runtime_name, alloc);
    update_json_field(obj_runtime, "bin", m_js_runtime_bin, alloc);

    update_json_field(obj, "main_src", m_js_main_src, alloc);
    update_json_field(obj, "runtime", obj_runtime, alloc);
    update_json_field(doc, "javascript", obj, alloc);
}

void JsBackend::promptInit(manifest_settings_t& common)
{
    common.package_manager = draw_entry_menu("Choose a package manager", packageManagers(), common.package_manager);
    m_js_runtime_name      = draw_entry_menu("Choose a runtime",
                                             {
                                                 "Node.js",
                                                 "Bun",
                                                 "Deno",
                                                 "QuickJS",
                                                 "V8",
                                             },
                                             "");

    if (auto it = m_runtimes.find(m_js_runtime_name); it != m_runtimes.end())
        m_js_runtime_bin = it->second;

    m_js_main_src = draw_input_menu("Main entry path", m_js_main_src);
}

void JsBackend::generateFiles(const manifest_settings_t& s)
{
    FileHandler                         f;
    rapidjson::Document                 doc;
    rapidjson::Document::AllocatorType& alloc = doc.GetAllocator();

    info("Creating package.json");
    autogen_empty_json("package.json", true);

    f.open("package.json", "r+");

    populate_doc(f, doc);
    update_json_field(doc, "name", s.project_name, alloc);
    update_json_field(doc, "version", s.project_version, alloc);
    update_json_field(doc, "description", s.project_description, alloc);

    update_json_field(doc, "scripts", rapidjson::Value(rapidjson::kObjectType), alloc);
    update_json_field(doc["scripts"], "start", m_js_runtime_bin + " " + m_js_main_src, alloc);

    update_json_field(doc, "keywords", rapidjson::Value(rapidjson::kArrayType), alloc);
    update_json_field(doc, "author", s.author, alloc);
    update_json_field(doc, "license", s.license, alloc);
    update_json_field(doc, "main", m_js_main_src, alloc);
    update_json_field(doc, "type", "commonjs", alloc);
    write_to_json(f, doc);

    info("Creating {} ...", m_js_main_src);
    fs::create_directories(fs::path(m_js_main_src).parent_path());
    output_to_file(m_js_main_src, "console.log('Hello, World!');");
}

bool JsBackend::syncPkgManifest(const manifest_settings_t& /*common*/, const manifest_update_t& upd)
{
    FileHandler         f;
    rapidjson::Document doc;

    autogen_empty_json("package.json");
    f.open("package.json", "r+");
    JsonUtils::populate_doc(f, doc);

    bool dirty = false;
    auto apply = [&](const std::optional<std::string>& val, const char* key) {
        if (val)
        {
            JsonUtils::update_json_field(doc, key, *val);
            dirty = true;
        }
    };

    // Common fields package.json mirrors
    apply(upd.project_name, "name");
    apply(upd.project_version, "version");
    apply(upd.project_description, "description");
    apply(upd.author, "author");
    apply(upd.license, "license");

    if (upd.js_main_src)
    {
        m_js_main_src = *upd.js_main_src;
        update_json_field(doc, "main", *upd.js_main_src);
        dirty = true;
    }

    if (dirty)
        write_to_json(f, doc);

    return dirty;
}
