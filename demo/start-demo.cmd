@echo off
chcp 65001 >nul
setlocal
cd /d "%~dp0"
call "%~dp0start.cmd" %*
if errorlevel 1 pause
