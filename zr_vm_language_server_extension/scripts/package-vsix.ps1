[CmdletBinding()]
param(
    [string]$NativeBuildDir = "",
    [string]$WasmBuildDir = "",
    [string]$NativeConfig = "Debug",
    [string]$NativeGenerator = "Visual Studio 18 2026",
    [string]$NativeArch = "x64",
    [string]$NativeToolset = "v145",
    [int]$Jobs = 8,
    [string]$NpmRegistry = "https://registry.npmjs.org/",
    [switch]$ForceConfigure,
    [switch]$SkipNpmInstall
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

$scriptDir = Split-Path -Parent $MyInvocation.MyCommand.Path
$extensionRoot = Split-Path -Parent $scriptDir
$repositoryRoot = Split-Path -Parent $extensionRoot
$runningOnWindows = $env:OS -eq "Windows_NT"

function Write-Step {
    param([string]$Message)

    Write-Host "==> $Message"
}

function Resolve-AbsolutePath {
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

function Convert-ToWslPath {
    param([string]$NativePath)

    $resolved = [System.IO.Path]::GetFullPath($NativePath).Replace("\", "/")
    $match = [System.Text.RegularExpressions.Regex]::Match($resolved, "^([A-Za-z]):/(.*)$")
    if (-not $match.Success) {
        return $resolved
    }

    return "/mnt/$($match.Groups[1].Value.ToLowerInvariant())/$($match.Groups[2].Value)"
}

function Get-BashLiteral {
    param([string]$Value)

    return "'" + ($Value -replace "'", "'`"`'`"`'") + "'"
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

function Find-VsDevCmd {
    $candidates = @()

    if ($env:VSDEVCMD_PATH) {
        $candidates += $env:VSDEVCMD_PATH
    }

    $candidates += @(
        "D:\Tools\VS\Common7\Tools\VsDevCmd.bat",
        "C:\Program Files\Microsoft Visual Studio\2026\Community\Common7\Tools\VsDevCmd.bat",
        "C:\Program Files\Microsoft Visual Studio\2026\Professional\Common7\Tools\VsDevCmd.bat",
        "C:\Program Files\Microsoft Visual Studio\2026\Enterprise\Common7\Tools\VsDevCmd.bat",
        "C:\Program Files\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat",
        "C:\Program Files\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat",
        "C:\Program Files\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat"
    )

    foreach ($candidate in $candidates) {
        if ($candidate -and (Test-Path -LiteralPath $candidate)) {
            return (Resolve-Path -LiteralPath $candidate).Path
        }
    }

    $vswhere = Join-Path ${env:ProgramFiles(x86)} "Microsoft Visual Studio\Installer\vswhere.exe"
    if (Test-Path -LiteralPath $vswhere) {
        $resolved = & $vswhere `
            -latest `
            -products * `
            -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 `
            -find Common7\Tools\VsDevCmd.bat
        if ($LASTEXITCODE -eq 0 -and $resolved) {
            return $resolved | Select-Object -First 1
        }
    }

    throw "Could not locate VsDevCmd.bat. Set VSDEVCMD_PATH or install Visual Studio Build Tools."
}

function Import-VsDevCmdEnvironment {
    param(
        [string]$VsDevCmdPath,
        [string]$Arch
    )

    Write-Step "Importing Visual Studio environment"
    $output = & cmd.exe /d /c "call `"$VsDevCmdPath`" -no_logo -arch=$Arch -host_arch=$Arch >nul && set"
    if ($LASTEXITCODE -ne 0) {
        throw "Failed to import Visual Studio environment from $VsDevCmdPath"
    }

    foreach ($line in $output) {
        if ($line -match "^(.*?)=(.*)$") {
            [System.Environment]::SetEnvironmentVariable($matches[1], $matches[2], "Process")
        }
    }
}

function Ensure-NativeConfigure {
    param([string]$BuildDir)

    if ((-not $ForceConfigure) -and (Test-Path -LiteralPath (Join-Path $BuildDir "CMakeCache.txt"))) {
        return
    }

    Write-Step "Configuring native build"
    $args = @(
        "-S", $repositoryRoot,
        "-B", $BuildDir,
        "-G", $NativeGenerator,
        "-A", $NativeArch
    )

    if (-not [string]::IsNullOrWhiteSpace($NativeToolset)) {
        $args += @("-T", $NativeToolset)
    }

    $args += @(
        "-DBUILD_TESTS=OFF",
        "-DBUILD_LANGUAGE_SERVER_EXTENSION=OFF"
    )

    Invoke-Checked -FilePath "cmake" -ArgumentList $args -WorkingDirectory $repositoryRoot
}

function Ensure-WasmConfigure {
    param([string]$BuildDir)

    if ((-not $ForceConfigure) -and (Test-Path -LiteralPath (Join-Path $BuildDir "CMakeCache.txt"))) {
        return
    }

    Write-Step "Configuring wasm build"
    $repoRootWsl = Convert-ToWslPath $repositoryRoot
    $buildDirWsl = Convert-ToWslPath $BuildDir
    $bashCommand = @(
        "cd $(Get-BashLiteral $repoRootWsl)",
        "emcmake cmake -S . -B $(Get-BashLiteral $buildDirWsl) -G Ninja -DCMAKE_BUILD_TYPE=Release -DBUILD_TESTS=OFF -DBUILD_WASM=ON -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF"
    ) -join " && "

    Invoke-Checked -FilePath "wsl" -ArgumentList @("bash", "-lc", $bashCommand) -WorkingDirectory $repositoryRoot
}

if ([string]::IsNullOrWhiteSpace($NativeBuildDir)) {
    $NativeBuildDir = Join-Path $repositoryRoot "build\codex-lsp"
} else {
    $NativeBuildDir = Resolve-AbsolutePath -BasePath $repositoryRoot -PathValue $NativeBuildDir
}

if ([string]::IsNullOrWhiteSpace($WasmBuildDir)) {
    $WasmBuildDir = Join-Path $repositoryRoot "build\codex-lsp-wasm"
} else {
    $WasmBuildDir = Resolve-AbsolutePath -BasePath $repositoryRoot -PathValue $WasmBuildDir
}

if ($runningOnWindows) {
    $vsDevCmd = Find-VsDevCmd
    Import-VsDevCmdEnvironment -VsDevCmdPath $vsDevCmd -Arch $NativeArch
}

Ensure-NativeConfigure -BuildDir $NativeBuildDir
Ensure-WasmConfigure -BuildDir $WasmBuildDir

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

Write-Step "Packaging VSIX"
Invoke-Checked -FilePath "npm" -ArgumentList @("run", "package") -WorkingDirectory $extensionRoot

$vsixFile = Get-ChildItem -LiteralPath $extensionRoot -Filter "*.vsix" |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1

if ($vsixFile -eq $null) {
    throw "Packaging completed but no .vsix file was found in $extensionRoot"
}

Write-Step "VSIX created: $($vsixFile.FullName)"
