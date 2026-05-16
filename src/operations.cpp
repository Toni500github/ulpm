#include "operations.hpp"

#include <filesystem>
#include <string>
#include <vector>

#include "backend_registry.hpp"
#include "fmt/ranges.h"
#include "terminal_display.hpp"
#include "tiny-process-library/process.hpp"
#include "util.hpp"

namespace fs = std::filesystem;

static std::vector<std::string> get_licenses()
{
    static std::vector<std::string> v;
    if (v.empty())
    {
        v = { "Apache-2.0",       "BSD-2-Clause",      "BSD-3-Clause",
              "GPL-2.0-only",     "GPL-2.0-or-later",  "GPL-3.0-only",
              "GPL-3.0-or-later", "LGPL-2.1-only",     "LGPL-2.1-or-later",
              "LGPL-3.0-only",    "LGPL-3.0-or-later", "MIT",
              "MPL-2.0",          "AGPL-3.0-only",     "AGPL-3.0-or-later",
              "EPL-1.0",          "EPL-2.0",           "CDDL-1.0",
              "Unlicense",        "CC0-1.0",           "None" };
    }
    return v;
}

static void download_license(const std::string& license)
{
    const std::string& url = "https://raw.githubusercontent.com/spdx/license-list-data/master/text/" + license + ".txt";
    if (TinyProcessLib::Process({ "curl", "-fL", url, "-o", "LICENSE.txt" }).get_exit_status() != 0)
        die("Failed to download to file LICENSE.txt");
}

void op_init(Manifest& manifest, const cmd_options_t& opts, const manifest_update_t& upd)
{
    manifest_settings_t& s = manifest.settings();

    if (upd.project_name)
        s.project_name = *upd.project_name;
    if (upd.project_description)
        s.project_description = *upd.project_description;
    if (upd.project_version)
        s.project_version = *upd.project_version;
    if (upd.package_manager)
        s.package_manager = *upd.package_manager;
    if (upd.license)
        s.license = *upd.license;
    if (upd.author)
        s.author = *upd.author;
    if (upd.language)
        s.language = *upd.language;

    if (!manifest.empty())
    {
        if (!opts.init_force && !opts.init_yes && !askUserYorN(false, "{} is not empty. Overwrite?", MANIFEST_NAME))
            die("Aborted.");
        manifest.doc().SetObject();  // clear in-memory; save() will truncate on disk
    }

    if (!opts.init_yes)
    {
        s.language = draw_entry_menu("Choose the language", g_registry.languageNames(), s.language);
    }
    else
    {
        if (s.project_name.empty())
            die("--yes requires --project_name=<name>");
        if (s.project_version.empty())
            die("--yes requires --project_version=<version>");
        if (s.language.empty())
            die("--yes requires --language=<lang>");
    }

    if (s.package_manager.empty())
    {
        if (s.language == "rust")
            s.package_manager = "cargo";
        else if (s.language == "javascript")
            die("--language=javascript requires a --package_manager. Choose from:\x1b[0m\n - npm\n - yarn \n - pnpm");
    }

    std::unique_ptr<LanguageBackend> backend = g_registry.create(s.language);
    if (!backend)
        die("Unknown language '{}'\n\nAvailable backends:\n - {}",
            s.language,
            fmt::join(g_registry.languageNames(), "\n - "));

    if (!opts.init_yes)
    {
        backend->promptInit(s);
        s.project_name        = draw_input_menu("Project name", s.project_name);
        s.project_description = draw_input_menu("Description", s.project_description);
        s.project_version     = draw_input_menu("Version", s.project_version.empty() ? "0.0.1" : s.project_version);
        s.author              = draw_input_menu("Author", s.author.empty() ? "Name <email@example.com>" : s.author);
        s.license             = draw_entry_menu("Choose a license", get_licenses(), s.license);
    }

    display.shutdown();
    manifest.setBackend(std::move(backend));
    manifest.save();
    manifest.backend()->generateFiles(s);

    if (s.license != "None")
    {
        if (fs::exists("LICENSE.txt") && !opts.init_force)
        {
            warn("LICENSE.txt already exists, skipping download");
        }
        else
        {
            fs::remove("LICENSE.txt");
            info("Downloading license '{}'...", s.license);
            download_license(s.license);
            info("Remember to update the copyright year and holder.");
        }
    }
    info("Done!");
}

void op_set(Manifest& manifest, const manifest_update_t& upd)
{
    if (!manifest.backend())
        die("No language set in {}. Run 'ulpm init' first.", MANIFEST_NAME);

    manifest_settings_t& s = manifest.settings();

    bool dirty = false;
    auto apply = [&](std::string& field, const std::optional<std::string>& val) {
        if (!val)
            return;
        field = *val;
        dirty = true;
    };

    // Apply common field updates — no language knowledge needed here
    apply(s.project_name, upd.project_name);
    apply(s.project_version, upd.project_version);
    apply(s.project_description, upd.project_description);
    apply(s.author, upd.author);
    apply(s.license, upd.license);
    apply(s.package_manager, upd.package_manager);

    bool pkg_dirty = manifest.backend()->syncPkgManifest(s, upd);

    if (dirty)
    {
        manifest.save();
        info("Updated {}", MANIFEST_NAME);
    }

    if (pkg_dirty)
    {
        info("Updated package manifest");
    }
}

void op_run(Manifest& manifest, const std::string& cmd, const cmd_options_t& opts)
{
    if (!manifest.backend())
        die("No language set in {}. Run 'ulpm init' first.", MANIFEST_NAME);

    manifest.backend()->validate(manifest.settings());

    rapidjson::Document& doc = manifest.doc();
    const std::string&   pm  = manifest.settings().package_manager;

    if (!doc.HasMember("commands") || !doc["commands"].IsObject())
        die("'commands' entry is not an object in " MANIFEST_NAME);
    if (!doc["commands"].HasMember(cmd.c_str()))
        die("Unknown command '{}' for package manager '{}'", cmd, pm);

    const rapidjson::Value& jcmd = doc["commands"][cmd.c_str()];
    // excevp() like
    if (jcmd.IsArray())
    {
        std::vector<std::string> arg_cmd;

        for (const rapidjson::Value& value : jcmd.GetArray())
        {
            if (!value.IsString())
                die("Command array for {} must contain only strings", cmd);
            arg_cmd.emplace_back(value.GetString());
        }

        for (const std::string& arg : opts.arguments)
            arg_cmd.emplace_back(arg);

        debug("Running: {}", arg_cmd);
        if (TinyProcessLib::Process(arg_cmd).get_exit_status() != 0)
            die("Command failed: {}", arg_cmd);
    }
    // running in a shell
    else if (jcmd.IsString())
    {
        const std::string exec = fmt::format("{} {}", jcmd.GetString(), fmt::join(opts.arguments, " "));
        debug("Running: {}", exec);
        if (TinyProcessLib::Process(exec).get_exit_status() != 0)
            die("Command failed: {}", exec);
    }
    else
    {
        die("Command for {} is neither an array or string");
    }
}
