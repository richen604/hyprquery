# HyprQuery (hyq)

A command-line utility for querying configuration values from Hyprland and hyprland-related configuration files using the hyprlang parsing library.

## Features

- Query any value from Hyprland configuration files
- Support for variables and nested includes via `source` directives
- Load schema files to provide default values
- JSON output format for integration with other tools
- Environment variable expansion in file paths
- Cross-platform compatibility across Linux distributions

## Installation

### Dependencies

- C++20 compatible compiler
- CMake 3.15+
- pkg-config (optional, for system hyprlang detection)
- hyprlang (automatically downloaded if not found on system)

### Building from Source

```bash
# Clone the repository
git clone https://github.com/username/hyprquery.git
cd hyprquery

# Create build directory
mkdir build && cd build

# Configure the project
cmake ..

# Build
make -j$(nproc)

# Install (optional)
sudo make install
```

### Build Options

- `USE_SYSTEM_HYPRLANG` (ON/OFF): Whether to use system-installed hyprlang or build from source. Default: ON
- `HYPRLANG_VERSION` (string): Specific version of hyprlang to use when building from source. Default: "v0.6.0"
- `STRICT_MODE` (ON/OFF): Enable strict mode checks. Default: OFF

Example:

```bash
# Use a specific version of hyprlang
cmake -DHYPRLANG_VERSION=v0.7.0 ..

# Force building hyprlang from source even if system version exists
cmake -DUSE_SYSTEM_HYPRLANG=OFF ..
```

### Distribution Packages

Debian/Ubuntu:

```
# Coming soon
```

Arch Linux:

```
# Coming soon
```

## Usage

Basic syntax:

```
hyq [OPTIONS] --query KEY CONFIG_FILE
```

### Examples

Query a simple value:

```bash
hyq --query "general:border_size" ~/.config/hypr/hyprland.conf
```

Query with JSON output:

```bash
hyq --json --query "decoration:blur:enabled" ~/.config/hypr/hyprland.conf
```

Query with a schema file:

```bash
hyq --schema ~/.config/hypr/schema.json --query "general:gaps_in" ~/.config/hypr/hyprland.conf
```

Follow source directives:

```bash
hyq -s --query "general:border_size" ~/.config/hypr/hyprland.conf
```

### Options

- `--query KEY`: Specify the key to query from the config file
- `--schema PATH`: Load a schema file with default values
- `--allow-missing`: Don't fail if the value is missing
- `--get-defaults`: Get default keys from schema
- `--strict`: Enable strict mode validation
- `--json`, `-j`: Output result in JSON format
- `--source`, `-s`: Follow source directives in config files

### Environment Variables

- `LOG_LEVEL`: Set the log level (debug, info, warn, error, critical)

## Schema Files

Schema files define the format and default values for configuration options. They are JSON files with the following structure:

```json
{
  "hyprlang_schema": [
    {
      "value": "general:border_size",
      "type": "INT",
      "data": {
        "default": 2,
        "min": 0,
        "max": 20
      }
    },
    {
      "value": "general:gaps_in",
      "type": "INT",
      "data": {
        "default": 5,
        "min": 0,
        "max": 50
      }
    }
  ]
}
```

## ABI Compatibility

HyprQuery is designed to work with different versions of hyprlang by:

1. Preferring system-installed hyprlang when available
2. Falling back to building a specific version from source
3. Using a compatibility layer to handle API/ABI differences

When building, you can specify which version of hyprlang to use with the `HYPRLANG_VERSION` CMake option.

## License

[MIT License](LICENSE)

## Contributing

Contributions are welcome! Please feel free to submit a Pull Request.
