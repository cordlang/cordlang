@echo off
set VSCODE="C:\Users\burge\AppData\Local\Programs\Microsoft VS Code\bin\code.cmd"
cd /d "%~dp0editor\vscode"
npx.cmd --yes @vscode/vsce package
%VSCODE% --uninstall-extension cordlang.cordlang
for %%f in (cordlang-*.vsix) do %VSCODE% --install-extension "%%f"
echo Extension updated.
