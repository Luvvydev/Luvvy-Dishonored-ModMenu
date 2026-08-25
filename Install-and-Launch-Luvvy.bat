@echo off
cd /d "%~dp0"
powershell.exe -NoProfile -ExecutionPolicy Bypass -File "%~dp0Install-Luvvy.ps1"
if errorlevel 1 (
    echo.
    echo Luvvy install failed.
    pause
    exit /b 1
)
echo Launching Dishonored...
start "" "%~dp0Binaries\Win32\Dishonored.exe"
