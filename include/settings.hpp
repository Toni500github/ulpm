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
    std::string project_name;
    std::string project_description;
} manifest_defaults;

class Manifest
{
public:
    Manifest();
    void init_project(struct cmd_options_t& cmd_options);

private:
    const std::string   _manifest_name = "ulpm.json";
    rapidjson::Document m_doc;
    ManiSettings&       m_settings;
    void                write_to_file(std::FILE* file);
};
}  // namespace Settings
