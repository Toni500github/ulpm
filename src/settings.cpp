#include "settings.hpp"

#include <cstdio>
#include <filesystem>

#include "fmt/os.h"
#include "rapidjson/document.h"
#include "rapidjson/error/en.h"
#include "rapidjson/filereadstream.h"
#include "rapidjson/filewritestream.h"
#include "rapidjson/prettywriter.h"
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

Manifest::Manifest() : m_settings(manifest_defaults)
{
    if (fs::exists(_manifest_name))
        return;

    constexpr std::string_view json = R"({})";
    auto f = fmt::output_file(_manifest_name, fmt::file::CREATE | fmt::file::RDWR | fmt::file::TRUNC);
    f.print("{}", json);
    f.close();
}

void Manifest::init_project(struct cmd_options_t& cmd_options)
{
    FILE* file = fopen(_manifest_name.c_str(), "r+");
    if (cmd_options.init_yes)
    {
        write_to_file(file);
        fclose(file);
        return;
    }

    Hashes hashes;

    m_settings.language = draw_entry_menu({ "javascript", "go (WIP)", "c++ (WIP)" },
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
    hashes.pm = fnv1a16::hash(m_settings.prefered_pm);

    m_settings.project_name        = draw_input_menu("Name of the project:", m_settings.project_name);
    m_settings.project_description = draw_input_menu("Description of the project:", m_settings.project_description);

    m_settings.license = draw_entry_menu({ "Apache-2.0",        "BSD-2-Clause",  "BSD-3-Clause",      "GPL-2.0-only",
                                           "GPL-2.0-or-later",  "GPL-3.0-only",  "GPL-3.0-or-later",  "LGPL-2.1-only",
                                           "LGPL-2.1-or-later", "LGPL-3.0-only", "LGPL-3.0-or-later", "MIT",
                                           "MPL-2.0",           "AGPL-3.0-only", "AGPL-3.0-or-later", "EPL-1.0",
                                           "EPL-2.0",           "CDDL-1.0",      "Unlicense",         "CC0-1.0" },
                                         "Choose a license for the project", m_settings.license);
    write_to_file(file);
    fclose(file);
    if (fs::exists("LICENSE.txt"))
    {
        warn("LICENSE.txt already exists, skipping download");
        return;
    }

    info("Downloading license {} to LICENSE.txt ...", m_settings.license);
    std::string err;
    const std::string url =
        "https://raw.githubusercontent.com/spdx/license-list-data/master/text/" + m_settings.license + ".txt";
    if (Process("curl -L " + url + " -o LICENSE.txt", "", nullptr, [&err](const char* buf, size_t n) {
            err.assign(buf, n);
        }).get_exit_status() != 0)
        die("Failed to download to file LICENSE.txt: {}", err);

    info("Done! Remember to modify the copyright holder and year");
}

void Manifest::write_to_file(std::FILE* file)
{
    char                      buf[UINT16_MAX] = { 0 };
    rapidjson::FileReadStream stream(file, buf, sizeof(buf));

    if (m_doc.ParseStream(stream).HasParseError())
    {
        fclose(file);
        die("Failed to parse {}: {} at offset {}", _manifest_name, rapidjson::GetParseError_En(m_doc.GetParseError()),
            m_doc.GetErrorOffset());
    }

    rapidjson::Document::AllocatorType& allocator = m_doc.GetAllocator();
    m_doc.AddMember("project_name",
                    rapidjson::Value(m_settings.project_name.c_str(), m_settings.project_name.size(), allocator),
                    allocator);
    m_doc.AddMember(
        "project_description",
        rapidjson::Value(m_settings.project_description.c_str(), m_settings.project_description.size(), allocator),
        allocator);
    m_doc.AddMember("license", rapidjson::Value(m_settings.license.c_str(), m_settings.license.size(), allocator),
                    allocator);
    m_doc.AddMember("language", rapidjson::Value(m_settings.language.c_str(), m_settings.language.size(), allocator),
                    allocator);
    m_doc.AddMember("pm", rapidjson::Value(m_settings.prefered_pm.c_str(), m_settings.prefered_pm.size(), allocator),
                    allocator);

    // seek back to the beginning to overwrite
    fseek(file, 0, SEEK_SET);

    char                                                writeBuffer[UINT16_MAX] = { 0 };
    rapidjson::FileWriteStream                          writeStream(file, writeBuffer, sizeof(writeBuffer));
    rapidjson::PrettyWriter<rapidjson::FileWriteStream> fileWriter(writeStream);
    fileWriter.SetFormatOptions(rapidjson::kFormatSingleLineArray);  // Disable newlines between array elements
    m_doc.Accept(fileWriter);

    ftruncate(fileno(file), ftell(file));
    fflush(file);
}
