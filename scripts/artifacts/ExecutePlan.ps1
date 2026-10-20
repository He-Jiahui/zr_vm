[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$SkillRoot,
    [Parameter(Mandatory)][string]$BuildRoot,
    [Parameter(Mandatory)][string]$TestsRoot,
    [Parameter(Mandatory)][string]$TmpRoot,
    [Parameter(Mandatory)][string]$PermissionConfig,
    [Parameter(Mandatory)][string]$PlanPath,
    [Parameter(Mandatory)][string]$ReceiptPath,
    [switch]$Apply
)
$ErrorActionPreference = 'Stop'
$roots = @($BuildRoot, $TestsRoot, $TmpRoot)
$arguments = @{
    PlanPath = $PlanPath
    AllowedRoot = $roots
    ReceiptPath = $ReceiptPath
    PermissionConfig = $PermissionConfig
    Apply = $Apply
}
& (Join-Path $SkillRoot 'scripts/Clean-ArtifactPlan.ps1') @arguments
exit $LASTEXITCODE
