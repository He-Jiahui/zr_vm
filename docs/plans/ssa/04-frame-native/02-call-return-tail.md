---
related_code:
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_tail_call.c
  - zr_vm_core/src/zr_vm_core/function_frame_place.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/function.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_tail_call.c
  - zr_vm_core/src/zr_vm_core/function_frame_place.c
  - zr_vm_core/src/zr_vm_core/execution/execution_call_transfer.c
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/core/test_ssa_call_return_tail.c
  - tests/core/test_tail_reuse_callinfo_reset.c
  - tests/core/test_vm_closure_precall.c
  - tests/parser/test_call_binding_pipeline.c
doc_type: milestone-detail
status: in-progress
---

# 04.02 调用返回、结果转发与尾调用复用

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 降低参数、return buffer 和尾调用边界上的复制，保持异常与 receiver writeback。

**Architecture：** 从 typed signature 生成 caller/callee frame transfer plan；标量和 inline span 直接传递，动态/ownership 边界走统一慢路。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M2。前置 03.03 仅取其 guard/result 协议部分作为输入；不等待 03.03 的 M3 完整门禁。
- 前置：[04.01 Packed frame 与精确存储布局](../04-frame-native/01-frame-layout.md)；[03.03 已解析目标、PIC 与 Guard 失效](../03-interpreter-binding/03-guarded-caches.md)。
- 交付：call transfer plan、return forwarding、tail reuse 判定与 cleanup 序列。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

core 已有 precall reset 与 tail reuse 测试，且 `execution/execution_tail_call.c` 已存在尾调用实现；本任务在该文件内扩展 eligibility 判定与 frame 复用，不新建第二个尾调用文件，也不能只为新 ABI 引入另一个隐式 frame 初始化协议。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/function.c` | 参数/返回布局契约入口 |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c` | 调用与返回 handler 分离 |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/execution/execution_tail_call.c` | 既有尾调用路径；扩展安全判定与 frame 复用，作为唯一尾调用实现 |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/function_frame_place.c` | 返回区与 receiver place |
| 计划新增 | `zr_vm_core/src/zr_vm_core/execution/execution_call_transfer.c` | 调用参数和返回区搬运 |
| 计划新增测试 | `tests/core/test_ssa_call_return_tail.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 生成传递计划** 根据 signature/layout/ownership 决定 scalar copy、span copy、move、borrow 或 boxed bridge；变参和 typed function value 明确各自 contract。

- [ ] **2. 转发 return buffer** caller destination 与 callee return storage 可共用时直接写入；有 alias、异常中间状态或 required writeback 则保留独立缓冲。

- [ ] **3. 限定尾调用复用** 无待执行 finally/drop、无指向旧 frame 的 borrow、无必须保留 debug frame 时才复用；参数搬移处理重叠，不先清掉仍需读取的值。

- [ ] **4. 补边界清理** call 成功、throw、native failure、null callable、suspend 各自只清理已初始化所有者一次；旧 cache/context 不能泄漏到复用 frame。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
transferPlan = classify(signature, sourceLayout, targetLayout, ownership)
forwardReturn = noAliasConflict && compatibleLayout && commitOrderPreserved
tailEligible = noPendingCleanup && noEscapingFrameAlias
               && compatibleContinuation && debugPolicyAllows
if tailEligible:
    moveArgumentsOverlapSafely()
    dropOnlyRetiredInitializedSlots()
    installNewFrameDescriptorAndGeneration()
else: allocateNormalCallFrame()
```

尾调用是自动优化而不是语言承诺，条件不足时普通调用。setter receiver 写回发生在语言规定的成功/失败边界。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| 重叠参数 swap 与深尾递归 | 值正确、栈增长符合 eligibility |
| finally/drop 活跃的尾位置 | 不复用 frame |
| inline struct return 中抛异常 | 目标不接受半写结果 |
| native re-entry 与 typed closure | context/arguments 不遗留 |

复用回归入口：`tests/core/test_tail_reuse_callinfo_reset.c`、`tests/core/test_vm_closure_precall.c`。历史计数只作为核对线索，实施时重跑并记录实际总数。

登记新 CTest 名 `ssa_call_return_tail` 和可执行目标 `zr_vm_ssa_call_return_tail_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_call_return_tail_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_call_return_tail$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** call/return/tail 事件与旧语义一致；收益分别报告复制量、指令数和帧大小。

**失败恢复：** 转发和尾复用可独立关闭，保留相同 typed ABI；不能改变用户可观察异常栈策略。

**文档交付：** 新增 docs/core-runtime/call-return-transfer.md；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrCallTransferPlan SZrCallTransferPlan;
typedef struct SZrCallTransferInput SZrCallTransferInput;
TZrBool ZrCore_Execution_PrepareTransfer(const SZrCallTransferInput *input,
    SZrCallTransferPlan *plan, SZrExecutionDiagnostic *diagnostic);
TZrBool ZrCore_Execution_CanReuseTailFrame(const SZrCallTransferPlan *plan);
/* SZrExecutionDiagnostic 为 core 自有运行期诊断类型，由本方向定义；不引用 parser 侧 SZrExecIrDiagnostic */
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| 传递类别 | typed signature/layout/ownership | transfer helper 不能猜 boxing/borrow 方式 |
| return commit state | callee normal/exception exit | caller 只接受已提交结果 |
| retired slot initialized bits | frame state | 尾复用只清理真正退出的所有者 |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 实现 transfer plan 与现有 precall 行为对照，不先启复制消除。

- [ ] **批次 2：** 启用 return forwarding，覆盖 alias、inline struct 和异常时半写问题。

- [ ] **批次 3：** 加入 tail eligibility 与重叠搬移；深递归/cleanup/debug 策略分别验收。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange tail args use previous frame slots that will be cleared
assert args moved before clear and each source read once
arrange pending finally
assert tail reuse=false
arrange callee throws after partial aggregate write
assert caller visible destination follows original commit rule
```

### 迁移结束检查

返回值、native ref/out 和 setter writeback 共享明确提交规则。尾调用拒绝原因进入 remark，不需要业务标注强制尾递归。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。

## Scoped progress: dynamic meta-tail fallback window (2026-09-29)

The tail-call fallback now copies the active outgoing window using the span
from its current callable slot to `state->stackTop`. This matters when dynamic
dispatch has replaced a callable object with its `@call` function and inserted
the original object as its receiver before frame reuse declines. The original
tail opcode or cache argument count no longer describes that expanded window.

The regression is in `tests/parser/test_call_binding_pipeline.c`. It checks the
ordinary dynamic-call result first, then runs a separate `DYN_TAIL_CALL` graph
whose inline-struct `@call` parameter forces the fallback path. The final
sentinel argument is part of the asserted result. This closes only that
fallback-window defect; the 04.02 transfer-plan, return-buffer, cleanup, and
other tail-reuse work remains open.

The edit keeps the existing fallback-window helper in the dispatcher because
it owns the slot-staging copy and stack-anchor restoration; only its input count
source changed. A future extraction should move the cohesive tail-call
preparation/fallback family, rather than splitting out this small calculation.

The focused MSVC build completed 708/708 edges in
`D:/tmp/zr_vm/ssa-artifact-v6-msvc`. The call-binding pipeline direct test
passed 18/18; the adjacent tail-reuse call-info direct test passed 4/4; and the
registered `call_binding_pipeline` plus `ssa_call_return_tail` CTest gates
passed 2/2. Full 04.02 acceptance remains open.
