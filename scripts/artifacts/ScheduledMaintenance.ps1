[CmdletBinding()]
param(
    [ValidateSet('Install', 'Show', 'Remove', 'Export', 'Run')][string]$Action = 'Show',
    [string]$PythonPath = 'C:/Users/HeJiahui/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe',
    [string]$PowerShellPath = (Get-Command pwsh -ErrorAction Stop).Source,
    [string]$OutputPath
)
Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

function Get-ArtifactTaskXml {
    param([string]$PythonPath, [string]$PowerShellPath, [string]$UserSid)
    # PowerShell single quoted literals escape apostrophes by doubling them.
    $python = $PythonPath.Replace("'", "''")
    $shell = $PowerShellPath.Replace("'", "''")
    $command = "& '$python' -X utf8 -B 'E:/Git/zr_vm/scripts/artifacts/maintenance.py' --mode apply --repo 'E:/Git/zr_vm' --skill-root 'D:/CodexData/.codex/skills/artifact-lifecycle' --powershell '$shell'; exit `$LASTEXITCODE"
    $arguments = '-NoLogo -NoProfile -NonInteractive -WindowStyle Hidden -Command "' + $command + '"'
    $exeXml = [Security.SecurityElement]::Escape($PowerShellPath)
    $argsXml = [Security.SecurityElement]::Escape($arguments)
    $sidXml = [Security.SecurityElement]::Escape($UserSid)
    return @"
<Task version="1.4" xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task">
  <RegistrationInfo><Description>Codex zr_vm artifact maintenance v1</Description><URI>\Codex\zr_vm-artifact-maintenance</URI></RegistrationInfo>
  <Triggers><CalendarTrigger><StartBoundary>2026-01-01T04:00:00+08:00</StartBoundary><Enabled>true</Enabled><ScheduleByDay><DaysInterval>1</DaysInterval></ScheduleByDay></CalendarTrigger></Triggers>
  <Principals><Principal id="Author"><UserId>$sidXml</UserId><LogonType>InteractiveToken</LogonType><RunLevel>LeastPrivilege</RunLevel></Principal></Principals>
  <Settings><MultipleInstancesPolicy>IgnoreNew</MultipleInstancesPolicy><DisallowStartIfOnBatteries>false</DisallowStartIfOnBatteries><StopIfGoingOnBatteries>false</StopIfGoingOnBatteries><StartWhenAvailable>true</StartWhenAvailable><AllowHardTerminate>true</AllowHardTerminate><Enabled>true</Enabled><Hidden>true</Hidden><ExecutionTimeLimit>PT0S</ExecutionTimeLimit></Settings>
  <Actions Context="Author"><Exec><Command>$exeXml</Command><Arguments>$argsXml</Arguments><WorkingDirectory>E:/Git/zr_vm</WorkingDirectory></Exec></Actions>
</Task>
"@
}

function Assert-ArtifactTask {
    param([string]$ActualXml, [string]$ExpectedXml)
    [xml]$actual = $ActualXml
    [xml]$expected = $ExpectedXml
    $ns = [Xml.XmlNamespaceManager]::new($actual.NameTable)
    $ns.AddNamespace('t', 'http://schemas.microsoft.com/windows/2004/02/mit/task')
    $en = [Xml.XmlNamespaceManager]::new($expected.NameTable)
    $en.AddNamespace('t', 'http://schemas.microsoft.com/windows/2004/02/mit/task')
    # Scheduler export can reorder fields, assign principal IDs and add defaults.
    # Compare field values and cardinality, rather than serialized XML spelling.
    foreach ($container in @('Actions', 'Principals', 'Triggers')) {
        $nodes = $actual.SelectNodes('/t:Task/t:' + $container, $ns)
        if ($nodes.Count -ne 1 -or $nodes[0].ChildNodes.Count -ne 1) {
            throw "Task ownership/configuration mismatch at $container; refusing to manage this task."
        }
    }
    $required = @('RegistrationInfo/Description', 'RegistrationInfo/URI',
        'Actions/Exec/Command', 'Actions/Exec/Arguments', 'Actions/Exec/WorkingDirectory',
        'Principals/Principal/UserId', 'Principals/Principal/LogonType', 'Principals/Principal/RunLevel',
        'Triggers/CalendarTrigger/Enabled', 'Triggers/CalendarTrigger/ScheduleByDay/DaysInterval')
    # Windows omits these safe defaults from exported task XML. The boolean
    # defaults are specified in Microsoft's task-scheduler-schema; RunLevel=0
    # (LeastPrivilege) was confirmed through the registered task's COM API.
    $omittedDefaults = @{
        'Principals/Principal/RunLevel' = 'LeastPrivilege'
        'Triggers/CalendarTrigger/Enabled' = 'true'
    }
    foreach ($path in $required) {
        $xpath = '/t:Task/t:' + $path.Replace('/', '/t:')
        $a = $actual.SelectNodes($xpath, $ns)
        $e = $expected.SelectSingleNode($xpath, $en)
        if ($a.Count -eq 0 -and $omittedDefaults.ContainsKey($path) -and $e.InnerText -ceq $omittedDefaults[$path]) { continue }
        if ($a.Count -ne 1 -or $a[0].InnerText -cne $e.InnerText) {
            throw "Task ownership/configuration mismatch at $path; refusing to manage this task."
        }
    }
    $principal = $actual.SelectSingleNode('/t:Task/t:Principals/t:Principal', $ns)
    $actions = $actual.SelectSingleNode('/t:Task/t:Actions', $ns)
    if (-not $principal.GetAttribute('id') -or $actions.GetAttribute('Context') -cne $principal.GetAttribute('id')) {
        throw 'Task ownership/configuration mismatch at principal context; refusing to manage this task.'
    }
    $boundary = $actual.SelectNodes('/t:Task/t:Triggers/t:CalendarTrigger/t:StartBoundary', $ns)
    $expectedBoundary = $expected.SelectSingleNode('/t:Task/t:Triggers/t:CalendarTrigger/t:StartBoundary', $en).InnerText
    if ($boundary.Count -ne 1 -or $boundary[0].InnerText -notmatch '(Z|[+-]\d\d:\d\d)$' -or
        [DateTimeOffset]::Parse($boundary[0].InnerText) -ne [DateTimeOffset]::Parse($expectedBoundary)) {
        throw 'Task ownership/configuration mismatch at StartBoundary; refusing to manage this task.'
    }
    # Allow harmless scheduler defaults, while rejecting alternate schedules or delayed/expired triggers.
    foreach ($node in $actual.SelectSingleNode('/t:Task/t:Triggers/t:CalendarTrigger', $ns).ChildNodes) {
        if ($node.LocalName -notin @('StartBoundary', 'Enabled', 'ScheduleByDay', 'RandomDelay') -or
            ($node.LocalName -eq 'RandomDelay' -and $node.InnerText -ne 'PT0S')) {
            throw 'Task ownership/configuration mismatch at trigger options; refusing to manage this task.'
        }
    }
    foreach ($setting in $expected.Task.Settings.ChildNodes) {
        $a = $actual.SelectNodes('/t:Task/t:Settings/t:' + $setting.LocalName, $ns)
        if ($a.Count -eq 0 -and $setting.LocalName -in @('Enabled', 'AllowHardTerminate') -and $setting.InnerText -ceq 'true') { continue }
        if ($a.Count -ne 1 -or $a[0].InnerText -cne $setting.InnerText) {
            throw "Task setting mismatch at $($setting.LocalName); refusing to manage this task."
        }
    }
}

function Assert-ArtifactExportPath {
    param([string]$OutputPath, [string]$PythonPath)
    # Reuse the effective project permission parser and no-reparse checks; argv
    # carries the requested path without interpolating it into executable code.
    $code = @'
import sys
sys.path.insert(0, 'E:/Git/zr_vm/scripts/artifacts')
import policy
p = policy.no_alias(sys.argv[1])
root = policy.no_alias('E:/cargo-targets/zr_vm/reports/artifact-maintenance')
if not policy.within(p, root, equal=False):
    raise ValueError('Output must stay inside the maintenance report namespace')
settings = policy.load_config('E:/Git/zr_vm/.codex/config.toml')
if not settings.can_write(p):
    raise ValueError('Effective project configuration denies output writing')
if p.suffix.lower() != '.xml' or not p.parent.is_dir() or p.exists():
    raise ValueError('Expected a new .xml file in an existing directory')
print(str(p))
'@
    $result = @(& $PythonPath -X utf8 -B -c $code $OutputPath 2>&1)
    if ($LASTEXITCODE -ne 0) { throw "Export path rejected: $($result -join ' ')" }
    return [string]$result[-1]
}

function Test-ArtifactPrerequisites {
    param([string]$PythonPath, [string]$PowerShellPath)
    foreach ($file in @($PythonPath, $PowerShellPath, 'E:/Git/zr_vm/scripts/artifacts/maintenance.py', 'E:/Git/zr_vm/scripts/artifacts/policy.py', 'E:/Git/zr_vm/.codex/config.toml', 'D:/CodexData/.codex/skills/artifact-lifecycle/SKILL.md')) {
        if (-not [IO.File]::Exists($file)) { throw "Required file does not exist: $file" }
    }
    if ([IO.Path]::GetFileName($PythonPath) -ine 'python.exe' -or [IO.Path]::GetFileName($PowerShellPath) -ine 'pwsh.exe') {
        throw 'Expected python.exe and pwsh.exe runtimes.'
    }
}

function Invoke-ArtifactSchedule {
    [CmdletBinding()]
    param(
        [ValidateSet('Install', 'Show', 'Remove', 'Export', 'Run')][string]$Action,
        [string]$PythonPath,
        [string]$PowerShellPath,
        [string]$OutputPath
    )
    Test-ArtifactPrerequisites -PythonPath $PythonPath -PowerShellPath $PowerShellPath
    $sid = [Security.Principal.WindowsIdentity]::GetCurrent().User.Value
    $xml = Get-ArtifactTaskXml -PythonPath $PythonPath -PowerShellPath $PowerShellPath -UserSid $sid
    if ($Action -eq 'Export') {
        $OutputPath = Assert-ArtifactExportPath -OutputPath $OutputPath -PythonPath $PythonPath
        $stream = [IO.File]::Open($OutputPath, [IO.FileMode]::CreateNew, [IO.FileAccess]::Write, [IO.FileShare]::None)
        try {
            $bytes = [Text.UTF8Encoding]::new($false).GetBytes($xml)
            $stream.Write($bytes, 0, $bytes.Length)
        } finally { $stream.Dispose() }
        return [ordered]@{ action = $Action; path = $OutputPath; user_sid = $sid; start_boundary = '2026-01-01T04:00:00+08:00' } | ConvertTo-Json
    }
    if ($Action -eq 'Run') {
        & $PythonPath -X utf8 -B 'E:/Git/zr_vm/scripts/artifacts/maintenance.py' --mode apply --repo 'E:/Git/zr_vm' --skill-root 'D:/CodexData/.codex/skills/artifact-lifecycle' --powershell $PowerShellPath
        if ($LASTEXITCODE -ne 0) { throw "Maintenance exited with code $LASTEXITCODE" }
        return
    }
    $taskName = 'zr_vm-artifact-maintenance'
    $taskPath = '\Codex\'
    $existing = @(Get-ScheduledTask -ErrorAction Stop | Where-Object { $_.TaskName -ieq $taskName -and $_.TaskPath -ieq $taskPath })
    if ($existing.Count -gt 1) { throw 'Ambiguous task identity.' }
    if ($existing.Count -eq 1) {
        $actual = Export-ScheduledTask -TaskName $taskName -TaskPath $taskPath -ErrorAction Stop
        Assert-ArtifactTask -ActualXml $actual -ExpectedXml $xml
    }
    $state = 'absent'
    if ($Action -eq 'Install' -and $existing.Count -eq 0) {
        # Explicit +08 boundary preserves the intended Beijing trigger across host zones.
        Register-ScheduledTask -TaskName $taskName -TaskPath $taskPath -Xml $xml -ErrorAction Stop | Out-Null
        $actual = Export-ScheduledTask -TaskName $taskName -TaskPath $taskPath -ErrorAction Stop
        Assert-ArtifactTask -ActualXml $actual -ExpectedXml $xml
        $state = 'installed'
    } elseif ($Action -eq 'Remove' -and $existing.Count -eq 1) {
        Unregister-ScheduledTask -TaskName $taskName -TaskPath $taskPath -Confirm:$false -ErrorAction Stop
        $state = 'removed'
    } elseif ($existing.Count -eq 1) {
        $state = 'verified'
    }
    [ordered]@{ action = $Action; state = $state; task = $taskPath + $taskName; user_sid = $sid; start_boundary = '2026-01-01T04:00:00+08:00'; xml = $(if ($state -in @('verified', 'installed')) { $actual } else { $null }) } | ConvertTo-Json -Depth 4
}

if ($MyInvocation.InvocationName -ne '.') {
    Invoke-ArtifactSchedule -Action $Action -PythonPath $PythonPath -PowerShellPath $PowerShellPath -OutputPath $OutputPath
}
