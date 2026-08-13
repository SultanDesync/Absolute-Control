@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0new-builder-run.ps1" %*
exit /b %ERRORLEVEL%
