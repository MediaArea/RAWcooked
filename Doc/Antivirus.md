# RAWcooked and antivirus software

Image sequences are made of many files (e.g. 100,000 DPX files for a film). Antivirus software with real-time (on-access) scanning checks each file when it is opened, so the time spent by the antivirus can be much larger than the time spent by RAWcooked, especially for small files and for files which were just modified (e.g. after an edit with `--edit`).

This page explains how to exclude the folders processed by RAWcooked from the real-time scanning. It is a trade-off between speed and security: exclude only folders with trusted content (e.g. content produced by your own scanner or by your colleagues), and scan content from untrusted sources at least once before excluding it, or keep it in a folder which is not excluded.

If your computer is managed by an IT department, the antivirus settings may be enforced by a policy: ask them for the exclusions.

## Measured impact

On a Windows test computer (NVMe SSD, Microsoft Defender), after a purge of the file cache:

| Command | Files | Without exclusion | With exclusion |
| --- | --- | --- | --- |
| `--edit copyright=...` | 100,000 small DPX files (14 KB) | ~80 s | ~5 s |
| `--all --no-encode` | 10,000 DPX files (6 MB, 58 GB) | ~20 s (first run: ~24 s) | ~19 s |

Microsoft Defender keeps a cache of the files already scanned: a file which is not modified is scanned once, the next runs are quicker. A file modified (e.g. by an edit) is scanned again when it is opened again, so editing many files then processing them again is slow without exclusion. After a folder is removed from the exclusions, the first run is slower (each file is scanned once).

## Windows

### Microsoft Defender Antivirus

Exclusion of a folder (including its sub-folders), from a PowerShell window run as administrator:

```powershell
Add-MpPreference -ExclusionPath "D:\Scans\Film1"
```

List of the excluded folders:

```powershell
(Get-MpPreference).ExclusionPath
```

Removal of the exclusion:

```powershell
Remove-MpPreference -ExclusionPath "D:\Scans\Film1"
```

The same can be done from the user interface: Windows Security > Virus & threat protection > Virus & threat protection settings > Manage settings > Exclusions > Add or remove exclusions > Add an exclusion > Folder.

It is also possible to exclude the files opened by RAWcooked instead of folders (process exclusion). It is broader (any file opened by RAWcooked, including files from untrusted sources), so prefer folder exclusions:

```powershell
Add-MpPreference -ExclusionProcess "C:\Program Files\RAWcooked\rawcooked.exe"
```

### Dev Drive (Windows 11)

A Dev Drive is a volume (ReFS) for which Microsoft Defender can use a "performance mode": the files are scanned asynchronously, the opening of the files is not delayed by the scan. It keeps the antivirus protection while reducing its impact. See the Microsoft documentation about Dev Drive for creating one, and put the image sequences on it.

### Other antivirus software

Most antivirus software has a similar folder exclusion setting (often named "exclusions", "exceptions" or "allowed list"). See the documentation of the vendor.

### Files in WSL (Windows Subsystem for Linux)

Files stored in the Linux file system of WSL 2 (e.g. in `/home`) are not scanned file by file by Microsoft Defender. Files on Windows drives accessed from WSL (e.g. in `/mnt/d`) are Windows files and are scanned as on Windows.

## macOS

The antivirus built into macOS (XProtect) does not scan data files such as DPX files when they are opened, no setting is needed.

With Microsoft Defender for Endpoint on macOS:

```sh
mdatp exclusion folder add --path "/Volumes/Scans/Film1/"
mdatp exclusion list
mdatp exclusion folder remove --path "/Volumes/Scans/Film1/"
```

With other security software (endpoint protection), see the documentation of the vendor or ask your IT department.

Spotlight indexes the new files, so decoding many files may also be slowed by Spotlight. A volume or a folder can be excluded from Spotlight in System Settings > Siri & Spotlight > Spotlight Privacy (or Spotlight > Search Privacy, depending on the macOS version), or for a whole volume:

```sh
sudo mdutil -i off "/Volumes/Scans"
```

## Linux

Most Linux distributions do not have real-time antivirus scanning by default, no setting is needed.

With Microsoft Defender for Endpoint on Linux:

```sh
mdatp exclusion folder add --path "/mnt/scans/film1/"
mdatp exclusion list
mdatp exclusion folder remove --path "/mnt/scans/film1/"
```

With ClamAV on-access scanning (`clamonacc`), add the folder to `clamd.conf` then restart the ClamAV services:

```
OnAccessExcludePath /mnt/scans/film1
```

With other security software, see the documentation of the vendor or ask your IT department.

## Benchmarking

When measuring the speed of RAWcooked, keep in mind that:

- the first run after adding or removing an exclusion, or after modifying the files, includes the scan of all files by the antivirus;
- the file cache of the operating system makes the next runs quicker. For measuring the speed with files read from the storage, empty the file cache before each run: `sync; echo 3 | sudo tee /proc/sys/vm/drop_caches` on Linux, `sync; sudo purge` on macOS, and on Windows the "Empty Standby List" command of RAMMap (Sysinternals) as administrator.
