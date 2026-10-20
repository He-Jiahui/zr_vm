"""Scheduler contract tests; never register or execute a Windows task."""
import json
from pathlib import Path
import shutil
import subprocess
import unittest
import xml.etree.ElementTree as ET

REPO = Path(__file__).resolve().parents[2]
SCRIPT = REPO / 'scripts/artifacts/ScheduledMaintenance.ps1'
PWSH = shutil.which('pwsh')
NS = {'t': 'http://schemas.microsoft.com/windows/2004/02/mit/task'}

@unittest.skipUnless(PWSH, 'PowerShell is required')
class ScheduleTests(unittest.TestCase):
    def ps(self, body):
        result = subprocess.run([PWSH, '-NoLogo', '-NoProfile', '-NonInteractive', '-Command',
            "$ErrorActionPreference='Stop'; . '" + str(SCRIPT).replace("'", "''") + "'; " + body],
            text=True, capture_output=True, encoding='utf-8')
        self.assertEqual(result.returncode, 0, result.stderr)
        return result.stdout.strip()

    def xml(self):
        return ET.fromstring(self.ps("Get-ArtifactTaskXml -PythonPath 'C:\\Runtime Space\\python.exe' -PowerShellPath 'C:\\Runtime Space\\pwsh.exe' -UserSid 'S-1-5-21-123'"))

    def test_daily_beijing_trigger_and_safe_principal(self):
        root = self.xml()
        self.assertEqual(root.find('t:Triggers/t:CalendarTrigger/t:StartBoundary', NS).text, '2026-01-01T04:00:00+08:00')
        self.assertEqual(root.find('t:Triggers/t:CalendarTrigger/t:ScheduleByDay/t:DaysInterval', NS).text, '1')
        self.assertEqual(root.find('t:Principals/t:Principal/t:UserId', NS).text, 'S-1-5-21-123')
        self.assertEqual(root.find('t:Principals/t:Principal/t:LogonType', NS).text, 'InteractiveToken')
        self.assertEqual(root.find('t:Principals/t:Principal/t:RunLevel', NS).text, 'LeastPrivilege')
        self.assertEqual(root.find('t:Settings/t:MultipleInstancesPolicy', NS).text, 'IgnoreNew')
        self.assertEqual(root.find('t:Settings/t:StartWhenAvailable', NS).text, 'true')

    def test_hidden_action_quotes_fixed_paths(self):
        root = self.xml()
        self.assertEqual(root.find('t:Actions/t:Exec/t:Command', NS).text, r'C:\Runtime Space\pwsh.exe')
        args = root.find('t:Actions/t:Exec/t:Arguments', NS).text
        self.assertIn('-WindowStyle Hidden', args)
        self.assertIn("& 'C:\\Runtime Space\\python.exe' -X utf8 -B", args)
        self.assertIn("'E:/Git/zr_vm/scripts/artifacts/maintenance.py' --mode apply --repo 'E:/Git/zr_vm'", args)
        self.assertIn("--skill-root 'D:/CodexData/.codex/skills/artifact-lifecycle'", args)
        self.assertIn("--powershell 'C:\\Runtime Space\\pwsh.exe'", args)
        self.assertIn('exit $LASTEXITCODE', args)

    def test_matching_task_accepted_and_collision_rejected(self):
        output = self.ps("$x=Get-ArtifactTaskXml -PythonPath 'C:/python.exe' -PowerShellPath 'C:/pwsh.exe' -UserSid 'S-1-5-21-123'; Assert-ArtifactTask -ActualXml $x -ExpectedXml $x; $bad=$x.Replace('Codex zr_vm artifact maintenance v1','Someone else'); try { Assert-ArtifactTask -ActualXml $bad -ExpectedXml $x; throw 'accepted collision' } catch { if ($_.Exception.Message -eq 'accepted collision') { throw }; 'rejected' }")
        self.assertEqual(output, 'rejected')

    def test_action_or_principal_changes_rejected(self):
        for old, new in [('C:/python.exe', 'C:/evil.exe'), ('S-1-5-21-123', 'S-1-5-21-999'), ('LeastPrivilege', 'HighestAvailable')]:
            with self.subTest(new=new):
                output = self.ps("$x=Get-ArtifactTaskXml -PythonPath 'C:/python.exe' -PowerShellPath 'C:/pwsh.exe' -UserSid 'S-1-5-21-123'; $bad=$x.Replace('"+old+"','"+new+"'); try { Assert-ArtifactTask -ActualXml $bad -ExpectedXml $x; throw 'accepted collision' } catch { if ($_.Exception.Message -eq 'accepted collision') { throw }; 'rejected' }")
                self.assertEqual(output, 'rejected')

    def test_all_task_management_actions_refuse_collision_before_mutation(self):
        for action in ['Install', 'Show', 'Remove']:
            with self.subTest(action=action):
                output = self.ps("""
function Test-ArtifactPrerequisites { param($PythonPath, $PowerShellPath) }
function Get-ScheduledTask { [CmdletBinding()]param(); [pscustomobject]@{TaskName='zr_vm-artifact-maintenance';TaskPath='\\Codex\\'} }
function Export-ScheduledTask { [CmdletBinding()]param($TaskName,$TaskPath); '<Task xmlns="http://schemas.microsoft.com/windows/2004/02/mit/task"><RegistrationInfo><Description>foreign task</Description></RegistrationInfo></Task>' }
function Register-ScheduledTask { throw 'UNSAFE registration' }
function Unregister-ScheduledTask { throw 'UNSAFE removal' }
try {
  Invoke-ArtifactSchedule -Action '""" + action + """' -PythonPath 'C:/python.exe' -PowerShellPath 'C:/pwsh.exe'
  throw 'accepted collision'
} catch {
  if ($_.Exception.Message -notlike 'Task ownership/configuration mismatch*') { throw }
  'rejected'
}
""")
                self.assertEqual(output, 'rejected')

    def test_matching_install_is_idempotent_without_registration(self):
        output = self.ps("""
function Test-ArtifactPrerequisites { param($PythonPath, $PowerShellPath) }
function Get-ScheduledTask { [CmdletBinding()]param(); [pscustomobject]@{TaskName='zr_vm-artifact-maintenance';TaskPath='\\Codex\\'} }
function Export-ScheduledTask { [CmdletBinding()]param($TaskName,$TaskPath); Get-ArtifactTaskXml -PythonPath 'C:/python.exe' -PowerShellPath 'C:/pwsh.exe' -UserSid ([Security.Principal.WindowsIdentity]::GetCurrent().User.Value) }
function Register-ScheduledTask { throw 'UNSAFE registration' }
function Unregister-ScheduledTask { throw 'UNSAFE removal' }
Invoke-ArtifactSchedule -Action Install -PythonPath 'C:/python.exe' -PowerShellPath 'C:/pwsh.exe'
""")
        self.assertEqual(json.loads(output)['state'], 'verified')
        task_path = json.loads(output)['task'][:-len('zr_vm-artifact-maintenance')]
        self.assertEqual(task_path, '\\Codex\\')
        self.assertEqual(len(task_path), 7)
        self.assertEqual([ord(ch) for ch in task_path], [92, 67, 111, 100, 101, 120, 92])

    def test_semantic_comparison_accepts_scheduler_normalization(self):
        output = self.ps("""
$x=Get-ArtifactTaskXml -PythonPath 'C:/python.exe' -PowerShellPath 'C:/pwsh.exe' -UserSid 'S-1-5-21-123'
[xml]$a=$x
$n=[Xml.XmlNamespaceManager]::new($a.NameTable); $n.AddNamespace('t',$a.DocumentElement.NamespaceURI)
$p=$a.SelectSingleNode('//t:Principal',$n)
$p.SetAttribute('id','SchedulerAssignedId')
$a.Task.Actions.SetAttribute('Context','SchedulerAssignedId')
$user=$p.SelectSingleNode('t:UserId',$n); $p.RemoveChild($user) | Out-Null; $p.AppendChild($user) | Out-Null
$trigger=$a.SelectSingleNode('//t:CalendarTrigger',$n)
$extra=$a.CreateElement('RandomDelay',$a.DocumentElement.NamespaceURI); $extra.InnerText='PT0S'; $trigger.AppendChild($extra) | Out-Null
Assert-ArtifactTask -ActualXml $a.OuterXml -ExpectedXml $x
'accepted'
""")
        self.assertEqual(output, 'accepted')

    def test_omitted_safe_defaults_match_actual_windows_export(self):
        output = self.ps("""
$x=Get-ArtifactTaskXml -PythonPath 'C:/python.exe' -PowerShellPath 'C:/pwsh.exe' -UserSid 'S-1-5-21-123'
$a=$x.Replace('<RunLevel>LeastPrivilege</RunLevel>','').Replace('<Enabled>true</Enabled>','').Replace('<AllowHardTerminate>true</AllowHardTerminate>','')
Assert-ArtifactTask -ActualXml $a -ExpectedXml $x
'accepted'
""")
        self.assertEqual(output, 'accepted')

    def test_safe_default_normalization_rejects_unsafe_or_duplicate_values(self):
        output = self.ps("""
$x=Get-ArtifactTaskXml -PythonPath 'C:/python.exe' -PowerShellPath 'C:/pwsh.exe' -UserSid 'S-1-5-21-123'
foreach ($pair in @(
  @('<RunLevel>LeastPrivilege</RunLevel>','<RunLevel>HighestAvailable</RunLevel>'),
  @('<RunLevel>LeastPrivilege</RunLevel>','<RunLevel>LeastPrivilege</RunLevel><RunLevel>LeastPrivilege</RunLevel>'),
  @('<Enabled>true</Enabled>','<Enabled>false</Enabled>'),
  @('<Enabled>true</Enabled>','<Enabled>true</Enabled><Enabled>true</Enabled>'),
  @('<AllowHardTerminate>true</AllowHardTerminate>','<AllowHardTerminate>false</AllowHardTerminate>'),
  @('<AllowHardTerminate>true</AllowHardTerminate>','<AllowHardTerminate>true</AllowHardTerminate><AllowHardTerminate>true</AllowHardTerminate>'),
  @('<UserId>S-1-5-21-123</UserId>',''),
  @('<LogonType>InteractiveToken</LogonType>',''),
  @('<StartWhenAvailable>true</StartWhenAvailable>',''),
  @('<RunLevel>LeastPrivilege</RunLevel>','<RunLevel></RunLevel>')
)) {
  try { Assert-ArtifactTask -ActualXml $x.Replace($pair[0],$pair[1]) -ExpectedXml $x; throw 'accepted unsafe default' }
  catch { if ($_.Exception.Message -notmatch '^Task (ownership/configuration|setting) mismatch') { throw } }
}
'rejected'
""")
        self.assertEqual(output, 'rejected')

    def test_semantic_comparison_rejects_duplicate_actions_principals_triggers(self):
        output = self.ps("""
$x=Get-ArtifactTaskXml -PythonPath 'C:/python.exe' -PowerShellPath 'C:/pwsh.exe' -UserSid 'S-1-5-21-123'
foreach ($container in @('Actions','Principals','Triggers')) {
  [xml]$a=$x; $node=$a.Task.$container; $node.AppendChild($node.FirstChild.CloneNode($true)) | Out-Null
  try { Assert-ArtifactTask -ActualXml $a.OuterXml -ExpectedXml $x; throw 'accepted duplicate' }
  catch { if ($_.Exception.Message -notlike 'Task ownership/configuration mismatch*') { throw } }
}
'rejected'
""")
        self.assertEqual(output, 'rejected')

    def test_export_path_is_confined_before_any_write(self):
        output = self.ps("""
$python='C:/Users/HeJiahui/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe'
foreach ($path in @('C:/outside.xml','E:/Git/zr_vm/scheduler.xml','E:/cargo-targets/zr_vm/reports/artifact-maintenance-other/x.xml','E:/cargo-targets/zr_vm/reports/artifact-maintenance/../outside.xml')) {
  try { Assert-ArtifactExportPath -OutputPath $path -PythonPath $python; throw 'accepted unsafe path' }
  catch { if ($_.Exception.Message -notlike 'Export path rejected*') { throw } }
}
$good=Assert-ArtifactExportPath -OutputPath 'E:/cargo-targets/zr_vm/reports/artifact-maintenance/uncreated-test.xml' -PythonPath $python
if ($good -ine 'E:\\cargo-targets\\zr_vm\\reports\\artifact-maintenance\\uncreated-test.xml') { throw 'bad canonical path' }
'confined'
""")
        self.assertEqual(output, 'confined')

    def test_registration_is_read_back_and_verified(self):
        output = self.ps("""
function Test-ArtifactPrerequisites { param($PythonPath, $PowerShellPath) }
function Get-ScheduledTask { [CmdletBinding()]param() }
function Register-ScheduledTask { [CmdletBinding()]param($TaskName,$TaskPath,$Xml); $script:registered=$Xml }
function Export-ScheduledTask { [CmdletBinding()]param($TaskName,$TaskPath); $script:registered.Replace('IgnoreNew','Parallel') }
try {
  Invoke-ArtifactSchedule -Action Install -PythonPath 'C:/python.exe' -PowerShellPath 'C:/pwsh.exe'
  throw 'accepted wrong settings'
} catch {
  if ($_.Exception.Message -notlike 'Task setting mismatch*') { throw }
  'rejected'
}
""")
        self.assertEqual(output, 'rejected')

if __name__ == '__main__':
    unittest.main()
