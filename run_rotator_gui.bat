@echo off
title Global Region Rotator
set "PATH=%LOCALAPPDATA%\Python\pythoncore-3.15-64\DLLs;%PATH%"
cd /d "%~dp0tools"
python global_region_rotator_gui.py
pause
