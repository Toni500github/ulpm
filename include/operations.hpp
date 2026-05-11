#pragma once

#include "manifest.hpp"

struct cmd_options_t
{
    bool                     init_force = false;
    bool                     init_yes   = false;
    std::vector<std::string> arguments;  // for run
};

void op_init(Manifest& manifest, const cmd_options_t& opts, const manifest_update_t& upd);
void op_set(Manifest& manifest, const manifest_update_t& upd);
void op_run(Manifest& manifest, const std::string& cmd, const cmd_options_t& opts);
