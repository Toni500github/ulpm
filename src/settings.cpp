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
#include "util.hpp"

using namespace Settings;
namespace fs = std::filesystem;

Manifest::Manifest()
{
    if (fs::exists(_manifest_name))
        return;

    constexpr std::string_view json = R"({})";
    auto f = fmt::output_file(_manifest_name, fmt::file::CREATE | fmt::file::RDWR | fmt::file::TRUNC);
    f.print("{}", json);
    f.close();
}

void Manifest::init_project()
{
    FILE* file = fopen(_manifest_name.c_str(), "r+");

    m_settings.language = draw_menu({ "javascript", "go (WIP)", "c++ (WIP)" }, "Which language do you want to use?");
    switch (fnv1a16::hash(m_settings.language))
    {
        case "javascript"_fnv1a16:
            m_settings.prefered_pm = draw_menu({ "npm", "yarn", "pnpm" }, "Choose a preferred package manager to use");
            break;
        default: die("language '{}' is indeed WIP", m_settings.language);
    }

    char                      buf[UINT16_MAX] = { 0 };
    rapidjson::FileReadStream stream(file, buf, sizeof(buf));

    if (m_doc.ParseStream(stream).HasParseError())
    {
        fclose(file);
        die("Failed to parse {}: {} at offset {}", _manifest_name, rapidjson::GetParseError_En(m_doc.GetParseError()),
            m_doc.GetErrorOffset());
    }

    rapidjson::Document::AllocatorType& allocator = m_doc.GetAllocator();
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
    fclose(file);
}
