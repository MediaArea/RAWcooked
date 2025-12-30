$ErrorActionPreference = 'Stop';

$packageArgs = @{
  PackageName    = "${env:ChocolateyPackageName}"
  Url64bit       = "https://mediaarea.net/download/binary/rawcooked/25.12/RAWcooked_CLI_25.12_Windows_x64.zip"
  Checksum64     = '0000000000000000000000000000000000000000000000000000000000000000'
  ChecksumType64 = 'sha256'
  UnzipLocation = "$(split-path -parent $MyInvocation.MyCommand.Definition)\rawcooked"
}

Install-ChocolateyZipPackage @packageArgs
