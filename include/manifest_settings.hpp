#pragma once
#include <optional>
#include <string>

#define MANIFEST_NAME "ulpm.json"

struct manifest_settings_t
{
    std::string project_name;
    std::string project_description;
    std::string project_version;
    std::string author;
    std::string license;
    std::string language;
    std::string package_manager;
};

struct manifest_update_t
{
    // Common
    std::optional<std::string> project_name;
    std::optional<std::string> project_description;
    std::optional<std::string> project_version;
    std::optional<std::string> author;
    std::optional<std::string> license;
    std::optional<std::string> language;
    std::optional<std::string> package_manager;

    // JS-specific
    std::optional<std::string> js_main_src;

    // Rust-specific
    std::optional<std::string> rust_edition;
};
