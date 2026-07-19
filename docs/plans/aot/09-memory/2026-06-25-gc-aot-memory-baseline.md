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

