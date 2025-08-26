#include <cstdio>
#include <string>

#include "rapidjson/document.h"
#include "util.hpp"

namespace Settings
{
inline struct ManiSettings
{
    std::string language;
    std::string prefered_pm;
    std::string license;
    std::string project_name;
    std::string project_description;
    std::string project_version;
    std::string js_main;
    std::string author;
} manifest_defaults;

class Manifest
{
public:
    Manifest();
    void init_project(struct cmd_options_t& cmd_options);

private:
    const std::string   _manifest_name = "ulpm.json";
    std::FILE*          m_file;
    rapidjson::Document m_doc;
    ManiSettings&       m_settings;
    void                create_manifest(std::FILE* file);
};
}  // namespace Settings
