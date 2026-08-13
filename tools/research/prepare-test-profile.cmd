@echo off
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0prepare-test-profile.ps1" %*
exit /b %ERRORLEVEL%
