---
related_code:
  - zr_vm_core/include/zr_vm_core/gc_domain.h
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_mutator.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_task_effects.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/gc_domain.h
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_mutator.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_task_effects.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_send_sync.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_share.c
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/core/test_ssa_domain_sharing.c
  - tests/core/test_gc_domain_multimutator.c
  - tests/core/test_resource_same_domain_handoff.c
doc_type: milestone-detail
status: planned
---

# 06.03 同域多 Worker 与自动 Send/Sync

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 建立同一 domain 多 worker 共享堆的合法访问规则，并自动推导 Send/Sync。

**Architecture：** ownership/type/effect 摘要决定传值或共享资格；domain mutator 注册、root snapshot 和 safepoint rendezvous 与 GC 一致。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M2。
- 前置：[06.01 Region、TLAB、Remembered set 与 Minor GC](../06-gc-domain/01-young-allocation.md)；[01.03 Memory、Effect、异常边与验证器](../01-execir-ssa/03-effects-verifier.md)。
- 交付：Send/Sync 推断、共享/可变访问验证、domain mutator 生命周期。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

gc_domain_mutator.c 和 gc_domain.h 已有多 mutator 接口；语言 trait 证明与 runtime domain admission 需要两层协作，不能只检查一个 capability bit。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_core/include/zr_vm_core/gc_domain.h` | domain/worker contract |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/gc/gc_domain_mutator.c` | attach/detach/rendezvous |
| 现有，修改/复用 | `zr_vm_parser/src/zr_vm_parser/compiler/compiler_task_effects.c` | 已有 task/worker 语义校验 |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/analysis/exec_ir_send_sync.c` | 结构性 trait 推导与负因子链 |
| 计划新增 | `zr_vm_core/src/zr_vm_core/gc/gc_domain_share.c` | 同域值交接/共享入口 |
| 计划新增测试 | `tests/core/test_ssa_domain_sharing.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 定义推断规则** Send 表示可转移线程所有权，Sync 表示共享引用可安全访问；二者不能合并。私有 aggregate 递归推导，thread-affine native、stack alias、borrowed ref-like 是阻断因素。

- [ ] **2. 检查共享可变值** immutable/安全 value 可共享；mutable 必须 mutex/atomic/ownership protocol，原子引用计数只保护计数不保护 payload。捕获闭包逐字段验证。

- [ ] **3. 规范 mutator 注册** worker 在分配或持 managed ref 前 attach；detach 发布根并确认无活跃 frame/callback；GC rendezvous 处理阻塞 native worker 和超时诊断。

- [ ] **4. 验证内存序与取消** 发布对象完成初始化后 release，消费者 acquire；任务取消、异常、worker 退出清理 owners 和 roots；TSan 覆盖真实跨线程调度。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
inferSend(type) = allFieldsTransferable && noThreadAffineResource
inferSync(type) = allSharedAccessSafe && noUnsynchronizedMutation
share(value, domain):
    require sameDomain && inferredSync && lifetimeValid
    publishInitializedValueWithRelease()
receive(): acquirePublishedValue(); registerRootsBeforeUse()
borrowedOrStackAlias -> rejectAtSourceCapture
```

Send 但非 Sync 的值可唯一交接，不能同时共享；domain membership 不代替 data-race 证明。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| immutable aggregate 共享 | 自动推断通过 |
| 原子引用计数包裹非同步可变 payload | 共享拒绝 |
| thread-affine/native/ref-like 捕获 | 具体字段与源位置诊断 |
| worker 退出时 GC/回调并发 | 无悬空 root/死锁 |

复用回归入口：`tests/core/test_gc_domain_multimutator.c`、`tests/core/test_resource_same_domain_handoff.c`。历史计数只作为核对线索，实施时重跑并记录实际总数。

登记新 CTest 名 `ssa_domain_sharing` 和可执行目标 `zr_vm_ssa_domain_sharing_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_domain_sharing_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_domain_sharing$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** 编译推断与 runtime admission 同时验证，同域 race/GC 生命周期全覆盖；未知外部类型保守拒绝非法共享。

**失败恢复：** 不满足共享 contract 时选择合法独占交接或复制；不能自动共享并希望运行期锁解决未知语义。

**文档交付：** 新增 docs/core-runtime/domain-worker-sharing.md；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrDomainShareRequest SZrDomainShareRequest;
typedef struct SZrDomainContract SZrDomainContract;
typedef struct SZrDomainDiagnostic SZrDomainDiagnostic; /* core 自有运行期诊断，不复用 parser 侧 SZrExecIrDiagnostic */
TZrBool ZrCore_Domain_GetContract(struct SZrState *state, SZrDomainContract *out);
TZrBool ZrCore_Domain_ShareValue(const SZrDomainShareRequest *request,
    SZrDomainDiagnostic *diagnostic);
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| Send/Sync proof | type/ownership/effect 固定点 | 编译捕获检查，不由 runtime 猜类型名 |
| mutator registration | domain runtime | GC snapshot 覆盖所有 managed 活跃线程 |
| published shared value | release/acquire admission | 消费者不能看到半初始化状态 |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 实现 trait 推断与反例原因链，区分 Send-only handoff 与 Sync shared reference。

- [ ] **批次 2：** 接 runtime domain admission 和 attach/detach，验证 roots 与 native in-flight 状态。

- [ ] **批次 3：** 加入多 worker stress、取消/退出/GC rendezvous 与 TSan；不足时回到下层原子/ownership 修复。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange Shared<T> whose T has unsynchronized mutable field
assert atomic refcount does not make T Sync
arrange Send-only unique owner
assert handoff allowed, simultaneous sharing rejected
arrange worker detach with live managed callback
assert detach waits/rejects as contract, no missing GC root
```

### 迁移结束检查

删除“同域即安全”的隐式捷径。borrowed/stack alias/thread-affine handle 的阻断消息应定位到具体捕获或字段。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。

