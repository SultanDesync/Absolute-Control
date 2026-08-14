@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0validate-current.ps1" %*
exit /b %ERRORLEVEL%
