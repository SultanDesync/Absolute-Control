@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0discard-builder-run.ps1" %*
exit /b %ERRORLEVEL%
