@echo off
echo ==========================================
echo Auto Upload Script for ESP32 Cannon
echo ==========================================
echo.

REM Get current branch name
for /f "tokens=*" %%a in ('git rev-parse --abbrev-ref HEAD') do set CURRENT_BRANCH=%%a
echo Current branch: %CURRENT_BRANCH%
echo.

echo [1/4] Fetching latest code from GitHub...
git fetch origin
if errorlevel 1 (
    echo ERROR: Failed to fetch from remote!
    pause
    exit /b 1
)

echo [2/4] Resetting local files to match remote...
git reset --hard origin/%CURRENT_BRANCH%
if errorlevel 1 (
    echo ERROR: Failed to reset to remote branch!
    pause
    exit /b 1
)

echo [3/4] Cleaning build directory...
if exist .pio rmdir /s /q .pio

echo [4/4] Uploading to ESP32...
platformio run --target upload
if errorlevel 1 (
    echo ERROR: Upload failed!
    pause
    exit /b 1
)

echo.
echo ==========================================
echo Upload completed successfully!
echo ==========================================
pause
