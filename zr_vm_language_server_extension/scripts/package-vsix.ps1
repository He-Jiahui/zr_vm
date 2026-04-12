[CmdletBinding()]
param(
    [string]$NativeBuildDir = "",
    [string]$WasmBuildDir = "",
    [string]$NativeConfig = "Debug",
    [int]$Jobs = 8,
    [string]$NpmRegistry = "https://registry.npmjs.org/",
    [switch]$SkipNpmInstall
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$extensionRoot = Split-Path -Parent $scriptDir
$repositoryRoot = Split-Path -Parent $extensionRoot

function Write-Step {
    param([string]$Message)

    Write-Host "==> $Message"
}

function Resolve-PortablePath {
    param(
        [string]$BasePath,
        [string]$PathValue
    )

    if ([string]::IsNullOrWhiteSpace($PathValue)) {
        throw "Path value must not be empty."
    }

    if ([System.IO.Path]::IsPathRooted($PathValue)) {
        return [System.IO.Path]::GetFullPath($PathValue)
    }

    return [System.IO.Path]::GetFullPath((Join-Path $BasePath $PathValue))
}

function Invoke-Checked {
    param(
        [string]$FilePath,
        [string[]]$ArgumentList,
        [string]$WorkingDirectory
    )

    Push-Location $WorkingDirectory
    try {
        Write-Host ">> $FilePath $($ArgumentList -join ' ')"
        & $FilePath @ArgumentList
        if ($LASTEXITCODE -ne 0) {
            throw "Command failed with exit code ${LASTEXITCODE}: $FilePath"
        }
    } finally {
        Pop-Location
    }
}

if ([string]::IsNullOrWhiteSpace($NativeBuildDir)) {
    $NativeBuildDir = Join-Path $repositoryRoot "build\codex-lsp"
} else {
    $NativeBuildDir = Resolve-PortablePath -BasePath $repositoryRoot -PathValue $NativeBuildDir
}

if ([string]::IsNullOrWhiteSpace($WasmBuildDir)) {
    $WasmBuildDir = Join-Path $repositoryRoot "build\codex-lsp-wasm"
} else {
    $WasmBuildDir = Resolve-PortablePath -BasePath $repositoryRoot -PathValue $WasmBuildDir
}

$env:ZR_NATIVE_BUILD_DIR = $NativeBuildDir
$env:ZR_NATIVE_BUILD_CONFIG = $NativeConfig
$env:ZR_WASM_BUILD_DIR = $WasmBuildDir
$env:ZR_BUILD_JOBS = $Jobs.ToString()
$env:ZR_WASM_BUILD_JOBS = $Jobs.ToString()
$env:npm_config_registry = $NpmRegistry

if (-not $SkipNpmInstall -and -not (Test-Path -LiteralPath (Join-Path $extensionRoot "node_modules"))) {
    Write-Step "Installing extension dependencies"
    Invoke-Checked -FilePath "npm" -ArgumentList @("install", "--package-lock=false") -WorkingDirectory $extensionRoot
}

Write-Step "Packaging VSIX with bundled native + wasm assets"
Invoke-Checked -FilePath "npm" -ArgumentList @("run", "package") -WorkingDirectory $extensionRoot

$vsixFile = Get-ChildItem -LiteralPath $extensionRoot -Filter "*.vsix" |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1

if ($vsixFile -eq $null) {
    throw "Packaging completed but no .vsix file was found in $extensionRoot"
}

Write-Step "VSIX created: $($vsixFile.FullName)"
