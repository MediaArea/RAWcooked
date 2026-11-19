##  Copyright (c) MediaArea.net SARL. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license that can
##  be found in the License.html file in the root of the source tree.
##

$ErrorActionPreference = "Stop"

#-----------------------------------------------------------------------
# Setup
$release_directory = Split-Path -Parent $MyInvocation.MyCommand.Path
$version = (Get-Content (Join-Path (Split-Path $release_directory -Parent) "Project\version.txt") -Raw).Trim()

#-----------------------------------------------------------------------
# Cleanup
Push-Location (Join-Path (Split-Path $release_directory -Parent) "Project\MSVC2022\CLI")
    MSBuild "/t:Clean"
Pop-Location

#-----------------------------------------------------------------------
# Build
Push-Location (Join-Path (Split-Path $release_directory -Parent) "Project\MSVC2022\CLI")
    MSBuild "/p:Configuration=Release;Platform=x64"
Pop-Location

#-----------------------------------------------------------------------
# Check
# The Release configurations of the projects use the static runtime (RuntimeLibrary MultiThreaded), the executable must not need the Visual C++ Redistributable
$executable = Join-Path (Split-Path $release_directory -Parent) "Project\MSVC2022\CLI\x64\Release\RAWcooked.exe"
if (Get-Command "dumpbin" -ErrorAction SilentlyContinue) {
    $dependents = (dumpbin /nologo /dependents $executable) -join "`n"
    if ($LASTEXITCODE -ne 0) {
        throw "dumpbin failed on ${executable}:`n$dependents"
    }
    if ($dependents -match "MSVCP|VCRUNTIME|api-ms-win-crt-|ucrtbase") {
        throw "$executable depends on the Visual C++ runtime DLLs:`n$dependents"
    }
} else {
    Write-Warning "dumpbin not found, runtime dependencies of $executable not checked"
}
