/*
 * Copyright 2025 Toni500git
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
 * following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following
 * disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
 * following disclaimer in the documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote
 * products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

#define RAPIDJSON_HAS_STDSTRING 1
#define TOML_HEADER_ONLY 0
#include "settings.hpp"

#include <unistd.h>

#include <algorithm>
#include <cstdio>
#include <filesystem>
#include <sstream>
#include <string_view>
#include <vector>

#include "fmt/format.h"
#include "fmt/os.h"
#include "fmt/ranges.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/rapidjson.h"
#include "tiny-process-library/process.hpp"
#include "toml++/toml.hpp"
#include "util.hpp"

using namespace Settings;
using namespace TinyProcessLib;
using namespace JsonUtils;
namespace fs = std::filesystem;

rapidjson::Document   config_doc;
constexpr const char* config_json = R"({
    "languages": {
        "javascript": {
            "package_managers": ["npm", "yarn", "pnpm"],
            "js_runtimes": [
                { "name": "Node.js", "binary": "node" },
                { "name": "Bun", "binary": "bun" },
                { "name": "Deno", "binary": "deno" },
                { "name": "QuickJS", "binary": "qjs" },
                { "name": "V8", "binary": "d8" },
                { "name": "JavaScriptCore", "binary": "jsc" },
                { "name": "SpiderMonkey", "binary": "js" }
            ]
        },
        "rust": {
            "package_managers": ["cargo"],
            "rust_editions": ["2024", "2021", "2018", "2015"]
        },
        "c++": {
            "package_managers": []
        }
    },
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
        "cargo": {
            "run": "cargo run",
            "install": "cargo add",
            "build": "cargo build"
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

static void download_license(const std::string& license)
{
    const std::string& url = "https://raw.githubusercontent.com/spdx/license-list-data/master/text/" + license + ".txt";
    if (Process({ "curl", "-fL", url, "-o", "LICENSE.txt" }).get_exit_status() != 0)
        die("Failed to download to file LICENSE.txt");
}

// Ensures a sub-table exists for a given key. Returns a reference to the sub-table.
static toml::table& ensure_table(toml::table& parent, const std::string_view key)
{
    if (toml::node* node = parent[key].node())
        if (auto* tbl = node->as_table())
            return *tbl;

    auto [it, inserted] = parent.insert(key, toml::table{});
    return *it->second.as_table();
}

static void output_to_file(const std::string_view path, const std::string& content)
{
    auto f = fmt::output_file(path.data(), fmt::file::CREATE | fmt::file::WRONLY);
    f.print("{}", content);
    f.close();
}

static void generate_cargo_toml(toml::table& tbl, const ManiSettings& settings)
{
    toml::table& package = ensure_table(tbl, "package");
    package.insert_or_assign("name", settings.project_name);
    package.insert_or_assign("version", settings.project_version);
    package.insert_or_assign("description", settings.project_description);
    package.insert_or_assign("license", settings.license);
    ensure_table(tbl, "dependencies");
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

void Manifest::validate_manifest()
{
    // check language
    if (!m_doc.HasMember("language") || !m_doc["language"].IsString())
        die("Missing/Non-string field 'language' in {}", MANIFEST_NAME);
    if (!config_doc["languages"].HasMember(m_settings.language))
        die("Invalid language '{}'. Valid: {}", m_settings.language,
            fmt::join(vec_from_members(config_doc["languages"]), ", "));

    // check package_manager
    if (!m_doc.HasMember("package_manager") || !m_doc["package_manager"].IsString())
        die("Missing/Non-string field 'package_manager' in {}", MANIFEST_NAME);
    const std::vector<std::string>& lang_pm =
        vec_from_array(config_doc["languages"][m_settings.language]["package_managers"]);
    if (std::find(lang_pm.begin(), lang_pm.end(), m_settings.package_manager) == lang_pm.end())
        die("Invalid package manager '{}' for language '{}'. Valid: {}", m_settings.package_manager,
            m_settings.language, fmt::join(lang_pm, ", "));

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
        m_settings.js_runtime =
            draw_entry_menu("Choose a Javascript runtime",
                            vec_from_obj_array(config_doc["languages"][m_settings.language]["js_runtimes"], "name"),
                            m_settings.js_runtime);
        m_settings.js_runtime =
            find_value_from_obj_array(config_doc["languages"][m_settings.language]["js_runtimes"], m_settings.js_runtime, "binary");

        m_settings.package_manager =
            draw_entry_menu("Choose a preferred package manager to use",
                            vec_from_array(config_doc["languages"][m_settings.language]["package_managers"]),
                            m_settings.package_manager);
    }
    else if (m_settings.language == "rust")
    {
        m_settings.package_manager = "cargo";
        m_settings.rust_edition    = draw_entry_menu(
            "Choose a rust edition", vec_from_array(config_doc["languages"][m_settings.language]["rust_editions"]),
            m_settings.rust_edition);
    }
    else
    {
        die("language '{}' is WIP", m_settings.language);
    }

    m_settings.project_name        = draw_input_menu("Name of the project", m_settings.project_name);
    m_settings.project_description = draw_input_menu("Description of the project", m_settings.project_description);
    m_settings.project_version     = draw_input_menu("Initial Version of the project", m_settings.project_version.empty() ? "v0.0.1" : m_settings.project_version);
    m_settings.author              = draw_input_menu("Author of the project", m_settings.author.empty() ? "Name <email@example.com>" : m_settings.author);
    if (m_settings.language == "javascript")
        m_settings.js_main_src = draw_input_menu("Path to main javascript entry", m_settings.js_main_src.empty() ? "src/main.js" : m_settings.js_main_src);

    m_settings.license =
        draw_entry_menu("Choose a license for the project", vec_from_array(config_doc["licenses"]), m_settings.license);

skip_menu:
    create_manifest(m_file);
    fclose(m_file);

    if (m_settings.license != "Custom")
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
    else if (m_settings.language == "rust")
    {
        info("Initializing cargo project ...");
        if (Process("cargo init").get_exit_status() != 0)
        {
            warn_stat("Failed to run 'cargo init'. We going to do it ourself");
            info("Creating main entry at 'src/main.rs' ...");
            fs::create_directory("src");
            output_to_file("src/main.rs", "fn main() {\n\tprintln!(\"Hello, World!\");\n}");

            info("Auto generating Cargo.toml ...");
            output_to_file("Cargo.toml", "[package]\n\n[dependencies]");
        }
        toml::table tbl = toml::parse_file("Cargo.toml");
        generate_cargo_toml(tbl, m_settings);
        std::stringstream ss;
        ss << tbl;
        output_to_file("Cargo.toml", ss.str());
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

    m_doc.AddMember("commands", rapidjson::Value(rapidjson::kObjectType), allocator);
    m_doc["commands"].AddMember(rapidjson::Value(m_settings.package_manager.c_str(), m_settings.package_manager.size()),
                                rapidjson::Value(rapidjson::kObjectType), allocator);
    m_doc["commands"][m_settings.package_manager].AddMember(
        "run",
        rapidjson::Value(config_doc["commands"][m_settings.package_manager]["run"].GetString(),
                         config_doc["commands"][m_settings.package_manager]["run"].GetStringLength()),
        allocator);
    m_doc["commands"][m_settings.package_manager].AddMember(
        "install",
        rapidjson::Value(config_doc["commands"][m_settings.package_manager]["install"].GetString(),
                         config_doc["commands"][m_settings.package_manager]["install"].GetStringLength()),
        allocator);
    m_doc["commands"][m_settings.package_manager].AddMember(
        "build",
        rapidjson::Value(config_doc["commands"][m_settings.package_manager]["build"].GetString(),
                         config_doc["commands"][m_settings.package_manager]["build"].GetStringLength()),
        allocator);

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
    const std::string& exec =
        fmt::format("{} {}", m_doc["commands"][m_settings.package_manager][cmd].GetString(), fmt::join(arguments, " "));
    debug("Running {}", exec);
    if (Process(exec).get_exit_status() != 0)
        die("Failed to execute '{}'", exec);
}

void Manifest::set_project_settings(const cmd_options_t& cmd_options)
{
    rapidjson::Document js_pkg_doc;
    toml::table         cargo_toml_tbl;
    std::FILE*          pkg_manifest;
    bool                manifest_updated = false;
    bool                pkg_updated      = false;

    if (m_settings.language == "javascript")
    {
        if (!fs::exists("package.json"))
            autogen_empty_json("package.json");
        pkg_manifest = fopen("package.json", "r+");
        populate_doc(pkg_manifest, js_pkg_doc);
    }
    else if (m_settings.language == "rust")
    {
        if (!fs::exists("Cargo.toml"))
        {
            auto f = fmt::output_file("Cargo.toml", fmt::file::CREATE | fmt::file::RDWR);
            f.print("[package]\n\n[dependencies]");
            f.close();
        }
        pkg_manifest = fopen("Cargo.toml", "r+");
        try
        {
            cargo_toml_tbl = toml::parse_file("Cargo.toml");
        }
        catch (const toml::parse_error& err)
        {
            die("Parsing config file 'Cargo.toml' failed:\n"
                "{}\n"
                "\t(error occurred at line {} column {})",
                err.description(), err.source().begin.line, err.source().begin.column);
        }
    }

    if (!manifest_defaults.language.empty())
    {
        m_settings.language = manifest_defaults.language;
        update_json_field(m_doc, "language", manifest_defaults.language);
        manifest_updated = true;
    }

    if (!manifest_defaults.package_manager.empty())
    {
        update_json_field(m_doc, "package_manager", manifest_defaults.package_manager);
        if (!m_doc["commands"].HasMember(manifest_defaults.package_manager))
        {
            rapidjson::Document::AllocatorType& allocator = m_doc.GetAllocator();
            m_doc["commands"].AddMember(
                rapidjson::Value(m_settings.package_manager.c_str(), m_settings.package_manager.size()),
                rapidjson::Value(rapidjson::kObjectType), allocator);
            m_doc["commands"][m_settings.package_manager].AddMember(
                "run",
                rapidjson::Value(config_doc["commands"][m_settings.package_manager]["run"].GetString(),
                                 config_doc["commands"][m_settings.package_manager]["run"].GetStringLength()),
                allocator);
            m_doc["commands"][m_settings.package_manager].AddMember(
                "install",
                rapidjson::Value(config_doc["commands"][m_settings.package_manager]["install"].GetString(),
                                 config_doc["commands"][m_settings.package_manager]["install"].GetStringLength()),
                allocator);
            m_doc["commands"][m_settings.package_manager].AddMember(
                "build",
                rapidjson::Value(config_doc["commands"][m_settings.package_manager]["build"].GetString(),
                                 config_doc["commands"][m_settings.package_manager]["build"].GetStringLength()),
                allocator);
        }
        manifest_updated = true;
    }

    if (!manifest_defaults.license.empty())
    {
        update_json_field(m_doc, "license", manifest_defaults.license);
        if (m_settings.language == "javascript")
            update_json_field(js_pkg_doc, "license", manifest_defaults.license);
        else if (m_settings.language == "rust")
            cargo_toml_tbl["package"].as_table()->insert_or_assign("license", manifest_defaults.license);

        if ((fs::exists("LICENSE.txt") && !cmd_options.init_force) || manifest_defaults.license == "Custom")
            warn("LICENSE.txt already exists, use --force to overwrite");
        else
        {
            info("Removing LICENSE.txt");
            fs::remove("LICENSE.txt");
            info("Downloading license {} to LICENSE.txt ...", manifest_defaults.license);
            download_license(manifest_defaults.license);
        }
        pkg_updated      = true;
        manifest_updated = true;
    }

    if (!manifest_defaults.project_name.empty())
    {
        update_json_field(m_doc, "project_name", manifest_defaults.project_name);
        if (m_settings.language == "javascript")
            update_json_field(js_pkg_doc, "name", manifest_defaults.project_name);
        else if (m_settings.language == "rust")
            cargo_toml_tbl["package"].as_table()->insert_or_assign("name", manifest_defaults.project_name);

        manifest_updated = true;
        pkg_updated      = true;
    }

    if (!manifest_defaults.project_version.empty())
    {
        update_json_field(m_doc, "project_version", manifest_defaults.project_version);
        if (m_settings.language == "javascript")
            update_json_field(js_pkg_doc, "version", manifest_defaults.project_version);
        else if (m_settings.language == "rust")
            cargo_toml_tbl["package"].as_table()->insert_or_assign("version", manifest_defaults.project_version);

        manifest_updated = true;
        pkg_updated      = true;
    }

    if (!manifest_defaults.project_description.empty())
    {
        update_json_field(m_doc, "project_description", manifest_defaults.project_description);
        if (m_settings.language == "javascript")
            update_json_field(js_pkg_doc, "description", manifest_defaults.project_description);
        else if (m_settings.language == "rust")
            cargo_toml_tbl["package"].as_table()->insert_or_assign("description",
                                                                   manifest_defaults.project_description);

        manifest_updated = true;
        pkg_updated      = true;
    }

    if (!manifest_defaults.author.empty())
    {
        update_json_field(m_doc, "author", manifest_defaults.author);
        if (m_settings.language == "javascript")
        {
            update_json_field(js_pkg_doc, "author", manifest_defaults.author);
            pkg_updated = true;
        }
        manifest_updated = true;
    }

    if (manifest_updated)
    {
        write_to_json(m_file, m_doc);
        info("Updated {}", MANIFEST_NAME);
    }

    if (pkg_updated)
    {
        if (m_settings.language == "javascript")
        {
            write_to_json(pkg_manifest, js_pkg_doc);
        }
        else if (m_settings.language == "rust")
        {
            std::stringstream ss;
            ss << cargo_toml_tbl;
            output_to_file("Cargo.toml", ss.str());
        }
        info("Updated package.json");
    }
}
