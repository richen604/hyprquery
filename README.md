# HyprQuery

HyprQuery is a configuration parser for hypr\* config files. It allows you to query configuration values from a specified config file and optionally use a schema file to add default values.

## Installation

To build HyprQuery, you need to have a C++ compiler and CMake installed. Follow these steps to build the project:

```sh
git clone https://github.com/HyDE-Project/hyprquery.git
cd hyprquery
mkdir build
cd build
cmake -DCMAKE_BUILD_TYPE=Release ..
make
```

## Usage

hyprquery provides `hyq` as a binary.

```sh
hyq --query <query> config_file [--schema <schema_file>] [--allow-missing] [--get-defaults] [--strict] [--json]
```

### Options

- `--query <query>`: Query to execute (required).
- `config_file`: Path to the configuration file (required).
- `--schema <schema_file>`: Path to the schema file.
- `--allow-missing`: Allow missing values.
- `--get-defaults`: Get default keys.
- `--strict`: Enable strict mode.
- `--json, -j`: Output result in JSON format.

### Examples

Query a configuration value from a config file:

```sh
hyq --query some_key config.json
```

Query a configuration value with a schema file:

```sh
hyq --query some_key config.json --schema schema.json
```

Output the result in JSON format:

```sh
hyq --query some_key config.json --json
```

## Schema

As per as i'm dumb, I cannot find a way for hyprlang to just work like jq. Therefore we required a schema so we can parse the target file correctly.
This is also helpful to handle data types and default fallbacks.

See [schema](./schema).
