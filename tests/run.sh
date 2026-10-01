#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
mkdir -p build/host build
gcc -std=c11 -Wall -Wextra -Werror -O2 -Isrc -I. \
    src/trace.c src/ui.c src/font5x7.c tests/test_logic.c \
    -o build/host/test_logic
gcc -std=c11 -Wall -Wextra -Werror -O2 -Isrc -I. \
    src/trace.c src/ui.c src/font5x7.c tests/preview.c \
    -o build/host/preview
./build/host/test_logic
./build/host/preview build/preview.ppm
python3 tests/ppm_to_png.py build/preview.ppm build/preview.png
echo "preview: build/preview.png"
