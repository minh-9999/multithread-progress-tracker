@echo off
setlocal

if "%~1"=="" (
    echo Usage: send_slack.cmd job_summary.json
    exit /b 1
)

set JSON_FILE=%~1

if not exist %JSON_FILE% (
    echo File not found: %JSON_FILE%
    exit /b 1
)

:: === SET YOUR SLACK WEBHOOK URL HERE ===
@REM set WEBHOOK_URL=https://hooks.slack.com/services/YOUR/WEBHOOK/URL
set WEBHOOK_URL=https://webhook.site/c593ae4c-16d9-4f90-95c0-770828637e99

:: Read JSON content into a variable
for /f "usebackq delims=" %%a in (%JSON_FILE%) do (
    set "DATA=%%a"
)

:: Send message to Slack
@REM curl -X POST -H "Content-Type: application/json" --data "%DATA%" %WEBHOOK_URL%
curl -X POST -H "Content-Type: application/json" --data "%DATA%" %WEBHOOK_URL%

endlocal
