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
    write_to_file(file);
    fclose(file);
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
