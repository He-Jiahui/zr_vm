---
related_code:
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_arithmetic.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_calls.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_cleanup.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_setup.c
implementation_files:
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_arithmetic.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_calls.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_cleanup.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_setup.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_c.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_llvm.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_coverage.c
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/parser/test_ssa_c_llvm_lowering.c
  - tests/parser/test_aot_c_metadata_binding_loader.c
  - tests/parser/test_aot_c_value_semir_contracts.c
doc_type: milestone-detail
status: planned
---

# 07.02 C、LLVM 同源代码生成

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 让 C 与 LLVM 共同实现完整 typed lowering，并与 ExecIR/ExecBC 保持值、异常、GC 和 ownership 一致。

**Architecture：** 共用 AOTIR 和 runtime slow helpers，后端只负责合法代码生成；每个操作族带正向/边界/失败 fixture 和覆盖登记。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M4；完成 M1 四后端语义门禁。
- 前置：[07.01 共享 AOTIR 与后端 ABI 收敛](../07-aot-backends/01-aotir-contract.md)；[08.01 版本化 Artifact 与无地址 Relocation](../08-artifact-hotpatch/01-schema-relocation.md)。
- 交付：C/LLVM scalar/control/call/member/array/string/inline/EH/async lowering 与注册加载。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

现有 C 后端已按 typed arithmetic/calls/frames/cleanup 等分文件；优先接入这些职责，不把新功能重新汇总进单个 emitter。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_typed_arithmetic.c` | 改读 typed AOTIR |
| 现有，修改/复用 | `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_lowering_calls.c` | 统一 token call 和 bridge |
| 现有，修改/复用 | `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_cleanup.c` | 消费共享 cleanup/EH map |
| 现有，修改/复用 | `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_frame_setup.c` | 消费共享 frame/root descriptor |
| 计划新增 | `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_c.c` | C emitter 的 AOTIR 驱动入口 |
| 计划新增 | `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_llvm.c` | LLVM emitter 的 AOTIR 驱动入口 |
| 计划新增 | `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_ir_coverage.c` | 按操作登记 native/bridge/fallback |
| 计划新增测试 | `tests/parser/test_ssa_c_llvm_lowering.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 按族迁移代码生成** 先 scalar/convert/control，再 call/member/accessor/meta，后 array/string/inline/iterator/async；每族先冻结 oracle fixture 再连接两后端。

- [ ] **2. 避免宿主未定义行为** 整数 checked/wrapping、移位、除零、signed overflow、NaN/±0 按 ZR contract 生成明确代码；严格模式禁用隐式 FMA/重排。

- [ ] **3. 实现 EH/root/cleanup** mayGc 调用前发布精确 roots，可抛边清理已初始化 owners；native exception 转换、suspend state 和 deopt 使用统一 state map。

- [ ] **4. 登记与加载** token→entry/methodInfo/import 运行期注册；可 patch call 经过 entry cell，frozen call 可直接；生成源码含 ABI/layout/hash 静态断言。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
emit(operation):
    validateAotIrSchema(operation)
    chooseNativeInstructionOrSharedSemanticHelper()
    if mayGc: publishRoots()
    if mayThrow: connectExceptionalSuccessorAndCleanup()
    if maySuspend: saveLogicalResumeState()
    recordCoverage(native | runtimeBridge | interpreterFallback)
strictIntAdd -> explicitDefinedOverflowSemantics
strictFloatExpr -> noUnpermittedReassociationOrContraction
```

调用共享原生 runtime helper 可分类为 native-assisted，调用解释器必须单独计数；不能用辅助函数名掩盖解释 fallback。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| 整数极值/除零、NaN/±0、转换 | 四后端规则一致 |
| getter/native/virtual/interface 交叉调用 | token/slot/receiver 语义一致 |
| GC/异常/finally/inline struct writeback | 事件和根平衡一致 |
| async/iterator 挂起恢复 | 同一逻辑 resume 点，不重复效果 |

复用回归入口：`tests/parser/test_aot_c_metadata_binding_loader.c`、`tests/parser/test_aot_c_value_semir_contracts.c`。历史计数只作为核对线索，实施时重跑并记录实际总数。

登记新 CTest 名 `ssa_c_llvm_lowering` 和可执行目标 `zr_vm_ssa_c_llvm_lowering_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_c_llvm_lowering_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_c_llvm_lowering$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** 主索引的 M1/M4 全语义矩阵逐项完成，不能用 scalar demo 代替；unsupported/fallback 均可见。

**失败恢复：** 某个族未通过则保持显式 unsupported/legacy 状态；不发布错误 native code。

**文档交付：** 同步 docs/wiki 的 AOT C/LLVM 支持矩阵和 ABI 文档；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrAotEmitOptions SZrAotEmitOptions;
typedef struct SZrAotEmitResult SZrAotEmitResult;
TZrBool ZrParser_AotIr_EmitC(const SZrAotIrModule *module,
    const SZrAotEmitOptions *options, SZrAotEmitResult *result);
TZrBool ZrParser_AotIr_EmitLlvm(const SZrAotIrModule *module,
    const SZrAotEmitOptions *options, SZrAotEmitResult *result);
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| AOTIR/schema support | 共享 lowering | emitter 不引入独立语言规则 |
| physical root/EH maps | 各后端 codegen | 与 logical state map 逐点校验 |
| entry registration | 生成代码 + loader | token/signature/layout 校验后才调用 |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 按 scalar/convert/control 做首批四后端差分，包括所有数值边界。

- [ ] **批次 2：** 补 call/member/accessor/meta/inline/container，所有 native/helper/fallback 分类记录。

- [ ] **批次 3：** 补 exception/cleanup/async/iterator 与 GC stress，完成 M1/M4 全形态矩阵。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange signed overflow/invalid shift/division boundary
assert generated C avoids undefined behavior and matches oracle
arrange AOT setter throws after allocating managed object
assert cleanup/writeback/root balance equals ExecBC
arrange unsupported operation falls to interpreter
assert result may be correct but native coverage decreases
```

### 迁移结束检查

C/LLVM 的简单输出能编译不等于后端完成。生成代码必须实际加载执行，并验证语义原语的每个正常/异常出口。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。

