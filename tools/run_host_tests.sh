#!/bin/sh
# Runs every host test from the repository root and stops at the first failure.
set -eu
cd "$(dirname "$0")/.."
.venv/bin/python -m unittest tools/test_matrix_hub.py tools/test_usb_app.py tools/test_firmware_state.py tools/test_web_build.py tools/test_preview_tools.py
for t in tools/test_*.cjs; do node "$t"; done
mkdir -p .local/tests
for t in panel_orientation serial_line_buffer led_encoder rgb565 color_calibration pattern_store preference_writes palettes; do
  clang++ -std=c++17 -Itools/test-support "tools/test_$t.cpp" -o ".local/tests/$t"
  ".local/tests/$t"
done
clang++ -std=c++17 -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -Itools/test-support \
  tools/test_native_engines.cpp src/patterns/NativeEngines.cpp src/patterns/FastMath.cpp -o .local/tests/native_engines
.local/tests/native_engines
clang++ -std=c++17 -O1 -fsanitize=address,undefined -fno-sanitize-recover=all -Itools/test-support \
  tools/test_collection.cpp src/patterns/FastMath.cpp -o .local/tests/collection
.local/tests/collection
