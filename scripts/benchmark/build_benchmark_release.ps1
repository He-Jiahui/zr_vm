<#
.SYNOPSIS
    Configure and build zr_vm in Release under build/benchmark-<toolchain>-release for benchmarks.

.DESCRIPTION
    - gcc / clang: runs scripts/benchmark/build_benchmark_release.sh inside WSL (same paths as /mnt/...).
    - msvc: configures with the newest Visual Studio generator, Release, tests ON, then builds.

.PARAMETER Toolchain
    gcc | clang | msvc

.PARAMETER Jobs
    Parallel build jobs (default: number of processors).

.PARAMETER RepoRoot
    Repository root (default: inferred from script location).

.EXAMPLE
    pwsh ./scripts/benchmark/build_benchmark_release.ps1 -Toolchain msvc
.EXAMPLE
    pwsh ./scripts/benchmark/build_benchmark_release.ps1 -Toolchain gcc
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)]
    [ValidateSet("gcc", "clang", "msvc")]
    [string]$Toolchain,

    [int]$Jobs = 0,

    [string]$RepoRoot = ""
)

Set-StrictMode -Version Latest
$ErrorActionPreference = "Stop"

function Find-RepoRoot {
    param([string]$StartPath)
    $current = Resolve-Path $StartPath
    while ($true) {
        if ((Test-Path (Join-Path $current.Path "CMakeLists.txt")) -and
                (Test-Path (Join-Path $current.Path "tests"))) {
            return $current.Path
        }
        $parent = Split-Path $current.Path -Parent
        if ([string]::IsNullOrWhiteSpace($parent) -or $parent -eq $current.Path) {
            throw "Could not locate repository root from $StartPath"
        }
        $current = Resolve-Path $parent
    }
}

# 将用户指定的 Windows 仓库路径转换为 Linux 构建脚本可用的绝对路径；转换失败立即终止。
function Convert-ToWslPath {
    param([string]$WindowsPath)
    $normalized = $WindowsPath -replace "\\", "/"
    $converted = & wsl "wslpath" "-a" $normalized
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($converted)) {
        throw "Failed to convert Windows path to WSL path: $WindowsPath"
    }
    return $converted.Trim()
}

# 在子进程运行 VsDevCmd 后把其环境变量带回当前 PowerShell 进程，供后续 CMake 使用。
function Import-VsDevCmdEnvironment {
    param(
        [string]$VsDevCmdPath = "",
        [string]$Arch = "x64",
        [string]$HostArch = "x64"
    )

    $candidates = @()
    if (-not [string]::IsNullOrWhiteSpace($VsDevCmdPath)) {
        $candidates += $VsDevCmdPath
    }
    $candidates += @(
        "${env:ProgramFiles(x86)}\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\Common7\Tools\VsDevCmd.bat",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Professional\Common7\Tools\VsDevCmd.bat",
        "${env:ProgramFiles}\Microsoft Visual Studio\2022\Enterprise\Common7\Tools\VsDevCmd.bat"
    )

    $resolved = $null
    foreach ($p in $candidates) {
        if (Test-Path -LiteralPath $p) {
            $resolved = (Resolve-Path -LiteralPath $p).ProviderPath
            break
        }
    }
    if ($null -eq $resolved) {
        throw "VsDevCmd.bat not found. Install Visual Studio Build Tools or set VsDevCmd path."
    }

    $commandLine = 'call "{0}" -arch={1} -host_arch={2} >nul && set' -f `
        ($resolved -replace '"', '""'), $Arch, $HostArch
    $environmentLines = & cmd.exe /d /s /c $commandLine 2>&1
    if ($LASTEXITCODE -ne 0) {
        throw "VsDevCmd failed with exit code $LASTEXITCODE."
    }
    foreach ($line in $environmentLines) {
        $text = $line.ToString()
        $separatorIndex = $text.IndexOf('=')
        if ($separatorIndex -lt 1) { continue }
        $name = $text.Substring(0, $separatorIndex)
        $value = $text.Substring($separatorIndex + 1)
        Set-Item -Path ("Env:{0}" -f $name) -Value $value
    }
}

# 仅从本机 CMake 实际支持的 Visual Studio 生成器中选择版本最高者。
function Get-PreferredVisualStudioGenerator {
    $capabilitiesJson = & cmake -E capabilities
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($capabilitiesJson)) {
        throw "Failed to query CMake generator capabilities (exit $LASTEXITCODE)."
    }
    $capabilities = $capabilitiesJson | ConvertFrom-Json
    $visualStudioGenerators = @(
        $capabilities.generators |
            Where-Object { $_.name -match '^Visual Studio \d+ \d{4}$' } |
            Sort-Object -Property @{
                Expression = {
                    if ($_.name -match '^Visual Studio (\d+) (\d{4})$') {
                        [int]$Matches[1]
                    } else { 0 }
                }
            } -Descending
    )
    if ($visualStudioGenerators.Count -eq 0) {
        throw "No Visual Studio CMake generator reported by 'cmake -E capabilities'."
    }
    return $visualStudioGenerators[0].name
}

$resolvedRoot = if ($RepoRoot) {
    (Resolve-Path $RepoRoot).Path
} else {
    Find-RepoRoot $PSScriptRoot
}

if ($Jobs -le 0) {
    $Jobs = [Environment]::ProcessorCount
    if ($Jobs -lt 1) { $Jobs = 8 }
}

Write-Host "Unit Test - Benchmark Release build ($Toolchain)"
Write-Host "repo_root=$resolvedRoot"

$start = Get-Date

if ($Toolchain -eq "gcc" -or $Toolchain -eq "clang") {
    if (-not (Get-Command wsl -ErrorAction SilentlyContinue)) {
        throw "WSL is required to build gcc/clang from this script on Windows. Use build_benchmark_release.sh inside Linux/WSL."
    }
    $wslRoot = Convert-ToWslPath $resolvedRoot
    $bashScript = "${wslRoot}/scripts/benchmark/build_benchmark_release.sh"
    # BUG: -RepoRoot 含单引号时此处的 shell 引号提前闭合，WSL 无法定位仓库；可由该参数传入此类有效路径。
    # BUG: -Jobs 参数在此分支未转交 Linux 构建脚本，调用者指定的并行度不会生效。
    $inner = "set -euo pipefail; cd '$wslRoot'; bash '$bashScript' '$Toolchain'"
    wsl bash -lc $inner
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
}
else {
    if (-not (Get-Command cmake -ErrorAction SilentlyContinue)) {
        Import-VsDevCmdEnvironment
    }
    $gen = Get-PreferredVisualStudioGenerator
    Write-Host "Using generator: $gen"
    $buildDir = Join-Path $resolvedRoot "build\benchmark-msvc-release"
    # BUG: 全新配置仅启用 BUILD_TESTS；performance_report 的注册开关默认 OFF，后续提示的 ctest 命令找不到该测试。
    & cmake "-S" $resolvedRoot `
        "-B" $buildDir `
        "-G" $gen `
        "-A" "x64" `
        "-DBUILD_TESTS=ON" `
        "-DBUILD_LANGUAGE_SERVER_EXTENSION=OFF"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    & cmake "--build" $buildDir "--config" "Release" "--parallel" "$Jobs"
    if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }

    Write-Host "ZR_VM_CMAKE_BUILD_DIR=$buildDir"
    Write-Host "next (PowerShell): `$env:ZR_VM_CMAKE_BUILD_DIR='$buildDir'; ctest --test-dir '$buildDir' -C Release -R '^performance_report`$'"
}

$elapsed = [int]((Get-Date) - $start).TotalSeconds
Write-Host "Pass - Cost Time:${elapsed}(s) - Benchmark Release build ($Toolchain)"
Write-Host "----"

exit 0
