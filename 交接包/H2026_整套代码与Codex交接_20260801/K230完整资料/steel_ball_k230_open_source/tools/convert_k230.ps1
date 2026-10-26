param(
    [Parameter(Mandatory = $true)]
    [string]$OnnxPath,
    [string]$Python = "python",
    [string]$OfficialToolRoot = "",
    [ValidateRange(0, 5)]
    [int]$PtqOption = 1
)

$ErrorActionPreference = "Stop"
$projectRoot = Split-Path -Parent $PSScriptRoot
$onnx = (Resolve-Path -LiteralPath $OnnxPath).Path
$calibration = Join-Path $projectRoot "steel_ball_yolo\calibration"
$localDotnet = Join-Path $projectRoot ".dotnet"

if (Test-Path -LiteralPath (Join-Path $localDotnet "dotnet.exe") -PathType Leaf) {
    $env:DOTNET_ROOT = $localDotnet
    $env:PATH = "$localDotnet;$env:PATH"
}

$sitePackages = (& $Python -c "import site; print(site.getsitepackages()[0])").Trim()
if ($LASTEXITCODE -ne 0 -or
    -not (Test-Path -LiteralPath $sitePackages -PathType Container)) {
    throw "Could not locate Python site-packages for: $Python"
}
$env:NNCASE_PLUGIN_PATH = $sitePackages
$env:PATH = "$sitePackages;$env:PATH"

if (-not $OfficialToolRoot) {
    $OfficialToolRoot = Join-Path $projectRoot "third_party\test_yolov8"
}

$converter = Join-Path $OfficialToolRoot "detect\to_kmodel.py"
if (-not (Test-Path -LiteralPath $converter -PathType Leaf)) {
    throw @"
The official converter is missing:
  $converter

Download and extract:
  https://kendryte-download.canaan-creative.com/developer/k230/yolo_files/test_yolov8.zip

The extracted directory must contain detect\to_kmodel.py.
"@
}

if (-not (Test-Path -LiteralPath $calibration -PathType Container)) {
    throw "Calibration directory is missing: $calibration"
}

$compatibilityModules = $PSScriptRoot
$previousPythonPath = $env:PYTHONPATH
if ($previousPythonPath) {
    $env:PYTHONPATH = "$compatibilityModules;$previousPythonPath"
} else {
    $env:PYTHONPATH = $compatibilityModules
}

# The official converter creates and recursively removes ./tmp. Run it in a
# unique, contained directory so an unrelated project directory cannot be
# removed accidentally.
$workRoot = Join-Path $projectRoot ".kmodel_work"
$workDirectory = Join-Path $workRoot ([Guid]::NewGuid().ToString("N"))
New-Item -ItemType Directory -Force -Path $workDirectory | Out-Null

try {
    Push-Location -LiteralPath $workDirectory
    try {
        & $Python $converter `
            --target k230 `
            --model $onnx `
            --dataset $calibration `
            --input_width 320 `
            --input_height 320 `
            --ptq_option $PtqOption
    } finally {
        Pop-Location
    }
} finally {
    $env:PYTHONPATH = $previousPythonPath
}

if ($LASTEXITCODE -ne 0) {
    throw "KModel conversion failed with exit code $LASTEXITCODE"
}

$generated = [System.IO.Path]::ChangeExtension($onnx, ".kmodel")
if (-not (Test-Path -LiteralPath $generated -PathType Leaf)) {
    throw "Converter completed, but no kmodel was found next to ONNX: $generated"
}

$deployDirectory = Join-Path $projectRoot "k230_deploy"
$deployModel = Join-Path $deployDirectory "steel_ball_yolov8n_320.kmodel"
Copy-Item -LiteralPath $generated -Destination $deployModel -Force
Write-Output "KModel: $deployModel"
