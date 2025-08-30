#include <cstdio>
#include <string>
#include <vector>

#include "rapidjson/document.h"

struct cmd_options_t
{
    bool                     init_force = false;
    bool                     init_yes   = false;
    std::vector<std::string> arguments;
};

#define MANIFEST_NAME "ulpm.json"

namespace Settings
{
inline struct ManiSettings
{
    std::string language;
    std::string package_manager;
    std::string license;
    std::string project_name;
    std::string project_description;
    std::string project_version;
    std::string js_main_src;
    std::string js_runtime;
    std::string rust_edition;
    std::string author;
} manifest_defaults;

class Manifest
{
public:
    Manifest();
    void init_project(const cmd_options_t& cmd_options);
    void run_cmd(const std::vector<std::string>& arguments);
    void set_project_settings(const cmd_options_t& cmd_options);
    void validate_manifest();

private:
    std::FILE*          m_file;
    rapidjson::Document m_doc;
    ManiSettings        m_settings;
    void                create_manifest(std::FILE* file);
};
}  // namespace Settings
