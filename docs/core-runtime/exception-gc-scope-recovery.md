---
related_code:
  - tests/core/test_aot_gc_root_frame.c
  - tests/core/test_aot_gc_root_frame_exception.inc
  - zr_vm_core/include/zr_vm_core/exception.h
  - zr_vm_core/src/zr_vm_core/exception.c
  - zr_vm_core/src/zr_vm_core/exception_internal.h
  - zr_vm_core/src/zr_vm_core/exception_try_run.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_internal.h
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/exception_try_run.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_scopes.c
plan_sources:
  - docs/plans/ssa/06-gc-domain/03-domain-sharing.md
  - docs/plans/ssa/06-gc-domain/04-cross-domain-clone.md
tests:
  - tests/core/test_aot_gc_root_frame.c
  - tests/core/test_exception_gc_scopes.c
  - tests/core/test_aot_gc_root_frame_exception.inc
  - tests/core/test_gc_nested_mutation.c
  - tests/acceptance/ssa-exception-gc-scope-recovery.md
doc_type: module
---

# Protected Exception GC Scopes

`ZrCore_Exception_TryRun` captures the registered mutator's execution, native
and locked mutation depths at entry. A local `ZrCore_Exception_Throw` restores
these depths before its C11 non-local jump. This preserves a worker or native
caller's outer scopes when a nested job fails.

## Snapshot And Identity

`SZrGcDomainScopeSnapshot` stores values copied under the domain coordination
lock. It owns no registry pointer or root handle. Domain pointer, domain
identity and mutator ID must still match at restoration, so table growth is
safe and detach/reattach cannot reuse another registration's lock depths.
Callbacks must retain their entry domain registration and caller-owned scopes.
If the registration changes or a callback has released caller-owned mutation
levels, restoration does not invent replacement lock ownership.

The private TryRun context has the public recovery point as its first member.
The automatic context and callback-return flag are volatile across `setjmp`;
the recovery status can change during Throw without making saved fields
indeterminate after `longjmp`. Normal return retains ordinary Begin/End and
Enter/Leave behavior, including the existing FINE result contract.

## Restoration Order

Local Throw first normalizes its error and writes the recovery status. It then
restores the saved AOT root-chain top and depth without traversing abandoned
callback nodes. Under the coordination lock, it releases exactly the recursive
mutation levels added since TryRun entry, restores caller depths and native
mode, and derives the surviving running/inactive/detached/critical status.
The current domain epoch is used rather than a stale parked status or epoch.
Only then is the status broadcast to collectors and other mutators.

Retained caller mutation levels stay locked until their caller ends them.
An empty entry releases every callback-owned level, allowing another mutator
to enter the mutation lock after catch. Nested TryRun restores each distinct
entry snapshot. Catch repeats root-chain restoration without releasing another
thread's locks on the legacy worker forwarding path.

## Files And Bounds

`exception_try_run.c` owns the protected context and local restoration helper;
`gc_domain_scopes.c` owns registry snapshots and depth restoration. The shared
private exception header preserves the existing C/native-C++ unwind selection.
The extraction reduces `exception.c` from 1,053 to 989 lines.

The verified guarantee is for local Throw caught on its owning native thread
in the C11 configuration. Forced-C++ destructor reentry, raw native exceptions,
legacy cross-thread forwarding and abandoned transfer resources are separate
contracts. In particular, this change releases mutation locks but does not
release a clone transaction's temporary root handles or native allocations.

Validation and failure evidence are in
[the acceptance record](../../tests/acceptance/ssa-exception-gc-scope-recovery.md).

## Hand-Written Root-Frame Fixture Boundaries

The core root-frame target textually includes the exception fixture. Its
callbacks use real local TryRun/Throw and host-owned frame nodes; it does not
compile or execute a generated AOT entry ([fixture boundary](../../tests/core/test_aot_gc_root_frame.c#L22)). Throw(FINE) still exits
non-locally. Normal controls explicitly Pop their own nodes; nested controls
record the inner chain before repairing the test scene, so later repair does
not replace the saved inner assertions. GC is gated on those unrepaired
observations to avoid scanning callback-local nodes whose lifetimes ended.

The [following-minor helper](../../tests/core/test_aot_gc_root_frame_exception.inc#L174) only checks the snapshot kind after requesting a step.
TODO: qualify actual work or completed collection counters before treating it
as proof of a fresh minor collection. Object-bearing outer-root cases also
check survivor retention; null-root controls only establish chain structure.

Exception cases destroy state before final Unity assertions. Base cases in
the including C file can assert before destruction; TODO: inspect their
specific Unity failure cleanup ownership at the [empty teardown hook](../../tests/core/test_aot_gc_root_frame.c#L28). The empty hook does
not own a shared state, and this limitation does not describe all cases as leaks.
These fixture bounds add no new execution evidence to the acceptance record.

## 本次 Windows Clang 编译比较边界

既有 `core-resource-public-contract-compile-validation-r3` 的 AOTGC before/after
均自然退出 1，各有 5 个 UCRT `__declspec(noreturn)` 错误；Clang
`stdnoreturn.h` 的宏由 Unity 先引入后污染随后包含的 CRT 声明。
missing.inc 叠加错误在修正 overlay 后已消失，保留错误不是注释回归。
本批只在 [fixture include 契约](../../tests/core/test_aot_gc_root_frame.c#L4) 附近补具体 TODO：沿既有
`zr_vm_aot_gc_root_frame_test` 核查 Windows Clang/Unity 与 CRT 头顺序兼容性。
原 include、宏、指令、表达式及运行行为保持；新 44 单元候选未重新编译。
先前八个 TU 比较运行捕获的是原 43 单元 after，本次新增 TODO 的精确字节
没有 native compile 信用；失败比较也不提供 compile pass 或 runtime/GC 信用。
