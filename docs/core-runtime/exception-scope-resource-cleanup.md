---
related_code:
  - zr_vm_core/src/zr_vm_core/exception_try_run.c
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/ownership.c
  - zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_cleanup_registration.c
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  - tests/core/test_close_proxy_instruction.c
  - tests/core/test_close_proxy.c
  - tests/core/test_close_proxy_instruction.c
  - tests/core/test_close_meta_exception.c
  - tests/cmake/close-proxy-tests.cmake
  - zr_vm_core/include/zr_vm_core/closure.h
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_core/include/zr_vm_core/ownership.h
  - zr_vm_core/src/zr_vm_core/closure.c
  - zr_vm_core/src/zr_vm_core/closure_close_meta_guard.c
  - zr_vm_core/src/zr_vm_core/closure_close_meta_guard.h
  - zr_vm_core/src/zr_vm_core/closure_close_proxy_token.c
  - zr_vm_core/src/zr_vm_core/closure_close_proxy_token.h
  - zr_vm_core/src/zr_vm_core/execution/execution_control.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_budget.c
  - zr_vm_core/src/zr_vm_core/exception.c
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/ownership_shared.c
  - zr_vm_library/include/zr_vm_library/aot_runtime.h
  - zr_vm_library/src/zr_vm_library/aot_runtime.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/exception_try_run.c
  - zr_vm_core/src/zr_vm_core/gc/gc_mark.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/ownership.c
  - zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_cleanup_registration.c
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  - zr_vm_core/include/zr_vm_core/closure.h
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_core/src/zr_vm_core/closure.c
  - zr_vm_core/src/zr_vm_core/closure_close_meta_guard.c
  - zr_vm_core/src/zr_vm_core/closure_close_meta_guard.h
  - zr_vm_core/src/zr_vm_core/closure_close_proxy_token.c
  - zr_vm_core/src/zr_vm_core/closure_close_proxy_token.h
  - zr_vm_core/src/zr_vm_core/execution/execution_control.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/ownership_shared.c
  - zr_vm_library/src/zr_vm_library/aot_runtime.c
plan_sources:
  - user: 2026-10-04 首方 core close 三 fixture 注释与实际调用契约审查
  - user: 2026-07-19 按 docs/plans/syntax 严格执行并逐里程碑提交
  - user: 2026-09-27 继续 docs/plans/ssa、完成验证并逐子任务提交
  - docs/plans/syntax/2026-07-18-03-struct-ref-struct-span-layout-design.md
  - docs/plans/ssa/03-interpreter-binding/01-dispatch-boundaries.md
  - docs/plans/ssa/04-frame-native/04-roots-observation.md
tests:
  - tests/core/test_close_proxy.c
  - tests/core/test_close_proxy_instruction.c
  - tests/library/test_close_proxy_aot_runtime.c
  - tests/core/test_close_meta_exception.c
  - tests/cmake/close-proxy-tests.cmake
  - tests/parser/test_buffer_pool_ffi.c
  - tests/parser/test_resource_shared_weak.c
  - tests/core/test_type_layout_inline_copy.c
  - tests/parser/test_aot_c_call_shared_library_smoke.c
doc_type: module-detail
---

# Exception Scope Resource Cleanup

## Purpose

`using(resource)` resources are represented by the VM's to-be-closed stack chain. Normal
scope exit already closes registrations, but exception transfer must also close
the resources created inside the abandoned try scope before control reaches a
catch or finally block. Syntax03 M5 makes that ordering explicit for PoolLease and
other close-meta providers.

## Handler Checkpoints

Every pushed `SZrVmExceptionHandlerState` saves the current to-be-closed chain as
a stack-relative offset. The offset survives stack relocation. During exception
unwind, the VM resolves the saved boundary and repeatedly closes the top
registration while it is above the boundary.

Cleanup runs before:

- entering a matching catch;
- entering a finally block with a pending exception;
- popping a handler that cannot handle the exception;
- leaving a handler whose finally phase throws again.

Outer registrations remain linked because their stack positions are at or below
the saved boundary. Nested handlers therefore close only the resources whose
lexical scope is being abandoned, in LIFO order.

## Close Call Scratch Contract

A close meta call needs three values: callable, resource receiver, and error
argument. Scratch reservation may grow or relocate the stack, so the resource is
first saved as a stack offset and reloaded after reservation. The error object is
built directly in the third scratch slot. It is never constructed next to the
registered resource, where it could overwrite another live local or cached
callable.

The close function runs without yield during exception unwind. A normal close
receives null; exceptional close receives the current exception status projected
as an error value. The existing close-registration pop remains the single source
of truth, so the same registration cannot be invoked twice by one unwind.

## Pending-error close callbacks

When an exceptional close calls script `@close`, the outer Error must stay
available to the callback as its error argument without remaining the VM's
active exception during that nested call. Otherwise `RESUME_AFTER_NATIVE_CALL`
can resume exception dispatch into the outer catch from inside `@close` and
clear the Error before the original unwind reaches it. The same failure occurs
for an ordinary `using(new Resource())` registration and for a close proxy.

`closure_value_call_close_meta` reserves one additional scratch slot only
when an Error is pending. `closure_close_meta_guard.c` initializes a real native
call-info frame in that slot. Exception dispatch stops at this frame, while a
nested `TryRun` captures a new error raised by the callback. The guard saves the
original exception value and status and roots its Error object through an AOT
root frame before clearing the ambient exception. If `@close` returns normally,
the guard restores the original Error and its thread status only when `TryRun`
returns `FINE`, the thread status is still `FINE`, and no exception is active.
If a budget poll terminates the callback call, the guard preserves the
execution-terminated state and does not restore the saved Error after the poll
clears `hasCurrentException`. If `@close` throws, the new Error remains active and
takes precedence. The callback argument is a separate rooted stack value
throughout the call. A status without an active Error continues through the
existing callback path: nested exception dispatch and `CATCH` only act when
`hasCurrentException` is set.

The guard restores the outer call-info node, native-call yield count,
execution-budget native-frame marker, and logical stack top after the callback.
It trims callback handlers to the entry depth and rejects a handler underflow.
The logical top is reconstructed from the scratch-slot byte
offset; `outer->functionTop` remains a frame high-water boundary and cannot be
used as the logical top because repeated cleanup would advance it every time.
Stack offsets survive stack growth. The current local `TryRun` recovery restores
the AOT root stack and depth captured at entry when the callback throws. The close
guard then checks the chain against its still-live root frame and repairs an
imbalance before handler cleanup, without walking abandoned C-local roots. A
normal-return callback that leaves extra roots is rejected as an error; the guard
pops its own saved-Error root after verification.

A direct native `@close` throw can also bypass the callback's ordinary
`PostCall`. The guard discards exactly one directly linked native child frame
when it has the expected callback slot, no return destination, no inline frame
metadata, and no open upvalues or close registrations. The reusable `next`
chain remains linked. Other residual frame shapes, handler underflow, and
unexpected pending control are diagnosed rather than silently skipped; these
paths require a separate cleanup protocol before they can safely resume the
outer catch.

## Ownership handles

The same chain directly closes `Unique`, `Shared`, `Weak`, and `Loan` values. Frame-layout locals
can keep their physical value outside the dense stack slot used by the close chain, so ownership
operations synchronize a retained cleanup mirror before and after overwrite, move, share, weak,
upgrade, release, and loan transitions. This prevents an exception from releasing a stale control
or leaving an extra strong/weak count alive.

Shared/Weak value parameters are also balanced across calls. After a callee successfully copies a
non-borrowed parameter, the caller staging owner is released. An exception then closes only the
callee copy plus other live lexical registrations. The final strong release marks the stable
control dead before resource Drop, which makes an upgrade attempted during Drop return empty.

## Distinct physical frame values

A frame-layout VALUE slot can have a dense registered cleanup cell and a distinct
physical `SZrTypeValue`. `closure_registered_mirror_frame_value` resolves this
pair through the active call-info chain. Proxy lookup accepts active VM frames;
the ordinary registration path may also use native frames with metadata.

The release branches have different obligations. If both owner cells retain the
same non-null ownership control, `closure_value_call_close_meta` releases the
physical reference and then the registered reference: each cell owns a retained
reference. If both cells are direct UNIQUE/LOANED aliases without a control,
the registered cell is reset before the physical owner is released, preventing
duplicate direct Drop. The ordinary close path therefore cannot be summarized
as always clearing the dense cell first. The proxy path separately stages the
chosen receiver in its registered high slot and clears source representations
before requesting the callback. These branches express their actual slot and
reference obligations; they do not establish a general reentry or relocation
guarantee for every destructor path.

Ownership cleanup reaches `ZrCore_Ownership_ReleaseValue`. Resource classes use
their Drop/destructor contract; ordinary closable objects use the CLOSE meta
callback. A borrowed view is cleared without releasing its owner or invoking
CLOSE. A plain proxy source without ownership cleanup or a callable CLOSE stays
readable. These distinctions must be preserved when adding registration callers.

## Close proxies for an existing local

An inner `using(existingLocal)` must add a cleanup registration above the current
to-be-closed marker, even when the local already has an outer registration. The
core API `ZrCore_Closure_MarkCloseProxy(state, proxySlot, sourceSlot)` registers
an empty, rooted high stack slot while retaining the original dense local as the
source. It fails without changing the close chain if the proxy slot is occupied,
is below the source or current marker, or lies outside the active stack. The
source may have an outer marker or no marker. A body-free `using existingLocal;`
in a nested lexical scope has the same need for a high registration.

The proxy slot contains a private, GC-scanned NativeData token with byte offsets
for the source and proxy slots relative to the stack base. Offsets survive stack
relocation. A file-private address identifies the token only inside the live
process; neither that address nor the NativeData representation is part of the
bytecode or artifact ABI. A token copied to another stack slot fails its slot
identity check. The private token functions live in `closure_close_proxy_token.c`
so `closure.c` retains ownership of the close chain and callback protocol.

At close, a plain value without ownership cleanup or a callable `@close`
leaves the logical source and its physical mirror readable. A borrowed view is
cleared in both locations, following `OWN_DROP` without invoking the object's
`@close` or releasing its owner. For values that require cleanup, the VM moves
the source into the rooted high slot, clears the source and any distinct VM
frame-layout physical mirror, then invokes its close meta or ownership release.
A callback that grows the stack or throws therefore sees the original local as
null. The close receiver is copied into scratch before the high slot is reset,
so it remains rooted for the callback.
When dense and physical cells both retain an ownership control, the redundant
mirror reference is released once; a direct owner alias is only tombstoned to
avoid duplicate Drop. Ordinary legacy close registrations retain their existing
physical-mirror lookup behavior; only proxy lookup requires an active VM frame
to avoid interpreting an inactive native call-info layout.

The proxy occupies one node in the existing marker chain. Normal scope exit
consumes that node through `CLOSE_SCOPE(1)`; exception unwind closes it above the
handler checkpoint before catch. Older markers remain linked, and their source
is already null when later popped. Nested proxies for a closable value see a
null source after the first close and are inert.

`MARK_CLOSE_PROXY` is appended at opcode 245. Its E operand names the new high
proxy slot, and A1 names the existing dense source slot. The interpreter and
frame-slot scan recognize both operands. AOT C and LLVM lower it to
`ZrLibrary_AotRuntime_MarkCloseProxy(state, frame, E, A1)`. The AOT helper uses
the ordinary cleanup registration preparation to select the high physical
VALUE slot, then passes the dense source to the core API so cleanup clears its
physical mirror as well. Invalid or out-of-order slots fail before registration.
The generated helper symbol is required when linking a module that contains
this opcode against the runtime; an older runtime cannot execute such a module.

## 捕获单元与关闭接口的调用限制

VM 闭包的捕获数组保存共享 `SZrClosureValue`。`FindOrCreateValue` 使捕获同一
活栈槽的闭包共享开放单元；开放链按地址降序排列。`CloseStackValue` 将阈值以上
且仍在逻辑栈顶以内的捕获复制到单元自身存储，摘链并补传已锚定逃逸及 GC 屏障，
它不调用 CLOSE。`CloseClosure` 先冻结捕获，再从关闭链逆序摘下登记并执行清理。
`CloseRegisteredValues` 只按数量消费登记；AOT cleanup helper 在每项消费前
另行调用 `CloseStackValue`。新增调用者必须明确哪一层负责冻结捕获和保持有效栈顶。

原生闭包同时保存捕获地址和 owner 尾数组，均由同一次 GC 对象分配管理。
`GetCaptureValue` 在 owner 的 tag 为 CLOSURE_VALUE 时通过单元重取当前值；
其他 owner 类型或无 owner 时返回直接捕获地址。消费者须保持相应对象或栈槽可达，
不能跨关闭或栈搬迁缓存旧地址。GC 的 native closure 扫描会扫描非空 owner；
`PropagateEscapeFromObject` 的分支则检查 owner 的类型：CLOSURE_VALUE owner
传播到共享单元；NULL 或其他类型 owner 且捕获值有效时，直接标记该捕获值逸出。
扫描可达性与逸出传播的条件不同，不能互相替代。

私有 proxy token 只保存当前 state 的 source/self 栈偏移及进程内身份，不持有
source 资源。GC 按 NATIVE_DATA 的 value 数组扫描 token，关闭时还必须重新检查
身份、原登记槽、自身偏移和 source 范围。`MarkCloseProxy` 的 false 表示校验或
安装未完成；分配层在完整 GC 后仍失败时可以抛出 MEMORY_ERROR，调用者不能把
它当作只返回 bool 的无抛出接口。非法、空、越界及非原槽 token 不获得关闭信用。

`InitValue` 的现存 TODO 针对 AOT 投影尚未发布到 projectedSelfValue 时的 GC 根：
应从 library 投影调用与 GC-capable 分配处核查，并用分配失败注入验证；本次注释
整合没有复现此风险，也没有授予 runtime 或 GC 保证。

## Generated-call exception transfer

AOT C and LLVM calls complete through resume-aware runtime boundaries. A normal
return finishes the prepared call. A caught exception that resumes in the same
generated caller refreshes that caller frame and dispatches at the resume
instruction. An exception that has already unwound beyond that caller is left
unchanged for the outer handler; it is not rewritten as an AOT runtime failure.
This keeps nested direct/meta-call cleanup on the same exception-scope contract
as interpreter execution.

## Verification

`zr_vm_close_proxy_aot_runtime_test` is a manual helper fixture for an ordinary
closable object. It constructs a `ZrAotGeneratedFrame` and active VM call-info;
it does not enter a generated AOT function. Dense logical source/proxy slots
0/1 map to separate physical VALUE storage at stack-slot offsets 2/3. The
outer registration occupies the physical source, while the higher physical
proxy token targets the dense source. The first `CloseScope(1)` clears both
source representations before the native close callback; the second removes
the already-null physical source without another callback, ending at the
`stackBase` close-chain sentinel. `CallWithoutYield(..., 0)` reaches ordinary
native pre-call dispatch and the prepared native-frame invocation, rather than
SingleResultFastRestore. The callback observes source nullness and call count;
it does not force GC, stack growth or reentry, and the fixture has no GC API or
GC-before/after assertions. The 2026-10-04 paired clang-cl check compiled both
original and comment candidate successfully; this is compile-only evidence,
with no runtime, GC, CTest or generated-entry execution credit.

`zr_vm_close_proxy_core_test` exercises 19 focused cases: one close with an older
marker, an unmarked source, plain source and distinct mirror preservation,
borrowed view reset without `@close`, exceptional close and handler boundary,
nested proxies, distinct dense/physical mirrors, full GC with an active token
and during the close callback, original Error survival across full GC, native
replacement Error and repeated call-info reuse, an AOT root frame abandoned by
a throwing native callback, cancelled budget preservation without callback
invocation, registration order rejection, AOT-like physical marker ordering,
a stale native-frame layout, copied-token rejection, and both retained control
and direct-owner mirror aliases. `zr_vm_type_layout_inline_copy_test`
protects the legacy physical-mirror path; `zr_vm_native_closure_value_test`
protects native closure metadata handling.

`zr_vm_close_meta_exception_test` covers four script-level paths: a plain
throw/catch control, repeated cleanup and call-info chain reachability, the
original Error reaching its catch after script `@close`, and a callback's new
Error replacing the original one. Its three injected `CloseProbe` types are
ordinary script classes with `@close`, constructed in `using(new CloseProbe())`;
they exercise ordinary close registration, rather than an existing-local proxy
or a resource-class destructor. The repeated case executes one compiled
function three times and checks catch result, reusable call-info reachability
and handler depth; it does not assert the script's `calls` counter. The preserve
case checks both the original message and exactly one callback, while the
replacement case checks the new message. These scripts do not force GC, and
the named nested-call case does not establish arbitrary recursive nesting.
See `tests/core/test_close_meta_exception.c:82`,
`tests/core/test_close_meta_exception.c:102`,
`tests/core/test_close_meta_exception.c:111`,
`tests/core/test_close_meta_exception.c:131` and
`tests/core/test_close_meta_exception.c:156`.

`zr_vm_close_proxy_instruction_test` runs six hand-written interpreter
instructions. `MARK_CLOSE_PROXY` uses E=2 for the high proxy and A1=1 for its
source; two subsequent `CLOSE_SCOPE(1)` instructions consume the proxy and the
older source registration. The assertions observe one callback, source clearing at the callback
and the stack-base close-chain sentinel. A separate
frame-slot scan uses E=9 and A1=3 and expects ten slots; because E is the larger
operand, this assertion alone cannot establish an independent contribution
from the lower A1 operand. This fixture does not compile or enter generated
AOT code. See `tests/core/test_close_proxy_instruction.c:101`,
`tests/core/test_close_proxy_instruction.c:102`,
`tests/core/test_close_proxy_instruction.c:105`,
`tests/core/test_close_proxy_instruction.c:107`,
`tests/core/test_close_proxy_instruction.c:161`.

The core proxy fixture constructs active VM frame-layout mirrors manually
where required. It observes dense/physical source clearing before callbacks,
close-chain ordering, ownership-release aliases and exception/budget cleanup.
Its full-GC requests are followed by receiver/type/message assertions; they
supply no collection-count or relocation-count assertion. The pending-error
native replacement case also checks call-info, handler, yield, budget and
logical-stack restoration, while the cancelled-budget case asserts no callback
and preservation of termination. These are fixture-specific observations,
not a proof of the entire runtime or generated AOT entry behavior. See
`tests/core/test_close_proxy.c:562`, `tests/core/test_close_proxy.c:640`,
`tests/core/test_close_proxy.c:644`, `tests/core/test_close_proxy.c:655`,
`tests/core/test_close_proxy.c:730` and `tests/core/test_close_proxy.c:733`.

CTest dispatches the three standalone Unity mains through actual `add_test`
COMMAND entries: `tests/cmake/close-proxy-tests.cmake:8` for `close_proxy_core`,
`tests/cmake/close-proxy-tests.cmake:20` for `close_meta_exception`, and
`tests/cmake/close-proxy-tests.cmake:29` for `close_proxy_instruction`.
The mains register 19, four and two cases, respectively, through `RUN_TEST`;
Unity supplies their setup/teardown lifecycle. The existing Windows compile-only before/after check completed six compile steps
successfully for these three core translation units. It supplies no link,
runtime, GC, CTest or generated-AOT execution proof. The overall eight-run result
remains false because both original and comment AOT-GC fixture compiles report
macro errors; these core compile pairs do not establish behavior correctness.

`zr_vm_buffer_pool_ffi_test` throws from inside `using(lease)`, catches outside,
then rents the same size again. The expected generation and return/reuse counters
prove that cleanup ran exactly once before catch and did not corrupt adjacent VM
state. Parent using/escape tests protect normal close and structured cleanup
behavior.

`zr_vm_resource_shared_weak_test` throws after creating Shared clones and a Weak observer, then
asserts that unwind runs resource Drop once. It also covers value-parameter copies, nested
Shared/Weak fields, final-strong behavior, and wake failure after the last
strong owner is dropped.

`zr_vm_type_layout_inline_copy_test` covers distinct dense/physical ownership
cells, including a resource Drop callback that verifies the dense alias is
already null and then forces stack growth. The AOT call and receiver shared
library suites cover caught nested exceptions, tail callable propagation, and
post-call Weak expiry through generated C and LLVM.

## VM 有界执行预算与终止清理（2026-10-05 静态契约审阅）

`execution_budget.h/c` 的预算域是一次受限导出调用。Rust binding 在
`zr_vm_rust_binding/src/zr_vm_rust_binding/execution_budget.c:58` 零初始化 scope，
设置限额后绑定到 state；完成时结算 GC、Poll、恢复线程并撤销借用绑定，最后发布 usage。
它与 async frame 标量调度预算各有用途，不提供跨线程共享预算记录的同步。
has* 开关区分未设置和零限额：指令/native 的零限额拒绝下一次入场；堆与已结算
GC 时间以严格大于限额拒绝，等于仍可放行。计数只在允许入场后累计并饱和。

deadline 是 `NowMicros` 同一单调时钟域的绝对微秒时间，在协作 Poll 时观察。
时钟故障返回最大值，使启用的期限检查拒绝继续；不能据此保证 GC 时间测量准确。
cancelToken 是借用的一次性原子取消信号，取消后没有 reset；宿主必须等预算调用和
所有并发读写结束才 Free。仅取消位原子化，不使 scope 的计数、终止原因或 GC 深度线程安全。
Poll 首次原因按取消、期限、堆、GC、指令顺序锁存，随后发布 EXECUTION_TERMINATED
并清除活动异常标志；后续 Poll 保持首次原因。没有副作用回滚承诺。

native 在真正进入回调前收费：`zr_vm_core/src/zr_vm_core/function.c:3630` 是实际
NativeEnter 调用。注册函数帧与紧接着的 binding 层用借用 call-info 身份去重一次，
返回路径清除该身份；它不持有帧、不注册 GC root，也不能中断正在运行的 C 回调。
dispatch 在 `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:5719` 的入场
Poll 消费指令，失败在 `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c:5722`
展开 VM 帧；native 返回边界只检查，不重复计指令。

BeginMemory 以既有 global 存量开启共享峰值窗口；同 global 的其他 mutator/GC worker
分配也进入统计，它不是单次调用净增长或 RSS。allocator 成功后记账，堆拒绝由后续 Poll
执行，故可暂时超过限额。execution_memory.c 已有并发 peak 低报 BUG 保持原证据与范围，
此次不修复，也不把预算比较当作精确隔离计量证明。

GC 只结算最外层 Begin/End 墙钟区间，内层不重新启动计时。实际 GC caller 在
`zr_vm_core/src/zr_vm_core/gc/gc.c:765` Begin，失败 STW 入口在
`zr_vm_core/src/zr_vm_core/gc/gc.c:768` End，正常结束在
`zr_vm_core/src/zr_vm_core/gc/gc.c:811` End；因此时间包含等待和失败入口窗口，
不是各 worker CPU 用时之和。调用方必须在同一串行 state 上以有限深度配对，
gcDepth 非原子且 UINT32 增量不饱和；宿主完成路径补齐未结束层数后再读取 usage。
未结算区间不会被限额抢占，时钟反向时该次 elapsed 记零。

Unwind 仅清理 VM 帧到 native 边界，丢弃 guest handler，先冻结开放捕获再消费帧值和
关闭登记；栈增长期间用偏移恢复位置。同步 TryRun 隔离 C 清理异常，不跨外部 C 回调栈，
也不保证失败的 close/drop 已完成全部资源释放。清理之后仍恢复执行终止状态；异常资源
协议与上文相同，不能把隔离异常视为成功清理证明。

本次完整范围为两源 62 单元（旧 32 行补齐字段、枚举值及 guards），沿用完整源码独审
与 r3 有限修订接受；只执行 after-aware 台账检查和只读 patch 检查，没有 native/build/
runtime 执行信用。上面的既有 fixture/验收叙述保持其历史范围，当前测试注册与正调用
仅支持静态 producer/caller/lifetime 证据。legacy inventory 的 reviewed 记录不自动成为
本次 whole62 或整个 module 已接受的证明。
