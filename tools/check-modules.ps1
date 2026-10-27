$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
$repositoryRoot = Split-Path $PSScriptRoot -Parent
# Include dependencies, matching the CMake library contracts.
$allowed = @{
    core = @('core')
    runtime = @('runtime', 'core')
    platform = @('platform', 'core')
    render = @('render', 'core')
    app = @('app', 'runtime', 'platform', 'render', 'core')
}
$violations = [System.Collections.Generic.List[string]]::new()
$sourceRoot = Join-Path $repositoryRoot 'src'
$files = @(Get-ChildItem -LiteralPath $sourceRoot -Recurse -File |
    Where-Object { $_.Extension -in '.cpp', '.h' })
foreach ($file in $files) {
    $relativePath = $file.FullName.Substring($sourceRoot.Length + 1).Replace('\', '/')
    if ($relativePath -eq 'main.cpp') { continue } # Native composition root.
    $module = $relativePath.Split('/')[0]
    if (-not $allowed.ContainsKey($module)) {
        $violations.Add("${relativePath}: no declared module")
        continue
    }
    $source = Get-Content -LiteralPath $file.FullName -Raw -Encoding UTF8
    foreach ($include in [regex]::Matches($source, '(?m)^\s*#include\s*[<"]([^">]+)[">]')) {
        $header = $include.Groups[1].Value
        if ($header.Contains('..')) { $violations.Add("${relativePath}: relative escape $header") }
        $target = $header.Split('/')[0]
        if ($allowed.ContainsKey($target) -and $target -notin $allowed[$module]) {
            $violations.Add("${relativePath}: forbidden dependency $header")
        }
        if ($header.Contains('/detail/') -and $target -ne $module) {
            $violations.Add("${relativePath}: private header $header")
        }
        if ($header -match '^(windows\.h|commdlg\.h|shellapi\.h|wrl/|d3d|dxgi)' -and $module -ne 'platform') {
            $violations.Add("${relativePath}: platform API $header")
        }
        if ($header -match '^lua\.(h|hpp)$' -and $module -ne 'runtime') {
            $violations.Add("${relativePath}: Lua API $header")
        }
        if ($header -match '^imgui' -and $module -notin 'app', 'render') {
            $violations.Add("${relativePath}: GUI API $header")
        }
    }
}
$luaAllowed = @{
    geometry = @()
    state = @()
    objects = @('geometry')
    interaction = @('geometry')
    ui = @()
}
foreach ($name in $luaAllowed.Keys) {
    $path = Join-Path $repositoryRoot "sdk/drawer/$name.lua"
    $source = Get-Content -LiteralPath $path -Raw -Encoding UTF8
    foreach ($import in [regex]::Matches($source, 'require\([''"]drawer\.([^''"]+)[''"]\)')) {
        $target = $import.Groups[1].Value
        if ($target -notin $luaAllowed[$name]) { $violations.Add("$path imports $target") }
    }
    # Ignore explanatory comments when checking pure geometry and state owners.
    $code = [regex]::Replace($source, '(?m)--[^\r\n]*', '')
    if ($name -eq 'geometry' -and $code -match '\b(_host|host|ctx)\b') {
        $violations.Add('geometry.lua depends on host or context')
    }
    if ($name -eq 'objects' -and $code -match '\b(self|ctx)\.temp\b') {
        $violations.Add('objects.lua reads interaction-owned temporary state')
    }
}
if ($violations.Count) { throw ($violations -join [Environment]::NewLine) }
Write-Output "Module include/import checks passed ($($files.Count) C++ files and 5 Lua modules)."
