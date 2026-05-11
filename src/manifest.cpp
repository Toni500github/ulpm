#include "manifest.hpp"

#include "backend_registry.hpp"
#include "manifest_settings.hpp"
#include "rapidjson/error/en.h"
#include "util.hpp"

static constexpr std::string_view config_json = R"({
    "commands": {
        "npm": {
            "run": "npm run",
            "install": "npm install",
            "build": "echo \"Not supported. Modify command to be used in ulpm.json\" && exit 1"
        },
        "yarn": {
            "run": "yarn run",
            "install": "yarn install",
            "build": "echo \"Not supported. Modify command to be used in ulpm.json\" && exit 1"
        },
        "pnpm": {
            "run": "pnpm run",
            "install": "pnpm install",
            "build": "echo \"Not supported. Modify command to be used in ulpm.json\" && exit 1"
        },
        "cargo": {
            "run": "cargo run",
            "install": "cargo add",
            "build": "cargo build"
        }
    }
})";

Manifest::Manifest()
{
    m_config_doc.Parse(config_json.data());

    if (m_config_doc.HasParseError())
    {
        die("Error config_json: {} at offset {}",
            rapidjson::GetParseError_En(m_config_doc.GetParseError()),
            m_config_doc.GetErrorOffset());
    }

    if (!m_config_doc.IsObject())
    {
        die("config_json root is not an object");
    }

    JsonUtils::autogen_empty_json(MANIFEST_NAME);

    m_file.open(MANIFEST_NAME, "r+");
    JsonUtils::populate_doc(m_file, m_doc);

    if (m_doc.ObjectEmpty())
        return;

    load_common_fields();

    if (!m_settings.language.empty())
    {
        m_backend = g_registry.create(m_settings.language);
        if (!m_backend)
            die("Unknown language '{}' in {}", m_settings.language, MANIFEST_NAME);

        m_backend->load(m_doc);
    }
}

void Manifest::load_common_fields()
{
    auto read = [&](const char* key, std::string& out) {
        if (m_doc.HasMember(key) && m_doc[key].IsString())
            out = m_doc[key].GetString();
    };

    read("project_name", m_settings.project_name);
    read("project_description", m_settings.project_description);
    read("project_version", m_settings.project_version);
    read("author", m_settings.author);
    read("license", m_settings.license);
    read("language", m_settings.language);
    read("package_manager", m_settings.package_manager);
}

void Manifest::save()
{
    m_file.reopen(MANIFEST_NAME, "w+");

    m_doc.SetObject();
    rapidjson::Document::AllocatorType& alloc = m_doc.GetAllocator();

    auto put = [&](const char* key, const std::string& value) {
        m_doc.AddMember(rapidjson::Value(key, alloc), rapidjson::Value(value.c_str(), alloc), alloc);
    };

    put("project_name", m_settings.project_name);
    put("project_description", m_settings.project_description);
    put("project_version", m_settings.project_version);
    put("author", m_settings.author);
    put("license", m_settings.license);
    put("language", m_settings.language);
    put("package_manager", m_settings.package_manager);

    {
        auto&       commands = m_config_doc["commands"];
        const char* pm       = m_settings.package_manager.c_str();

        if (!commands.HasMember(pm))
            die("Unknown package manager '{}'", pm);

        rapidjson::Value commandsValue;
        commandsValue.CopyFrom(commands[pm], alloc);

        m_doc.AddMember("commands", commandsValue, alloc);
    }

    if (m_backend)
        m_backend->save(m_doc);  // backend appends its own sub-object

    JsonUtils::write_to_json(m_file, m_doc);
}
