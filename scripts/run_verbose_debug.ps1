param(
    [ValidateSet("auto", "d3d11", "metal", "vulkan", "opengl", "software")]
    [string]$Backend = "auto"
)

$ErrorActionPreference = "Stop"

if ($PSVersionTable.PSEdition -ne "Core") {
    throw "This script requires PowerShell 7+ (pwsh)."
}

$RepoRoot = Split-Path -Parent $PSScriptRoot
$LogDir = Join-Path $RepoRoot "logs"
$Timestamp = Get-Date -Format "yyyyMMdd_HHmmss"
$StdoutLog = Join-Path $LogDir "verbose_run_${Timestamp}_stdout.log"
$StderrLog = Join-Path $LogDir "verbose_run_${Timestamp}_stderr.log"

New-Item -ItemType Directory -Force -Path $LogDir | Out-Null

$env:QT_LOGGING_RULES = "*.debug=true;qt.*.debug=true;qt.qml.debug=true;qt.qml.*=true;qt.quick.*=true;qt.scenegraph.*=true;qt.rhi.*=true"
$env:QT_FORCE_STDERR_LOGGING = "1"
$env:QSG_INFO = "1"
$env:QSG_RENDERER_DEBUG = "render"
$env:QSG_RHI_PROFILE = "1"
$env:QML_IMPORT_TRACE = "1"

Remove-Item Env:QSG_RHI_BACKEND -ErrorAction SilentlyContinue
switch ($Backend) {
    "d3d11" {
        if (-not $IsWindows) {
            throw "d3d11 backend is only available on Windows."
        }
        $env:QSG_RHI_BACKEND = "d3d11"
    }
    "metal" { $env:QSG_RHI_BACKEND = "metal" }
    "vulkan" { $env:QSG_RHI_BACKEND = "vulkan" }
    "opengl" { $env:QSG_RHI_BACKEND = "opengl" }
    "software" { $env:QSG_RHI_BACKEND = "software" }
    "auto" {
        if ($IsWindows) {
            $env:QSG_RHI_BACKEND = "d3d11"
        }
    }
}

$configureArgs = @(
    "f"
    "-m"
    "debug"
    "-y"
)

$runArgs = @(
    "run"
    "starryagent"
    "--logtype=verbose"
    "--max-old-space-size=1024"
    "--max-render-page-size=420"
)

Write-Host "Configuring: xmake $($configureArgs -join ' ')"
& xmake @configureArgs
if ($LASTEXITCODE -ne 0) {
    throw "xmake configure failed with exit code $LASTEXITCODE"
}

Write-Host "Launching: xmake $($runArgs -join ' ')"
Write-Host "stdout: $StdoutLog"
Write-Host "stderr: $StderrLog"
$process = Start-Process -FilePath "xmake" `
    -ArgumentList $runArgs `
    -PassThru `
    -WorkingDirectory $RepoRoot `
    -RedirectStandardOutput $StdoutLog `
    -RedirectStandardError $StderrLog

try {
    if (-not $process.WaitForExit(60000)) {
        Write-Warning "Process exceeded 60 seconds, forcing termination."
        Stop-Process -Id $process.Id -Force
    }
} finally {
    if (Get-Process -Id $process.Id -ErrorAction SilentlyContinue) {
        Stop-Process -Id $process.Id -Force -ErrorAction SilentlyContinue
    }
}
