---
related_code:
  - zr_vm_core/include/zr_vm_core/task_runtime.h
  - zr_vm_lib_thread/include/zr_vm_lib_thread/module.h
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime_internal.h
  - zr_vm_lib_system/include/zr_vm_lib_system/module.h
  - zr_vm_lib_system/src/zr_vm_lib_system/module.c
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/src/zr_vm_library/native_binding.c
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/compile_expression.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/task_runtime.h
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime_internal.h
  - zr_vm_lib_system/src/zr_vm_lib_system/module.c
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/src/zr_vm_library/native_binding.c
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/compile_expression.c
plan_sources:
  - user: 2026-04-05 Task / Coroutine / Thread 并发模型重构计划
tests:
  - tests/module/test_module_system.c
  - tests/parser/test_type_inference.c
  - tests/task/test_task_runtime.c
  - tests/thread/test_thread_runtime.c
  - tests/fixtures/projects/native_numeric_pipeline/src/main.zr
  - tests/fixtures/projects/native_math_export_probe/src/main.zr
doc_type: category-index
---

# Library And Builtins

本目录记录内建 native library、宿主暴露 API，以及这些 API 如何被 parser 和运行时识别。

## 当前主题

- `../parser-and-semantics/ffi-extern-declarations.md`
  - source-level `%extern` 声明如何 lower 到 `zr.ffi.loadLibrary(...)` / `getSymbol(...)`
  - extern signature descriptor、layout descriptor 和 callback delegate 的消费规则
- `zr-task-runtime.md`
  - `zr.task` builtin 已切到 `TaskRunner<T>` / `Task<T>` / `IScheduler` / `defaultScheduler`
  - `%async` / `%await` 现在对接 builtin hidden helper，而不是旧 `spawn/await` 公开 helper
- `zr-coroutine-runtime.md`
  - `zr.coroutine` builtin 提供 isolate 级 `coroutineScheduler`
  - 手动 `step/pump` 与 `autoCoroutine` 的当前行为边界
- `zr-thread-runtime.md`
  - `zr.thread` 提供 `Send/Sync` marker contract、worker isolate、thread scheduler，以及 `Transfer/Channel/Shared/WeakShared`
  - 同步容器收敛为 `UniqueMutex/SharedMutex`，guard 是 affine 的 `Lock/SharedLock`
  - 跨 isolate transport 只允许 `Send` payload 与 thread transport handles
- `zr-system-submodules.md`
  - `zr.system` 从扁平模块拆成 6 个叶子模块和 1 个聚合根模块
  - `zr.system.fs` 现在提供 `File` / `Folder` / `FileStream` 对象模型、`SystemFileInfo` 快照 struct，以及兼容函数薄封装
  - `FileStream` 作为 owned `handle_id` wrapper，只在 extern/native 边界自动 lowering 到 `i32`

## 阅读顺序

1. 先看 `zr-task-runtime.md`，了解 `TaskRunner/Task/defaultScheduler` 这条新的 builtin 任务抽象。
2. 再看 `zr-coroutine-runtime.md`，了解 isolate 内建协程调度器和手动 pump 路径。
3. 接着看 `zr-thread-runtime.md`，了解 worker isolate、`Send/Sync` contract、shared control cell 和 mutex/guard 约束。
4. 然后看 `../parser-and-semantics/ffi-extern-declarations.md`，了解 source-level FFI 如何接入 `zr.ffi`。
5. 最后看 `zr-system-submodules.md`，了解本仓库当前的 `zr.system` 结构、叶子 API 和元信息约束。
