@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0evaluate-phase.ps1" %*
exit /b %ERRORLEVEL%
