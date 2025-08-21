#include <string>

#include "rapidjson/document.h"

namespace Settings
{
struct ManiSettings
{
    std::string language;
    std::string prefered_pm;
};

class Manifest
{
public:
    Manifest();
    void init_project();

private:
    const std::string   _manifest_name = "ulpm.json";
    rapidjson::Document m_doc;
    ManiSettings        m_settings;
};
}  // namespace Settings
