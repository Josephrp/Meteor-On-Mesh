# Build Mayhem MDK firmware in Docker and export artifacts to ../out and WebInstaller.
param(
    [switch]$NoWebInstallerCopy,
    [switch]$RebuildImage
)

$ErrorActionPreference = "Stop"
$MayhemRoot = Split-Path -Parent $PSScriptRoot
Set-Location $MayhemRoot

if (-not (Get-Command docker -ErrorAction SilentlyContinue)) {
    throw "docker not found in PATH"
}

docker info 2>&1 | Out-Null
if ($LASTEXITCODE -ne 0) {
    throw "Docker daemon is not running. Start Docker Desktop and retry."
}

New-Item -ItemType Directory -Force -Path out | Out-Null

$imageExists = docker image inspect meshtonic-mdk-idf:v5.4.1 2>$null
if ($RebuildImage -or -not $imageExists) {
    Write-Host "Building local image meshtonic-mdk-idf:v5.4.1 (one-time base pull from espressif/idf:v5.4.1)..."
    docker compose -f docker/docker-compose.yml build idf-build
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} else {
    Write-Host "Using cached image meshtonic-mdk-idf:v5.4.1"
}

Write-Host "Compiling firmware in container..."
docker compose -f docker/docker-compose.yml run --rm idf-build
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

if (-not $NoWebInstallerCopy) {
    Copy-Item -Force out/merged.bin WebInstaller/merged.bin
    Write-Host "Updated WebInstaller/merged.bin"
}

Write-Host ""
Write-Host "Build artifacts:"
Get-ChildItem out | Format-Table Name, Length, LastWriteTime
Write-Host "Done. Flash merged.bin @ 0x0 or use WebInstaller."
