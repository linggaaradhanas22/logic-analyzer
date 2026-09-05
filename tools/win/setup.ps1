[CmdletBinding()]
param(
    [string]$Python = "py -3.11",
    [switch]$InstallPackages
)

$ErrorActionPreference = "Stop"
$ProjectRoot = (Resolve-Path (Join-Path $PSScriptRoot "..\..")).Path
$Venv = Join-Path $ProjectRoot ".venv"
$Build = Join-Path $ProjectRoot "build"

Write-Host "PROJECT_ROOT=$ProjectRoot"
foreach ($name in @("questa", "native", "vivado", "vitis", "esp32", "release", "reports", "tmp", "cache")) {
    New-Item -ItemType Directory -Force -Path (Join-Path $Build $name) | Out-Null
}

if (-not (Test-Path -LiteralPath (Join-Path $Venv "Scripts\python.exe"))) {
    Write-Host "Creating project-local Python environment"
    Invoke-Expression "$Python -m venv `"$Venv`""
}

if ($InstallPackages) {
    $ProjectPython = Join-Path $Venv "Scripts\python.exe"
    & $ProjectPython -m pip install --upgrade pip
    if ($LASTEXITCODE -ne 0) { throw "pip upgrade failed with exit code $LASTEXITCODE" }
    & $ProjectPython -m pip install -e $ProjectRoot
    if ($LASTEXITCODE -ne 0) { throw "project package installation failed with exit code $LASTEXITCODE" }
}

Write-Host "Environment scaffold ready. Run:"
Write-Host "  $Venv\Scripts\python.exe tools\check_env.py --write-lock"
Write-Host "  $Venv\Scripts\python.exe tools\run.py test-native"
