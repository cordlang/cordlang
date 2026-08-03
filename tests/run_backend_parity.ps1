# Backend parity report (tiers + compile smoke)
#
# Prints Official / Candidate / Experimental tiers and runs a fast compile
# smoke for Official + Candidate. Meta backends are smoke-only with -IncludeMeta.
#
# Usage (repo root, after build.bat):
#   powershell -ExecutionPolicy Bypass -File tests\run_backend_parity.ps1
#   powershell -ExecutionPolicy Bypass -File tests\run_backend_parity.ps1 -IncludeMeta
#   powershell -ExecutionPolicy Bypass -File tests\run_backend_parity.ps1 -Full
#
# Default: compile fixtures for react, svelte, vue + build esm - must pass
# IncludeMeta: also compile solid, email, pdf, next, sveltekit (soft unless -StrictMeta)
# Full: also run templates/counter --check for react+svelte (+ vue) and preview --smoke
#
# See docs/BACKENDS.md

param(
  [switch]$IncludeMeta,
  [switch]$StrictMeta,
  [switch]$Full
)

$ErrorActionPreference = "Continue"

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

$Fixture = Join-Path $Root "tests\fixtures\basic_counter.cord"
if (-not (Test-Path -LiteralPath $Fixture)) {
  Write-Host "FAIL: missing $Fixture" -ForegroundColor Red
  exit 1
}

Write-Host "Cordlang backend parity"
Write-Host "  root: $Root"
Write-Host "  exe:  $Cordlang"
Write-Host ""
Write-Host "Tiers (docs/BACKENDS.md):" -ForegroundColor Cyan
Write-Host "  Official:    esm/preview, react, svelte"
Write-Host "  Candidate:   vue"
Write-Host "  Experimental: solid, html, email, pdf, next, sveltekit"
Write-Host ""

$failed = 0
$metaFailed = 0

function Test-Compile([string]$backend, [string]$tier) {
  Write-Host "=== compile [$tier] --backend $backend ===" -ForegroundColor Cyan
  $null = & $Cordlang compile $Fixture --backend $backend 2>&1
  if ($LASTEXITCODE -ne 0) {
    Write-Host "FAIL: compile --backend $backend (exit $LASTEXITCODE)" -ForegroundColor Red
    return $false
  }
  Write-Host "PASS: $backend" -ForegroundColor Green
  return $true
}

foreach ($pair in @(
  @{ be = "react"; tier = "Official" },
  @{ be = "svelte"; tier = "Official" },
  @{ be = "vue"; tier = "Candidate" }
)) {
  if (-not (Test-Compile $pair.be $pair.tier)) { $failed++ }
}

Write-Host "=== build [Official] esm (templates/counter) ===" -ForegroundColor Cyan
$App = Join-Path $Root "templates\counter"
if (Test-Path -LiteralPath (Join-Path $App "cordlang.json")) {
  Push-Location $App
  try {
    & $Cordlang build esm 2>&1 | Out-Null
    if ($LASTEXITCODE -ne 0) {
      Write-Host "FAIL: build esm (exit $LASTEXITCODE)" -ForegroundColor Red
      $failed++
    } else {
      Write-Host "PASS: build esm" -ForegroundColor Green
    }
  } finally {
    Pop-Location
  }
} else {
  Write-Host "SKIP: templates/counter missing" -ForegroundColor Yellow
}

if ($IncludeMeta) {
  foreach ($be in @("solid", "email", "pdf", "next", "sveltekit", "html")) {
    if (-not (Test-Compile $be "Experimental")) {
      if ($StrictMeta) {
        $failed++
      } else {
        $metaFailed++
        Write-Host "(soft) experimental failure counted separately" -ForegroundColor Yellow
      }
    }
  }
}

if ($Full) {
  Write-Host ""
  Write-Host "=== -Full: template --check + preview smoke ===" -ForegroundColor Cyan
  $tpl = Join-Path $Root "tests\run_template_check.ps1"
  if (Test-Path $tpl) {
    & powershell -ExecutionPolicy Bypass -File $tpl
    if ($LASTEXITCODE -ne 0) { $failed++ }
  }
  Push-Location $App
  try {
    Write-Host "=== cordlang run vue --check (Candidate) ===" -ForegroundColor Cyan
    & $Cordlang run vue --check
    if ($LASTEXITCODE -ne 0) {
      Write-Host "FAIL: run vue --check" -ForegroundColor Red
      $failed++
    } else {
      Write-Host "PASS: run vue --check" -ForegroundColor Green
    }
  } finally {
    Pop-Location
  }
  $smoke = Join-Path $Root "tests\run_preview_smoke.ps1"
  if (Test-Path $smoke) {
    & powershell -ExecutionPolicy Bypass -File $smoke
    if ($LASTEXITCODE -ne 0) { $failed++ }
  }
}

Write-Host ""
Write-Host "Matrix:" -ForegroundColor Cyan
Write-Host "  feature           esm  react  svelte  vue"
Write-Host "  compile smoke     yes  yes    yes     yes candidate"
Write-Host "  template --check  n/a  yes*   yes*    yes* with -Full"
Write-Host "  * Official CI via run_template_check; vue via -Full / VUE_PROMOTION"
Write-Host ""

if ($metaFailed -gt 0) {
  Write-Host "Experimental soft failures: $metaFailed - use -StrictMeta to fail" -ForegroundColor Yellow
}

if ($failed -gt 0) {
  Write-Host "Results: $failed Official/Candidate failure(s)" -ForegroundColor Red
  exit 1
}

Write-Host "Results: Official + Candidate OK" -ForegroundColor Green
exit 0
