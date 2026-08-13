@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-host.ps1" %*
exit /b %ERRORLEVEL%
