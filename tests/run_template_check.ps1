# Scaffold a stock template + vite build for react and svelte.
# Usage (from repo root, after building cordlang.exe):
#   powershell -ExecutionPolicy Bypass -File tests\run_template_check.ps1
# Optional: -Template counter|landing|dashboard|form-fetch|docs-shell

param(
  [string]$Template = "counter"
)

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

$App = Join-Path $Root "templates\$Template"
if (-not (Test-Path -LiteralPath (Join-Path $App "cordlang.json"))) {
  Write-Host "FAIL: templates/$Template/cordlang.json missing" -ForegroundColor Red
  exit 1
}

Write-Host "Cordlang template --check"
Write-Host "  root: $Root"
Write-Host "  exe:  $Cordlang"
Write-Host "  app:  $App"

$failed = 0
foreach ($backend in @("react", "svelte")) {
  Write-Host ""
  Write-Host "=== cordlang run $backend --check ===" -ForegroundColor Cyan
  Push-Location $App
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
Write-Host "Results: react + svelte --check OK ($Template)" -ForegroundColor Green
exit 0
