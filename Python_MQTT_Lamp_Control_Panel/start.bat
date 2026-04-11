@echo off
setlocal
chcp 65001 >nul

set PROFILE=%~1
if "%PROFILE%"=="" set PROFILE=desktop_local

echo ========================================
echo Starting Smart Lamp Control Panel...
echo Active profile: %PROFILE%
echo ========================================

if exist "%~dp0venv\Scripts\activate.bat" (
  call "%~dp0venv\Scripts\activate.bat"
)

python "%~dp0lamp_control_panel.py" --profile %PROFILE%

if errorlevel 1 (
  echo.
  echo Program exited with an error. Check logs above.
)

pause
endlocal
