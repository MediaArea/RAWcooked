##  Copyright (c) MediaArea.net SARL. All Rights Reserved.
##
##  Use of this source code is governed by a BSD-style license that can
##  be found in the License.html file in the root of the source tree.
##

# Benchmark of --edit with the file cache of the system emptied before each run (as if the files were read for the first time), Windows only
# Needs to run as administrator, and purge.exe (built from purge.cpp, by default next to this script)
# The files of the sequence are modified: a new Copyright value at each run, so every file is written
# The sequence should be on a local volume (the cache of a server is not emptied) and in a folder excluded from the real-time scan of the antivirus (see Doc/Antivirus.md), else each run includes the scan of all files modified by the previous run
# Example: .\Benchmark_Edit_Cold.ps1 -Exes ([ordered]@{ old = "C:\old\rawcooked.exe"; new = "C:\new\rawcooked.exe" }) -Sequence D:\DPX -Threads 1,8,default
param(
    [Parameter(Mandatory)][System.Collections.IDictionary]$Exes, # Name and path of each RAWcooked binary to compare ([ordered] for keeping the order)
    [Parameter(Mandatory)][string]$Sequence, # Directory of the image sequence
    [string[]]$Threads = @("default"), # Values of --io-threads, "default" for not using this option
    [ValidateRange(1, 1000)][int]$Runs = 3,
    [string]$Purge = (Join-Path $PSScriptRoot "purge.exe"),
    [string]$Log = "Benchmark_Edit_Cold.txt"
)

function Out-Log([string]$Line) { $Line; Add-Content -Path $Log -Value $Line -Encoding UTF8 }
function Test-Application([string]$Path) { [bool](Get-Command -Name $Path -CommandType Application -ErrorAction SilentlyContinue) }
function Get-Median($Values) { $s = @($Values | Sort-Object); $m = [math]::Floor($s.Count / 2); if ($s.Count % 2) { $s[$m] } else { ($s[$m - 1] + $s[$m]) / 2 } }

# Checks
$Builds = @($Exes.GetEnumerator() | ForEach-Object { [pscustomobject]@{ Name = [string]$_.Key; Path = [string]$_.Value } }) # Entries, not indexes (a number as key would be a position in an ordered dictionary)
foreach ($Build in $Builds) {
    if (-not (Test-Application $Build.Path)) { "ERROR: $($Build.Name): $($Build.Path) not found"; exit 1 }
}
if (-not (Test-Application $Purge)) { "ERROR: $Purge not found, build it from purge.cpp"; exit 1 }
$Sequence = (Resolve-Path -LiteralPath $Sequence -ErrorAction Stop).ProviderPath
if ($Sequence -notmatch '^[A-Za-z]:\\$') { $Sequence = $Sequence.TrimEnd('\', '/') } # A trailing backslash would escape the closing quote of the argument
$Volume = if ($Sequence -match '^([A-Za-z]):') { $Matches[1] } else { "C" } # Volume flushed by purge.exe, the cache is emptied for all volumes

Set-Content -Path $Log -Value "Cold cache benchmark of $Sequence, $(Get-Date -Format s)" -Encoding UTF8
function Invoke-Purge {
    & $Purge $Volume
    if ($LASTEXITCODE) { Out-Log "ERROR: purge failed (rc=$LASTEXITCODE), see the message above"; exit 1 }
}
Invoke-Purge

$Stamp = Get-Date -Format yyyyMMddHHmmss # Copyright values not same as the ones of a previous benchmark
$k = 0
foreach ($t in $Threads) {
    $Times = @{}; $Failed = @{}; foreach ($Build in $Builds) { $Times[$Build.Name] = @(); $Failed[$Build.Name] = 0 }
    for ($r = 0; $r -lt $Runs; $r++) {
        foreach ($Build in $Builds) { # Interleaved, for the same conditions
            $k++
            $a = @("--no-check-padding", "--edit", "Copyright=cold$Stamp-$k", $Sequence)
            if ($t -ne "default") { $a = @("--io-threads", $t) + $a }
            Invoke-Purge
            $sw = [Diagnostics.Stopwatch]::StartNew()
            $rc = try { & $Build.Path @a *> $null; $LASTEXITCODE } catch { -1 }
            $sw.Stop()
            if ($rc) {
                Out-Log "ERROR $($Build.Name) threads=$t rc=$rc"
                $Failed[$Build.Name]++
            }
            else {
                $Times[$Build.Name] += $sw.Elapsed.TotalSeconds
            }
            "  run $k $($Build.Name) threads=$t $($sw.Elapsed.TotalSeconds.ToString('0.000')) s"
        }
    }
    foreach ($Build in $Builds) {
        $v = @($Times[$Build.Name])
        $Line = "{0,-8} threads={1,-8}" -f $Build.Name, $t
        if ($v.Count) { $Line += " median={0:0.000} min={1:0.000}  all: {2}" -f (Get-Median $v), ($v | Measure-Object -Minimum).Minimum, (($v | ForEach-Object { $_.ToString("0.000") }) -join " ") }
        if ($Failed[$Build.Name]) { $Line += "  FAILED $($Failed[$Build.Name])/$Runs" }
        Out-Log $Line
    }
}
Out-Log "DONE"
