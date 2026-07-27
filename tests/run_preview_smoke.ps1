# Cordlang ESM preview smoke (in-process handler)
param()
$ErrorActionPreference = "Stop"
$Root = Split-Path -Parent $PSScriptRoot
Set-Location $Root

$Cordlang = Join-Path $Root "cordlang.exe"
if (-not (Test-Path $Cordlang)) { $Cordlang = Join-Path $Root "cordlang" }
if (-not (Test-Path $Cordlang)) {
  Write-Host "FAIL: cordlang not built" -ForegroundColor Red
  exit 1
}

$proj = Join-Path $Root "templates\counter"
if (-not (Test-Path (Join-Path $proj "cordlang.json"))) {
  Write-Host "FAIL: templates/counter missing" -ForegroundColor Red
  exit 1
}

Push-Location $proj
try {
  & $Cordlang run --smoke
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} finally {
  Pop-Location
}

Write-Host "PASS: preview smoke" -ForegroundColor Green
exit 0
