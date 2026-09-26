---
related_code:
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_internal.h
  - zr_vm_core/src/zr_vm_core/execution/execution_budget.c
  - zr_vm_common/include/zr_vm_common/zr_instruction_conf.h
implementation_files:
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_core/src/zr_vm_core/execution/execution_internal.h
  - zr_vm_core/src/zr_vm_core/execution/execution_budget.c
  - zr_vm_common/include/zr_vm_common/zr_instruction_conf.h
  - zr_vm_core/src/zr_vm_core/execution/execution_context.h
  - zr_vm_core/src/zr_vm_core/execution/execution_safepoint.c
  - zr_vm_core/src/zr_vm_core/execution/execution_cold.c
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/core/test_ssa_dispatch_boundaries.c
  - tests/core/test_execution_dispatch_callable_metadata.c
  - tests/core/test_precall_frame_slot_reset.c
doc_type: milestone-detail
status: planned
---

# 03.01 Dispatch 拆分、状态缓存与冷路径

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 降低解释器 dispatch 的固定成本，同时明确所有可能使局部缓存失效的 VM 边界。

**Architecture：** dispatch 保留热循环和 computed-goto/switch；按 numeric/control/call/member/safepoint/cold error 拆职责，通过显式 execution context 同步状态。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M3。
- 前置：[01.05 直接解释 Oracle 与无优化后端投影](../01-execir-ssa/05-oracle-projections.md)；[00.01 基线、性能指标与真实执行模式](../00-measurement-contracts/01-baseline-metrics.md)。
- 交付：可独立审查的 dispatch 模块拆分、状态 publish/reload 协议和同口径对照。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

execution_dispatch.c 和 compiler_quickening.c 已是巨型文件；拆分应先保持生成行为一致，再减少状态读写，不能一边搬文件一边无证据重写热点。注意 `zr_vm_core/src/zr_vm_core/execution/` 下已存在按责任拆分的文件：execution_numeric.c、execution_control.c、execution_member_access.c、execution_object.c、execution_memory.c、execution_meta_access.c、execution_tail_call.c、execution_budget.c 与 execution_internal.h——本任务的迁移目标是这些既有拆分文件，不新建平行结构；`pollExecutionBudgetAtBoundedBackedges` 草案对接既有 execution_budget.c 协议，不另起一套预算轮询。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c` | 保留主 dispatch 与 handler glue |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/execution/execution_internal.h` | 既有内部共享声明；局部状态字段并入而非重复定义 |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/execution/execution_numeric.c` 等既有 execution_*.c | 各 handler family 的迁移落点（numeric/control/member/object/memory/meta/tail） |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/execution/execution_budget.c` | 执行预算 poll 的唯一所有者 |
| 现有，修改/复用 | `zr_vm_common/include/zr_vm_common/zr_instruction_conf.h` | 统一 opcode 枚举/dispatch 数据源 |
| 计划新增 | `zr_vm_core/src/zr_vm_core/execution/execution_context.h` | pc/base/top/domain/profile/generation/layout 局部状态 |
| 计划新增 | `zr_vm_core/src/zr_vm_core/execution/execution_safepoint.c` | 统一发布与重载边界 |
| 计划新增 | `zr_vm_core/src/zr_vm_core/execution/execution_cold.c` | 错误构造、动态 meta、异常慢路径 |
| 计划新增测试 | `tests/core/test_ssa_dispatch_boundaries.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 无行为拆分** 用现有 opcode 分组把剩余 handler 迁入既有 execution_*.c family 文件，保持局部热 handler 可内联；检查汇编/代码尺寸，不能把每条指令变成额外 C 函数调用。

- [ ] **2. 定义局部缓存** 缓存 pc/frame base/top/domain/profile/generation/layout；所有字段定义唯一 authoritative owner，调试读取前必须同步。

- [ ] **3. 集中边界协议** call、GC、throw、native re-entry、debug、suspend 前发布 roots/pc/top；返回后重新读取可能移动的 frame/stack，禁止沿用悬空 base。

- [ ] **4. 比较 dispatch 形式** GNU/Clang computed-goto、MSVC switch 来自同一 opcode 表；错误处理冷分离，branch layout 由测量决定，不默认复制所有 hot path。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
while running:
    i = fetch(local.pc)
    if handlerMayCallOrSafepoint(i):
        publish(state, local)
        executeBoundaryOperation()
        local = reload(state)
    else:
        executeLeafHandler(local, i)
    pollExecutionBudgetAtBoundedBackedges()
# publish/reload is mandatory around native callbacks and debug entry
```

leaf handler 必须有不分配、不抛出、不重入、不 suspend 的可验证约束；任一不满足即走边界协议。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| 深递归导致 stack 扩容 | reload 后 base 正确 |
| native callback 中 GC/重入 | roots 和 pc 可用 |
| debug breakpoint 后恢复 | 源位置和 frame 一致 |
| GNU computed-goto 与 MSVC switch | 同一 opcode 覆盖和行为 |

复用回归入口：`tests/core/test_execution_dispatch_callable_metadata.c`、`tests/core/test_precall_frame_slot_reset.c`。历史计数只作为核对线索，实施时重跑并记录实际总数。

登记新 CTest 名 `ssa_dispatch_boundaries` 和可执行目标 `zr_vm_ssa_dispatch_boundaries_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_dispatch_boundaries_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_dispatch_boundaries$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** 拆分先通过全 opcode 行为差分，再评价局部缓存收益；代表集无稳定超 3% 非预期退化。

**失败恢复：** 保持单一生成源，可单独关闭状态缓存优化；不能通过丢 safepoint 或错误信息提高 benchmark。

**文档交付：** 新增 docs/core-runtime/interpreter-state-boundaries.md；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrExecutionLocalContext SZrExecutionLocalContext;
void ZrCore_Execution_PublishBoundary(struct SZrState *state,
    const SZrExecutionLocalContext *context);
void ZrCore_Execution_ReloadBoundary(struct SZrState *state,
    SZrExecutionLocalContext *context);
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| local pc/base/top | dispatch 当前循环 | 只有 leaf handler 可无同步修改 |
| root-visible state | publish helper | GC/native/debug/scheduler 只读已发布状态 |
| reload epoch/layout | 边界返回后的 state | 旧 base/profile/layout 指针不可继续使用 |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 纯职责迁移：每批只搬一种 handler family，比较 opcode 表与回归，不同时改语义。

- [ ] **批次 2：** 集中 publish/reload，给边界 helper 标注可分配/抛错/重入属性，替换散落同步代码。

- [ ] **批次 3：** 再缓存局部状态与冷分离；查看目标汇编和 code size，按 representative workloads 测量。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange native callback forces stack growth and moving GC
assert post-callback base differs if moved and next load uses new base
arrange debug observes PC at throwing instruction
assert reported source matches original opcode
arrange leaf handler unexpectedly allocates in instrumented build
assert boundary contract violation detected with reason code BOUNDARY_LEAF_ALLOCATED and offending opcode/pc
```

### 迁移结束检查

任何 C helper 新增 mayGc/mayThrow 行为时都需更新 opcode boundary metadata；不要靠开发者记得在所有调用点手写同步。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。

