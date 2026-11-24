$ErrorActionPreference = "Stop"

python -m PyInstaller `
    --noconfirm `
    --clean `
    --onefile `
    --windowed `
    --exclude-module tkinter `
    --name "RTC-DS3231-Sync" `
    tools\rtc_gui.py

Write-Host "Executable tersedia di dist\RTC-DS3231-Sync.exe"
