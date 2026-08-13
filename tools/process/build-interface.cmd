@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-interface.ps1" %*
exit /b %ERRORLEVEL%
