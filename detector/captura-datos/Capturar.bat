@echo off
cd /d "%~dp0"
python -c "import serial" 2>nul || python -m pip install pyserial
python capturador.py
if errorlevel 1 pause
