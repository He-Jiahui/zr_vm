---
related_code:
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_core/include/zr_vm_core/call_binding.h
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_core/include/zr_vm_core/call_binding.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effects.c
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/parser/test_ssa_effects_verifier.c
  - tests/parser/test_ssa_cfg_effects_faults.c
  - tests/acceptance/ssa-cfg-natural-loop-effects.md
doc_type: milestone-detail
status: planned
---

# 01.03 Memory、Effect、异常边与验证器

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 以显式 memory/effect token 和 verifier 约束所有优化，保护异常、GC、ownership 与调度顺序。

**Architecture：** memory token 按可证明不别名的区域分区；不可确定 native/global 写入合并为保守 top effect。验证器分结构、SSA、类型、effect、生命周期阶段，输出确定性错误。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M1 correctness gate。
- 前置：[01.02 Canonical facts lowering、CFG 与 SSA 构造](../01-execir-ssa/02-ssa-construction.md)。
- 交付：读写/效果模型、token phi、验证器与 malformed IR 负测。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

现有 semantic facts 不能自动证明任意重排安全；共享 ExecIR 必须显式表达 mayThrow/mayAllocate/mayGc/maySuspend 与异常边。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_parser/include/zr_vm_parser/semantic_ir.h` | 映射 effect 和 place 事实 |
| 现有，修改/复用 | `zr_vm_core/include/zr_vm_core/call_binding.h` | 验证调用 signature/operation/slot |
| 计划新增 | `zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify.c` | 对不可信 IR 做界限与结构校验 |
| 计划新增 | `zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_verify_effects.c` | memory/effect/异常/cleanup 顺序检查 |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_effects.c` | 生成 token 链与保守摘要 |
| 计划新增测试 | `tests/parser/test_ssa_effects_verifier.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 定义 effect region** frame、heap、global、native、GC、ownership、scheduler、I/O 分别编码；alias 不明时 join 区域，readonly 只减少已证明的写影响。

- [ ] **2. 生成 token 链** load 消费对应 memory 版本，store 产生新版本；可观察顺序使用 effect chain。CFG 合流产生 token phi，invoke 正常/异常出口各有明确定义。

- [ ] **3. 按依赖验证** 先检查边界/ID/数量，再 CFG/支配，再 type/layout/ownership，再 token 顺序和 safepoint map；先检查后解引用，不信任 artifact 声明的 effect。

- [ ] **4. 统一优化前后验证** 每个 pass 在输入/输出执行 verifier；失败保存最小 IR 和 pass 名并停止该编译，不能带着损坏 IR 进入后端。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
verify(module):
    checkRangesAndLimits()
    checkBlockTerminatorsAndEdges()
    checkUniqueDefinitionsAndDominance()
    checkPhiPredecessorsAndTypes()
    checkInstructionSchemaAndCallContracts()
    checkMemoryVersionsAndEffectContinuity()
    checkExceptionalResultsAndCleanupEdges()
    checkRootsAtSafepointsAndSuspendLifetimes()
    return diagnosticList
unknownNativeCall.effects = ALL_OBSERVABLE_REGIONS
```

纯计算仍可能陷阱或抛出，不能仅因无 store 就提前执行。Effect token 不授予系统权限；patch 必须另走能力验证。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| 重复 ValueId、不支配 use、phi 缺边 | 具体 IR diagnostic |
| store 后 load 错用旧 memory token | 验证拒绝 |
| tagged memory token range 缺少 opcode schema 声明的 region | 按输入/输出方向拒绝并保留 function/block/instruction/source 定位；全 untagged legacy range 继续兼容 |
| 移动 drop/throw/await 越过外部写入 | 效果链或异常区域验证失败 |
| 伪造 readonly native 与未声明 GC | 不接受不可信摘要，调用契约验证拒绝 |

本任务新增测试先独立运行，再进入完整 SSA 差分矩阵。

登记新 CTest 名 `ssa_effects_verifier` 和可执行目标 `zr_vm_ssa_effects_verifier_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_effects_verifier_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_effects_verifier$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** malformed 结构无需执行即可拒绝；pass 前后验证默认启用，所有错误可定位 function/block/instruction/source。

**失败恢复：** 保守重建分析可降优化强度；验证失败不能当作普通 deopt 在运行期兜底。

**文档交付：** 新增 docs/instruction-generation/execir-verifier.md；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
TZrBool ZrParser_ExecIr_Verify(const SZrExecIrModule *module,
    SZrExecIrDiagnostic *diagnostic);
TZrBool ZrCore_ExecIr_VerifyFunction(const SZrExecIrFunction *function,
    SZrExecIrDiagnostic *diagnostic);
/* Parser entry delegates structural checks to the core verifier. */
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| token region/version | effects builder | verifier 检查支配、phi 与 may-alias join |
| instruction schema flags | opcode schema + trusted callee contract | artifact 声明不能降低真实 effects |
| diagnostic position | 结构/类型/effect verifier | 先报界限错误再遍历，保证畸形数据不会崩溃 |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 实现结构验证，模糊测试 counts/ranges/edges，确保无越界与无递归爆栈。

- [ ] **批次 2：** 加入 SSA/type/layout/signature 检查；每类错误至少两个独立 malformed fixture。

- [ ] **批次 3：** 加入 memory/effect/exception/cleanup/suspend 验证；每个 pass 两端调用且做 verifier 时间统计。


### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange store produces memoryVersion=2 followed by load using version=1
assert effect verifier rejects stale memory dependency
arrange phi includes nonexistent predecessor edge
assert precise PHI_EDGE error, no invalid dereference
arrange mayThrow operation with result on exceptional successor
assert RESULT_UNAVAILABLE error
arrange typed natural-loop effects synthesis and fail the core phi-incoming pool realloc
assert OOM and no effect facts are published; retry the same CFG and require Core effect verification
arrange the same natural-loop fixture and fail the core memory-token pool realloc after phi append
assert OOM, restore the previous logical phiIncomingCount, and require same-CFG retry to pass Core effect verification
```

The natural-loop failure-injection fixture covers these two real core pool
reallocation sites and the retry verifier gate. It does not run a full
compiler/source-language loop oracle; see
`tests/acceptance/ssa-cfg-natural-loop-effects.md` for the recorded result and
remaining coverage gap. This focused evidence does not complete the broader
01.03 or M1 plan.

### 迁移结束检查

验证器是 loader 可调用的低层组件，不能依赖 parser runtime AST。error code 作为稳定诊断注册，测试不只匹配易变的人类描述字符串。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。
