---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic_ir.c
  - zr_vm_core/include/zr_vm_core/type_layout.h
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic_ir.c
  - zr_vm_core/include/zr_vm_core/type_layout.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_alias.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_ranges.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_gvn.c
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/parser/test_ssa_gvn_range.c
doc_type: milestone-detail
status: planned
---

# 02.02 别名分析、GVN 与范围证明

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 自动完成别名、null/type/shape/layout/range 证明，并据此执行 GVN/CSE 和消除重复检查。

**Architecture：** 以 memory region/version、effect 序列和 dominance 作为等价约束；范围事实是边敏感的抽象解释结果，guard 事实带 generation 与失效点。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M6。
- 前置：[02.01 Pass 管理、SCCP、复制传播与 DCE](../02-automatic-optimization/01-pass-manager-scalar.md)。
- 交付：别名与范围分析、GVN、null/bounds guard 消除和阻断原因。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

已有 place 与 bounds 事实应转为 ExecIR facts；类型相同不能证明指向不同对象，readonly 也不自动排除其他别名写入。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_parser/src/zr_vm_parser/semantic_ir.c` | 复用 place/bounds 的定义与查询 |
| 现有，修改/复用 | `zr_vm_core/include/zr_vm_core/type_layout.h` | 读取布局合同 |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_alias.c` | base/projection/escape 建立 alias 分类 |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_ranges.c` | 区间、空值、类型与边事实 |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_gvn.c` | 按操作及 memory 版本做编号 |
| 计划新增测试 | `tests/parser/test_ssa_gvn_range.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 建立别名格** disjoint/may-alias/must-alias，字段不重叠仅在 layout 和 base 身份已证明时成立；未知 FFI 写入使相关 facts 失效。

- [ ] **2. 计算范围和空值** 沿条件分支缩小区间，循环使用 widening 后 narrowing；index 证明同时涵盖非负、length 和整数溢出，不能只证明 index < length。

- [ ] **3. 实现 GVN** 纯值 key 为 opcode/type/flags/operands；load 额外包含 alias region 与 memory token。只有支配的等价定义可以复用，可能抛错的操作不能跨异常观察点。

- [ ] **4. 消除检查并保留 guard** 每次删除写 remark 带 proof ID；layout/shape 事实跨 call、await、reload 是否有效由效果和 generation 证明决定。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
loadKey = (base, projection, type, layoutHash, memoryVersion)
reuseLoad only if dominates(existing, use) and sameExceptionRegion
boundsProven = 0 <= min(index) and max(index) < min(length)
               and noOverflow(indexArithmetic)
unknownWrite(region) -> invalidateAliasingLoadsAndFacts(region)
shapeFact = {typeId, shapeId, generation, validityRegion}
```

浮点相等、NaN 与 ±0 不适合随意代数重写。Meta getter/自定义索引不是普通 load；未证明纯和稳定时不得 CSE。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| 两次字段读取之间 alias store | 第二次不能复用 |
| 负 index、溢出 induction、可变 length | 保留 bounds check |
| 重复纯计算位于支配块 | GVN 消除且结果一致 |
| getter 有 I/O、call 改变 shape | 不合并 getter，旧 shape proof 失效 |

本任务新增测试先独立运行，再进入完整 SSA 差分矩阵。

登记新 CTest 名 `ssa_gvn_range` 和可执行目标 `zr_vm_ssa_gvn_range_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_gvn_range_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_gvn_range$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** 每种消除带可追溯 proof，guard 删除无异常顺序变化；alias 不确定时保守正确。

**失败恢复：** 分析无结论只保留原检查；如果证明链损坏 verifier/差分必须失败，不能恢复为乐观默认值。

**文档交付：** 新增 docs/instruction-generation/execir-alias-range-analysis.md；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrExecIrAnalysisContext SZrExecIrAnalysisContext;
TZrBool ZrParser_ExecIr_AnalyzeAliasAndRanges(SZrExecIrFunction *function,
    SZrExecIrAnalysisContext *analysis, SZrExecIrDiagnostic *diagnostic);
TZrBool ZrParser_ExecIr_RunGvn(SZrExecIrFunction *function,
    SZrExecIrAnalysisContext *analysis, SZrExecIrDiagnostic *diagnostic);
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| base/projection alias sets | place/escape analysis | GVN key 依赖区域与版本，不以 offset 单独判等 |
| edge range facts | 条件/归纳范围分析 | guard 删除限定在支配和 validity region 内 |
| proof witness | 每次 null/bounds/shape 消除 | remarks/verifier 能定位证明来源和失效点 |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 先实现保守 alias query，测试 identical/disjoint/unknown，不做任何消除。

- [ ] **批次 2：** 加 range/null/shape 数据流与 widening，用整数边界反例固定精度与安全性。

- [ ] **批次 3：** 启用 GVN 和 check elimination，分别对未知 call、GC、reload、await 清除对应事实。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange p and q may alias; load p.x; store q.x; load p.x
assert second load is not reused
arrange index=-1 and index < length
assert bounds check not removed
arrange pure dominated add repeated twice
assert one value number and identical semantic events
```

### 迁移结束检查

关闭 GVN 不应影响 alias/range 的其他消费者；分析结果必须有 revision 失效协议，不能让 CSE 删除了检查后仍使用旧 proof。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。

