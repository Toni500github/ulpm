#include "manifest.hpp"

#include "backend_registry.hpp"
#include "fmt/ranges.h"
#include "manifest_settings.hpp"
#include "rapidjson/error/en.h"
#include "util.hpp"

static constexpr std::string_view config_json = R"({
    "commands": {
        "npm": {
            "run": ["npm", "run"],
            "install": ["npm", "install"],
            "build": "echo \"Not supported. Modify command to be used in ulpm.json\" && exit 1"
        },
        "yarn": {
            "run": ["yarn", "run"],
            "install": ["yarn", "install"],
            "build": "echo \"Not supported. Modify command to be used in ulpm.json\" && exit 1"
        },
        "pnpm": {
            "run": ["pnpm", "run"],
            "install": ["pnpm", "install"],
            "build": "echo \"Not supported. Modify command to be used in ulpm.json\" && exit 1"
        },
        "cargo": {
            "run": ["cargo", "run"],
            "install": ["cargo", "add"],
            "build": ["cargo", "build"]
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
    if (!m_doc.HasMember("project") || !m_doc["project"].IsObject())
        die("project field in {} is not an object", MANIFEST_NAME);

    auto read = [&](const char* key, std::string& out) {
        rapidjson::Value& project = m_doc["project"];
        if (project.HasMember(key) && project[key].IsString())
            out = project[key].GetString();
    };

    read("name", m_settings.project_name);
    read("description", m_settings.project_description);
    read("version", m_settings.project_version);
    read("author", m_settings.author);
    read("license", m_settings.license);
    read("language", m_settings.language);
    read("package_manager", m_settings.package_manager);
}

void Manifest::save()
{
    if (!m_backend)
        die("Unknown language '{}'", m_settings.language);

    info("Saving " MANIFEST_NAME "...");
    m_file.reopen(MANIFEST_NAME, "w+");
    m_doc.SetObject();
    rapidjson::Document::AllocatorType& alloc = m_doc.GetAllocator();

    JsonUtils::update_json_field(m_doc, "project", rapidjson::Value(rapidjson::kObjectType));
    auto put = [&](const char* key, const std::string& value) {
        JsonUtils::update_json_field(m_doc["project"], key, value, alloc);
    };

    put("name", m_settings.project_name);
    put("description", m_settings.project_description);
    put("version", m_settings.project_version);
    put("author", m_settings.author);
    put("license", m_settings.license);
    put("language", m_settings.language);
    put("package_manager", m_settings.package_manager);

    {
        auto&       commands = m_config_doc["commands"];
        const char* pm       = m_settings.package_manager.c_str();

        if (!commands.HasMember(pm))
            die("Unknown package manager '{}'. Choose from:\x1b[0m\n - {}",
                pm,
                fmt::join(m_backend->packageManagers(), "\n - "));

        rapidjson::Value commandsValue;
        commandsValue.CopyFrom(commands[pm], alloc);

        JsonUtils::update_json_field(m_doc, "commands", commandsValue);
    }

    m_backend->save(m_doc);  // backend appends its own sub-object

    JsonUtils::write_to_json(m_file, m_doc);
}
