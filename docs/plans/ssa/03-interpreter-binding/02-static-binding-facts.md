---
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_member_resolution.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_call_binding.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_member_resolution.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_support.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_call_binding.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_binding_facts.c
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/parser/test_ssa_static_binding_facts.c
  - tests/parser/test_call_binding_pipeline.c
  - tests/parser/test_typed_call_binding.c
doc_type: milestone-detail
status: planned
---

# 03.02 成员链与调用 Binding facts 收敛

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 把成员链、accessor、meta、虚/接口和函数值的静态定位 facts 完整映射到 ExecIR binding。

**Architecture：** type inference 发布每段 receiver/place/member 定位和最终调用 contract，compiler 只消费事实；每段必要对象读取保留，最终成员不再生成名称查找。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M3；在既有 CallBinding（历史 CallBinding M1 交付，非本计划 M1）基础上向 SSA facts 收敛。
- 前置：[01.03 Memory、Effect、异常边与验证器](../01-execir-ssa/03-effects-verifier.md)。
- 交付：链路 facts、binding row index、precise diagnostics 和 ExecIR/ExecBC 对照。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

CallBinding contract、pipeline/runtime/relocation tests 已存在；本任务扩展至共享 IR，不重复发明目标表。编译事实源为 type_inference_member_resolution.c。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_member_resolution.c` | 发布 receiver、MemberId/offset/slot、signature、accessor 操作 |
| 现有，修改/复用 | `zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_support.c` | 消费 facts，不重新解析字符串 |
| 现有，修改/复用 | `zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_types.c` | 保留类型与 receiver 语义 |
| 现有，修改/复用 | `zr_vm_parser/src/zr_vm_parser/compiler/compiler_call_binding.c` | 复用 binding 行分配和校验 |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_binding_facts.c` | 将整链 facts 映射为 load/project/call |
| 计划新增测试 | `tests/parser/test_ssa_static_binding_facts.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 列清所有链段** module.Type.method 与 obj.a.b.invoke 分开；前者直接 token，后者 project/load 保留 null/ownership 检查，再调用最终 token/slot。

- [ ] **2. 统一 accessor/meta** getter/setter/meta 用 operation 字段，不按成员名分支；setter 保留值、receiver、writeback 和异常提交顺序。

- [ ] **3. 保留多态形式** virtual/interface 用 dispatchSlot，typed callable 用 signature+context；slot 定位不等于固定具体函数，运行期仍允许合法 receiver 多态。

- [ ] **4. 前置精确失败** unknown/ambiguous/signature/layout/missing contract 在编译/链接阶段报具体源范围；优化关闭时也不允许静态字符串回退。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
for segment in resolvedChain:
    if segment.isRuntimeField:
        receiver = ProjectAndLoad(receiver, segment.memberId, segment.layout)
    else if segment.isAccessor:
        receiver = CallBinding(segment.getContract, receiver)
call = EmitCall(finalContract.bindingKind, finalContract.row, receiver, args)
assertNoFinalNameLookup(call)
optionalGuard protects all following receiver reads and argument effects
```

稳定 binding 不等于省略求值；字段读取、getter 副作用和 optional suffix 跳过必须保留。动态访问仅由显式动态语义产生。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| module.Type.method() | 最终直接 token，无 final member 字符串 opcode |
| obj.a.b.invoke() | a/b 读取保留，invoke 定位固定 |
| getter/setter/meta 与 inline struct | 操作 contract 一致且 writeback 正确 |
| 未知、重载歧义、typed signature 错 | 分别诊断并定位最终失败段 |

复用回归入口：`tests/parser/test_call_binding_pipeline.c`、`tests/parser/test_typed_call_binding.c`。历史计数只作为核对线索，实施时重跑并记录实际总数。

登记新 CTest 名 `ssa_static_binding_facts` 和可执行目标 `zr_vm_ssa_static_binding_facts_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_static_binding_facts_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_static_binding_facts$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** 全部静态调用形态有 fact→ExecIR→ExecBC 证据，既有 CallBinding 测试重跑；不得把历史存在的实现当本任务新增成果。

**失败恢复：** fact 缺失时修复生产方，不在消费者补成员名查找逻辑。

**文档交付：** 更新 docs/parser-and-semantics 的 member/call-binding 模块说明；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrExecIrBindingFacts SZrExecIrBindingFacts;
TZrBool ZrParser_ExecIr_ProjectBindingFacts(
    const SZrExecIrBindingFacts *facts, SZrExecIrFunction *function,
    SZrExecIrDiagnostic *diagnostic);
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| 每段链定位 | type_inference_member_resolution | compiler 不重新查 overload/member 名 |
| CallBinding row | compiler binding builder | ExecIR/ExecBC/AOT 保存同一 contract identity |
| receiver place/writeback | canonical ownership/place facts | accessor/meta lowering 保留求值和清理 |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 扩展现有 pipeline fixture，先记录每段 receiver/type/MemberId/slot，而不改变旧发码。

- [ ] **批次 2：** 把事实投影到 ExecIR，按 direct/virtual/interface/typed function/get/set/meta 分别接入。

- [ ] **批次 3：** 切换 compile_expression 消费者，删除冗余 final member lookup；保留必要中间对象读取。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange module.Type.method()
assert final lookupCount=0 and bindingKind=DIRECT
arrange obj.a.b.invoke() with getters recording [a,b]
assert evaluation order [a,b,invoke] and invoke located by token/slot
arrange ambiguous overload
assert compile error at final member range before artifact write
```

### 迁移结束检查

使用源码字符串搜索作为测试辅助可以，生产消费者不可出现按具体类名/成员名的分支。每个成员解析失败的诊断需绑定 chain segment index。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。

