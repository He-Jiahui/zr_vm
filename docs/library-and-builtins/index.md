---
related_code:
  - zr_vm_lib_debug/include/zr_vm_lib_debug/module.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/module.c
  - zr_vm_lib_debug/CMakeLists.txt
  - zr_vm_core/include/zr_vm_core/task_runtime.h
  - zr_vm_lib_thread/include/zr_vm_lib_thread/module.h
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_internal.h
  - zr_vm_lib_system/include/zr_vm_lib_system/module.h
  - zr_vm_lib_system/src/zr_vm_lib_system/module.c
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_cached.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.c
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_lib_container/src/zr_vm_lib_container/contiguous_view.c
  - zr_vm_lib_container/src/zr_vm_lib_container/module.c
  - zr_vm_lib_container/src/zr_vm_lib_container/pooling.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_pointer_view.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_contiguous_view.c
implementation_files:
  - zr_vm_lib_debug/include/zr_vm_lib_debug/module.h
  - zr_vm_lib_debug/src/zr_vm_lib_debug/module.c
  - zr_vm_lib_debug/CMakeLists.txt
  - zr_vm_core/include/zr_vm_core/task_runtime.h
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime.c
  - zr_vm_lib_thread/src/zr_vm_lib_thread/runtime/runtime_internal.h
  - zr_vm_lib_system/src/zr_vm_lib_system/module.c
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_cached.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.c
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_lib_container/src/zr_vm_lib_container/contiguous_view.c
  - zr_vm_lib_container/src/zr_vm_lib_container/module.c
  - zr_vm_lib_container/src/zr_vm_lib_container/pooling.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/runtime.c
  - zr_vm_lib_ffi/src/zr_vm_lib_ffi/ffi_runtime/ffi_runtime_pointer_view.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_contiguous_view.c
plan_sources:
  - user: 2026-06-21 按 docs/plans/debug 优化 debug 调试能力
  - docs/plans/debug/04-script-debug-library.md
  - user: 2026-04-05 Task / Coroutine / Thread 并发模型重构计划
  - docs/plans/syntax/2026-07-18-03-struct-ref-struct-span-layout-design.md
tests:
  - tests/library/test_debug_library.c
  - tests/library/test_native_binding_direct_call.c
  - tests/debug/test_debug_traceback.c
  - tests/debug/test_debug_hook_core.c
  - tests/debug/test_debug_introspection.c
  - tests/module/test_module_system.c
  - tests/parser/test_type_inference.c
  - tests/task/test_task_runtime.c
  - tests/thread/test_thread_runtime.c
  - tests/parser/test_span_core.c
  - tests/parser/test_span_semantic_ir_cases.c
  - tests/parser/test_buffer_pool_ffi.c
  - tests/parser/test_aot_c_value_type_shared_library_smoke.c
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
- `zr-debug-module.md`
  - `debug` native module 的受信/沙箱注册入口，以及 `traceback/getinfo/local/upvalue/hook` 首批脚本 API
  - 写能力默认由宿主 opt-in，沙箱描述符拒绝 `setlocal/setupvalue/sethook`
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
- `zr-container-contiguous-views.md`
  - `Span<T>` / `ReadOnlySpan<T>` 的 protocol/role、inline representation 与 readonly weakening
  - array/owner/native-pinned source 的 SemIR loan、bounds proof 和 VM/AOT 等价边界
- `zr-pooling-and-pinned-ffi-views.md`
  - `BufferPool` / `PoolLease<T>` 的 single-return、generation 和异常清理合同
  - `BufferHandle.pin()` / `Ptr<u8>.span()` 的显式 pin、延迟释放和 moving-GC 地址稳定边界
- `../core-runtime/gc-domain-multimutator-and-owner-handoff.md`
  - native descriptor 的 `GcAware` / `BlockingDetached` / `NoSafepointCritical` mode
  - generic、cached、known-native direct与readonly index-contract callback的统一enter/leave合同

## 阅读顺序

1. 先看 `zr-task-runtime.md`，了解 `TaskRunner/Task/defaultScheduler` 这条新的 builtin 任务抽象。
2. 再看 `zr-coroutine-runtime.md`，了解 isolate 内建协程调度器和手动 pump 路径。
3. 接着看 `zr-thread-runtime.md`，了解 worker isolate、`Send/Sync` contract、shared control cell 和 mutex/guard 约束。
4. 连续内存算法和通用借用事实看 `zr-container-contiguous-views.md`。
5. pool lease 与 pinned native provider 的具体生命周期看 `zr-pooling-and-pinned-ffi-views.md`。
6. 然后看 `../parser-and-semantics/ffi-extern-declarations.md`，了解 source-level FFI 如何接入 `zr.ffi`。
7. 再看 `zr-system-submodules.md`，了解本仓库当前的 `zr.system` 结构、叶子 API 和元信息约束。
8. 调试脚本或宿主嵌入 debug 库时，看 `zr-debug-module.md`。
