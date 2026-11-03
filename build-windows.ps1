$ErrorActionPreference = 'Stop'
cmake -S . -B build-x86 -G "Visual Studio 17 2022" -A Win32
cmake --build build-x86 --config Release
if ($LASTEXITCODE -ne 0) { throw "x86 build failed" }
cmake -S . -B build-x64 -G "Visual Studio 17 2022" -A x64
cmake --build build-x64 --config Release
if ($LASTEXITCODE -ne 0) { throw "x64 build failed" }
Write-Host "Generated: build-x86/Release/HEGWorker32.exe and build-x64/Release/HEGController64.exe"
