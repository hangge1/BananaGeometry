param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [switch]$Package
)
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    & cmake -S . -B build/windows -G 'Visual Studio 17 2022' -A x64
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
    & cmake --build build/windows --config $Configuration --parallel
    if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
    & ctest --test-dir build/windows -C $Configuration --output-on-failure
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
    if ($Package) {
        & cmake --install build/windows --config $Configuration --prefix out/Drawer
        if ($LASTEXITCODE -ne 0) { throw 'Install failed' }
    }
} finally {
    Pop-Location
}
