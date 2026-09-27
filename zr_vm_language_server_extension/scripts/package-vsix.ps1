# package.json 的 package:vsix 和手工发布入口使用此包装器，把同一组构建参数传给
# vsce 的 vscode:prepublish 链：compile -> native/WASM build -> sync -> VSIX。
# 调用方需提供可用的 npm、原生工具链及 WASM 工具链；自定义构建目录须专用于本项目。
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

# 包装器的阶段提示；用于区分依赖安装、预发布构建和最终归档。
function Write-Step {
    param([string]$Message)

    Write-Host "==> $Message"
}

# 命令行覆盖目录可以是仓库根相对路径或绝对路径，统一交给 Node 构建脚本使用。
# 不验证目录归属；下游构建脚本可能递归清理该目录，调用方须先确认其可丢弃。
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

# 每个外部命令固定在扩展根执行，以便 npm 读取正确的 package.json；非零退出码终止发布链。
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

# 发布包装器选择构建目录；默认值与 artifact-layout 的约定相同。
# 覆盖值从仓库根解析，再通过环境变量传给预发布的 Node 脚本。
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

# vsce 会在 npm run package 内另起 vscode:prepublish 子进程；环境变量在这条子进程链中传递。
$env:ZR_NATIVE_BUILD_DIR = $NativeBuildDir
$env:ZR_NATIVE_BUILD_CONFIG = $NativeConfig
$env:ZR_WASM_BUILD_DIR = $WasmBuildDir
$env:ZR_BUILD_JOBS = $Jobs.ToString()
$env:ZR_WASM_BUILD_JOBS = $Jobs.ToString()
$env:npm_config_registry = $NpmRegistry

# 已安装依赖由调用方维护；仅在 node_modules 缺失且未显式跳过时安装。
# TODO: 当前只按目录存在判断，不核查与 package-lock.json 是否一致；确认发布环境
# 是否要求可重复的依赖版本及损坏 node_modules 的恢复入口。
if (-not $SkipNpmInstall -and -not (Test-Path -LiteralPath (Join-Path $extensionRoot "node_modules"))) {
    Write-Step "Installing extension dependencies"
    Invoke-Checked -FilePath "npm" -ArgumentList @("install", "--package-lock=false") -WorkingDirectory $extensionRoot
}

# npm run package 调用 vsce，vsce 再执行 vscode:prepublish；构建和同步成功后才会生成归档。
Write-Step "Packaging VSIX with bundled native + wasm assets"
Invoke-Checked -FilePath "npm" -ArgumentList @("run", "package") -WorkingDirectory $extensionRoot

# BUG: 若扩展根已有 LastWriteTime 晚于本次产物的旧 VSIX（例如未来时间戳），
# 此选择会报告旧文件；发布链只检查有无 *.vsix，未绑定 vsce 刚生成的输出。
$vsixFile = Get-ChildItem -LiteralPath $extensionRoot -Filter "*.vsix" |
    Sort-Object LastWriteTime -Descending |
    Select-Object -First 1

if ($vsixFile -eq $null) {
    throw "Packaging completed but no .vsix file was found in $extensionRoot"
}

Write-Step "VSIX created: $($vsixFile.FullName)"
