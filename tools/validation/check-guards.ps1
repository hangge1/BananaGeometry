param([string]$RunId)
$ErrorActionPreference = 'Stop'
Import-Module (Join-Path $PSScriptRoot 'common.psm1') -Force
$context = New-ValidationContext -Task 'guards' -RunId $RunId
$status = 'failed'
try {
    $root = $context.RepositoryRoot
    $source = Join-Path $context.TemporaryDirectory 'guards/src/core'
    New-Item -ItemType Directory -Path $source -Force | Out-Null
    [System.IO.File]::WriteAllText((Join-Path $source 'forbidden.cpp'), '#include "app/application.h"')
    $result = Invoke-ValidationCommand $context 'positive-module-check' powershell @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', (Join-Path $root 'tools/check-modules.ps1'))
    $result = Invoke-ValidationCommand $context 'negative-module-check' powershell @('-NoProfile', '-ExecutionPolicy', 'Bypass', '-File', (Join-Path $root 'tools/check-modules.ps1'), '-SourceRoot', (Split-Path $source -Parent)) -AllowFailure
    if ($result.ExitCode -eq 0 -or ($result.Output + $result.ErrorOutput) -notmatch 'forbidden dependency app/application.h') { throw 'Forbidden include was not rejected for the expected reason' }
    $fixture = Join-Path $context.TemporaryDirectory 'guards/cmake'
    New-Item -ItemType Directory -Path $fixture -Force | Out-Null
    $rules = (Join-Path $root 'cmake/ModuleRules.cmake').Replace('\', '/')
    $cmake = "cmake_minimum_required(VERSION 3.24)`nproject(BoundaryCheck NONE)`ninclude(`"$rules`")`nadd_library(drawer_core INTERFACE)`nset_property(TARGET drawer_core PROPERTY LINK_LIBRARIES drawer_app)`ndrawer_check_dependencies(drawer_core drawer_options)`n"
    [System.IO.File]::WriteAllText((Join-Path $fixture 'CMakeLists.txt'), $cmake)
    $result = Invoke-ValidationCommand $context 'negative-cmake-check' cmake @('-S', $fixture, '-B', (Join-Path $fixture 'output')) -AllowFailure
    if ($result.ExitCode -eq 0 -or ($result.Output + $result.ErrorOutput) -notmatch 'drawer_core must not depend on drawer_app') { throw 'Forbidden target dependency was not rejected for the expected reason' }
    $status = 'passed'
} finally { Complete-ValidationContext $context $status }
