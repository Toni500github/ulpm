/*
 * Copyright 2025 Toni500git
 *
 * Redistribution and use in source and binary forms, with or without modification, are permitted provided that the
 * following conditions are met:
 *
 * 1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following
 * disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the
 * following disclaimer in the documentation and/or other materials provided with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote
 * products derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES,
 * INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
 * DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY,
 * WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE
 * OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 *
 */

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
    void run_cmd(const std::string& cmd, const std::vector<std::string>& arguments);
    void set_project_settings(const cmd_options_t& cmd_options);
    void validate_manifest();

private:
    std::FILE*          m_file;
    rapidjson::Document m_doc;
    ManiSettings        m_settings;
    void                create_manifest(std::FILE* file);
};
}  // namespace Settings
