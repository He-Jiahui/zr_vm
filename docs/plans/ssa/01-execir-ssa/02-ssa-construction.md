---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semir.c
  - zr_vm_parser/include/zr_vm_parser/compiler.h
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_semir.c
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_cfg.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa.c
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/parser/test_ssa_construction.c
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_place_cfg_graph.c
doc_type: milestone-detail
status: planned
---

# 01.02 Canonical facts lowering、CFG 与 SSA 构造

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 从前置语义 facts 构造完整 CFG 和真正的值 SSA，包括循环和异常前驱。

**Architecture：** builder 消费 canonical Type/Place/Ownership/Effect facts；可提升局部使用 dominator/frontier 插 phi 并重命名，地址被取走的局部保留显式 place load/store。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M1 foundation。
- 前置：[01.01 ExecIR 数据模型与模块边界](../01-execir-ssa/01-core-model.md)。
- 交付：SemIR→ExecIR builder、CFG/dominator/phi 构造及源位置映射。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

compiler.h 存在 preSemanticIr；compiler_semir.c 是 ExecBC 后投影，不能拿后投影作为统一 IR 的输入。应审计 compiler_semantic_ir.c 缺失的前置事实，再补其生产方。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_parser/src/zr_vm_parser/compiler/compiler_semantic_ir.c` | 补 canonical lowering 所需事实 |
| 现有，修改/复用 | `zr_vm_parser/src/zr_vm_parser/compiler/compiler_semir.c` | 逐步限制为旧路径兼容诊断 |
| 现有，修改/复用 | `zr_vm_parser/include/zr_vm_parser/compiler.h` | 加入 ExecIR 构建状态，不绑定字节码 PC |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_build.c` | 从前置 facts 构造操作 |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_cfg.c` | 边、支配树与 frontier |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_ssa.c` | 局部提升、phi 和重命名 |
| 计划新增测试 | `tests/parser/test_ssa_construction.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 先建 CFG** 显式正常/异常/cleanup/suspend 边；terminator 唯一，短路/optional access 保留独立分支，参数和外部值在 entry 定义。

- [ ] **2. 筛选可提升 Place** 只有局部、无地址逃逸、无外部别名的标量提升；其余保持 place，避免把 struct writeback 或 borrowed storage 当纯值。

- [ ] **3. 插 phi 并重命名** 计算可达块、dominators、迭代 frontier 和活跃性，插 pruned phi；沿支配树维护每个局部定义栈，phi operand 按 predecessor edge 编号。

- [ ] **4. 接入构建入口** ZrParser_ExecIr_Build 返回结构化诊断；旧 ExecBC 路径并行作差分基线，不能通过解析旧 opcode 补缺失语义。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
for place in promotableLocals:
    phiBlocks = iteratedDominanceFrontier(defBlocks(place), liveIn(place))
    insertPhi(place, phiBlocks)
rename(block):
    push(block.phiDefinitions)
    replaceLocalReadsWithTopDefinition()
    push(localWrites)
    for edge in block.successors: setPhiIncoming(edge, currentDefinitions)
    for child in dominatorChildren(block): rename(child)
    pop(definitionsPushedInBlock)
rejectReadBeforeDefinitionExceptLanguageDefinedInitialization()
```

异常边上的定义可用性按抛出点而非 block 末尾处理；可抛操作必要时拆 block。不可达删除前保留诊断源位置；loop backedge 和多条同前驱边不能混淆。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| 菱形赋值与循环携带值 | phi 入边数、值及支配关系正确 |
| try/finally、throw 中断赋值 | 异常边不读取未完成结果 |
| obj?.method(sideEffect()) | 空 receiver 不执行参数及后续链 |
| 取地址局部、inline struct setter | 保留 place 和 writeback，不错误提升 |

复用回归入口：`tests/parser/test_pre_semantic_ir.c`、`tests/parser/test_place_cfg_graph.c`。历史计数只作为核对线索，实施时重跑并记录实际总数。

登记新 CTest 名 `ssa_construction` 和可执行目标 `zr_vm_ssa_construction_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_construction_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_construction$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** 标准控制流、可选链、异常与循环 fixture 可由前置 facts 构造；不依赖 compiler_quickening.c。

**失败恢复：** builder 失败报告缺失 canonical fact，不能静默回用后投影伪装 SSA 成功。

**文档交付：** 更新 docs/parser-and-semantics 的语义流水线与 CFG 说明；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrExecIrBuildInput SZrExecIrBuildInput;
TZrBool ZrParser_ExecIr_Build(const SZrExecIrBuildInput *input,
    SZrExecIrModule *output, SZrExecIrDiagnostic *diagnostic);
TZrBool ZrParser_ExecIr_BuildSsa(SZrExecIrFunction *function,
    SZrExecIrDiagnostic *diagnostic);
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| canonical facts | compiler_semantic_ir.c 与 type inference | builder 只消费，无缺失时字符串反解 |
| CFG edge identity | exec_ir_cfg.c | phi 绑定 edgeId 而非仅 blockId |
| definition stacks/use lists | exec_ir_ssa.c | rename 后每 use 有且只有一可达支配定义 |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 只建 block/terminator/edge/source，不优化；用 if/loop/short-circuit/try cases 核对 CFG。

- [ ] **批次 2：** 做可提升 Place 判定与 phi insertion；保留 address-taken locals 的内存语义。

- [ ] **批次 3：** 完成 rename、critical-edge 约定和异常点 block split，接 verifier 与 oracle。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange loop header phi x from entry=0 and backedge=x+1
assert incoming edge IDs and value definitions exact
arrange invoke produces result only on normal edge
assert catch cannot use unproduced result
arrange receiver=null in receiver?.m(sideEffect())
assert sideEffect count=0 and suffix loads=0
```

### 迁移结束检查

查找所有从 compiler_semir 后投影补类型/目标事实的路径，为每项建立前置 producer 并迁走。不能通过在 builder 内调用 compile_expression 再解析生成指令完成“SSA 构造”。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。

