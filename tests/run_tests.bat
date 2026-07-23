@echo off
REM Cordlang golden tests (Windows)
REM Usage:
REM   tests\run_tests.bat
REM   tests\run_tests.bat -UpdateGoldens
setlocal
cd /d "%~dp0.."
powershell -NoProfile -ExecutionPolicy Bypass -File "%~dp0run_tests.ps1" %*
exit /b %ERRORLEVEL%
