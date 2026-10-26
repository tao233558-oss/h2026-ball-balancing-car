param(
    [string]$Python = ""
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$thirdParty = Join-Path $projectRoot "third_party"
$dotnetRoot = Join-Path $projectRoot ".dotnet"

if (-not $Python) {
    $Python = Join-Path $projectRoot ".venv-yolo\Scripts\python.exe"
}

if (-not (Test-Path -LiteralPath $Python -PathType Leaf)) {
    throw "Python was not found: $Python"
}

New-Item -ItemType Directory -Force -Path $thirdParty | Out-Null

# These changes only affect this child PowerShell process. They prevent stale
# system proxy settings from breaking the downloads.
$env:NO_PROXY = "*"
Remove-Item Env:HTTP_PROXY -ErrorAction SilentlyContinue
Remove-Item Env:HTTPS_PROXY -ErrorAction SilentlyContinue
Remove-Item Env:ALL_PROXY -ErrorAction SilentlyContinue

function Invoke-Download {
    param(
        [Parameter(Mandatory = $true)]
        [string]$Url,
        [Parameter(Mandatory = $true)]
        [string]$Destination
    )

    $partial = "$Destination.part"
    & curl.exe --noproxy "*" -L --retry 2 --connect-timeout 30 `
        $Url -o $partial

    if ($LASTEXITCODE -ne 0) {
        Write-Output "Normal DNS failed; retrying through encrypted AliDNS..."
        & curl.exe --noproxy "*" -L --retry 5 --connect-timeout 30 `
            --doh-url "https://dns.alidns.com/dns-query" `
            --resolve "dns.alidns.com:443:223.5.5.5" `
            $Url -o $partial
    }

    if ($LASTEXITCODE -ne 0) {
        throw "Download failed: $Url"
    }

    if (-not (Test-Path -LiteralPath $partial -PathType Leaf) -or
        (Get-Item -LiteralPath $partial).Length -lt 1024) {
        throw "The downloaded file is missing or too small: $partial"
    }

    Move-Item -LiteralPath $partial -Destination $Destination -Force
}

Write-Output "[1/4] Installing local .NET SDK 7..."
$dotnetExe = Join-Path $dotnetRoot "dotnet.exe"
if (-not (Test-Path -LiteralPath $dotnetExe -PathType Leaf)) {
    $dotnetInstaller = Join-Path $thirdParty "dotnet-install.ps1"
    Invoke-Download `
        -Url "https://dot.net/v1/dotnet-install.ps1" `
        -Destination $dotnetInstaller

    & powershell.exe -NoProfile -ExecutionPolicy Bypass `
        -File $dotnetInstaller `
        -Channel "7.0" `
        -InstallDir $dotnetRoot
    if ($LASTEXITCODE -ne 0) {
        throw ".NET SDK 7 installation failed with exit code $LASTEXITCODE"
    }
}

$env:DOTNET_ROOT = $dotnetRoot
$env:PATH = "$dotnetRoot;$env:PATH"
& $dotnetExe --version

Write-Output "[2/4] Installing nncase 2.11.0..."
& $Python -m pip --isolated install `
    --index-url "https://mirrors.tuna.tsinghua.edu.cn/pypi/web/simple" `
    --timeout 120 `
    --retries 10 `
    "nncase==2.11.0"
if ($LASTEXITCODE -ne 0) {
    throw "nncase installation failed with exit code $LASTEXITCODE"
}

Write-Output "[3/4] Installing the matching nncase-kpu wheel..."
$kpuWheel = Join-Path $thirdParty `
    "nncase_kpu-2.11.0-py2.py3-none-win_amd64.whl"
if (-not (Test-Path -LiteralPath $kpuWheel -PathType Leaf) -or
    (Get-Item -LiteralPath $kpuWheel).Length -lt 1024) {
    Invoke-Download `
        -Url "https://github.com/kendryte/nncase/releases/download/v2.11.0/nncase_kpu-2.11.0-py2.py3-none-win_amd64.whl" `
        -Destination $kpuWheel
}

& $Python -m pip install $kpuWheel
if ($LASTEXITCODE -ne 0) {
    throw "nncase-kpu installation failed with exit code $LASTEXITCODE"
}

Write-Output "[4/4] Downloading the official K230 YOLOv8 converter..."
$converterZip = Join-Path $thirdParty "test_yolov8.zip"
if (-not (Test-Path -LiteralPath $converterZip -PathType Leaf) -or
    (Get-Item -LiteralPath $converterZip).Length -lt 1024) {
    Invoke-Download `
        -Url "https://kendryte-download.canaan-creative.com/developer/k230/yolo_files/test_yolov8.zip" `
        -Destination $converterZip
}

$expectedConverter = Join-Path $thirdParty "test_yolov8\detect\to_kmodel.py"
if (-not (Test-Path -LiteralPath $expectedConverter -PathType Leaf)) {
    Expand-Archive -LiteralPath $converterZip -DestinationPath $thirdParty -Force
}

$converter = Get-ChildItem -LiteralPath $thirdParty -Recurse `
    -Filter "to_kmodel.py" -File |
    Where-Object { $_.Directory.Name -eq "detect" } |
    Select-Object -First 1

if (-not $converter) {
    throw "The archive was extracted, but detect\to_kmodel.py was not found."
}

& $Python -c "import _nncase; print('nncase:', _nncase.__version__)"
if ($LASTEXITCODE -ne 0) {
    throw "nncase import check failed with exit code $LASTEXITCODE"
}

Write-Output ""
Write-Output "KModel prerequisites are ready."
Write-Output "Converter: $($converter.FullName)"
Write-Output "Please paste the complete output back into Codex."
