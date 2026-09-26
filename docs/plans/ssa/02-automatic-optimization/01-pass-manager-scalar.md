---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_dce.c
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/parser/test_ssa_pass_manager_scalar.c
doc_type: milestone-detail
status: planned
---

# 02.01 Pass 管理、SCCP、复制传播与 DCE

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 建立确定的 pass pipeline，并先实现容易验证的常量传播、SCCP、copy propagation 与 DCE。

**Architecture：** pass manager 声明分析依赖和失效集合，变换前后运行 verifier。标量变换以严格语义 evaluator 为唯一常量计算入口。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M6；基础优化可在 M1 foundation 后独立推进。
- 前置：[01.05 直接解释 Oracle 与无优化后端投影](../01-execir-ssa/05-oracle-projections.md)。
- 交付：pass registry、分析缓存/失效协议、标量 passes 和优化事件接口。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

compiler_quickening.c 包含大量后期 opcode 选择，不能继续承载 SSA 数据流。新增 exec_ir/passes 目录独立实现，保留 quickening 为 ExecBC 投影消费者。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c` | 只保留消费已证明事实的适配入口 |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_pass_manager.c` | pass 次序、验证、预算、分析失效 |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_sccp.c` | 可执行边与常量格 |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_dce.c` | 保留可观察行为的删除/复制传播 |
| 计划新增测试 | `tests/parser/test_ssa_pass_manager_scalar.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 定义 pass contract** 登记 requires/preserves/invalidates 与 changed 标记；改变 CFG 必须失效 dominators/loops/liveness。记录每 pass 时间和 IR hash，支持单 pass 关闭进行二分。

- [ ] **2. 实现 SCCP** 值格使用 unknown/constant/overdefined，边有 executable 状态；phi 只合并可执行前驱。整数溢出、除零、NaN、负零按语言规则计算。

- [ ] **3. 实现复制传播与 DCE** 替换等价 ValueId 后维护 use-list；无使用不等于无效果，保留 call、alloc 的可观察失败、drop、throw、barrier、await 和 volatile/native 读取。

- [ ] **4. 控制迭代与 remark** 工作队列收敛，设 compile-time budget；发布成功/阻断事件给 11.02，预算耗尽输出有效但较少优化的 IR。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
for pass in selectedPipeline:
    Verify(ir)
    ensureAnalyses(pass.requires)
    result = pass.run(ir, compileBudget)
    invalidate(result.changedDomains - pass.preserves)
    Verify(ir)
    emitRemark(pass, result)
DCE.erase(i) only if noUses(i) and provenUnobservable(i)
SCCP.phi = join(values from executableIncomingEdges)
```

代码改动失败与预算耗尽不是同一状态：前者停止编译并报告，后者保留最后有效 IR。默认严格数值，不允许用宿主 C 未定义溢出计算常量。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| 菱形中不可执行分支除零 | 不可误触发异常 |
| 无使用但会抛异常的调用 | 仍执行 |
| 负零、NaN、整型边界常量 | 和 oracle 一致 |
| 重复跑 pipeline | 达到固定点且 hash 稳定 |

本任务新增测试先独立运行，再进入完整 SSA 差分矩阵。

登记新 CTest 名 `ssa_pass_manager_scalar` 和可执行目标 `zr_vm_ssa_pass_manager_scalar_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_pass_manager_scalar_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_pass_manager_scalar$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** 每个 pass 独立差分与 verifier 通过；不添加函数级标注；编译预算可控。

**失败恢复：** 关闭单 pass 后仍能生成可执行 IR；保留最小失败 fixture，不能移除负测规避问题。

**文档交付：** 新增 docs/instruction-generation/execir-pass-pipeline.md；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrExecIrOptimizeOptions SZrExecIrOptimizeOptions;
typedef struct SZrExecIrOptimizationResult SZrExecIrOptimizationResult;
TZrBool ZrParser_ExecIr_Optimize(SZrExecIrModule *module,
    const SZrExecIrOptimizeOptions *options, SZrExecIrOptimizationResult *result);
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| analysis revision | pass manager 在 CFG/value/effect 变更时递增 | 分析缓存按 revision 校验，不能只靠 changed bool |
| SCCP lattice | worklist evaluator | 仅 executable edges 贡献 phi |
| pass budget/remark | profile 配置与执行统计 | 超预算保留有效 IR，失败停止 codegen |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 实现空 pass pipeline、依赖登记、前后 verifier 和失败快照；注入一个故意损坏 IR 的测试 pass。

- [ ] **批次 2：** 实现 SCCP/copy propagation，使用显式数值 evaluator；测试不可达异常和常量格循环收敛。

- [ ] **批次 3：** 实现 DCE 与 bounded fixed-point；删除 use-list、source map、state map 的孤儿引用，保留可观察分配和 drop。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange unused call that appends event E
act DCE
assert call and event E remain
arrange unknown -> constant -> overdefined propagation cycle
assert convergence and no stale analysis reuse
arrange pass exceeds budget after valid transform
assert last valid IR retained and budget remark emitted
```

### 迁移结束检查

优化 passes 不读取 bytecode PC 或 cache target pointer；pass 开关只影响优化强度，不能决定语义是否合法。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。

