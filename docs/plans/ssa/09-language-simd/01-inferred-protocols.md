---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_task_effects.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership.c
  - zr_vm_core/include/zr_vm_core/type_layout.h
  - docs/zr_language_specification.md
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_task_effects.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership.c
  - zr_vm_core/include/zr_vm_core/type_layout.h
  - docs/zr_language_specification.md
  - zr_vm_parser/include/zr_vm_parser/optimization_facts.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_protocols.c
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/parser/test_ssa_inferred_protocols.c
doc_type: milestone-detail
status: planned
---

# 09.01 自动语义摘要与通用能力协议

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 通过现有类型、ownership、effect 和 protocol 自动推断优化能力，避免业务代码增加性能注解。

**Architecture：** canonical facts 是能力唯一来源；语言已有 readonly/ref/SendSync/async 保持语义，优化器消费稳定摘要和通用 protocol，不根据容器类名特判。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M6 语言性质；M7 数据能力。
- 前置：[02.04 跨函数摘要、去虚化与内联](../02-automatic-optimization/04-interprocedural-inlining.md)；[06.03 同域多 Worker 与自动 Send/Sync](../06-gc-domain/03-domain-sharing.md)；[05.02 Typed array、连续 Slice 与 Bounds](../05-data-layout/02-arrays-slices.md)。
- 交付：能力协议、自动摘要、跨模块序列化和诊断规则。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

现行语法使用 fn、init/new/own、Unique/Shared/Weak 与 ref；测试必须按 docs/zr_language_specification.md，不能引入旧 % 前缀或假设 @hot/@nogc 标注。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_parser/src/zr_vm_parser/compiler/compiler_task_effects.c` | 发布 alloc/throw/suspend 等摘要 |
| 现有，修改/复用 | `zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership.c` | 所有权/borrow 摘要 |
| 现有，修改/复用 | `zr_vm_core/include/zr_vm_core/type_layout.h` | layout capability |
| 现有，修改/复用 | `docs/zr_language_specification.md` | 现行语法与语义清单 |
| 计划新增 | `zr_vm_parser/include/zr_vm_parser/optimization_facts.h` | 统一事实查询，不暴露 AST 猜测 |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_protocols.c` | 连续访问、迭代、immutable/persistent 能力 |
| 计划新增测试 | `tests/parser/test_ssa_inferred_protocols.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 定义推断生产方** 函数 body 推 purity/receiver mutation/alloc/throw/escape；结构字段推 SendSync；layout producer 推连续性/alignment。公开摘要包含版本/hash 与证明范围。

- [ ] **2. 设计通用 protocol** contiguous view、typed array、batch、iteration、immutable/persistent 更新分别声明操作/signature/effect/layout；编译器通过 token/capability 识别，类型名任意。

- [ ] **3. 保持语言安全边界** 未知优化事实仅阻断优化；违反 borrow/suspend/worker/native lifetime 仍编译错误。第三方外部 native 若缺 effect contract 则按 unknown effects，不能推 pure。

- [ ] **4. 验证自动性** 相同源码在无任何新增性能标注下通过 build profiles 获得优化；remarks 给出具体 blocker，只有语义必要的 API 设计才要求用户选择。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
OptimizationFacts = {type, layout, ownership, effect, escape, protocol, validity}
queryFacts(valueOrCall) -> Proven | Unknown(reason) | InvalidLanguageUse
Proven -> enableLegalTransformation
Unknown -> keepBaselineAndEmitMissedRemark
InvalidLanguageUse -> compileError
protocolRecognition uses metadataToken and contract, never spelling
```

immutable 容器可共享不代表内部 native handle 自动 Sync；persistent container 的结构共享仍需 GC/ownership contract。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| 同协议不同类型名 | 相同优化机会 |
| 没有性能注解的私有函数 | 可推导 pure/escape 摘要 |
| 公开接口只给稳定摘要 | 消费者不猜函数 body |
| 非法 borrow 跨 await 与未知纯度 | 前者报错，后者只阻断优化 |

本任务新增测试先独立运行，再进入完整 SSA 差分矩阵。

登记新 CTest 名 `ssa_inferred_protocols` 和可执行目标 `zr_vm_ssa_inferred_protocols_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_inferred_protocols_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_inferred_protocols$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** 自动事实贯通 parser→IR→后端→remarks；新增语言能力有明确协议且不污染共享 runtime。

**失败恢复：** 协议能力缺失走原合法调用；不会要求所有业务函数补注解才能编译。

**文档交付：** 新增 docs/parser-and-semantics/inferred-optimization-protocols.md；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrOptimizationFacts SZrOptimizationFacts;
typedef struct SZrOptimizationFactQuery SZrOptimizationFactQuery;
TZrBool ZrParser_Optimization_QueryFacts(const SZrOptimizationFactQuery *query,
    SZrOptimizationFacts *facts, SZrExecIrDiagnostic *diagnostic);
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| pure/throw/alloc/escape | body/SCC inference | 未知值有 reason，不假定安全 |
| SendSync/layout/protocol | 类型字段与 metadata 合同 | 优化器通用消费，不匹配类名 |
| public summary | module contract writer | 跨模块只消费稳定、已验证、hash 匹配摘要 |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 整理现有语义事实目录，明确每种 proof 的唯一 producer 和 validity。

- [ ] **批次 2：** 实现通用 protocol 查询和源码/二进制摘要一致性，先不新增语法标注。

- [ ] **批次 3：** 以无性能注解的实际程序验证优化与 remarks，覆盖陌生类型实现同协议的案例。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange two unrelated type names implement identical contiguous contract
assert both receive same legal optimization opportunity
arrange no effect summary for external native
assert conservative unknown effects, not pure
arrange invalid borrow across suspend
assert compile error, not merely missed optimization
```

### 迁移结束检查

所有 missed remarks 应可追溯到事实缺失或成本，不能把“添加 @hot/@pure 才优化”当默认解决方式。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。

