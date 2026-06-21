# Launch Meshtonic standalone LWD web app (HackRF + ESP32PP bridge).
$ErrorActionPreference = "Stop"
$MayhemRoot = Split-Path -Parent $MyInvocation.MyCommand.Path
$Lwd = Join-Path $MayhemRoot "lora-wideband-decoder"
Set-Location $Lwd

if (-not (Get-Command python -ErrorAction SilentlyContinue)) {
    throw "python not found in PATH"
}

Write-Host "Starting Meshtonic standalone app (browser opens to Meshtonic MDK tab)..."
python run/meshtonic_app.py @args
