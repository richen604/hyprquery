#!/usr/bin/env sh

# This script updates the compile_commands.json file in the build directory
# based on the current CMake configuration.

DIR="$(dirname "$(dirname "$0")")"

copy_compile_commands() {

    cmake -DCMAKE_EXPORT_COMPILE_COMMANDS=ON "$DIR/build"
    rm -f "$DIR/compile_commands.json"
    cp -f "$DIR/build/compile_commands.json" "$DIR"
}

# Check if the copy was successful
if copy_compile_commands; then
    echo "Successfully updated compile_commands.json"
else
    echo "Failed to update compile_commands.json"
fi
