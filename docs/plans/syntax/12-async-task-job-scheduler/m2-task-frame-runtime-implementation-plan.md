# Syntax 12 M2: Task/Frame Runtime

## 目标

实现计划 12 的 Task/frame runtime：同步完成保持在调用路径，不租用协程帧；
仅在真实 pending await 时提升为可复用的 frame。frame 的状态、存活 slot、GC
根和 Drop 清理由 layout 描述，不能由 `zr.task` 动态对象字段或调用名称推断。

## Exact Write Set

| 层 | 路径 | 责任 |
|---|---|---|
| core runtime | `zr_vm_core/include/zr_vm_core/task_frame_runtime.h`、`zr_vm_core/src/zr_vm_core/task_frame_runtime.c` | Task completion、pending-frame promotion、state transition、GC root/drop map、pool reuse 与 non-Copy result transfer。 |
| tests | `tests/task/test_task_frame_runtime.c`、`tests/CMakeLists.txt` | sync/suspend/resume/fault/multi-await/non-Copy/GC/drop/pool focused runtime evidence。 |
| docs | `docs/core-runtime/task-frame-runtime.md`、`docs/core-runtime/index.md`、本计划、M2 record、acceptance record | runtime contract、进度与验证。 |

## 实施步骤

1. **RED: runtime state contract**
   - 用独立 core Unity target 固定 sync completion 无 frame allocation；pending
     task 才可进入 `SUSPENDED` 并记录 stable state id。
   - 固定 single/multiple suspension、multiple await of Copy results、fault 的
     initialized-only drop map、finally-before-cleanup、GC root survival、pool
     reuse 与 non-Copy result exactly-once transfer。

2. **Green: structured frame layout**
   - 引入 layout-owned slot descriptors。每个跨 suspension slot 明确其 GC-root
     和 cleanup role；frame 不把这些事实序列化为 dynamic object fields。
   - suspend 才从 pool 取得 frame。complete/fault/early free 都先执行 layout
     的一次性 finally，再释放 initialized slots、根 handles 并归还 frame；sync
     completion 不触发 frame allocation。

3. **Regression and acceptance**
   - 先运行新的 core runtime target，再运行 M1 task/CFG/type targets；随后以
     独立 GCC、Clang、MSVC 目录重放。
   - 更新 module docs、acceptance 与状态记录，并以 isolated Git index
     exact-stage 本表路径和 parent-plan M2 link，提交单一 M2 commit。

## 不在本里程碑内

- Job/Scheduler、ThreadScheduler、跨 domain transport 与 Send/Sync 属于 M3-M5。
- artifact/AOT frame serialization、debug logical stack、LSP 和删除
  `TaskRunner`/`autoCoroutine` 属于 M6。
- 本里程碑建立可由 M3 lowering 消费的 frame runtime，不保留第二个 public
  scheduler protocol。
