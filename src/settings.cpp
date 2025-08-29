#define RAPIDJSON_HAS_STDSTRING 1
#include "settings.hpp"

#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <string_view>
#include <vector>

#include "fmt/format.h"
#include "fmt/os.h"
#include "fmt/ranges.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"
#include "rapidjson/prettywriter.h"
#include "rapidjson/rapidjson.h"
#include "tiny-process-library/process.hpp"
#include "util.hpp"

using namespace Settings;
using namespace TinyProcessLib;
namespace fs = std::filesystem;

#define VERBOSE_STDOUT !cmd_options_verbose ? nullptr : [](const char*, size_t) {}

rapidjson::Document   config_doc;
constexpr const char* config_json = R"({
    "languages": {
        "javascript": {
            "package_managers": ["npm", "yarn", "pnpm"],
            "js_runtimes": ["node", "bun", "deno", "qjs", "d8", "jsc", "js"]
        },
        "rust": {
            "package_managers": []
        },
        "c++": {
            "package_managers": []
        }
    },
    "licenses": [
        "Apache-2.0", "BSD-2-Clause", "BSD-3-Clause",
        "GPL-2.0-only", "GPL-2.0-or-later", "GPL-3.0-only",
        "GPL-3.0-or-later", "LGPL-2.1-only", "LGPL-2.1-or-later",
        "LGPL-3.0-only", "LGPL-3.0-or-later", "MIT",
        "MPL-2.0", "AGPL-3.0-only", "AGPL-3.0-or-later",
        "EPL-1.0", "EPL-2.0", "CDDL-1.0",
        "Unlicense", "CC0-1.0", "Custom"
    ]
})";

static void write_to_json(std::FILE* file, const rapidjson::Document& doc)
{
    // seek back to the beginning to overwrite
    fseek(file, 0, SEEK_SET);

    char                                                writeBuffer[UINT16_MAX] = { 0 };
    rapidjson::FileWriteStream                          writeStream(file, writeBuffer, sizeof(writeBuffer));
    rapidjson::PrettyWriter<rapidjson::FileWriteStream> fileWriter(writeStream);
    fileWriter.SetFormatOptions(rapidjson::kFormatSingleLineArray);  // Disable newlines between array elements
    doc.Accept(fileWriter);

    ftruncate(fileno(file), ftell(file));
    fflush(file);
}

static void autogen_empty_json(const std::string_view name)
{
    auto f = fmt::output_file(name.data(), fmt::file::CREATE | fmt::file::WRONLY | fmt::file::TRUNC);
    f.print("{{}}");
    f.close();
}

static void populate_doc(std::FILE* file, rapidjson::Document& doc)
{
    if (!file)
    {
        perror("fopen");
        return;
    }

    char                      buf[UINT16_MAX] = { 0 };
    rapidjson::FileReadStream stream(file, buf, sizeof(buf));

    if (doc.ParseStream(stream).HasParseError())
    {
        fclose(file);
        die("Failed to parse json file: {} at offset {}", rapidjson::GetParseError_En(doc.GetParseError()),
            doc.GetErrorOffset());
    };
}

static bool download_license(const std::string& license)
{
    const std::string& url = "https://raw.githubusercontent.com/spdx/license-list-data/master/text/" + license + ".txt";
    if (Process("curl -L " + url + " -o LICENSE.txt", "", VERBOSE_STDOUT).get_exit_status() != 0)
    {
        error("Failed to download to file LICENSE.txt");
        return false;
    }

    return true;
}

static void generate_js_package_json(const ManiSettings& settings)
{
    autogen_empty_json("package.json");
    std::FILE*          file = fopen("package.json", "r+");
    rapidjson::Document doc;
    populate_doc(file, doc);
    auto& alloc = doc.GetAllocator();

    doc.AddMember("name", settings.project_name, alloc);
    doc.AddMember("version", settings.project_version, alloc);
    doc.AddMember("description", settings.project_description, alloc);
    doc.AddMember("main", settings.js_main_src, alloc);
    doc.AddMember("scripts", rapidjson::Value(rapidjson::kObjectType), alloc);
    doc["scripts"].AddMember("start", settings.js_runtime + " " + settings.js_main_src, alloc);
    doc.AddMember("keywords", rapidjson::Value(rapidjson::kArrayType), alloc);
    doc.AddMember("author", settings.author, alloc);
    doc.AddMember("license", settings.license, alloc);
    doc.AddMember("type", "commonjs", alloc);
    write_to_json(file, doc);

    fclose(file);
}

static std::vector<std::string> vec_from_members(const rapidjson::Value& obj)
{
    std::vector<std::string> keys;
    if (!obj.IsObject())
        return keys;
    keys.reserve(obj.MemberCount());
    for (auto const& item : obj.GetObject())
        keys.emplace_back(item.name.GetString());
    return keys;
}

static std::vector<std::string> vec_from_array(const rapidjson::Value& array)
{
    std::vector<std::string> keys;
    if (!array.IsArray())
        return keys;
    keys.reserve(array.Size());
    for (auto& e : array.GetArray())
        if (e.IsString())
            keys.emplace_back(e.GetString());
    return keys;
}

void Manifest::validate_manifest()
{
    // check language
    if (!m_doc.HasMember("language") || !m_doc["language"].IsString())
        die("Missing/Non-string field 'language' in {}", MANIFEST_NAME);
    if (!config_doc["languages"].HasMember(m_settings.language))
        die("Invalid language '{}'. Valid: {}", m_settings.language, fmt::join(vec_from_members(config_doc["languages"]), ", "));

    // check package manager
    if (!m_doc.HasMember("package_manager") || !m_doc["package_manager"].IsString())
        die("Missing/Non-string field 'package_manager' in {}", MANIFEST_NAME);
    const std::vector<std::string>& lang_pm = vec_from_array(config_doc["languages"][m_settings.language]["package_managers"]); 
    if (std::find(lang_pm.begin(), lang_pm.end(), m_settings.package_manager) == lang_pm.end())
        die("Invalid package manager '{}' for language '{}'. Valid: {}", m_settings.package_manager, m_settings.language, fmt::join(lang_pm, ", "));

    // check license
    if (!m_doc.HasMember("license") || !m_doc["license"].IsString())
        die("Missing/Non-string field 'license' in {}", MANIFEST_NAME);
    const std::vector<std::string>& licenses = vec_from_array(config_doc["licenses"]); 
    if (std::find(licenses.begin(), licenses.end(), m_settings.license) == licenses.end())
        die("Invalid license '{}'. Valid: {}", m_settings.license, fmt::join(licenses, ", "));

    // check project_name
    if (!m_doc.HasMember("project_name") || !m_doc["project_name"].IsString())
        die("Missing/Non-string field 'project_name' in {}", MANIFEST_NAME);

    // check project_description
    if (!m_doc.HasMember("project_description") || !m_doc["project_description"].IsString())
        die("Missing/Non-string field 'project_name' in {}", MANIFEST_NAME);

    // check project_version
    if (!m_doc.HasMember("project_version") || !m_doc["project_version"].IsString())
        die("Missing/Non-string field 'project_version' in {}", MANIFEST_NAME);

    // check author
    if (!m_doc.HasMember("author") || !m_doc["author"].IsString())
        die("Missing/Non-string field 'author' in {}", MANIFEST_NAME);
}

Manifest::Manifest() : m_settings(manifest_defaults)
{
    if (config_doc.Parse(config_json).HasParseError())
    {
        die("Error config_json: {} at offset {}", rapidjson::GetParseError_En(config_doc.GetParseError()),
            config_doc.GetErrorOffset());
    }

    if (!fs::exists(MANIFEST_NAME))
        autogen_empty_json(MANIFEST_NAME);
    m_file = fopen(MANIFEST_NAME, "r+");
    populate_doc(m_file, m_doc);
    if (m_doc.ObjectEmpty())
        return;

    if (m_doc.HasMember("language") && m_doc["language"].IsString())
        m_settings.language = m_doc["language"].GetString();

    if (m_doc.HasMember("package_manager") && m_doc["package_manager"].IsString())
        m_settings.package_manager = m_doc["package_manager"].GetString();

    if (m_doc.HasMember("license") && m_doc["license"].IsString())
        m_settings.license = m_doc["license"].GetString();

    if (m_doc.HasMember("project_name") && m_doc["project_name"].IsString())
        m_settings.project_name = m_doc["project_name"].GetString();

    if (m_doc.HasMember("project_description") && m_doc["project_description"].IsString())
        m_settings.project_description = m_doc["project_description"].GetString();

    if (m_doc.HasMember("project_version") && m_doc["project_version"].IsString())
        m_settings.project_version = m_doc["project_version"].GetString();

    if (m_doc.HasMember("author") && m_doc["author"].IsString())
        m_settings.author = m_doc["author"].GetString();

    if (m_doc.HasMember("javascript") && m_doc["javascript"].IsObject())
        m_settings.js_runtime = m_doc["javascript"]["runtime"].GetString();
}

void Manifest::init_project(const cmd_options_t& cmd_options)
{
    if (!m_doc.ObjectEmpty())
    {
        if (cmd_options.init_force ||
            askUserYorN(false, "The manifest {} is not empty. Do you want to overwrite all options?", MANIFEST_NAME))
        {
            fclose(m_file);
            m_file = fopen(MANIFEST_NAME, "w+");
            autogen_empty_json(MANIFEST_NAME);
            populate_doc(m_file, m_doc);
            autogen_empty_json("package.json");
            fclose(m_file);
            m_file = fopen(MANIFEST_NAME, "r+");
        }
    }
    // --yes doesn't open menus and accept only my cli arguments
    if (cmd_options.init_yes)
    {
        goto skip_menu;
    }

    m_settings.language = draw_entry_menu("Which language do you want to use?",
                                          vec_from_members(config_doc["languages"]), m_settings.language);
    if (m_settings.language == "javascript")
    {
        m_settings.js_runtime = draw_entry_menu(
            "Choose a Javascript runtime", vec_from_array(config_doc["languages"][m_settings.language]["js_runtimes"]),
            m_settings.js_runtime);
    }
    else
    {
        die("language '{}' is WIP", m_settings.language);
    }

    m_settings.package_manager = draw_entry_menu(
        "Choose a preferred package manager to use",
        vec_from_array(config_doc["languages"][m_settings.language]["package_managers"]), m_settings.package_manager);

    m_settings.project_name        = draw_input_menu("Name of the project", m_settings.project_name);
    m_settings.project_description = draw_input_menu("Description of the project", m_settings.project_description.empty() ? "v0.0.1" : m_settings.project_version);
    m_settings.project_version     = draw_input_menu("Initial Version of the project", m_settings.project_version);
    m_settings.author              = draw_input_menu("Author of the project", m_settings.author.empty() ? "Name <email@example.com>" : m_settings.author);
    if (m_settings.language == "javascript")
        m_settings.js_main_src = draw_input_menu("Path to main javascript entry", m_settings.js_main_src.empty() ? "src/main.js" : m_settings.js_main_src);

    m_settings.license =
        draw_entry_menu("Choose a license for the project", vec_from_array(config_doc["licenses"]), m_settings.license);

skip_menu:
    create_manifest(m_file);
    fclose(m_file);

    if (m_settings.license != "None")
    {
        if (fs::exists("LICENSE.txt") && !cmd_options.init_force)
            warn("LICENSE.txt already exists, skipping download");
        else
        {
            fs::remove("LICENSE.txt");
            info("Downloading license {} to LICENSE.txt ...", m_settings.license);
            download_license(m_settings.license);
            info("Done! Remember to modify the copyright holder and year");
        }
    }

    if (m_settings.language == "javascript")
    {
        info("Creating package.json ...");
        generate_js_package_json(m_settings);

        info("Creating main entry at '{}' ...", m_settings.js_main_src);
        const fs::path& js_main_src_path = fs::path(m_settings.js_main_src);
        fs::create_directories(js_main_src_path.parent_path());
        auto f = fmt::output_file(js_main_src_path.string(), fmt::file::CREATE | fmt::file::WRONLY);
        f.print("console.log('Hello World!');");
        f.close();
    }
    info("Done!");
}

void Manifest::create_manifest(std::FILE* file)
{
    rapidjson::Document::AllocatorType& allocator = m_doc.GetAllocator();
    m_doc.AddMember("project_name", m_settings.project_name, allocator);
    m_doc.AddMember("project_description", m_settings.project_description, allocator);
    m_doc.AddMember("project_version", m_settings.project_version, allocator);
    m_doc.AddMember("author", m_settings.author, allocator);
    m_doc.AddMember("license", m_settings.license, allocator);
    m_doc.AddMember("language", m_settings.language, allocator);
    m_doc.AddMember("package_manager", m_settings.package_manager, allocator);
    m_doc.AddMember(rapidjson::Value(m_settings.language.c_str(), m_settings.language.size()),
                    rapidjson::Value(rapidjson::kObjectType), allocator);

    if (m_settings.language == "javascript")
    {
        m_doc[m_settings.language].AddMember("runtime", m_settings.js_runtime, allocator);
    }

    write_to_json(file, m_doc);
}

void Manifest::run_cmd(const std::string& cmd, const std::vector<std::string>& arguments)
{
    const std::string& exec = fmt::format("{} run {} {}", m_settings.package_manager, cmd, fmt::join(arguments, " "));
    debug("Running {}", exec);
    if (Process(exec, "", VERBOSE_STDOUT).get_exit_status() != 0)
        die("Failed to run cmd '{}'", cmd);
}

void Manifest::set_project_settings(const cmd_options_t& cmd_options)
{
    rapidjson::Document js_pkg_doc;
    bool                manifest_updated     = false;
    bool                package_json_updated = false;

    if (m_settings.language == "javascript" && !fs::exists("package.json"))
        autogen_empty_json("package.json");
    std::FILE* js_pkg_file = fopen("package.json", "r+");
    populate_doc(js_pkg_file, js_pkg_doc);

    if (!manifest_defaults.language.empty() && m_settings.language != manifest_defaults.language)
    {
        update_json_field(m_doc, "language", manifest_defaults.language);
        manifest_updated = true;
    }

    if (!manifest_defaults.package_manager.empty() && m_settings.package_manager != manifest_defaults.package_manager)
    {
        update_json_field(m_doc, "package_manager", manifest_defaults.package_manager);
        manifest_updated = true;
    }

    if (!manifest_defaults.license.empty() && m_settings.license != manifest_defaults.license)
    {
        update_json_field(m_doc, "license", manifest_defaults.license);
        manifest_updated = true;
        if ((fs::exists("LICENSE.txt") && !cmd_options.init_force) || manifest_defaults.license != "Custom")
            warn("LICENSE.txt already exists, use --force to overwrite");
        else
        {
            info("Removing LICENSE.txt");
            fs::remove("LICENSE.txt");
            if (m_settings.license != "None")
            {
                info("Downloading license {} to LICENSE.txt ...", m_settings.license);
                download_license(m_settings.license);
            }
        }
    }

    if (!manifest_defaults.project_name.empty() && m_settings.project_name != manifest_defaults.project_name)
    {
        update_json_field(m_doc, "project_name", manifest_defaults.project_name);
        update_json_field(js_pkg_doc, "name", manifest_defaults.project_name);
        manifest_updated     = true;
        package_json_updated = true;
    }

    if (!manifest_defaults.project_description.empty() &&
        m_settings.project_description != manifest_defaults.project_description)
    {
        update_json_field(m_doc, "project_description", manifest_defaults.project_description);
        update_json_field(js_pkg_doc, "description", manifest_defaults.project_description);
        manifest_updated     = true;
        package_json_updated = true;
    }

    if (!manifest_defaults.author.empty() && m_settings.author != manifest_defaults.author)
    {
        update_json_field(m_doc, "author", manifest_defaults.author);
        update_json_field(js_pkg_doc, "author", manifest_defaults.author);
        manifest_updated     = true;
        package_json_updated = true;
    }

    if (manifest_updated)
    {
        write_to_json(m_file, m_doc);
        info("Updated {}", MANIFEST_NAME);
    }

    if (package_json_updated)
    {
        freopen("package.json", "w+", js_pkg_file);
        write_to_json(js_pkg_file, js_pkg_doc);
        info("Updated package.json");
    }
}

void Manifest::update_json_field(rapidjson::Document& pkg_doc, const std::string& field, const std::string& value)
{
    rapidjson::Document::AllocatorType& allocator = pkg_doc.GetAllocator();

    if (pkg_doc.HasMember(field))
    {
        debug("changing {} from {} to {}", field, pkg_doc[field].GetString(), value);
        pkg_doc[field].SetString(value.c_str(), value.length(), allocator);
    }
    else
    {
        debug("adding {} with value {}", field, value);
        pkg_doc.AddMember(rapidjson::Value(field.c_str(), field.length(), allocator),
                          rapidjson::Value(value.c_str(), value.length(), allocator), allocator);
    }
}
