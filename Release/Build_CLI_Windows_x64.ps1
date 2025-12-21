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
# Prepare
 Push-Location (Join-Path (Split-Path $release_directory -Parent) "Project\MSVC2022")
    ((Get-Content -Path "CLI\RAWcooked_CLI.vcxproj") -Replace "MultiThreadedDLL", "MultiThreaded") | Set-Content -Path "CLI\RAWcooked_CLI.vcxproj"
    ((Get-Content -Path "Lib\RAWcooked_Lib.vcxproj") -Replace "MultiThreadedDLL", "MultiThreaded") | Set-Content -Path "Lib\RAWcooked_Lib.vcxproj"
Pop-Location

#-----------------------------------------------------------------------
# Build
Push-Location (Join-Path (Split-Path $release_directory -Parent) "Project\MSVC2022\CLI")
    MSBuild "/p:Configuration=Release;Platform=x64"
Pop-Location
