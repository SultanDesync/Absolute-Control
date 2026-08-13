@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0evaluate-builder-run.ps1" %*
exit /b %ERRORLEVEL%
