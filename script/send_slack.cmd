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

:: === GET SLACK WEBHOOK URL FROM ENV VARIABLE ===
if "%SLACK_WEBHOOK_URL%"=="" (
    echo Error: SLACK_WEBHOOK_URL environment variable not set
    exit /b 1
)

set WEBHOOK_URL=%SLACK_WEBHOOK_URL%

:: Read JSON content into a variable
set "DATA="
for /f "usebackq delims=" %%a in (%JSON_FILE%) do (
    set "LINE=%%a"
    call set "DATA=%%DATA%%%%LINE%%"
)

:: Send message to Slack
@REM curl -X POST -H "Content-Type: application/json" --data "%DATA%" %WEBHOOK_URL%
curl -X POST -H "Content-Type: application/json" --data "%DATA%" %WEBHOOK_URL%

endlocal
