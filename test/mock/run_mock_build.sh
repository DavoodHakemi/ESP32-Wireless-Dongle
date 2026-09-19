#!/usr/bin/env bash
# Mock compile/link verification for the dongle firmware (verification level 2
# fallback when the PlatformIO platform cache is unavailable).
#
# Compiles every src/*.cpp with mock Arduino/ESP32/FreeRTOS/BLE/A2DP stub
# headers under -Wall -Wextra -Werror, then links the objects with the mock
# runtime into build/firmware_mock.elf and executes one mock boot pass.
#
# Usage: bash test/mock/run_mock_build.sh
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/../.." && pwd)"
OUT="$ROOT/build"
CXX="${CXX:-g++}"
# -Wno-class-memaccess: the real PlatformIO build does not enable -Werror, and
# the firmware's zeroing of the trivially laid out scan-entry table via
# memset() only trips this pedantic host warning. Keep host strictness aligned
# with the real build so mock results stay comparable.
CXXFLAGS="-std=gnu++17 -Wall -Wextra -Werror -Wno-class-memaccess -I$ROOT/include -I$ROOT/test/mock/mock_include -O0 -pthread"

mkdir -p "$OUT"

SOURCES=()
while IFS= read -r src; do
    SOURCES+=("$src")
done < <(find "$ROOT/src" -name '*.cpp' | sort)

echo "Mock build: ${#SOURCES[@]} firmware sources + runtime + entry"

for src in "${SOURCES[@]}"; do
    obj="$OUT/$(echo "${src#$ROOT/}" | tr '/' '_').o"
    echo "  CXX $(basename "$src")"
    $CXX $CXXFLAGS -c "$src" -o "$obj"
done

echo "  CXX mock_runtime"
$CXX $CXXFLAGS -c "$ROOT/test/mock/mock_runtime.cpp" -o "$OUT/mock_runtime.o"

echo "  CXX main_mock"
$CXX $CXXFLAGS -c "$ROOT/test/mock/main_mock.cpp" -o "$OUT/main_mock.o"

echo "  LINK firmware_mock.elf"
$CXX $CXXFLAGS -o "$OUT/firmware_mock.elf" "$OUT"/*.o

echo "  RUN firmware_mock.elf"
"$OUT/firmware_mock.elf"

echo "MOCK BUILD PASS"
