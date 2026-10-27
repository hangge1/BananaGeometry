param(
    [ValidateSet('Debug', 'Release')][string]$Configuration = 'Debug',
    [switch]$Package,
    [switch]$Benchmarks
)
$ErrorActionPreference = 'Stop'
Push-Location $PSScriptRoot
try {
    if ($Benchmarks -and $Configuration -ne 'Release') {
        throw 'Use -Configuration Release for performance measurements'
    }
    & powershell -NoProfile -ExecutionPolicy Bypass -File tools/check-modules.ps1
    if ($LASTEXITCODE -ne 0) { throw 'Module boundary check failed' }
    $benchmarkOption = if ($Benchmarks) { 'ON' } else { 'OFF' }
    & cmake -S . -B build/windows -G 'Visual Studio 17 2022' -A x64 `
        -DBUILD_TESTING=ON "-DDRAWER_BUILD_BENCHMARKS=$benchmarkOption"
    if ($LASTEXITCODE -ne 0) { throw 'CMake configure failed' }
    & cmake --build build/windows --config $Configuration --parallel
    if ($LASTEXITCODE -ne 0) { throw 'Build failed' }
    & ctest --test-dir build/windows -C $Configuration -j 4 --output-on-failure `
        --output-junit "test-results-$Configuration.xml"
    if ($LASTEXITCODE -ne 0) { throw 'Tests failed' }
    if ($Benchmarks) {
        & powershell -NoProfile -ExecutionPolicy Bypass -File tools/run-benchmarks.ps1
        if ($LASTEXITCODE -ne 0) { throw 'Performance measurements failed' }
    }
    if ($Package) {
        & cmake --install build/windows --config $Configuration --prefix out/Drawer
        if ($LASTEXITCODE -ne 0) { throw 'Install failed' }
    }
} finally {
    Pop-Location
}
