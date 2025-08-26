#include "settings.hpp"

#include <cstdio>
#include <filesystem>
#include <string_view>

#include "fmt/os.h"
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
struct Hashes
{
    int language;
    int pm;
};

std::string err_msg;
static void err_msg_func(const char* buf, size_t n) { err_msg.assign(buf, n); };

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
    if (!fs::exists(name))
    {
        auto f = fmt::output_file(name.data(), fmt::file::CREATE | fmt::file::WRONLY);
        f.print("{{}}");
        f.close();
    }
}

static void populate_doc(std::FILE *file, rapidjson::Document& doc)
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
        die("Failed to parse {}: {} at offset {}", rapidjson::GetParseError_En(doc.GetParseError()),
            doc.GetErrorOffset());
    };
}

static bool download_license(const std::string& license)
{
    const std::string url = "https://raw.githubusercontent.com/spdx/license-list-data/master/text/" + license + ".txt";
    if (Process("curl -L " + url + " -o LICENSE.txt", "", nullptr, err_msg_func).get_exit_status() != 0)
    {
        error("Failed to download to file LICENSE.txt: {}", err_msg);
        return false;
    }

    return true;
}

static void generate_js_package_json(const ManiSettings& settings)
{
    autogen_empty_json("package.json");
    std::FILE* file = fopen("package.json", "r+");
    rapidjson::Document doc;
    populate_doc(file, doc);
    auto& alloc = doc.GetAllocator();

    doc.AddMember("name", rapidjson::Value(settings.project_name.c_str(), settings.project_name.size()), alloc);
    doc.AddMember("version", rapidjson::Value(settings.project_version.c_str(), settings.project_version.size()), alloc);
    doc.AddMember("description", rapidjson::Value(settings.project_description.c_str(), settings.project_description.size()), alloc);
    doc.AddMember("main", rapidjson::Value(settings.js_main.c_str(), settings.js_main.size()), alloc);
    doc.AddMember("scripts", rapidjson::Value(rapidjson::kObjectType), alloc);
    doc["scripts"].AddMember("test", rapidjson::Value("echo \"Error: no test specified\" && exit 1"), alloc);
    doc.AddMember("keywords", rapidjson::Value(rapidjson::kArrayType), alloc);
    doc.AddMember("author", rapidjson::Value(settings.author.c_str(), settings.author.size()), alloc);
    doc.AddMember("license", rapidjson::Value(settings.license.c_str(), settings.license.size()), alloc);
    doc.AddMember("type", rapidjson::Value("commonjs"), alloc);
    write_to_json(file, doc);
    
    fclose(file);
}

Manifest::Manifest() : m_settings(manifest_defaults)
{
    autogen_empty_json(_manifest_name);
    m_file = fopen(_manifest_name.c_str(), "r+");
    populate_doc(m_file, m_doc);
}

void Manifest::init_project(struct cmd_options_t& cmd_options)
{
    // --yes doesn't open menus and accept only my cli arguments
    if (cmd_options.init_yes)
    {
        create_manifest(m_file);
        fclose(m_file);
        return;
    }

    Hashes hashes;

    m_settings.language = draw_entry_menu({ "javascript", "rust (WIP)", "c++ (WIP)" },
                                          "Which language do you want to use?", m_settings.language);
    hashes.language     = fnv1a16::hash(m_settings.language);
    switch (hashes.language)
    {
        case "javascript"_fnv1a16:
            m_settings.prefered_pm = draw_entry_menu(
                { "npm", "yarn", "pnpm" }, "Choose a preferred package manager to use", m_settings.prefered_pm);
            break;
        default: die("language '{}' is indeed WIP", m_settings.language);
    }

    m_settings.project_name        = draw_input_menu("Name of the project:", m_settings.project_name);
    m_settings.project_description = draw_input_menu("Description of the project:", m_settings.project_description);
    m_settings.project_version     = draw_input_menu("Initial Version of the project (sugg. 0.0.1):", m_settings.project_version);
    m_settings.author              = draw_input_menu("Author of the project (e.g 'Name <email@example.com>'):", m_settings.author);
    if (m_settings.language == "javascript")
        m_settings.js_main         = draw_input_menu("Main javascript entry (e.g 'main.js')", m_settings.js_main);

    m_settings.license = draw_entry_menu({ "Apache-2.0",        "BSD-2-Clause",  "BSD-3-Clause",      "GPL-2.0-only",
                                           "GPL-2.0-or-later",  "GPL-3.0-only",  "GPL-3.0-or-later",  "LGPL-2.1-only",
                                           "LGPL-2.1-or-later", "LGPL-3.0-only", "LGPL-3.0-or-later", "MIT",
                                           "MPL-2.0",           "AGPL-3.0-only", "AGPL-3.0-or-later", "EPL-1.0",
                                           "EPL-2.0",           "CDDL-1.0",      "Unlicense",         "CC0-1.0",
                                           "Custom license" },
                                         "Choose a license for the project", m_settings.license);
    create_manifest(m_file);
    fclose(m_file);

    if (m_settings.license != "Custom license")
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
        info("Done!");
    }
}

void Manifest::create_manifest(std::FILE* file)
{
    rapidjson::Document::AllocatorType& allocator = m_doc.GetAllocator();
    m_doc.AddMember("project_name",
                    rapidjson::Value(m_settings.project_name.c_str(), m_settings.project_name.size(), allocator),
                    allocator);
    m_doc.AddMember(
        "project_description",
        rapidjson::Value(m_settings.project_description.c_str(), m_settings.project_description.size(), allocator),
        allocator);
    m_doc.AddMember("project_version", rapidjson::Value(m_settings.project_version.c_str(), m_settings.project_version.size()), allocator);
    m_doc.AddMember("author", rapidjson::Value(m_settings.author.c_str(), m_settings.author.size()), allocator);
    m_doc.AddMember("license", rapidjson::Value(m_settings.license.c_str(), m_settings.license.size(), allocator),
                    allocator);
    m_doc.AddMember("language", rapidjson::Value(m_settings.language.c_str(), m_settings.language.size(), allocator),
                    allocator);
    m_doc.AddMember("pm", rapidjson::Value(m_settings.prefered_pm.c_str(), m_settings.prefered_pm.size(), allocator),
                    allocator);

    write_to_json(file, m_doc);
}
