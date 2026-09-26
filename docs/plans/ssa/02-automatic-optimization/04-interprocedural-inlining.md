---
related_code:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_call_binding.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_member_resolution.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_call_binding.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_member_resolution.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_call_graph.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_devirtualize.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_inline.c
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/parser/test_ssa_interprocedural_inlining.c
doc_type: milestone-detail
status: planned
---

# 02.04 跨函数摘要、去虚化与内联

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 自动发布跨函数摘要，并安全实施静态绑定、去虚化、接口专门化、内联和返回转发。

**Architecture：** 调用图 SCC 上求 effect/escape 摘要固定点；去虚化用静态封闭性证明或 runtime guard，内联依赖明确的可热更策略。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M6；复用 M3 CallBinding。
- 前置：[02.03 逃逸、生命周期与分配消除](../02-automatic-optimization/03-escape-ownership.md)；[03.02 成员链与调用 Binding facts 收敛](../03-interpreter-binding/02-static-binding-facts.md)。
- 交付：函数摘要、call graph、inline policy 与依赖失效清单。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

SZrCallBindingContract 已携带 token/signature/layout。摘要应绑定 module/IR/signature hash；profile 单态不是目标永久唯一的证明。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_parser/src/zr_vm_parser/compiler/compiler_call_binding.c` | 复用已有目标 contract |
| 现有，修改/复用 | `zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_member_resolution.c` | 读取静态成员和接口槽事实 |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_call_graph.c` | SCC 和跨函数摘要 |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_devirtualize.c` | 封闭性/guard 去虚化 |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_inline.c` | 有限预算内联和 inline state map |
| 计划新增测试 | `tests/parser/test_ssa_interprocedural_inlining.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 推导稳定摘要** 私有 body 推 purity/receiver mutation/alloc/throw/escape/SendSync；递归 SCC 以保守格固定点收敛，外部 module 只消费经过验证的稳定摘要。

- [ ] **2. 解析直接/间接目标** 静态 token 可绑定即直接；虚/接口保留 slot，精确 receiver 证明或受保护 profile 可专门化；typed function value 仍验证 signature 与 closure context。

- [ ] **3. 加入内联预算** 按 IR cost、热度、递归深度、每函数/module 代码尺寸设预算；同时克隆 source/cleanup/root/deopt 信息，拒绝超预算而非膨胀无限版本。

- [ ] **4. 处理热更依赖** 冻结函数可直接内联；可 patch 函数 M6 第一版不跨边界内联，除非明确实现 version guard、依赖登记和全调用者去优化。对其 direct call 走 generation entry cell。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
summary(SCC) = leastConservativeFixedPoint(bodies, importedSummaries)
if exactReceiver && contractValid && targetFrozen:
    call = DirectTokenCall
else if profileMonomorphic && deoptMapAvailable:
    call = Guard(receiver, generation) ? SpecializedCall : SlotCall
if patchable(target) && noCallerInvalidationProtocol: doNotInline()
inline only if costWithinBudgets && cleanupAndStateMapsClonable
```

合法 receiver 多态退回已验证 slot；静态 token/signature/generation 无效必须 link error，不能用名称解析兜底。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| 递归函数与互递归 effect | 摘要收敛，副作用不丢失 |
| 单态训练后出现第二种 receiver | 正确 slot 调用或 deopt |
| 内联函数抛异常 | 原源位置和 cleanup 正确 |
| patchable 目标替换 | 未内联调用看到规定 generation，不执行过期代码 |

本任务新增测试先独立运行，再进入完整 SSA 差分矩阵。

登记新 CTest 名 `ssa_interprocedural_inlining` 和可执行目标 `zr_vm_ssa_interprocedural_inlining_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_interprocedural_inlining_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_interprocedural_inlining$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** 各调用形态有差分，摘要失效与缓存 key 完整；无需 @inline/@pure 才获得基本优化。

**失败恢复：** 摘要不匹配丢弃缓存重算；减少内联/去虚化不改变语言能力。

**文档交付：** 新增 docs/instruction-generation/interprocedural-optimization.md；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrExecIrCallGraph SZrExecIrCallGraph;
TZrBool ZrParser_ExecIr_BuildCallGraph(SZrExecIrModule *module,
    SZrExecIrCallGraph *graph, SZrExecIrDiagnostic *diagnostic);
TZrBool ZrParser_ExecIr_InlineCalls(SZrExecIrModule *module,
    const SZrExecIrCallGraph *graph, SZrExecIrDiagnostic *diagnostic);
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| callee summaries | SCC fixed-point 与已验证 imports | caller 仅在 hash/ABI 匹配时采用 |
| devirtualization proof | 精确 receiver 或 profile+guard | codegen 保留 slot baseline 和 deopt map |
| inline dependency | call graph + patch policy | 可 patch target 的修改触发拒绝内联或全依赖失效 |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 构建调用图和稳定摘要，先测试互递归及未知 native effects。

- [ ] **批次 2：** 加入精确 receiver 去虚化和 guarded target specialization，保留合法多态的 slot 路径。

- [ ] **批次 3：** 实现小函数内联与 inline state maps；代码尺寸、递归和 patchability 三个预算/限制分别验证。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange mutually recursive f/g with g performing I/O
assert both transitive summaries include I/O
arrange monomorphic trained site then new receiver type
assert slot baseline executes correct target
arrange patchable callee without caller deopt dependency
assert inlining blocked with PATCHABLE_TARGET reason
```

### 迁移结束检查

内联 source map 记录原 callee 与 callsite 双重位置；异常 traceback 和 drop 顺序不能被简化为调用者单一位置。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。

