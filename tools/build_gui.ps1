$ErrorActionPreference = "Stop"

python -m PyInstaller `
    --noconfirm `
    --clean `
    --onefile `
    --windowed `
    --exclude-module tkinter `
    --icon "assets\rtc-sync.ico" `
    --add-data "assets\rtc-sync.ico;assets" `
    --name "RTC-DS3231-Sync" `
    tools\rtc_gui.py

if ($LASTEXITCODE -ne 0) {
    throw "PyInstaller gagal dengan exit code $LASTEXITCODE"
}

Write-Host "Executable tersedia di dist\RTC-DS3231-Sync.exe"
