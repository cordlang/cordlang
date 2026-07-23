# G4: scaffold my-app + vite build for react and svelte
# Usage (from repo root, after building cordlang.exe):
#   powershell -ExecutionPolicy Bypass -File tests\run_myapp_check.ps1

$ErrorActionPreference = "Stop"

$ScriptDir = $PSScriptRoot
if ([string]::IsNullOrEmpty($ScriptDir)) {
  $ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
}
$Root = Split-Path -Parent $ScriptDir
Set-Location $Root

$Cordlang = Join-Path $Root "cordlang.exe"
if (-not (Test-Path -LiteralPath $Cordlang)) {
  $Cordlang = Join-Path $Root "cordlang"
}
if (-not (Test-Path -LiteralPath $Cordlang)) {
  Write-Host "FAIL: cordlang not found. Run build.bat first." -ForegroundColor Red
  exit 1
}

$MyApp = Join-Path $Root "my-app"
if (-not (Test-Path -LiteralPath (Join-Path $MyApp "cordlang.json"))) {
  Write-Host "FAIL: my-app/cordlang.json missing" -ForegroundColor Red
  exit 1
}

Write-Host "Cordlang my-app --check"
Write-Host "  root: $Root"
Write-Host "  exe:  $Cordlang"
Write-Host "  app:  $MyApp"

$failed = 0
foreach ($backend in @("react", "svelte")) {
  Write-Host ""
  Write-Host "=== cordlang run $backend --check ===" -ForegroundColor Cyan
  Push-Location $MyApp
  try {
    & $Cordlang run $backend --check
    if ($LASTEXITCODE -ne 0) {
      Write-Host "FAIL: run $backend --check (exit $LASTEXITCODE)" -ForegroundColor Red
      $failed = $failed + 1
    } else {
      Write-Host "PASS: run $backend --check" -ForegroundColor Green
    }
  } finally {
    Pop-Location
  }
}

if ($failed -gt 0) {
  Write-Host ""
  Write-Host "Results: $failed backend check(s) failed" -ForegroundColor Red
  exit 1
}

Write-Host ""
Write-Host "Results: react + svelte --check OK" -ForegroundColor Green
exit 0
