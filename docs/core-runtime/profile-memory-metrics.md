---
related_code:
  - zr_vm_core/include/zr_vm_core/profile.h
  - zr_vm_core/src/zr_vm_core/profile.c
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h
  - zr_vm_core/src/zr_vm_core/execution/execution_member_access.c
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c
  - zr_vm_core/src/zr_vm_core/gc/gc_object.c
  - tests/benchmarks/scripts/hotspot_summary.py
implementation_files:
  - zr_vm_core/include/zr_vm_core/profile.h
  - zr_vm_core/src/zr_vm_core/profile.c
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/object/object_super_array_internal.h
  - zr_vm_core/src/zr_vm_core/execution/execution_member_access.c
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c
  - zr_vm_core/src/zr_vm_core/gc/gc_object.c
  - tests/benchmarks/scripts/hotspot_summary.py
plan_sources:
  - user: 2026-08-30 M2 Task1 memory/object/GC profile counters
  - docs/plans/benchmark/optimize/03-memory-object-gc.md
tests:
  - tests/core/test_value_construction_profile.c
doc_type: module-detail
---

# Memory and GC profile metrics

`SZrProfileRuntime` exposes a fixed enum-indexed memory metric array guarded by
`recordMemory`. It tracks managed object allocation count/bytes, value-copy bytes,
write barriers, collection kinds, mark/rewrite work, promotion bytes, raw integer
access, raw/node synchronization, member-cache outcomes, and bytes scanned. The
shared memory metric counters use platform atomic additions (GNU/Clang relaxed;
MSVC Interlocked). These additions do not protect profile runtime lifetime or
serialize ordinary helper, instruction, slow-path, probe, or pause fields. Disabled recording is one unlikely null/flag check and does not
allocate or read a clock.

Pause data uses a fixed 256-entry ring stored in the global-owned
`SZrProfileRuntime`. Callers must serialize these ordinary writes. TODO: verify
sole-writer coordination at concurrent-major remark and compaction call sites;
the ring itself has no lock or thread ownership guard. Total count, total
microseconds, and maximum duration cover the currently instrumented
concurrent-major remark and compaction phases. Minor collections and outer
non-concurrent/full collection pauses are not yet added to this ring, so these fields
must not be interpreted as complete process pause accounting. The bounded sample
ring is emitted in chronological order and can be consumed for p99-compatible
quantiles without unbounded profile memory.

The profile is enabled with `ZR_VM_PROFILE_MEMORY=1`, independently of instruction,
helper, and slow-path recording. Allocation count and bytes cover GC-managed raw
objects created through `ZrCore_RawObject_New` and the region-allocation equivalent;
native allocator traffic and auxiliary payload allocations are outside this metric.
`scan_bytes` adds the raw object's base size once when
`garbage_collector_scan_object` scans it; separately allocated node-map entries and
other payload storage are not included. Raw integer hits and
materialization/synchronization are recorded at the canonical-storage helpers, while
member cache hit/miss/invalidation events are recorded around PIC lookup and clear
paths.

The JSON report emits these values in the `memory` array, omitting zero-valued
entries. `hotspot_summary.py` derives allocation bytes per managed allocation and
scan bytes per marked object from that array; these are event-normalized rates, not
benchmark-operation-normalized rates.

## Profile API contracts

画像接口的声明、公开 inline 和内部回调在源码中按 Summary、Parameters、Returns、
Constraints and lifecycle、References 组织，说明计数边界、借用关系、释放责任和报告副作用。

- `ZrCore_Profile_GlobalInit`：为新 global 建立可选画像存储并复制所需环境文本。
- `ZrCore_Profile_SetCurrentState`：把 state 所属 global 的画像借用到当前线程。
- `ZrCore_Profile_RecordValueCopyCurrent` 与 `ZrCore_Profile_RecordValueCopyFromState`：记录值复制的次数和字节事件。
- `ZrCore_Profile_RecordPauseFromState`：维护暂停累计量和固定容量的最近样本窗口。

函数指针注册和实际派发分别说明；计数表示对应事件边界，不能推断成功执行、
分配所有权转移或全部线程安全。已证明的 JSON 转义问题和并发协调 TODO 保持各自的适用边界。
