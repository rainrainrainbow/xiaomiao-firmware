#!/usr/bin/env bash
# new-rom.sh — Scaffold a new Xiaomiao console ROM project
#
# Usage: new-rom.sh <rom-name> <output-dir>
# Example: new-rom.sh my-game ~/projects/my-game
#
# Creates a complete ESP-IDF project with:
#   - main.c template (LCD, buttons, LVGL, return-to-loader)
#   - CMakeLists.txt (top-level + main/)
#   - sdkconfig.defaults (board config)
#   - partitions.csv (Loader-compatible)
#   - return_to_loader.h
#   - idf_component.yml (LVGL 9.5.0 dependency)

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
ASSETS_DIR="${SCRIPT_DIR}/../assets"

if [ $# -lt 2 ]; then
    echo "Usage: $0 <rom-name> <output-dir>"
    echo "Example: $0 my-game ~/projects/my-game"
    exit 1
fi

ROM_NAME="$1"
OUT_DIR="$2"

# Validate rom-name: lowercase, hyphens, alphanum only
if [[ ! "$ROM_NAME" =~ ^[a-z][a-z0-9-]*$ ]]; then
    echo "Error: rom-name must be lowercase letters/numbers/hyphens, starting with a letter"
    exit 1
fi

# Normalize for CMake project name (replace hyphens with underscores)
PROJECT_NAME="${ROM_NAME//-/_}"

if [ -d "$OUT_DIR" ]; then
    echo "Error: output directory already exists: $OUT_DIR"
    exit 1
fi

echo "Creating ROM project: $ROM_NAME → $OUT_DIR"

mkdir -p "$OUT_DIR/main"

# Top-level CMakeLists.txt
sed "s/__ROM_NAME__/$PROJECT_NAME/g" "$ASSETS_DIR/CMakeLists.txt" \
    > "$OUT_DIR/CMakeLists.txt"

# sdkconfig.defaults
cp "$ASSETS_DIR/sdkconfig.defaults" "$OUT_DIR/"

# partitions.csv
cp "$ASSETS_DIR/partitions.csv" "$OUT_DIR/"

# return_to_loader.h
cp "$ASSETS_DIR/return_to_loader.h" "$OUT_DIR/"

# main/main.c
sed "s/\[ROM_NAME\]/$ROM_NAME/g" "$ASSETS_DIR/main-template.c" \
    > "$OUT_DIR/main/main.c"

# main/CMakeLists.txt
sed "s/__ROM_NAME__/$PROJECT_NAME/g" "$ASSETS_DIR/main_CMakeLists.txt" \
    > "$OUT_DIR/main/CMakeLists.txt"

# main/idf_component.yml
cat > "$OUT_DIR/main/idf_component.yml" << 'EOF'
dependencies:
  lvgl/lvgl: "9.5.0"
EOF

# .gitignore
cat > "$OUT_DIR/.gitignore" << 'EOF'
build/
sdkconfig
sdkconfig.old
managed_components/
dependencies.lock
EOF

echo ""
echo "Done! Project structure:"
echo "  $OUT_DIR/"
echo "    ├── CMakeLists.txt"
echo "    ├── sdkconfig.defaults"
echo "    ├── partitions.csv"
echo "    ├── return_to_loader.h"
echo "    ├── .gitignore"
echo "    └── main/"
echo "        ├── CMakeLists.txt"
echo "        ├── main.c"
echo "        └── idf_component.yml"
echo ""
echo "Next steps:"
echo "  cd $OUT_DIR"
echo "  . ~/esp/esp-idf/export.sh"
echo "  idf.py build"
echo "  idf.py -p /dev/ttyACM0 flash"
echo ""
echo "Copy the built binary to your TF card:"
echo "  cp build/${PROJECT_NAME}.bin /sdcard/roms/${ROM_NAME}.bin"
echo "  (or build merged bin: idf.py merge-bin)"
