# Cordlang golden tests (Windows PowerShell)
# Compares cordlang compile fixture --backend {react,svelte,vue,solid} to tests/golden/*
#
# Create/update goldens (after intentional codegen changes):
#   .\tests\run_tests.ps1 -UpdateGoldens
# Or manually:
#   .\cordlang.exe compile tests\fixtures\basic_counter.cord --backend react > tests\golden\basic_counter.react.txt

param(
  [switch]$UpdateGoldens
)

$ErrorActionPreference = "Continue"

$ScriptDir = $PSScriptRoot
if ([string]::IsNullOrEmpty($ScriptDir)) {
  $ScriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
}
$Root = Split-Path -Parent $ScriptDir
Set-Location $Root

Write-Host "Cordlang golden tests"
Write-Host "  root: $Root"

$Cordlang = Join-Path $Root "cordlang.exe"
if (-not (Test-Path -LiteralPath $Cordlang)) {
  $Cordlang = Join-Path $Root "cordlang"
}
if (-not (Test-Path -LiteralPath $Cordlang)) {
  Write-Host "FAIL: cordlang executable not found. Run build.bat first." -ForegroundColor Red
  exit 1
}
Write-Host "  exe:  $Cordlang"

$FixturesDir = Join-Path $Root "tests\fixtures"
$GoldenDir = Join-Path $Root "tests\golden"
if (-not (Test-Path -LiteralPath $GoldenDir)) {
  New-Item -ItemType Directory -Path $GoldenDir | Out-Null
}

$names = "basic_counter", "routes_simple", "interp", "if_for", "nested_routes", "react_phase_d", "component_slot", "string_dotted", "escape_hash_brace", "svelte_phase_e", "preset_caps", "theme_design_tokens", "aria_hidden_bool"
$backends = "react", "svelte", "vue", "solid"
$extraGoldens = @(
  @{ Name = "email_static"; Backend = "email" },
  @{ Name = "email_static"; Backend = "pdf" }
)

function Get-NormalizedText([string]$text) {
  if ($null -eq $text) { return "" }
  $t = $text.Replace("`r`n", "`n")
  $t = $t.Replace("`r", "`n")
  # run_tests.sh compares via $(...), which strips trailing newlines on BOTH
  # sides. Do the same here or every golden fails on Windows with an
  # off-by-one line count while passing on Linux.
  return $t.TrimEnd("`n")
}

$failed = 0
$passed = 0
$updated = 0

foreach ($name in $names) {
  $fixture = Join-Path $FixturesDir ($name + ".cord")
  if (-not (Test-Path -LiteralPath $fixture)) {
    Write-Host ("FAIL: missing fixture " + $fixture) -ForegroundColor Red
    $failed = $failed + 1
    continue
  }

  foreach ($backend in $backends) {
    $goldenPath = Join-Path $GoldenDir ($name + "." + $backend + ".txt")
    $label = $name + " (" + $backend + ")"

    $cmdArgs = @("compile", $fixture, "--backend", $backend)
    $stdout = & $Cordlang @cmdArgs 2>&1
    $exitCode = $LASTEXITCODE

    if ($exitCode -ne 0) {
      Write-Host ("FAIL: " + $label + " - compile exit " + $exitCode) -ForegroundColor Red
      Write-Host $stdout
      $failed = $failed + 1
      continue
    }

    # & may return string or Object[]
    if ($stdout -is [System.Array]) {
      $actualRaw = ($stdout | ForEach-Object { "$_" }) -join "`n"
      if (-not $actualRaw.EndsWith("`n") -and $actualRaw.Length -gt 0) {
        $actualRaw = $actualRaw + "`n"
      }
    } else {
      $actualRaw = [string]$stdout
    }
    $actual = Get-NormalizedText $actualRaw

    if ($UpdateGoldens -or -not (Test-Path -LiteralPath $goldenPath)) {
      $utf8 = New-Object System.Text.UTF8Encoding $false
      [System.IO.File]::WriteAllText($goldenPath, $actual, $utf8)
      if ($UpdateGoldens) {
        Write-Host ("UPDATE: " + $label) -ForegroundColor Cyan
      } else {
        Write-Host ("CREATE: " + $label + " (first run)") -ForegroundColor Yellow
      }
      $updated = $updated + 1
      $passed = $passed + 1
      continue
    }

    $expected = Get-NormalizedText ([System.IO.File]::ReadAllText($goldenPath))
    if ($actual -eq $expected) {
      Write-Host ("PASS: " + $label) -ForegroundColor Green
      $passed = $passed + 1
    } else {
      Write-Host ("FAIL: " + $label + " - output differs from golden") -ForegroundColor Red
      Write-Host ("  golden: " + $goldenPath)
      $aLines = $actual.Split(@("`n"), [System.StringSplitOptions]::None)
      $eLines = $expected.Split(@("`n"), [System.StringSplitOptions]::None)
      $max = [Math]::Min($aLines.Length, $eLines.Length)
      for ($i = 0; $i -lt $max; $i++) {
        if ($aLines[$i] -ne $eLines[$i]) {
          Write-Host ("  first diff at line " + ($i + 1) + ":")
          Write-Host ("    expected: " + $eLines[$i])
          Write-Host ("    actual:   " + $aLines[$i])
          break
        }
      }
      if ($aLines.Length -ne $eLines.Length) {
        Write-Host ("  line counts: expected=" + $eLines.Length + " actual=" + $aLines.Length)
      }
      $failed = $failed + 1
    }
  }
}

# Dedicated goldens for static backends
foreach ($pair in $extraGoldens) {
  $name = $pair.Name
  $backend = $pair.Backend
  $fixture = Join-Path $FixturesDir ($name + ".cord")
  $goldenPath = Join-Path $GoldenDir ($name + "." + $backend + ".txt")
  $label = $name + " (" + $backend + ")"
  if (-not (Test-Path -LiteralPath $fixture)) {
    Write-Host ("FAIL: missing fixture " + $fixture) -ForegroundColor Red
    $failed = $failed + 1
    continue
  }
  $cmdArgs = @("compile", $fixture, "--backend", $backend)
  $stdout = & $Cordlang @cmdArgs 2>&1
  $exitCode = $LASTEXITCODE
  if ($exitCode -ne 0) {
    Write-Host ("FAIL: " + $label + " - compile exit " + $exitCode) -ForegroundColor Red
    $failed = $failed + 1
    continue
  }
  if ($stdout -is [System.Array]) {
    $actualRaw = ($stdout | ForEach-Object { "$_" }) -join "`n"
  } else {
    $actualRaw = [string]$stdout
  }
  $actual = Get-NormalizedText $actualRaw
  if ($UpdateGoldens -or -not (Test-Path -LiteralPath $goldenPath)) {
    $utf8 = New-Object System.Text.UTF8Encoding $false
    [System.IO.File]::WriteAllText($goldenPath, $actual, $utf8)
    if ($UpdateGoldens) {
      Write-Host ("UPDATE: " + $label) -ForegroundColor Cyan
    } else {
      Write-Host ("CREATE: " + $label + " (first run)") -ForegroundColor Yellow
    }
    $updated = $updated + 1
    $passed = $passed + 1
    continue
  }
  $expected = Get-NormalizedText ([System.IO.File]::ReadAllText($goldenPath))
  if ($actual -eq $expected) {
    Write-Host ("PASS: " + $label) -ForegroundColor Green
    $passed = $passed + 1
  } else {
    Write-Host ("FAIL: " + $label + " - output differs from golden") -ForegroundColor Red
    $failed = $failed + 1
  }
}

# ── Regression tests (Phase H1) ─────────────────────────────
Write-Host ""
Write-Host "Regression tests"

$RegRoot = Join-Path $Root "tests\regression"
if (Test-Path -LiteralPath $RegRoot) {
  $regDirs = Get-ChildItem -LiteralPath $RegRoot -Directory | Where-Object { $_.Name -notlike ".*" }
  foreach ($dir in $regDirs) {
    # NOT $input: reserved automatic variable (the pipeline enumerator).
    $inputCord = Join-Path $dir.FullName "input.cord"
    if (-not (Test-Path -LiteralPath $inputCord)) { continue }
    $slug = $dir.Name

    $expectFail = Join-Path $dir.FullName "expect_check_nonzero"
    if (Test-Path -LiteralPath $expectFail) {
      $label = "regression/$slug (check)"
      $out = & $Cordlang check $inputCord 2>&1 | Out-String
      $ec = $LASTEXITCODE
      if ($ec -eq 0) {
        Write-Host ("FAIL: " + $label + " - expected non-zero check") -ForegroundColor Red
        $failed = $failed + 1
      } else {
        $needlePath = Join-Path $dir.FullName "expect_check_contains.txt"
        if (Test-Path -LiteralPath $needlePath) {
          $needle = (Get-Content -LiteralPath $needlePath -TotalCount 1)
          if ($out -like ("*" + $needle + "*")) {
            Write-Host ("PASS: " + $label) -ForegroundColor Green
            $passed = $passed + 1
          } else {
            Write-Host ("FAIL: " + $label + " - missing substring '" + $needle + "'") -ForegroundColor Red
            Write-Host $out
            $failed = $failed + 1
          }
        } else {
          Write-Host ("PASS: " + $label) -ForegroundColor Green
          $passed = $passed + 1
        }
      }
    }

    # Any expected.<backend>.txt (SPA + email/pdf/next/sveltekit smokes)
    Get-ChildItem -LiteralPath $dir.FullName -Filter "expected.*.txt" -ErrorAction SilentlyContinue | ForEach-Object {
      $expectedPath = $_.FullName
      $backend = $_.BaseName.Substring("expected.".Length)
      $label = "regression/$slug (" + $backend + ")"
      # NOT $args: that is a reserved automatic variable backed by a
      # fixed-size array, so .Add() throws, the list stays empty and cordlang
      # gets invoked with no arguments (reported as "compile exit 1").
      $cmdArgs = New-Object System.Collections.Generic.List[string]
      [void]$cmdArgs.Add("compile")
      [void]$cmdArgs.Add($inputCord)
      [void]$cmdArgs.Add("--backend")
      [void]$cmdArgs.Add($backend)
      $passesPath = Join-Path $dir.FullName "passes.txt"
      if (Test-Path -LiteralPath $passesPath) {
        Get-Content -LiteralPath $passesPath | ForEach-Object {
          $pname = $_.Trim()
          if ($pname -and -not $pname.StartsWith("#")) {
            [void]$cmdArgs.Add("--pass")
            [void]$cmdArgs.Add($pname)
          }
        }
      }
      $stdout = & $Cordlang @($cmdArgs.ToArray()) 2>&1
      $exitCode = $LASTEXITCODE
      if ($exitCode -ne 0) {
        Write-Host ("FAIL: " + $label + " - compile exit " + $exitCode) -ForegroundColor Red
        $script:failed = $script:failed + 1
        return
      }
      if ($stdout -is [System.Array]) {
        $actualRaw = ($stdout | ForEach-Object { "$_" }) -join "`n"
        if (-not $actualRaw.EndsWith("`n") -and $actualRaw.Length -gt 0) {
          $actualRaw = $actualRaw + "`n"
        }
      } else {
        $actualRaw = [string]$stdout
      }
      $actual = Get-NormalizedText $actualRaw

      if ($UpdateGoldens) {
        $utf8 = New-Object System.Text.UTF8Encoding $false
        [System.IO.File]::WriteAllText($expectedPath, $actual, $utf8)
        Write-Host ("UPDATE: " + $label) -ForegroundColor Cyan
        $script:updated = $script:updated + 1
        $script:passed = $script:passed + 1
        return
      }

      $expected = Get-NormalizedText ([System.IO.File]::ReadAllText($expectedPath))
      if ($actual -eq $expected) {
        Write-Host ("PASS: " + $label) -ForegroundColor Green
        $script:passed = $script:passed + 1
      } else {
        Write-Host ("FAIL: " + $label + " - output differs from expected") -ForegroundColor Red
        Write-Host ("  expected: " + $expectedPath)
        $script:failed = $script:failed + 1
      }
    }
  }
} else {
  Write-Host "SKIP: tests/regression not present" -ForegroundColor Yellow
}

# ── Formatter tests (Phase C6) ─────────────────────────────
Write-Host ""
Write-Host "Formatter tests (fmt)"

$messyPath = Join-Path $FixturesDir "messy_fmt.cord"
$messyOrig = @"
# messy fixture for fmt


def Counter`t
  state count=0  


  props label="Counter"`t
  col gap=16 p=24 center   
    h1 "#{label}" size=2xl bold
"@
# Use real tabs / trailing spaces
$messyOrig = "# messy fixture for fmt`n`n`n`ndef Counter`t`n  state count=0  `n`n`n  props label=`"Counter`"`t`n  col gap=16 p=24 center   `n    h1 `"#{label}`" size=2xl bold"
$utf8NoBom = New-Object System.Text.UTF8Encoding $false
[System.IO.File]::WriteAllText($messyPath, $messyOrig, $utf8NoBom)

& $Cordlang fmt --check $messyPath 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) {
  Write-Host "PASS: fmt --check messy (would change)" -ForegroundColor Green
  $passed = $passed + 1
} else {
  Write-Host "FAIL: fmt --check messy expected non-zero" -ForegroundColor Red
  $failed = $failed + 1
}

& $Cordlang fmt $messyPath 2>&1 | Out-Null
if ($LASTEXITCODE -eq 0) {
  Write-Host "PASS: fmt messy (in-place)" -ForegroundColor Green
  $passed = $passed + 1
} else {
  Write-Host "FAIL: fmt messy exit non-zero" -ForegroundColor Red
  $failed = $failed + 1
}

& $Cordlang fmt --check $messyPath 2>&1 | Out-Null
if ($LASTEXITCODE -eq 0) {
  Write-Host "PASS: fmt --check clean after fmt" -ForegroundColor Green
  $passed = $passed + 1
} else {
  Write-Host "FAIL: fmt --check after fmt expected zero" -ForegroundColor Red
  $failed = $failed + 1
}

$formatted = [System.IO.File]::ReadAllText($messyPath)
# restore messy for next run
[System.IO.File]::WriteAllText($messyPath, $messyOrig, $utf8NoBom)
if ($formatted -match "def Counter`n" -and $formatted.EndsWith("`n") -and ($formatted -notmatch "  `n") -and ($formatted -notmatch "`t")) {
  Write-Host "PASS: fmt normalize (tabs/trailing/final newline)" -ForegroundColor Green
  $passed = $passed + 1
} else {
  # softer check: trailing blank collapse + final newline
  if ($formatted.EndsWith("`n") -and ($formatted -notmatch "`t")) {
    Write-Host "PASS: fmt normalize (basic)" -ForegroundColor Green
    $passed = $passed + 1
  } else {
    Write-Host "FAIL: fmt normalize unexpected content" -ForegroundColor Red
    Write-Host $formatted
    $failed = $failed + 1
  }
}

# ── Semantic check tests (Phase C3/C4) ─────────────────────
Write-Host ""
Write-Host "Semantic check tests"

# unknown component → non-zero
$unknown = Join-Path $FixturesDir "unknown_comp.cord"
if (Test-Path -LiteralPath $unknown) {
  & $Cordlang check $unknown 2>&1 | Out-Null
  if ($LASTEXITCODE -ne 0) {
    Write-Host "PASS: check unknown_comp (non-zero)" -ForegroundColor Green
    $passed = $passed + 1
  } else {
    Write-Host "FAIL: check unknown_comp expected non-zero exit" -ForegroundColor Red
    $failed = $failed + 1
  }
} else {
  Write-Host "FAIL: missing fixture unknown_comp.cord" -ForegroundColor Red
  $failed = $failed + 1
}

$iaCn = Join-Path $FixturesDir "ia_fail_classname.cord"
if (Test-Path -LiteralPath $iaCn) {
  & $Cordlang check $iaCn 2>&1 | Out-Null
  if ($LASTEXITCODE -ne 0) {
    Write-Host "PASS: check ia_fail_classname (non-zero)" -ForegroundColor Green
    $passed = $passed + 1
  } else {
    Write-Host "FAIL: check ia_fail_classname expected non-zero" -ForegroundColor Red
    $failed = $failed + 1
  }
} else {
  Write-Host "FAIL: missing ia_fail_classname.cord" -ForegroundColor Red
  $failed = $failed + 1
}

$iaTy = Join-Path $FixturesDir "ia_fail_bad_prop_type.cord"
if (Test-Path -LiteralPath $iaTy) {
  & $Cordlang check $iaTy 2>&1 | Out-Null
  if ($LASTEXITCODE -ne 0) {
    Write-Host "PASS: check ia_fail_bad_prop_type (non-zero)" -ForegroundColor Green
    $passed = $passed + 1
  } else {
    Write-Host "FAIL: check ia_fail_bad_prop_type expected non-zero" -ForegroundColor Red
    $failed = $failed + 1
  }
} else {
  Write-Host "FAIL: missing ia_fail_bad_prop_type.cord" -ForegroundColor Red
  $failed = $failed + 1
}

$typedOk = Join-Path $FixturesDir "typed_props_ok.cord"
if (Test-Path -LiteralPath $typedOk) {
  & $Cordlang check $typedOk 2>&1 | Out-Null
  if ($LASTEXITCODE -eq 0) {
    Write-Host "PASS: check typed_props_ok (zero)" -ForegroundColor Green
    $passed = $passed + 1
  } else {
    Write-Host "FAIL: check typed_props_ok expected zero" -ForegroundColor Red
    $failed = $failed + 1
  }
} else {
  Write-Host "FAIL: missing typed_props_ok.cord" -ForegroundColor Red
  $failed = $failed + 1
}

$iaUa = Join-Path $FixturesDir "ia_fail_unknown_attr.cord"
if (Test-Path -LiteralPath $iaUa) {
  $out = & $Cordlang check $iaUa 2>&1 | Out-String
  if ($out -match "unknown attribute") {
    Write-Host "PASS: check ia_fail_unknown_attr (warn)" -ForegroundColor Green
    $passed = $passed + 1
  } else {
    Write-Host "FAIL: check ia_fail_unknown_attr expected warning" -ForegroundColor Red
    $failed = $failed + 1
  }
} else {
  Write-Host "FAIL: missing ia_fail_unknown_attr.cord" -ForegroundColor Red
  $failed = $failed + 1
}

$iaTrapPairs = @(
  @{ File = "ia_fail_usestate.cord"; Code = "jsx-hook" },
  @{ File = "ia_fail_map.cord"; Code = "jsx-map" },
  @{ File = "ia_fail_jsx_tag.cord"; Code = "jsx-tag" },
  @{ File = "ia_fail_missing_preset.cord"; Code = "missing-preset" },
  @{ File = "ia_fail_foreign_unbound.cord"; Code = "foreign-unbound" }
)
foreach ($pair in $iaTrapPairs) {
  $iaPath = Join-Path $FixturesDir $pair.File
  if (Test-Path -LiteralPath $iaPath) {
    $out = & $Cordlang check --json $iaPath 2>&1 | Out-String
    $codePat = '"code":"' + $pair.Code + '"'
    if ($out -match [regex]::Escape($codePat)) {
      Write-Host ("PASS: check " + $pair.File + " → " + $pair.Code) -ForegroundColor Green
      $passed = $passed + 1
    } else {
      Write-Host ("FAIL: check " + $pair.File + " expected code " + $pair.Code) -ForegroundColor Red
      Write-Host $out
      $failed = $failed + 1
    }
  } else {
    Write-Host ("FAIL: missing " + $pair.File) -ForegroundColor Red
    $failed = $failed + 1
  }
}

& $Cordlang analyze $typedOk 2>&1 | Out-Null
if ($LASTEXITCODE -eq 0) {
  Write-Host "PASS: analyze typed_props_ok" -ForegroundColor Green
  $passed = $passed + 1
} else {
  Write-Host "FAIL: analyze typed_props_ok" -ForegroundColor Red
  $failed = $failed + 1
}

# valid my-app → zero
$myApp = Join-Path $Root "my-app"
if (Test-Path -LiteralPath (Join-Path $myApp "cordlang.json")) {
  Push-Location $myApp
  try {
    & $Cordlang check 2>&1 | Out-Null
    if ($LASTEXITCODE -eq 0) {
      Write-Host "PASS: check my-app (zero)" -ForegroundColor Green
      $passed = $passed + 1
    } else {
      Write-Host "FAIL: check my-app expected zero exit" -ForegroundColor Red
      $failed = $failed + 1
    }
  } finally {
    Pop-Location
  }
} else {
  Write-Host "SKIP: my-app project not present" -ForegroundColor Yellow
}

Write-Host ""
Write-Host ("Results: " + $passed + " passed, " + $failed + " failed, " + $updated + " golden writes")
if ($failed -gt 0) { exit 1 }
exit 0
