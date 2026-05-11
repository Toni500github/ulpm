#pragma once
#include <string>
#include <string_view>
#include <vector>

#include "manifest_settings.hpp"
#include "rapidjson/document.h"

class LanguageBackend
{
public:
    virtual ~LanguageBackend() = default;

    virtual std::string_view         name() const            = 0;
    virtual std::vector<std::string> packageManagers() const = 0;

    // Called by Manifest constructor after reading ulpm.json.
    // Load language-specific state from the document.
    virtual void load(const rapidjson::Document& doc) = 0;

    // Called by Manifest::save(). Serialize language-specific state
    // into the document before it is written to disk.
    virtual void save(rapidjson::Document& doc) const = 0;

    // ulpm init — interactive prompts for language-specific options.
    // May mutate common (e.g. lock package_manager to "cargo" for Rust).
    virtual void promptInit(manifest_settings_t& common) = 0;

    // ulpm init — scaffold language-specific files on disk.
    virtual void generateFiles(const manifest_settings_t& common) = 0;

    // ulpm set — apply update to the language's own package manifest
    // (package.json, Cargo.toml, …).
    // Returns true if the package manifest was actually modified.
    virtual bool syncPkgManifest(const manifest_settings_t& common, const manifest_update_t& upd) = 0;

    // ulpm run/build/install — optional extra validation before running.
    virtual void validate(const manifest_settings_t& /*common*/) const {}
};
