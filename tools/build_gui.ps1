$ErrorActionPreference = "Stop"

python -m PyInstaller `
    --noconfirm `
    --clean `
    --onefile `
    --windowed `
    --exclude-module tkinter `
    --icon "assets\rtc-sync.ico" `
    --add-data "assets\rtc-sync-window.png;assets" `
    --name "DS3231-RTC-Sync-Tool" `
    tools\rtc_gui.py

if ($LASTEXITCODE -ne 0) {
    throw "PyInstaller gagal dengan exit code $LASTEXITCODE"
}

Write-Host "Executable tersedia di dist\DS3231-RTC-Sync-Tool.exe"
