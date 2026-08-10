# Package Windows Cordlang release artifacts (.zip + portable .exe).
#
# Usage:
#   powershell -File scripts\package_windows.ps1 `
#     -Binary cordlang.exe -Version 0.0.013-alpha.1 -Arch x64 -OutDir dist

param(
  [Parameter(Mandatory = $true)]
  [string]$Binary,

  [Parameter(Mandatory = $true)]
  [string]$Version,

  [Parameter(Mandatory = $true)]
  [ValidateSet("x64", "arm64")]
  [string]$Arch,

  [string]$OutDir = "dist"
)

$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

if (-not (Test-Path $Binary)) {
  throw "Binary not found: $Binary"
}

$Version = $Version.TrimStart("v")
$Art = "cordlang-windows-$Arch"
$Stage = Join-Path $OutDir "_stage_$Art"
$ArtDir = Join-Path $Stage $Art

if (Test-Path $Stage) { Remove-Item $Stage -Recurse -Force }
New-Item -ItemType Directory -Force -Path $ArtDir | Out-Null
New-Item -ItemType Directory -Force -Path $OutDir | Out-Null

Copy-Item $Binary (Join-Path $ArtDir "cordlang.exe")
if (Test-Path "LICENSE") { Copy-Item "LICENSE" $ArtDir }
if (Test-Path "README.md") { Copy-Item "README.md" $ArtDir }

@"
Cordlang CLI $Version
Artifact: $Art
Arch: $Arch
Binary: cordlang.exe

Run cordlang.exe, or add this folder to PATH.
Docs: https://github.com/cordlangorg/cordlang
"@ | Set-Content -Encoding utf8 (Join-Path $ArtDir "INSTALL.txt")

$ZipPath = Join-Path $OutDir "$Art.zip"
if (Test-Path $ZipPath) { Remove-Item $ZipPath -Force }
Compress-Archive -Path $ArtDir -DestinationPath $ZipPath -Force
Write-Host "OK: $ZipPath"

# Portable single-file download (same binary, stable release name)
$ExeOut = Join-Path $OutDir "$Art.exe"
Copy-Item $Binary $ExeOut -Force
Write-Host "OK: $ExeOut"

Remove-Item $Stage -Recurse -Force
Write-Host "Packaging complete for $Art"
