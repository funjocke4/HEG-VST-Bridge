#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")"
cmake -S . -B build-mingw-x86 -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/mingw-x86.cmake" -DCMAKE_EXE_LINKER_FLAGS="-static -static-libgcc -static-libstdc++"
cmake --build build-mingw-x86 -j 2
cmake -S . -B build-mingw-x64 -DCMAKE_TOOLCHAIN_FILE="$PWD/cmake/mingw-x64.cmake" -DCMAKE_EXE_LINKER_FLAGS="-static -static-libgcc -static-libstdc++"
cmake --build build-mingw-x64 -j 2
echo "Fertig: build-mingw-x86/HEGWorker32.exe und build-mingw-x64/HEGController64.exe"
