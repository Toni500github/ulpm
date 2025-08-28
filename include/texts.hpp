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

#ifndef _TEXTS_HPP_
#define _TEXTS_HPP_

#include <string_view>

// cufetchpm
inline constexpr std::string_view ulpm_help = (R"(Usage: ulpm <command> [options]

Manage projects across multiple languages with a single universal CLI.

Commands:
    init                Initialize a new project with interactive prompts.
    set                 Modify settings in the manifest (equivalent to `init -f`).
    run <script>        Run a script using the chosen package manager (currently JavaScript only).

Global options:
    -h, --help          Show this help message
    -V, --version       Show version and build information
    -v, --verbose       Show output of executed commands
)");

inline constexpr std::string_view ulpm_help_init = (R"(Usage: ulpm init [options]

Initialize a new project with interactive prompts, or set defaults directly via options.

Options:
    -f, --force                  Overwrite existing project files
    -y, --yes                    Skip prompts and use default values
    -h, --help                   Show this help message

Manifest options:
        --language <lang>        Set the project language (e.g. rust, javascript)
        --package_manager <pm>   Specify package manager (e.g. npm, yarn)
        --project_name <name>    Name of the project
        --license <license>      Project license (e.g. MIT, GPL-3.0)
        --project_description    Short description of the project
        --author <author>        Author name and info

Examples:
    ulpm init
        Run interactive prompts to create a new project.

    ulpm init --language cpp --project_name MyApp --license MIT -y
        Initialize a C++ project named "MyApp" with license MIT, skipping prompts.

    ulpm init -f
        Re-initialize an existing project, overwriting manifest.
)");

inline constexpr std::string_view ulpm_help_set = (R"(Usage: ulpm set [options]

Modify project settings in the manifest. Equivalent to running `ulpm init --force`.

Options:
    -h, --help                   Show this help message

Manifest options:
        --language <lang>        Set the project language (e.g. rust, javascript)
        --package_manager <pm>   Specify package manager (e.g. npm, yarn)
        --project_name <name>    Name of the project
        --license <license>      Project license (e.g. MIT, GPL-3.0)
        --project_description    Short description of the project
        --author <author>        Author name and info

Examples:
    ulpm set --language javascript --package_manager yarn
        Update the project manifest to use javascript and yarn.

    ulpm set --license GPL-3.0
        Change the project license to GPL-3.0.

    ulpm set --author "Jane Doe <jane@example.com>"
        Update the author field in the manifest.
)");

inline constexpr std::string_view ulpm_help_run = (R"(
Usage: ulpm run [options] <script> [args...]

Run a script using the chosen package manager (currently JavaScript only).

Options:
    -h, --help           Show this help message

Examples:
    ulpm run build
        Run the "build" script from package.json.

    ulpm run test -- --watch
        Run the "test" script and pass "--watch" as an extra argument.
)");
#endif  // !_TEXTS_HPP_
