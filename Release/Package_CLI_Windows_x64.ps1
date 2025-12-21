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
$artifact = Join-Path $release_directory "RAWcooked_CLI_${version}_Windows_x64"
if (Test-Path $artifact) {
    Remove-Item $artifact -Force -Recurse
}

$artifact = Join-Path $release_directory "RAWcooked_CLI_${version}_Windows_x64.zip"
if (Test-Path $artifact) {
    Remove-Item $artifact -Force
}

$artifact = Join-Path $release_directory "RAWcooked_CLI_${version}_Windows_x64_DebugInfo.zip"
if (Test-Path $artifact) {
    Remove-Item $artifact -Force
}

#-----------------------------------------------------------------------
# Package cli
Push-Location $release_directory
    New-Item -ItemType Directory -Path "RAWcooked_CLI_${version}_Windows_x64" -Force

    Push-Location "RAWcooked_CLI_${version}_Windows_x64"
        Copy-Item (Join-Path (Split-Path $release_directory -Parent) "Project\MSVC2022\CLI\x64\Release\RAWcooked.exe") -Force
        Copy-Item (Join-Path (Split-Path $release_directory -Parent) "History_CLI.txt") -Force
        Copy-Item (Join-Path (Split-Path $release_directory -Parent) "License.html") -Force

         & 7za.exe a -r -tzip "..\RAWcooked_CLI_${version}_Windows_x64.zip" *
    Pop-Location
Pop-Location

#-----------------------------------------------------------------------
# Package pdb
Push-Location $release_directory
    & 7za.exe a -r -tzip "RAWcooked_CLI_${version}_Windows_x64_DebugInfo.zip" "..\Project\MSVC2022\CLI\x64\Release\RAWcooked.pdb"
Pop-Location
