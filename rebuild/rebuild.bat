@echo off
setlocal
set PATH=C:\msys64\mingw64\bin;%PATH%

echo Killing all cordlang processes...
powershell -NoProfile -Command "Get-Process cordlang -ErrorAction SilentlyContinue | Stop-Process -Force"
if exist cordlang.exe (
  ren cordlang.exe cordlang.exe.old
)

call build.bat
if %ERRORLEVEL% NEQ 0 goto :eof

if exist cordlang.exe.old del cordlang.exe.old

echo Installing globally...
powershell -NoProfile -Command ^
  "Get-Process cordlang -ErrorAction SilentlyContinue | Stop-Process -Force;" ^
  "Start-Sleep -Milliseconds 500;" ^
  "Ren 'C:\Users\burge\AppData\Local\Microsoft\WindowsApps\cordlang.exe' 'cordlang.exe.tmp' -EA 0;" ^
  "Copy-Item 'cordlang.exe' 'C:\Users\burge\AppData\Local\Microsoft\WindowsApps\cordlang.exe' -Force;" ^
  "Remove-Item 'C:\Users\burge\AppData\Local\Microsoft\WindowsApps\cordlang.exe.tmp' -Force -EA 0"

echo Ready.
