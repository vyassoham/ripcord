@echo off
title Ripcord Launcher

:: Check if Ripcord is already running and terminate stale instances
tasklist /fi "imagename eq Ripcord.exe" | find /i "Ripcord.exe" >nul
if %errorlevel% equ 0 (
    echo Closing previous background instance...
    taskkill /f /im Ripcord.exe >nul 2>&1
    timeout /t 1 /nobreak >nul
)

:: Clear any stale lockfile if present
if exist "%LOCALAPPDATA%\Ripcord\ripcord.lockfile" (
    del /f /q "%LOCALAPPDATA%\Ripcord\ripcord.lockfile" >nul 2>&1
)

cd /d "%~dp0dist"
start "" "Ripcord.exe"
