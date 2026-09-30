---
related_code:
  - zr_vm_parser/include/zr_vm_parser/exec_ir_execbc_vm.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm_validate.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm_place_uses.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm_phi_validate.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projection_common.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_instruction.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.h
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_instruction.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
  - zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.h
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_execbc.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_aot.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm_validate.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm_place_uses.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm_phi_validate.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm_internal.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_projection_common.c
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/parser/test_ssa_execbc_vm.c
  - tests/parser/test_ssa_execbc_vm_fixtures.inc
  - tests/parser/test_ssa_execbc_vm_trace.inc
  - tests/parser/test_ssa_execbc_vm_cfg_mutations.inc
  - tests/parser/test_ssa_execbc_vm_dead_place.c
  - tests/parser/test_ssa_execbc_vm_dead_place_support.inc
  - docs/core-runtime/execbc-vm-dead-place.md
  - tests/acceptance/ssa-execbc-vm-materialization.md
  - tests/parser/test_ssa_oracle_projections.c
  - tests/parser/test_semir_pipeline.c
  - tests/parser/test_aot_c_value_semir_contracts.c
doc_type: milestone-detail
status: in-progress
---

# 01.05 直接解释 Oracle 与无优化后端投影

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 建立 ExecIR 直接解释的语义 oracle，以及不依赖优化的 ExecBC/AOTIR 初始投影。

**Architecture：** oracle 逐条执行正交 IR 并复用共享语义 helper。ExecBC projection 可独立进入真实 Core VM 函数 materializer；当前只支持显式列出的无调用标量/控制子集，默认编译路径尚未切换。投影阶段只选表示与物理位置，AOTIR 模型在 07.01 完善，避免后端从 quickening 反推语义。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M1 foundation；完整四后端验收等待 07.02。
- 前置：[01.04 Ownership、挂起与状态恢复映射](../01-execir-ssa/04-state-maps.md)。
- 交付：最小标量/控制/调用 oracle、ExecBC projection、AOTIR projection 接缝与差分 fixture。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

现有编译器可直接发 ExecBC，backend_aot 有自己的 execution IR；迁移应以同一函数粒度切换，不混合部分新旧 effect 语义。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_parser/src/zr_vm_parser/compiler/compiler_instruction.c` | 接入新 ExecBC 生成结果 |
| 现有，修改/复用 | `zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c` | 旧路径仅作过渡基线 |
| 现有，修改/复用 | `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_exec_ir.h` | 收敛 adapter 输入 |
| 计划新增 | `zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_interpreter.c` | 直接 IR oracle |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_execbc.c` | 无优化映射与 phi parallel-copy |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_lower_aot.c` | 保留 SSA/effect 的 AOTIR 投影入口 |
| 新增，首个受限消费者实现待验证 | `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_execbc_vm.c`、`exec_ir_execbc_vm_validate.c`、`exec_ir_execbc_vm_phi_validate.c` | 先验证 raw phi assignments 与每条 edge 的 scheduled moves 等价，再把无 CALL、无 packed frame 的 i64 scalar/control projection 发射为真实 `SZrFunction` 指令；不使用 oracle 或投影 runner 作为 VM |
| 新增 API | `zr_vm_parser/include/zr_vm_parser/exec_ir_execbc_vm.h` | 暴露 Core function emission、PC map 与明确的 GC 根生命周期 |
| 计划新增测试 | `tests/parser/test_ssa_oracle_projections.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |
| 新增测试 | `tests/parser/test_ssa_execbc_vm.c` | 在 Core VM 执行真实发射函数，并与 OracleEx 结果及 PC-map 控制流对照。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 实现 oracle 最小闭环** 从 constant/arithmetic/phi/branch/call/return 开始，后续按 schema 逐项补 load/store/exception/cleanup/suspend；未覆盖 opcode 明确 unsupported。

- [ ] **2. 实现 phi 消除** 在边上生成 parallel-copy，critical edge 先拆分；循环 swap 使用临时 slot，不顺序覆盖输入。

- [ ] **3. 对齐后端接口** AOTIR 接缝保留 token/type/source/state map，adapter 由 07.01 接管；任何功能未落地时不假装 C/LLVM 可执行。

- [ ] **4. 按函数切流** 新路径验证、差分与 codegen 完整后切换该函数；旧 compiler_semir 后投影的消费者逐一迁移，最终删除被替代路径。
- [ ] **5. 建立真实 VM 函数 materializer** 只接受 i64 constant/ADD/SUB/compare、bool branch、phi edge copies 与 i64 return；Unsupported 和 malformed projection 在 Core function 分配前结构化失败，绝不隐式回到旧编译器。待 focused MSVC/GCC target 验证后记录此子集的实测结果。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
sourceFacts -> BuildExecIR -> Verify
                         -> OracleRun
                         -> LowerExecBC(optimization=off)
                         -> LowerAOTIR(optimization=off)
phiEdgeCopies = simultaneousAssignments(edge)
if copiesContainCycle: useFreshTemporarySlot()
assert(compareSemanticEvents(OracleRun, ExecBCRun))
C/LLVM parity is enabled only after 07.02 produces runnable code
```

oracle 不是第三套独立语言实现：数值、native、ownership、异常原语复用 core helper。直接 IR interpreter 的速度不作为生产性能目标。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| phi a=b,b=a 的循环边 | parallel-copy 正确 |
| 标量、call、异常、drop 的同源执行 | 事件轨迹与结果一致 |
| 未实现 opcode | 明确 unsupported，而不是 skip/no-op |
| 关闭 quickening 后 AOTIR 构建 | 仍可从 ExecIR 完整投影 |
| CFG successor 缺少对应 target predecessor occurrence | materializer 结构化拒绝，输出保持为空 |

复用回归入口：`tests/parser/test_semir_pipeline.c`、`tests/parser/test_aot_c_value_semir_contracts.c`。历史计数只作为核对线索，实施时重跑并记录实际总数。

新增真实 VM 对照入口 `ssa_exec_ir_execbc_vm` 先限定无参 i64 scalar/control 子集。每个成功 fixture 先运行 `ZrCore_ExecIr_RunOracleEx`，再 lower projection、materialize 为 `SZrFunction`，最后用 Core runtime harness 执行该 function；`RunOracleEx` 或 `ZrParser_ExecBcProjection_Run` 不能充当 VM。正向覆盖 slot zero、稀疏物理槽、compare 两条 phi 路径、critical edge、loop swap temporary、非首 entry block 的 parallel CFG edges，以及 pool 中的 i64/bool constants；CALL、非 i64 值和损坏 opcode 必须结构化拒绝且输出为空。

Materializer validation also derives raw phi copies from the block phi incoming
rows and checks each scheduled move sequence symbolically, including temporary
slot cycles. Nonzero instruction deopt IDs are unsupported. Synthetic split
blocks must remain empty and terminator-free because the emitter supplies their
edge moves and jump. CFG validation checks reverse adjacency occurrence
multiplicity in both directions, retaining legal parallel edges while rejecting
a source successor omitted from the target predecessor row. Negative tests
cover missing/wrong phi moves, a missing reverse CFG predecessor, and malformed
synthetic instruction ownership; all failures leave the emission output empty.

The focused MSVC evidence on 2026-09-30 is a 13/13-step incremental build and
5/5 selected CTests; `ssa_exec_ir_execbc_vm` passed all 16 Unity cases. The
non-first-entry parallel-edge trace includes synthetic block 3 (`{2, 3, 1}`),
and `test_phi_rejects_cfg_edge_missing_from_target_predecessors` passes. Exact
commands and logs are recorded in
`tests/acceptance/ssa-execbc-vm-materialization.md`.

This focused target uses directly constructed ExecIR fixtures. The source
branch fixture has separate producer-proof and canonical-type acceptance in
`tests/acceptance/ssa-source-execbc-vm.md`; it does not cover source loop phis or
general effects. Oracle, ExecBC, AOTIR, C and LLVM full-semantic parity still
gates M1, which remains open.

The separately scoped dead-place extension allows only metadata-free
`PLACE_BASE` rows whose results have no uses, lowering each row to a Core NOP
with its source/PC mapping. External provenance is accepted only when all its
uses are those dead bases and it has no instruction or phi-result definition.
Embedded or advertised GC maps, state/deopt maps, memory/effect metadata, live
place values, source promotion and `LOAD`/`STORE` remain unsupported. The new
independent hand-ExecIR VM/Oracle target passed all 13 cases in root's current
MSVC CTest run, alongside the 18-case scalar VM target. Cross-toolchain evidence
is recorded separately; this extension does not complete the M1 gate.

登记新 CTest 名 `ssa_oracle_projections` 和可执行目标 `zr_vm_ssa_oracle_projections_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_oracle_projections_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_oracle_projections$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** foundation 完成后可推进消费者；M1 全覆盖必须最终完成 oracle/ExecBC/C/LLVM 全语义矩阵，不能只用标量 demo 关闭 M1。

**失败恢复：** 迁移期间保留显式开发选项选择旧编译器；不能让错误的新 IR 隐式退回旧路径从而掩盖失败。

**文档交付：** 更新 docs/wiki 的编译流水线，列出每个操作的投影覆盖状态；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrExecIrExecutionInput SZrExecIrExecutionInput;
typedef struct SZrExecBcProjection SZrExecBcProjection;
typedef struct SZrAotIrModule SZrAotIrModule;
TZrBool ZrCore_ExecIr_RunOracle(const SZrExecIrExecutionInput *input,
    SZrExecIrDiagnostic *diagnostic);
TZrBool ZrParser_ExecIr_LowerExecBc(const SZrExecIrModule *module,
    SZrExecBcProjection *output, SZrExecIrDiagnostic *diagnostic);
TZrBool ZrParser_ExecIr_LowerAot(const SZrExecIrModule *module,
    SZrAotIrModule *output, SZrExecIrDiagnostic *diagnostic);
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| 正交操作语义 | core semantic helpers | oracle 与后端不能各自另写不同整数/异常规则 |
| phi parallel-copy plan | ExecBC lowering | critical edge 上执行，同时赋值不互相覆盖 |
| 投影 coverage | 每个 opcode lowering handler | 未实现项始终显式 unsupported |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 实现最小无优化 oracle 和标量/control ExecBC，先让 CFG/phi 差分可运行。

- [ ] **批次 2：** 按 effects/ownership/exception/suspend 补齐操作族，并在状态点比较完整事件。

- [ ] **批次 3：** 接 AOTIR adapter；等 07.02 真正运行两后端再关闭 M1 全语义门禁，旧路径删除单列审查。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange phi parallel copies {a<-b,b<-a} with a=3,b=5
assert resulting a=5,b=3
arrange AOTIR build with ExecBC emission disabled
assert valid typed AOTIR generated
arrange unknown opcode
assert oracle returns UNSUPPORTED and no successful observation
```

首个 materializer API 为 `ZrParser_ExecBcProjection_MaterializeVmFunction`，输出含 Core GC-managed function 和 native-owned PC map。成功返回后调用方必须在任何 GC 操作前立即根化 function；emission free 只释放 PC map，成功 function 由 state 销毁。Emitter 构造期间临时保活；失败时释放 partial function、撤销本次新增根并保持输出清零。Slot 0 映射为 VM `frameBase[0]`，frame 长度覆盖 sparse holes、projection temporary slots 和 `phiTemporarySlot + 1`；本子集不使用 packed frame。

ExecBC projection 结构没有 binding-row schema 或 contract 字段。ExecIR-to-ExecBC 上游 builder 在产生 projection 前校验源函数 schema 为 legacy；materializer 依据 projection 实际具有的 `runnable`、`bindingRow`、packed-frame metadata、数组与 ranges 校验输入，不假设不存在的 schema 字段。手工构造的 projection 不应被当作绕过上游 schema 检查的入口。

当 `temporarySlotCount` 为零时，`phiTemporarySlot` 不表示有效 slot，即使其零初始化值与物理 slot 0 相同；只有确有 phi cycle temporary 时才将该字段按临时 slot 验证。Focused VM fixture 覆盖普通 phi copy 从物理 slot 0 读取且无需临时 slot 的情况。

### 迁移结束检查

Oracle 只作为测试/调试 reference mode 暴露，默认生产运行模式明确为 ExecBC/AOT。对外报告不得把“IR 可打印”或“AOTIR 已生成”写成四后端执行一致。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。
