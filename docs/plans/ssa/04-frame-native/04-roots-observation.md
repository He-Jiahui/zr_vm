---
related_code:
  - zr_vm_core/src/zr_vm_core/debug_artifact.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/stack.c
  - zr_vm_core/src/zr_vm_core/exception.c
  - zr_vm_core/include/zr_vm_core/exception.h
  - zr_vm_core/include/zr_vm_core/gc.h
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
implementation_files:
  - zr_vm_core/src/zr_vm_core/debug_artifact.c
  - zr_vm_core/src/zr_vm_core/gc/gc_cycle.c
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/stack.c
  - zr_vm_core/src/zr_vm_core/exception.c
  - zr_vm_core/include/zr_vm_core/exception.h
  - zr_vm_core/include/zr_vm_core/gc.h
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  - zr_vm_core/src/zr_vm_core/execution/execution_frame_roots.c
  - zr_vm_core/src/zr_vm_core/execution/execution_frame_observation.c
plan_sources:
  - docs/plans/ssa/index.md
  - docs/plans/ssa/04-frame-native/04-roots-observation.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/core/test_ssa_roots_observation.c
  - tests/core/test_aot_gc_root_frame.c
  - tests/core/test_execution_add_stack_relocation.c
  - tests/core/test_execution_add_stack_relocation_aot_roots.inc
  - tests/core/test_aot_gc_root_frame_exception.inc
  - tests/parser/test_aot_c_shared_library_smoke.c
  - tests/parser/test_aot_c_shared_library_throw_root.inc
  - tests/cmake/aot-generated-root-tests.cmake
  - tests/acceptance/ssa-stack-root-frame-relocation.md
  - tests/acceptance/aot-root-frame-protected-unwind.md
  - tests/acceptance/ssa-generated-aot-throw-root.md
doc_type: milestone-detail
status: in-progress
---

# 04.04 精确根、Frame 重定位与调试观察

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 统一精确 roots、frame relocation 与 debugger 观察，避免表示优化导致不可扫描或不可恢复状态。

**Architecture：** 逻辑 state map 经 frame descriptor 投影为 safepoint 位置表；托管 base 与 derived offset 分离，观察时按需物化。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M2/M4/M7。
- 前置：[04.01 Packed frame 与精确存储布局](../04-frame-native/01-frame-layout.md)；[01.04 Ownership、挂起与状态恢复映射](../01-execir-ssa/04-state-maps.md)。
- 交付：ExecBC/C/LLVM 可复用的 root enumeration 和 observation adapter。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

已有 test_aot_gc_root_frame、debug_artifact 与 frame relocation 路径；不能把全堆扫描当成漏 root 的常规兜底。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/debug_artifact.c` | 把 source map 关联 logical value |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/gc/gc_cycle.c` | 复用 frame 根遍历接缝 |
| 现有，修改/复用 | `zr_vm_common/include/zr_vm_common/zr_aot_abi.h` | 声明统一 root frame ABI |
| 计划新增 | `zr_vm_core/src/zr_vm_core/execution/execution_frame_roots.c` | 按 map 枚举和更新精确 roots |
| 计划新增 | `zr_vm_core/src/zr_vm_core/execution/execution_frame_observation.c` | debug 物化与修改后的状态同步 |
| 计划新增测试 | `tests/core/test_ssa_roots_observation.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 投影 safepoint liveness** 只枚举活跃引用和初始化的 inline fields；标量 bit pattern 即使像指针也不能被扫描。

- [ ] **2. 处理 derived pointer** 保存 base root+offset，GC 移动/stack 扩容后重算；native pin 保持地址的生命周期必须可见。

- [ ] **2a. 重定位活动 AOT 根帧基址** VM 栈扩容时暂存并恢复栈内 `frameBase`，保持 `LOCAL_ADDRESS` 基址原址；拒绝位于可移动 VM 栈 allocation（包括 extra slots）的根帧链节点，并拒绝重复压入已活动节点以避免成环。失败分配须先恢复根帧/调用帧到旧栈，再恢复 GC stop 状态；测试强制移动 allocator 后直接检查多个 VM `frameBase` 与非空 `previous` 链恢复，另用只有 AOT 根帧可达的年轻普通对象验证 minor GC 存活和转入 survivor。当前 minor GC 对这些普通对象原址重分配，不要求指针改写。OOM 测试的 allocator 明确保留失败前的旧 block；此夹具不代表所有 allocator 均提供该保证。MSVC 专项验证已完成（focused target 增量构建 2/2 成功，direct 23 tests 全过）；旧 `stack.c` 单对象对照已验证为真实 RED：23 tests 中仅迁移断言失败，其余 22 通过，map 确认 `ZrCore_Stack_GrowTo` 来自旧对象且未抽取 fixed archive member。GCC/Clang focused runs 仍待执行。见 [独立 acceptance](../../../../tests/acceptance/ssa-stack-root-frame-relocation.md)。

- [ ] **3. 保留调试语义** 局部优化消失时报告 optimized-out 或物化合法值；debug 写入使相关 mirror/guard 失效，不能修改已经不存在的存储。

- [ ] **4. 统一压力测试** 在每类 safepoint 插强制 GC、栈扩容、异常、重入与 suspend；跟踪进入/退出 root frame 平衡。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
for location in gcMap[safepoint]:
    if location.isDerived: updateBaseThenRecomputeOffset()
    else if location.isManagedRoot && location.initialized: traceAndUpdate()
assert(rootFramePushCount == rootFramePopCountAtExit)
debugRead(value) -> mapLogicalToPhysicalOrMaterialize(value)
debugWrite(value) -> writebackAndInvalidateDependentFacts()
```

safepoint map 完整性必须在 codegen 前验证，机器码后端还需实际位置验证。异常退出不能漏 pop root frame。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| 标量值恰好像 heap 地址 | 不当 root |
| inline struct 内嵌引用且局部未初始化 | 仅合法字段被扫描 |
| GC 后 derived pointer 与 stack relocation | 地址更新正确 |
| debug 修改局部后继续优化代码 | mirror/guard 失效，读到新值 |

复用回归入口：`tests/core/test_aot_gc_root_frame.c`。历史计数只作为核对线索，实施时重跑并记录实际总数。

活动 AOT 根帧迁移使用直接测试目标 `zr_vm_execution_add_stack_relocation_test`；该目标没有 CTest 注册。其历史 test-only 失败曾要求普通对象根槽在 minor GC 后发生指针改写，这不是有效 RED：当前 collector 对该对象类型原址重分配。现行 fixture 改为验证两个 VM `FRAME_BYTE_OFFSET` 根和一个 `LOCAL_ADDRESS` 根仍存活并晋升到 survivor，同时单独断言多个 VM `frameBase` 指向新栈槽。

登记新 CTest 名 `ssa_roots_observation` 和可执行目标 `zr_vm_ssa_roots_observation_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_roots_observation_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_roots_observation$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** 所有后端 root/observation 映射同源；moving GC、异常和 debug 恢复无遗漏。

**失败恢复：** 缺少精确 map 的优化/后端不启用，不通过扩大扫描范围掩盖问题。

**文档交付：** 新增 docs/core-runtime/frame-roots-observation.md；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrFrameRootVisitor SZrFrameRootVisitor;
typedef struct SZrFrameObservationRequest SZrFrameObservationRequest;
TZrBool ZrCore_Execution_VisitFrameRoots(struct SZrState *state,
    const SZrFrameRootVisitor *visitor, SZrExecutionDiagnostic *diagnostic);
TZrBool ZrCore_Execution_ObserveFrame(const SZrFrameObservationRequest *request,
    SZrExecutionDiagnostic *diagnostic);
/* SZrExecutionDiagnostic 为 core 自有运行期诊断类型（04.02 起定义）；不引用 parser 侧 SZrExecIrDiagnostic */
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| GC liveness map | SSA/state-map builder | precise root scanner 不扫描未初始化值 |
| physical register/spill/offset | backend position map | 移动后 derived pointer 由 base 更新 |
| debug materialization state | observer | 调试写入必须使优化假设失效 |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 为无优化 frame 建精确 maps，与现有 root 遍历对照，不依靠全堆扫描。

- [ ] **批次 2：** 接 packed/scalar/inline/derived roots，使用强制 GC 验证所有 safe points。

- [ ] **批次 3：** 接 debug read/write 和 optimized-out 展示，模拟观察后 resume 的 cache invalidation。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange scalar bits equal heap address
assert not scanned as root
arrange derived pointer into moved array
assert updated base+offset used after GC
arrange debugger writes a scalar used in specialized loop
assert resumed loop observes new value or deopts safely
```

### 迁移结束检查

#### Protected AOT root-frame unwind

`ZrCore_Exception_TryRun` owns a private recovery context whose first member is
the thread-affine `SZrExceptionLongJump`. On the current C11 `longjmp` path, a
local Throw restores the entry AOT root-chain top and depth after status
normalization but before `MutatorUnwindScopes` publishes the mutator as
inactive. This ordering prevents a concurrent stop-the-world scan from
observing callback-local root nodes after their C lifetimes end. TryRun catch
repeats the direct assignment idempotently; neither path traverses abandoned
nodes. The callback-return flag also distinguishes normal return from
`Throw(FINE)`. Normal callbacks retain their Push/Pop behavior.

Forced-C++ Throw removes callback roots before stack unwinding runs
destructors; destructor reentry into VM/GC during that unwind is not covered by
this lifecycle guarantee. A native C++ exception that bypasses
`ZrCore_Exception_Throw` is restored only when TryRun catch is reached, with no
pre-catch ordering guarantee. The legacy worker-forward path does not access
another thread's private snapshot, and this change makes no new guarantee for
its existing cross-thread longjmp behavior.

The focused root-frame fixture covers empty and outer chains, nested TryRun,
`Throw(FINE)`, real minor GC with an outer young root, and normal Push/Pop. A
combined case forces moving `Stack_GrowTo` with an outer `FRAME_BYTE_OFFSET`
root and a callback-local root active, then checks the rebased outer frame and
minor-GC survival after Throw. The initial MSVC RED was confirmed on the three
non-local-exit chain checks before the combined case was added. Frozen-source
MSVC standalone verification passed 12/12, independently rerun by root with
source hashes and map provider checked. The GNU pre-inactive real-unwind wrap
probe passed both current-code cases; catch-only and HEAD variants failed the
same invariant with exit1. The full current native Core final increment then
built 5/5, direct root-frame suite passed 12/12, and registered root/legacy
capability CTests passed 2/2. Full GCC/Clang suites and generated-AOT shared-
library smoke remain pending. See
[the acceptance record](../../../../tests/acceptance/aot-root-frame-protected-unwind.md).

#### Generated shared C-AOT Throw integration (2026-10-01)

The Linux fixture now compiles source, emits executable C, builds its shared
library and reaches the generated throw helper through a direct C call. The
allocator observer requires that helper's unique THROW slot and payload,
then records a populated generated `LOCAL_ADDRESS` map pointing to a live
object distinct from the outer caller root. Error payload and post-TryRun
chain restoration passed the first qualified GCC/Clang runs.

Their final young-survivor expectation was invalid: automatic entry GC had
already promoted the live outer object at age two, including during a
test-only stop-flag attempt. The final fixture leaves automatic GC enabled and
completes a real full collection after restoring the caller's saved VM/native
frame window. The caller C root must retain the outer object while an unrooted
control is released; full-collection kind/count and root Push/Pop must balance.
The new Linux CTest is
`ssa_aot_generated_throw_root`, using `--throw-root-only`; GCC 11.4 and Clang
14 each passed it 1/1, and their direct core root suites passed 12/12. MSVC
rebuilt the test targets in 17/17 steps and
passed the adjacent core root suite 12/12; its Linux-only case is not executed.

This adds a bounded generated-C integration gate. It does not prove GC visited
or rewrote the generated local slot during the callback, wire packed ExecIR
roots, validate LLVM/deopt/suspend paths or complete 04.04. Commands and exact
platform evidence are in
[the generated-AOT acceptance](../../../../tests/acceptance/ssa-generated-aot-throw-root.md).

所有异常/挂起出口 root frame push/pop 平衡。生成 map 和实际 machine/frame location 的一致性需要后端测试，不能只验证逻辑 map 自洽。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。
