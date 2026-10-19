---
related_code:
  - scripts/artifacts/policy.py
  - scripts/artifacts/maintenance.py
  - scripts/artifacts/ExecutePlan.ps1
  - scripts/artifacts/ScheduledMaintenance.ps1
  - tests/tooling/test_artifact_maintenance.py
  - tests/tooling/test_artifact_schedule.py
implementation_files:
  - scripts/artifacts/policy.py
  - scripts/artifacts/maintenance.py
  - scripts/artifacts/ExecutePlan.ps1
  - scripts/artifacts/ScheduledMaintenance.ps1
plan_sources:
  - .codex/plans/20261016-artifact-maintenance-design.md
  - .codex/plans/20261016-artifact-maintenance-implementation.md
  - "user: 2026-10-16 E:/cargo-targets/zr_vm 定期扫描清理；每天北京时间 04:00"
tests:
  - tests/tooling/test_artifact_maintenance.py
  - tests/tooling/test_artifact_schedule.py
doc_type: workflow-detail
---

# 构建产物定期扫描与清理

这个工具维护 `E:/cargo-targets/zr_vm`，读取实际 checkout 的 `.codex/config.toml`，扫描 `build`、`tests`、`reports`、`cache`、`tmp`。每天北京时间 04:00 的 Windows 当前用户计划任务调用同一个一次执行命令。工作结束后进程退出，不需要 Codex 聊天保持打开。

自动清理以任务 owner 明确释放文件为依据。历史目录、未知 owner、源码候选、共享依赖缓存、其他任务报告、Git 元数据继续保留。2026-10-16 人工清理留下的 121 个待核对目录没有因此获得自动删除资格。

## 手工运行与计划任务

在 PowerShell 中预览：

```powershell
& 'C:/Users/HeJiahui/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' -X utf8 -B `
  'E:/Git/zr_vm/scripts/artifacts/maintenance.py' --mode preview `
  --repo 'E:/Git/zr_vm' `
  --skill-root 'D:/CodexData/.codex/skills/artifact-lifecycle' `
  --powershell (Get-Command pwsh).Source
```

`--mode scan` 只盘点，`--mode preview` 对合格文件调用删除器的预览，`--mode apply` 先预览、重新核对证据，再应用同一个文件计划。输出 JSON 指向本次 `summary.json`，包含真实删除数量和字节数；没有合格释放记录时删除 0 文件。

计划任务管理入口：

```powershell
& 'E:/Git/zr_vm/scripts/artifacts/ScheduledMaintenance.ps1' -Action Install
& 'E:/Git/zr_vm/scripts/artifacts/ScheduledMaintenance.ps1' -Action Show
& 'E:/Git/zr_vm/scripts/artifacts/ScheduledMaintenance.ps1' -Action Run
& 'E:/Git/zr_vm/scripts/artifacts/ScheduledMaintenance.ps1' -Action Remove
```

任务名为 `\Codex\zr_vm-artifact-maintenance`。`Run` 立即执行一次维护；`Show` 查询并校验已注册的任务；`Remove` 仅移除身份、命令和配置均匹配的任务，保留报告。`Export -OutputPath <绝对路径>` 将待注册 XML 写到已授权的维护报告目录，拒绝覆盖现有文件。

触发器为每日 `04:00:00+08:00`。使用当前用户 SID、`InteractiveToken` 和 `LeastPrivilege`，因此用户需要登录 Windows；电脑关机或未登录时不会执行，登录并满足系统条件后按 `StartWhenAvailable` 补跑。无需保存密码。动作以隐藏窗口执行，`IgnoreNew` 加上维护状态目录的文件锁阻止重叠；没有运行时限引发的强制终止，也不唤醒或停止其他作业。

安装会核对固定 checkout、Python、PowerShell 和生命周期工具位置，并查询系统实际注册的 XML。匹配的既有任务可以重复安装；同名任务的 owner 标记、动作、用户或必要设置不同就拒绝接管。安装失败会返回失败，不提权、不更改 ACL。运行时 Python/PowerShell/技能路径被移动后，需要先修复安装配置。

## owner 如何明确释放文件

任务清单位于 `reports/<任务ID>/artifacts.json`。任务结束、进程自然退出、没有未知或自有临时项后，owner 才能将状态设为 `closed`，并明确填写 `cleanup_candidates`。仅设为 closed 不会触发删除。

来源清单必须有 `schema_version: 1`、实际 `task_id`、明确 `owner`、本 checkout 的 `source_root`，以及本任务专用的 `output_roots` 子目录。候选必须严格位于该任务声明的 `build`、`tests` 或 `tmp` 子目录内；声明整个全局 build 根只代表权限范围，不代表独占归属。

候选格式如下，指纹字段必须由实际文件计算：

```json
{
  "path": "E:/cargo-targets/zr_vm/build/example-owner/old.obj",
  "kind": "compiled-output",
  "reason": "Owner 已完成此验证，后续构建不再依赖该目标文件",
  "released_at": "2026-10-16T10:00:00+08:00",
  "not_before": "2026-10-17T00:00:00+08:00",
  "bytes": 12345,
  "mtime_utc_ticks": 639277416000000000,
  "sha256": "<实际文件的 64 位小写十六进制 SHA256>"
}
```

`released_at` 和 `not_before` 必须有时区，后者不能早于前者。两个时间都已到才可进入清理计划。`mtime_utc_ticks` 是 .NET UTC ticks（自 0001 年开始，每 tick 100 ns），不是 Unix 时间。下面只读取一个明确路径并打印待核对的候选，不改清单：

```powershell
$file = Get-Item -LiteralPath 'E:/cargo-targets/zr_vm/build/example-owner/old.obj'
$now = [DateTimeOffset]::UtcNow.ToString('o')
[ordered]@{
  path = $file.FullName
  kind = 'compiled-output'
  reason = '填写 owner 实际释放理由'
  released_at = $now
  not_before = $now
  bytes = $file.Length
  mtime_utc_ticks = $file.LastWriteTimeUtc.Ticks
  sha256 = (Get-FileHash -LiteralPath $file.FullName -Algorithm SHA256).Hash.ToLowerInvariant()
} | ConvertTo-Json
```

允许类别为 `compiled-output`（常见对象、库、可执行文件、调试数据库等后缀），`generated-fixture` 和 `temporary-file`（`.tmp/.bin/.dat/.out`）。后缀只用于限制显式释放记录的类别，不会主动按后缀发现垃圾。源码、脚本、配置、日志、文本和未知类别不自动删除。多硬链接文件和 reparse point 也拒绝。

## 保留、依赖与删除保护

所有任务的 `retained`、`unknown` 以及声明的路径依赖参与保护，即使其状态已经 closed；也读取 `artifacts[].dependencies[].path` 等嵌套依赖。依赖对象中的 owner/说明字段不是额外的依赖名称。活动任务的专用 output 根、活动 `.codex/sessions/*.md` 中的路径或 owner 任务 ID、真实进程引用也阻止删除。会话状态只取文件开头 frontmatter 的唯一 status；缺失或含糊时按活动处理，正文中的历史 closed 示例不改变状态。依赖可以使用绝对路径或现有清单的 task ID；活动任务无法解释的依赖需要 owner 核对后再清理。保留的交付物必须继续写在 `retained` 中。

无法读取清单/会话、完整进程信息不足、盘点有错误或权限配置改变时，盘点仍可报告，删除会被跳过。当前用户可能无法读取系统或其他用户进程的命令和可执行路径，这会使有释放候选的清理批次保守跳过。报告只记录进程计数、不可见 PID 和错误；其他进程命令行只在内存中用于引用核对。

候选的大小、mtime、SHA256 必须与释放记录一致。执行前再次读取配置、清单、会话及进程，并复核全部证据；新依赖、配置变化或文件重建使这一轮跳过删除。最后复用已安装 `artifact-lifecycle/scripts/Clean-ArtifactPlan.ps1` 和 `GuardedFileDelete.cs`：先完整预检，再以持有祖先目录和文件句柄的方式校验并删除，拒绝被占用、替换、越界和路径别名。不使用 `cargo clean` 或递归目录删除。

## 状态、报告与磁盘占用

稳定目录为 `E:/cargo-targets/zr_vm/reports/artifact-maintenance`：

- `artifacts.json`：维护任务归属、运行目录、保留条件和上一次实际清理收据。
- `baseline.json`：第一次完整盘点，后续不重复生成。
- `current.sqlite3`：复用的当前路径、大小和 mtime 状态，事务结束后显式关闭连接。
- `maintenance.lock`：进程退出即自动释放的 Windows 字节锁，文件本身保留。
- `runs/<UTC时间-PID>/summary.json`：扫描、未知目录、跳过原因、删除事实和子进程退出结果。
- 同轮的 `delta.json`、`selection.json`、`plan.json`：后续变化、逐候选拒绝原因、明确路径与指纹计划。
- 有合格候选时的 `preview.json`、`cleanup.json`、逐文件 journal 和进程结果；证据改变时保留 `preapply-recheck.json`。

首轮完整基线与单份当前数据库会占用空间；后续保存变化及摘要，避免每天复制完整清单。自己的报告和临时状态不参与差异比较。记录扫描中现有路径的元数据，只有清单、会话、合格释放文件和已声明的校验证据需要读取内容。

每轮先登记归属，再在专用 `tmp/artifact-maintenance/<run-id>` 暂存写入文件。正常结束后该轮临时目录为空并移除；异常遗留会记录实际路径并保留复核。持久报告随计划任务保留，不自动删除其他任务证据。定期维护本身是持续 owner，不将它冒称为已关闭任务。

## 验证

```powershell
& 'C:/Users/HeJiahui/.cache/codex-runtimes/codex-primary-runtime/dependencies/python/python.exe' -X utf8 -B `
  -m unittest discover -s 'E:/Git/zr_vm/tests/tooling' -p 'test_artifact_*.py' -v
```

测试预先登记实际 E 盘夹具，不复制生产源码。测试覆盖显式释放与保留兄弟文件、改动指纹、活动归属/依赖/会话/进程、未知 owner、来源/缓存/Git 保护、配置拒写、junction、被占用文件、预览与执行之间的证据变化、状态复用、运行锁和计划任务 XML。计划任务单元测试使用 mock，不注册任务；真实安装与一次系统触发的结果单独保存在维护报告目录。

2026-10-16 的实际预览记录了 117,745 个可见文件；另有旧报告 `reports/comment-review-20261004-current/children/standard-unchanged-reference-helper-author-r1/fixtures` 下 42 个目录拒绝访问，因此如实返回退出码 1，并禁用该轮删除。当前普通用户的进程可见性也不完整。未改变这些目录的 ACL；未知构建和源码候选继续保留。实际任务 XML、触发结果、临时项核对和空间统计以 `E:/cargo-targets/zr_vm/reports/artifact-maintenance/summary.md` 及其引用的 JSON 为准。
