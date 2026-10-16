---
related_code:
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_mutation_internal.h
  - tests/parser/test_ssa_exec_ir_header_boundary.c
  - tests/cmake/ssa-execir-mutation-boundary.cmake
  - tests/cmake/ssa-execir-mutation-boundary-check.cmake
  - tests/parser/support/ssa_source_returned_execir_owner_shim.h
  - tests/parser/support/ssa_source_integer_expression_faults_producer_shim.h
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.h
  - zr_vm_core/CMakeLists.txt
  - zr_vm_core/include/zr_vm_core/aot_ir_owned_module.h
  - zr_vm_core/include/zr_vm_core/function_aot_ir.h
  - zr_vm_parser/include/zr_vm_parser/compiler_source_retention.h
implementation_files:
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_mutation_internal.h
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.h
  - zr_vm_core/CMakeLists.txt
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/include/zr_vm_core/exec_ir_opcode.def
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_builder.h
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/parser/test_ssa_exec_ir_header_boundary.c
  - tests/cmake/ssa-execir-mutation-boundary.cmake
  - tests/cmake/ssa-execir-mutation-boundary-check.cmake
  - tests/acceptance/ssa-execir-mutation-boundary.md
  - tests/parser/test_ssa_core_model.c
  - tests/core/test_ssa_function_aot_owner.c
  - tests/parser/test_ssa_source_returned_retention.c
doc_type: milestone-detail
status: planned
---

# 01.01 ExecIR 数据模型与模块边界

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 建立可被 core、parser、AOT/JIT 共同消费的正交 ExecIR，不再依赖 AST 或 ExecBC 解码。

**Architecture：** 低层模型与只读访问器位于 core；builder 和 pass 位于 parser。可变 operand、phi incoming、source/map 信息放紧凑 side arrays，函数内编号与模块 token 分离。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M1 foundation。
- 前置：[00.02 共享契约、语义边界与版本冻结](../00-measurement-contracts/02-contract-freeze.md)；[00.03 差分测试支架与里程碑门禁](../00-measurement-contracts/03-differential-harness.md)。
- 交付：Module/Function/Block/Instruction/Value/MemoryToken/EffectToken/FrameLayout/GcMap/DeoptState/SourceMap 类型与生命周期函数。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

semantic_ir.h 已有 place/loan/escape 与调用事实，应消费而非复制一套类型系统；现有 backend_aot_exec_ir.h 不应成为 core 的 include 依赖。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_parser/include/zr_vm_parser/semantic_ir.h` | 映射现有语义事实到执行模型 |
| 现有，修改/复用 | `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.h` | 列出旧 AOT 私有记录的迁移对应表 |
| 现有，修改/复用 | `zr_vm_core/CMakeLists.txt` | 保持 core 不依赖 parser |
| 计划新增 | `zr_vm_core/include/zr_vm_core/exec_ir.h` | 公共 ID、容器和只读查询 |
| 计划新增 | `zr_vm_core/include/zr_vm_core/exec_ir_opcode.def` | 正交语义操作及操作数/effect schema |
| 计划新增 | `zr_vm_core/src/zr_vm_core/exec_ir/exec_ir.c` | 内存生命周期、深复制和范围查询 |
| 计划新增 | `zr_vm_parser/include/zr_vm_parser/exec_ir_builder.h` | 构造 API |
| 计划新增测试 | `tests/parser/test_ssa_core_model.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 定义 ID 与容器** ValueId/BlockId/InstructionId 函数内唯一，ModuleId/TypeToken 稳定；INVALID 与 ENTRY 显式区分，所有容量增长先做乘加溢出检查。

- [ ] **2. 定义操作 schema** 把 arithmetic、place projection、load/store、call、alloc、barrier、drop、branch、invoke、throw、suspend 定义为正交操作；操作 schema 生成枚举和 operand 检查表。

- [ ] **3. 分离可持久化数据** 指令保存 binding row index 和 source/deopt index，不保存指针；运行期解析 TypeId、函数目标另表维护。指令数组允许构建期增长，发布后只读。

- [ ] **4. 提供生命周期与查询** Init/Free/Clone 清理所有 side arrays；跨函数复制重映射局部 ID，保持稳定 token。writer 逐字段写入，由 08.01 定义磁盘格式。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
Instruction = {opcode, resultRange, operandRange, typeToken, layoutId,
               memoryInRange, memoryOutRange, effectIn, effectOut,
               sourceId, deoptId, bindingRow, flags}
Value = {definitionId, typeToken, ownership, nullability}
Block = {instructionRange, predecessorRange, successorRange}
flags = mayAllocate | mayThrow | mayGc | maySuspend
Module = {functions, constants, contracts, layouts, sourceMaps}
RuntimeResolvedTables are not fields of serialized Module
```

模型中的 capability index 与 effect flags 不混用；memory token 的类型与普通值分离。CoreIR 名称在新接口中统一改称 ExecIR，不引入第三套语义操作集。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| 空模块、空函数与单 block | 生命周期和结构查询正确 |
| operand range 溢出或未知 opcode | 精确结构诊断 |
| clone 含 phi、binding 与 source map | 局部 ID 重映射，token/hash 保持 |
| 只链接 core 的读取程序 | 不需要 parser/AST/AOT 私有头 |

本任务新增测试先独立运行，再进入完整 SSA 差分矩阵。

登记新 CTest 名 `ssa_core_model` 和可执行目标 `zr_vm_ssa_core_model_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_core_model_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_core_model$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** 公共头可独立编译，所有记录有明确所有者和销毁方式；没有把 SZrInstruction 或 runtime pointer 当 IR 语义字段。

**失败恢复：** 新模型先不切换默认编译路径；模型冻结前只在测试使用，不发布产物。

**文档交付：** 新增 docs/instruction-generation/execir-model.md（`docs/instruction-generation/` 目录当前不存在，由本任务首次创建；后续 01.03、02.x、03.04、05.04、07.x、09.02 的交付文档均落在该目录）；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrExecIrModule SZrExecIrModule;
typedef struct SZrExecIrFunction SZrExecIrFunction;
typedef struct SZrExecIrBuildOptions SZrExecIrBuildOptions;
TZrBool ZrCore_ExecIr_CloneModule(const SZrExecIrModule *source,
    SZrExecIrModule *destination, SZrExecIrDiagnostic *diagnostic);
void ZrCore_ExecIr_FreeModule(SZrExecIrModule *module);
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| opcode schema | exec_ir_opcode.def | enum/arity/effect/type verifier 表同源 |
| operand/phi side arrays | parser builder | ID 与 range 永久有效或显式重编号 |
| module-owned arenas | core IR 生命周期 | 失败 clone 不修改 source，destination 可安全释放 |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 先定义 ID sentinel、vector/range 与类型引用形式，编译 core-only header probe。

- [ ] **批次 2：** 实现增长、深复制、删除与重编号 helper；分别注入每次 allocation failure。

- [ ] **批次 3：** 将现有 SemIR 与 AOT 私有类型映射成表，禁止新增重复 TypeLayout/Signature 实体。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange module with two functions using local ValueId=1 independently
act cloneModule()
assert function-scoped IDs remain unambiguous; tokens unchanged
arrange fail allocator at side-array allocation N
assert source unchanged; destination free is safe; no leaked allocation
arrange operandCount*elementSize overflows
assert reject before allocation
```

### 迁移结束检查

公共模型中不能留下 AST*、SZrFunction*、native pointer 或 SZrInstruction 原始语义依赖。把构建器私有 mutation helper 从公开 header 移到内部头，避免后端随意修改共享 IR。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。

## 已提交的有限 Function / returned-source 所有权前置

Core Function AOT owner 的 12 项生命周期证明、source constant 的 24 项、
descriptor OOM 的 16 项和 metadata heap 的 6 项，在当前依赖回放中共 58 项通过。
后继 `360a0aa0` 的真实返回入口 25 项证明显式 literal I64 保留的元数据在
CompilerState/AST 释放后有效，两个 owner 独立、实际释放一次，七项分配失败
清理后可重试。详见[验收记录](../../../acceptance/ssa-source-returned-retention.md)。

上述显式返回路径的 AOTIR owner 本身不保留 original/compacted ExecIR。后继独立
ExecIR 所有权已分别有限交付：

- `3496923f58c6f4be2bb61c82c51a7743bef9fba4`：通用独立 module owner，14 PASS；普通 source 与 old-view source 两组各 61 处故障，共 122 处，详见[验收记录](../../../acceptance/ssa-execir-owned-module.md)。
- `112cbf4256a819d554659c7bdfd9d77eb8a04610`：Function 独立 ExecIR 关联与生命周期，20 PASS；首次附加与替换各 61 处故障，详见[验收记录](../../../acceptance/ssa-function-exec-ir-owner.md)。
- `238fb35d160bad6142acee08da910742b0f1d2c9`：module copy 替换选定 Function，20 PASS；独立测量 module 60 与 replacement 28，共 88 处故障，详见[验收记录](../../../acceptance/exec-ir-module-copy.md)。

这些故障子观察属于各自有限 fixture 的 Unity 用例，不增加用例总数。普通默认
输出的接入及当前 Source finalized 返回链路仍须分别取得自己的验收证据。整叶
仍须完整核对所有记录及 side arrays 的所有者、Init/Free/Clone 和销毁路径，
验证跨函数局部 ID 重映射与稳定 token 保持。构建器 mutation 声明边界本批已收敛：
30 项声明（含十项 Reserve、两个可变访问器与 SetBindingRows）已移到 Core 私有
`exec_ir/exec_ir_mutation_internal.h`，见[有限验收](../../../../tests/acceptance/ssa-execir-mutation-boundary.md)。
GC-map 基数、operation-specific local-ID 映射和全模块故障覆盖仍须按实际 profile
取得专项证据；这些有限交付不勾选本计划任务或整叶退出门禁。

## 本批有限 API visibility checkpoint

公开头保留 model、生命周期、const/query 与 verify；Core/Parser/test 显式 include
私有头并仅配置 PRIVATE 目录，无公开头反向 include 或兼容别名。原签名、
ZR_CORE_API、实现与布局保持，C structs 本身仍可写，未授 shared DLL 符号隐藏信用。

实际 Windows header syntax 门禁为 RED r2 64 进程、GREEN r1 66 进程，均 W0；
RED 的两编译器各 30 个 mutation 公头探针全部编译成功，GREEN 的 public/private
正例全部成功，60 个负例均因所选符号未声明且恰一条主错误拒绝。两轮分别
195/196 个输入 pin 在各自 before/after 一致；每轮两个 discovery 不计门禁信用。
对应证据为 `reports/ssa-execir-mutation-boundary-01a0fe2b/` 下的
`red-header-r2-receipt.json`、`green-header-r1-receipt.json` 和
`header-validation-review.json`。永久 clang-cl CTest r4 已通过 32 个 syntax 模式，
核实 129 个输出、176 个实际头文件与 206 个 before/after pin，限此门禁 W0。
独立验收的 Source r3 为 820 TU、550 实际编译加 270 exact-cache、5 个 CTest
通过、55 条 warning；Lower r3 为 59 TU、55 实际编译加 4 缓存、4 个 CTest
通过，仅增量 W0。这两组包含实际 object、link 与受限元数据测试信用。
Caller r5 的 78 个实际配置 syntax 检查全部通过（Source 20、Lower 3、root 55），
873 个实际输入 before/after 一致，150 条 warning，不能称 W0。
新验收记录区分原始运行身份、两文件精确 EOF-comment 等价桥与共享 checkout
输入；候选提交仅纳入本批自有修改，不宣称 clean-HEAD 单独全量构建通过。
当前状态为有限边界已核验、可供发布，提交及清理由各自真实外部收据确认；
GC-map 基数、operation-specific 跨函数 local-ID 映射及完整 01.01/47 叶出口仍开放。
