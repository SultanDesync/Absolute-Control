@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Test-CurrentProcess.ps1" %*
exit /b %ERRORLEVEL%
