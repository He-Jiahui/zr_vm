---
related_code:
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_loan_liveness.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership.c
  - zr_vm_core/src/zr_vm_core/ownership_transfer_value_copy.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/semantic_ir_loan_liveness.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership.c
  - zr_vm_core/src/zr_vm_core/ownership_transfer_value_copy.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_escape.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_allocation.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_ownership_elision.c
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/parser/test_ssa_escape_ownership.c
doc_type: milestone-detail
status: planned
---

# 02.03 逃逸、生命周期与分配消除

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 根据所有权、逃逸和活跃性自动消除复制及临时分配，并保留可观察析构行为。

**Architecture：** 对象分配点与 place/loan 构成逃逸图；先做局部 copy/move elision，再做 stack/region allocation 和 allocation sinking。资源对象与普通 GC 值分别判断。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M6；为 M2 表示优化提供证明。
- 前置：[02.02 别名分析、GVN 与范围证明](../02-automatic-optimization/02-gvn-range.md)；[01.04 Ownership、挂起与状态恢复映射](../01-execir-ssa/04-state-maps.md)。
- 交付：逃逸摘要、lifetime 区间、分配决策与 ownership 安全变换。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

semantic_ir_loan_* 和 dataflow_ownership 已有所有权事实；不能重建一套靠类型名判断的逃逸器，也不能假定 reference count 原子操作就使 payload 安全。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_parser/src/zr_vm_parser/semantic_ir_loan_liveness.c` | 消费 loan 活跃事实 |
| 现有，修改/复用 | `zr_vm_parser/src/zr_vm_parser/type_inference/dataflow_ownership.c` | 使用 canonical ownership 状态 |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/ownership_transfer_value_copy.c` | 维持已有复制/清理语义 |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_escape.c` | 分配点/参数/返回/闭包逃逸图 |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_allocation.c` | 栈/region/sinking 决策 |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_ownership_elision.c` | copy/move/release 优化 |
| 计划新增测试 | `tests/parser/test_ssa_escape_ownership.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 构造逃逸关系** 追踪 field/array 存储、return、closure capture、await/native/worker 参数；未知调用 conservatively escapes，并保留原因链。

- [ ] **2. 判断可消除复制** 源值无后续读取且 drop 权转移清晰时 move elision；借用区间仍覆盖真实 storage，禁止优化后临时地址悬空。

- [ ] **3. 安排分配与清理** 非逃逸、无身份观察的 GC aggregate 可 stack/region；分配可能失败与 finalizer 时序可观察则不可随意 sinking。异常出口有统一 cleanup state。

- [ ] **4. 限制 release batching** 仅对已证明析构无可观察效果且生命周期变化合法的内部值批处理；own resource、自定义 drop、锁释放和外部 handle 按原语义顺序执行。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
escape(site) = union(returned, captured, storedEscaping, passedUnknown,
                     crossedSuspend, crossedWorker, nativeRetained)
if !escape && !identityObserved && cleanupRepresentable:
    candidate = StackOrRegion
else: candidate = ManagedHeap
elideCopy only if lastUse(source) and ownershipTransferIsUnique
batchRelease only if unobservableDrop and noLifetimeSensitiveObserver
```

GC roots 仍需记录 stack 上的托管引用；stack allocation 不意味着不用 GC map。deopt 需要把被消除 aggregate 物化并保持 identity/alias 关系。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| 小 struct 返回可直接转发 | 无多余拷贝且字段一致 |
| 地址捕获到闭包或 native 保留 | 不能栈分配 |
| own resource 在 finally 析构 | 次数和顺序不变 |
| 候选分配点跨 await | 移到合法 task frame 或保留 heap，不留下栈别名 |

本任务新增测试先独立运行，再进入完整 SSA 差分矩阵。

登记新 CTest 名 `ssa_escape_ownership` 和可执行目标 `zr_vm_ssa_escape_ownership_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_escape_ownership_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_escape_ownership$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** copy/alloc 减少有 IR 断言和事件差分，异常/GC/ownership 全路径通过；不能以改变析构时机换收益。

**失败恢复：** 未知逃逸保留 heap 和原 copy；失败的分配优化按单 pass 禁用，状态图不丢失。

**文档交付：** 新增 docs/parser-and-semantics/automatic-escape-ownership.md；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrExecIrEscapeSummary SZrExecIrEscapeSummary;
TZrBool ZrParser_ExecIr_AnalyzeEscape(const SZrExecIrFunction *function,
    SZrExecIrEscapeSummary *summary, SZrExecIrDiagnostic *diagnostic);
TZrBool ZrParser_ExecIr_ElideAllocationAndCopies(SZrExecIrFunction *function,
    const SZrExecIrEscapeSummary *summary, SZrExecIrDiagnostic *diagnostic);
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| escape graph edges | store/return/capture/native/suspend 操作 | 任一 unknown edge 保守传递逃逸 |
| initialized/moved/dropped state | ownership flow | 路径合流和 exceptional cleanup 都要验证 |
| allocation placement plan | escape + identity + cost | codegen/materializer/root builder 同时消费 |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 仅输出 escape 摘要及原因链，使用 native retained/closure/worker/return 反例证明判定。

- [ ] **批次 2：** 实现 last-use copy/move 消除和 return forwarding 的事实，保留可观察资源 drop 时点。

- [ ] **批次 3：** 再启用栈/region/sinking，增加 GC map 与 deopt 重建，测量真实分配减少和物化开销。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange native callback retains pointer beyond call
assert site=escapes and allocation remains stable heap/pinned as required
arrange destructor appends D and later I/O appends I
assert events remain [D,I], never [I,D]
arrange nonescaping aggregate contains managed reference
assert stack root map includes that reference
```

### 迁移结束检查

runtime 中按 type 名判断“这种对象可以栈分配”的捷径不得保留；eligible 决策来自可复用 facts，失败时能给出具体逃逸路径。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。

