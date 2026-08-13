@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0new-phase-prompt.ps1" %*
exit /b %ERRORLEVEL%
