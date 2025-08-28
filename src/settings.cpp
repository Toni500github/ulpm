#define RAPIDJSON_HAS_STDSTRING 1
#include "settings.hpp"

#include <unistd.h>

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
#include "switch_fnv1a.hpp"
#include "tiny-process-library/process.hpp"
#include "util.hpp"

using namespace Settings;
using namespace TinyProcessLib;
namespace fs = std::filesystem;

#define VERBOSE_STDOUT !cmd_options_verbose ? nullptr : [](const char*, size_t){}

static void write_to_json(std::FILE* file, rapidjson::Document& doc)
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
        die("Bailing out");
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

Manifest::Manifest() : m_settings(manifest_defaults)
{
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
        if (cmd_options.init_force || askUserYorN(false, "The manifest {} is not empty. Do you want to overwrite all options?", MANIFEST_NAME))
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
                                          { "javascript", "rust (WIP)", "c++ (WIP)" }, m_settings.language);
    switch (fnv1a16::hash(m_settings.language))
    {
        case "javascript"_fnv1a16:
            m_settings.package_manager = draw_entry_menu("Choose a preferred package manager to use",
                                                         { "npm", "yarn", "pnpm" }, m_settings.package_manager);
            m_settings.js_runtime =
                draw_entry_menu("Choose a Javascript runtime", { "node", "bun", "deno", "qjs", "d8", "jsc", "js" },
                                m_settings.js_runtime);
            break;
        default: die("language '{}' is indeed WIP", m_settings.language);
    }

    m_settings.project_name        = draw_input_menu("Name of the project", m_settings.project_name);
    m_settings.project_description = draw_input_menu("Description of the project", m_settings.project_description);
    m_settings.project_version     = draw_input_menu("Initial Version of the project", m_settings.project_version);
    m_settings.author              = draw_input_menu("Author of the project", m_settings.author);
    if (m_settings.language == "javascript")
        m_settings.js_main_src = draw_input_menu("Path to main javascript entry", m_settings.js_main_src);

    m_settings.license = draw_entry_menu("Choose a license for the project",
                                         { "Apache-2.0",       "BSD-2-Clause",      "BSD-3-Clause",
                                           "GPL-2.0-only",     "GPL-2.0-or-later",  "GPL-3.0-only",
                                           "GPL-3.0-or-later", "LGPL-2.1-only",     "LGPL-2.1-or-later",
                                           "LGPL-3.0-only",    "LGPL-3.0-or-later", "MIT",
                                           "MPL-2.0",          "AGPL-3.0-only",     "AGPL-3.0-or-later",
                                           "EPL-1.0",          "EPL-2.0",           "CDDL-1.0",
                                           "Unlicense",        "CC0-1.0",           "None" },
                                         m_settings.license);
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
