---
plan_id: aot-09-memory
record_id: 2026-06-25-gc-aot-memory-baseline
status: completed
completed_at: 2026-06-25 23:59 +08:00
source_plans:
  - docs/plans/aot/09-memory-management.md
evidence_scope: historical-baseline
---

# GC And AOT Memory Baseline

## 可复用结论

- AOT ABI 已有 type GC descriptor、method root map 与 active root-frame registration。
- GC 能 mark/rewrite frame byte offsets，并已有 allocation/call/back-edge safepoint insertion 基线。
- heap object stores 已汇入公共 write-barrier 边界，栈内 inline value store 不误加 heap barrier。
- boxing/unboxing bridge 和 native-call pin/unpin 已有公共入口及 FFI focused tests。

## 证据入口

- `tests/core/test_aot_gc_root_frame.c`
- `tests/gc/gc_tests.c`
- `tests/ffi/test_ffi_native_call_pin_contract.c`
- `tests/parser/test_aot_c_frame_setup_contracts.c`

## 不继承的完成声明

这些记录不证明 `Unique/Shared/Weak`、resource class Drop、`Gc<T>` bridge、PoolRef guard、slab NoScan 或 ref-like stack-map 已完成。只有 Canonical TypeLayout 的 GC pointer map 可以授权 NoScan。

## 2026-07-27 重新验收补充

- 已加载 AOT 动态库的 `codeRegistration`、类型布局和函数入口会被仍在 GC 堆中的对象引用，因此项目释放不能立即卸载动态库。
- `ZrLibrary_AotRuntime_FreeProjectState()` 先解除函数表的 GC pin，并把动态库句柄登记为延迟清理；`ZrCore_GlobalState_Free()` 在 `ZrCore_GarbageCollector_Free()` 完成后才执行该清理并卸载动态库。
- post-GC 清理槽采用单一所有者。若已有其他回调占用，AOT 句柄登记会拒绝复用该状态；调用方保留句柄，以可控泄漏避免 use-after-`dlclose` / `FreeLibrary`。
- 回归证据：`tests/parser/test_aot_c_value_type_shared_library_smoke.c` 覆盖加载、执行、项目释放和 GC/全局状态销毁的完整顺序。
