---
related_code:
  - zr_vm_core/src/zr_vm_core/task_frame_runtime.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_loan_liveness.c
  - zr_vm_core/src/zr_vm_core/exception.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/task_frame_runtime.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_loan_liveness.c
  - zr_vm_core/src/zr_vm_core/exception.c
  - zr_vm_core/include/zr_vm_core/exec_ir_state_map.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c
  - zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/parser/test_ssa_state_maps.c
  - tests/task/test_task_frame_runtime.c
  - tests/parser/test_semir_type_conflict_deopt.c
doc_type: milestone-detail
status: planned
---

# 01.04 Ownership、挂起与状态恢复映射

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 让异常、ownership cleanup、协程挂起与 deopt 共享可恢复的逻辑状态描述。

**Architecture：** 逻辑 frame state 引用 SSA value/constant/place 与 cleanup state；物理位置由各后端生成位置表。恢复在合法 safepoint 完成，不重复执行已提交的副作用。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M1 状态正确性；M2/M3/M4/M7 前置。
- 前置：[01.03 Memory、Effect、异常边与验证器](../01-execir-ssa/03-effects-verifier.md)。
- 交付：GC/EH/deopt/debug/suspend map schema 与状态重建接口。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

已有 task_frame_runtime 和 semantic_ir_loan_liveness；应保留现有 borrow/cleanup 规则，不能通过把所有临时值装箱来掩盖错误生命周期。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/task_frame_runtime.c` | 复用任务 frame 生命周期 |
| 现有，修改/复用 | `zr_vm_parser/src/zr_vm_parser/semantic_ir_loan_liveness.c` | 读取 loan 活跃范围 |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/exception.c` | 映射异常清理/恢复边界 |
| 计划新增 | `zr_vm_core/include/zr_vm_core/exec_ir_state_map.h` | 逻辑 frame、cleanup、roots 与 resume 描述 |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_state_maps.c` | 从活跃值与 effect 提交点生成 map |
| 计划新增 | `zr_vm_core/src/zr_vm_core/exec_ir/exec_ir_materialize.c` | 恢复解释状态与必要对象物化 |
| 计划新增测试 | `tests/parser/test_ssa_state_maps.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 编号可恢复点** 对 call/GC/throw/await/debug poll/guard exit 生成 sourceId 和 resumeId；区分指令前、结果提交后、cleanup 完成后。

- [ ] **2. 记录逻辑状态** 保存活跃值、inline struct field map、owner initialized/moved/dropped 状态、异常与 handler；root map 只列真正活跃托管引用及 derived base。

- [ ] **3. 拆开逻辑与物理映射** ExecBC slot、C spill、LLVM stack map、JIT register/spill 分别映射同一逻辑值，禁止 writer 写实际栈地址。

- [ ] **4. 实现恢复原子性** 先准备物化对象与目标 frame，失败保留原状态；完成后一次切换 resume token，确保 drop/writeback/native I/O 不重放。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
ResumeState = {functionToken, generation, sourceId, resumeId,
               values[], places[], cleanupStates[], exceptionState}
ValueLocation = Constant | FrameOffset | RegisterSpill | MaterializeAggregate
onGuardFailure:
    validateResumePoint()
    prepareRootsAndMaterialization()
    reconstructBaselineFrame()
    commitFrameSwitchOnce()
    resumeAfterLastCommittedEffect()
```

借用/ref-like 不能存入跨 await/worker 的状态；GC 移动后 derived pointers 由 base+offset 重算。恢复中分配仍需临时 root map，不能先丢旧 roots。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| getter 有副作用后 deopt | getter 只调用一次 |
| await 前活跃 borrowed frame alias | 编译拒绝 |
| setter 写回时异常或 GC | 原有提交顺序与 cleanup 保持 |
| 嵌套 inline frame 恢复 | 调用栈、source map、所有者 drop 次数一致 |

复用回归入口：`tests/task/test_task_frame_runtime.c`、`tests/parser/test_semir_type_conflict_deopt.c`。历史计数只作为核对线索，实施时重跑并记录实际总数。

登记新 CTest 名 `ssa_state_maps` 和可执行目标 `zr_vm_ssa_state_maps_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_state_maps_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_state_maps$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** 所有可恢复点有完整状态和可见效果分界；GC/EH/deopt/debug 不分别维护冲突的 frame 真相。

**失败恢复：** 缺少 state map 的候选优化不启用；状态重建失败返回确定错误并保留可清理 frame。

**文档交付：** 新增 docs/core-runtime/execution-state-maps.md；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrExecIrStateMap SZrExecIrStateMap;
typedef struct SZrExecIrResumeRequest SZrExecIrResumeRequest;
TZrBool ZrParser_ExecIr_BuildStateMaps(SZrExecIrFunction *function,
    SZrExecIrDiagnostic *diagnostic);
TZrBool ZrCore_ExecIr_MaterializeState(const SZrExecIrResumeRequest *request,
    SZrExecIrDiagnostic *diagnostic);
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| logical live values/cleanup bits | SSA liveness + ownership | 物理 frame/codegen 映射不得遗漏 |
| resume phase/effect checkpoint | lowering 在提交边界生成 | deopt 不重放已完成外部效果 |
| materialization identity map | runtime 恢复事务 | 多个 alias 物化为同一对象 |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 先记录不优化的逻辑 frame，并用 oracle 恢复到相同状态；冻结 before/after-effect 的编号规则。

- [ ] **批次 2：** 加入 scalarized aggregate、inline frames 和 cleanup 初始化位；对每类边界注入 GC/异常。

- [ ] **批次 3：** 实现 prepare/commit 的 frame switch，测试 OOM 发生在物化第 N 个对象时可安全清理。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange getter increments counter, guard fails after getter
act materializeAndResume()
assert counter=1, never 2
arrange two logical refs alias one eliminated aggregate
assert materialized refs have same identity
arrange OOM during materialization
assert original roots remain valid until cleanup or retry
```

### 迁移结束检查

deopt、debug、GC、EH 只能拥有自己的物理位置投影，不能分别维护四套相互冲突的 logical frame。明确释放原 frame 的最后一步发生在新 roots 已发布之后。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。

