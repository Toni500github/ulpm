# ulpm - Universal Language Package Manager

A single, unified command-line tool to initialize, configure, and run projects across different programming languages. Stop memorizing the specific commands for `npm`, `yarn`, `cargo`, etc. use `ulpm` instead.

## What is this?

`ulpm` is a CLI tool designed to simplify project management. Instead of using different commands for JavaScript (`npm run build`), Rust (`cargo build`), or other languages, you can use one consistent command: `ulpm run build`.

It helps you:
*   **Initialize new projects** with an interactive setup or quick command-line flags.
*   **Manage project metadata** (name, version, author, license) in a standardized way.
*   **Run scripts** using the appropriate underlying package manager for your project's language.
*   **Maintain a universal manifest file** (`ulpm.json`) that works alongside your existing `package.json` or `Cargo.toml`.

## Key Features

*   **Multi-Language Support:** Currently supports initializing and managing **JavaScript** and **Rust** projects, with a structure ready for more.
*   **Interactive Setup:** A user-friendly, menu-driven interface (`ulpm init`) to configure your new project.
*   **Non-Interactive Mode:** Supports `--yes` and command-line flags for automation and scripting.
*   **License Automation:** Automatically downloads standard open-source licenses for your project.
*   **Project Sync:** The `ulpm set` command lets you change project settings (like name or license) and automatically updates both the `ulpm.json` and the language-specific manifest file.

## How It Works
1. **The Universal Manifest:** `ulpm` creates and uses a `ulpm.json` file in your project root. This file stores your project's core metadata and tells `ulpm` which language and package manager you're using.
2. **Bridge, Not Replacement:** `ulpm` does not replace `npm` or `cargo`. It acts as a bridge, reading your configuration and calling the correct underlying commands for you.
3. **Interactive Menus:** The tool uses a text-based interface (powered by `ncurses`) to provide a smooth user experience during project setup.

## Quick Start

### Installation (From Source)

1.  Ensure you have the necessary dependencies: a C++ compiler, Make, and the development files for `ncurses`.
2.  Clone the repository and build the project:
    ```bash
    git clone --depth=1 https://github.com/Toni500github/ulpm
    cd ulpm
    make DEBUG=0
    ./build/release/ulpm
    ```

### Basic Usage

**1. Create a new project:**
```bash
# Launch the interactive setup wizard
ulpm init

# Or, create a project quickly without prompts
ulpm init --language javascript --project_name my_app --license MIT -y
```
**2. Run your project scripts:**
After initialization, you can use `ulpm` to run commands defined in your `package.json` or `Cargo.toml`.
```bash
ulpm run start
# This executes: `npm run start` or `cargo run`, depending on your project language.
```
**3. Modify project settings:**
Change your project's license and author in one command
```bash
ulpm set --license GPL-3.0 --author "Your Name <email@example.com>"
```

## License
This project is licensed under the BSD 3-Clause license. See the header in the source files for full details.

This README was AI generated with deepseek but reviewed by hand the content
