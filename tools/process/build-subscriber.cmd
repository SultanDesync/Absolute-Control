@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0build-subscriber.ps1" %*
exit /b %ERRORLEVEL%
