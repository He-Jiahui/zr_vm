---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic_ir.c
  - zr_vm_core/src/zr_vm_core/type_layout.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic_ir.c
  - zr_vm_core/src/zr_vm_core/type_layout.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sroa.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_data_layout.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_materialization_map.c
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/parser/test_ssa_aggregate_soa.c
  - tests/parser/test_aot_c_value_semir_contracts.c
doc_type: milestone-detail
status: planned
---

# 05.04 Aggregate 标量替换与 AoS/SoA

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 自动实施 SROA、unboxing 和封闭数据的 AoS/SoA，使热循环具有可用的连续布局。

**Architecture：** 先局部 aggregate scalar replacement，再对封闭集合做全生命周期 layout transformation；每次变换生成 layout/debug/deopt materialization map。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M6；M7 向量化前置。
- 前置：[02.03 逃逸、生命周期与分配消除](../02-automatic-optimization/03-escape-ownership.md)；[05.02 Typed array、连续 Slice 与 Bounds](../05-data-layout/02-arrays-slices.md)。
- 交付：SROA、私有字段布局候选、AoS/SoA 成本模型及物化桥接。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

Rust sroa.rs 明确排除地址可观察等情况，可借鉴 eligibility 分析；ZR 有 inline struct writeback/ownership，需要额外保留这些状态。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_parser/src/zr_vm_parser/semantic_ir.c` | 消费 aggregate/place facts |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/type_layout.c` | 内部布局映射接缝 |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sroa.c` | 逐字段标量替换 |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_data_layout.c` | AoS/SoA 候选和成本模型 |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_materialization_map.c` | 逻辑对象重建计划 |
| 计划新增测试 | `tests/parser/test_ssa_aggregate_soa.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 做局部 SROA** 仅访问已知字段且无整体地址观察的局部 aggregate 展开成 SSA 字段；union、overlay、逃逸到未知函数先拒绝。未初始化字段有独立状态。

- [ ] **2. 选择 SoA 候选** 根据字段使用密度、loop trip、alias 和 profile 估算搬运/bridge 成本；整个容器生命周期可控才变换，不对公开 slice 自动打散 storage。

- [ ] **3. 维护对象语义** 字段 GC/ownership 信息随物理列布局迁移；debug/deopt/native 边界按逻辑 layout materialize，保留对象图 alias 与 identity。

- [ ] **4. 发布可解释证据** remark 说明布局前后和阻断原因；静态 locality 估算标 estimated，PMU cache miss 实测标 measured，不能混为一谈。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
eligibleAggregate = noAddressObservation && noUnknownEscape
                    && noPublicLayoutConstraint && reconstructibleIdentity
SROA(aggregate) -> perFieldSSA + initializedBits
SoA(collection) -> columnsByField only if closedLifetime && profitable
Materialize(logicalObject) -> reuseIdentityMap + rebuildFields
layoutTransformHash includes logicalType, physicalColumns, bridgeVersion
```

release batching 与 SROA 是独立决策，展开字段不能改变 drop 顺序。循环融合/SoA 不能违反跨元素别名依赖。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| 只读取两个字段的局部 struct | 拆成 SSA，无堆分配 |
| ref 字段地址被 native 保留 | 阻止变换 |
| public/反射/序列化对象 | 稳定逻辑表示不变 |
| deopt 重建两个指向同对象的引用 | identity 仍相同 |

复用回归入口：`tests/parser/test_aot_c_value_semir_contracts.c`。历史计数只作为核对线索，实施时重跑并记录实际总数。

登记新 CTest 名 `ssa_aggregate_soa` 和可执行目标 `zr_vm_ssa_aggregate_soa_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_aggregate_soa_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_aggregate_soa$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** 转换/拒绝各有准确 remark；真实 workload 计入转换和物化成本后才判收益。

**失败恢复：** 无法证明封闭性就保留 AoS/boxed；不要要求用户通过大量性能标注强行满足优化。

**文档交付：** 新增 docs/instruction-generation/aggregate-layout-transform.md；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrExecIrLayoutTransformPlan SZrExecIrLayoutTransformPlan;
TZrBool ZrParser_ExecIr_PlanAggregateLayout(const SZrExecIrFunction *function,
    SZrExecIrLayoutTransformPlan *plan, SZrExecIrDiagnostic *diagnostic);
TZrBool ZrParser_ExecIr_ApplyAggregateLayout(SZrExecIrFunction *function,
    const SZrExecIrLayoutTransformPlan *plan, SZrExecIrDiagnostic *diagnostic);
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| eligibility | escape/visibility/alias 分析 | SROA 与 SoA 各自满足条件，不互相推断 |
| physical columns/field SSA | layout pass | root/drop/debug map 随变换同步 |
| materialization plan | layout/deopt builder | 保持 identity/alias，计入转换成本 |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 先实现局部 SROA，只处理无 overlay/地址观察的明确 struct；测试初始化位。

- [ ] **批次 2：** 做封闭数组 AoS/SoA 计划但先不应用，输出 profile/bridge 成本估计。

- [ ] **批次 3：** 应用收益为正的候选，验证 deopt/异常/GC 和公开边界物化；结合 PMU 验证实际 locality。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange private array uses only one field in a long loop
assert candidate includes measured/estimated bridge cost
arrange public slice escapes array storage
assert SoA blocked
arrange scalarized object has two aliases on deopt
assert one materialized identity with both refs attached
```

### 迁移结束检查

不得把 estimated cache locality 标成实测 cache miss 降低。释放顺序按逻辑字段/ownership contract，不能直接照物理 SoA 列顺序析构。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。

