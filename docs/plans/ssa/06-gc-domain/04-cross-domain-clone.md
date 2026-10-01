---
related_code:
  - zr_vm_core/src/zr_vm_core/ownership_transfer_cross_domain.c
  - zr_vm_core/src/zr_vm_core/ownership_transfer_value_copy.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/ownership_transfer_cross_domain.c
  - zr_vm_core/src/zr_vm_core/ownership_transfer_value_copy.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain.c
  - zr_vm_core/src/zr_vm_core/gc/gc_domain_clone.c
  - zr_vm_core/src/zr_vm_core/ownership_transfer_graph_decode.c
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/core/test_ssa_cross_domain_clone.c
  - tests/core/test_ssa_cross_domain_clone_oom.c
  - tests/core/test_resource_cross_domain_transfer.c
  - tests/core/test_resource_cross_domain_transfer_races.c
doc_type: milestone-detail
status: planned
---

# 06.04 跨域结构化复制与生命周期

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 将隔离 domain 间传值限定为可恢复的结构化复制，禁止共享 GC 指针和 ownership control。

**Architecture：** 两阶段 clone transaction：预检整图与资源 materializer，目标域创建独立对象图，成功后发布；Transfer 表示此复制协议，不隐含移动源指针。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M2。
- 前置：[06.03 同域多 Worker 与自动 Send/Sync](../06-gc-domain/03-domain-sharing.md)。
- 交付：跨域图预检、identity map、事务清理和错误定位。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

ownership_transfer_cross_domain.c 与 value_copy 已有 transfer 基础；需要明确其行为与索引“只能复制”的约束，不把 materialized shared handle 解释成跨域共享原始 managed object。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/ownership_transfer_cross_domain.c` | 复用 transfer 入口和事务 |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/ownership_transfer_value_copy.c` | 字段复制与 ownership 处理 |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/gc/gc_domain.c` | 目标域 admission |
| 计划新增 | `zr_vm_core/src/zr_vm_core/gc/gc_domain_clone.c` | 循环图、alias map 与目标分配事务 |
| 计划新增测试 | `tests/core/test_ssa_cross_domain_clone.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 整图预检** 检查类型/layout、资源复制能力、borrowed/thread-affine/stack alias、字节/节点/深度预算；失败不改变源对象。

- [ ] **2. 创建目标独立图** 用 source identity→destination object 表保留循环和图内共享，所有目标对象由目标 GC 管理；临时对象注册 transaction roots。

- [ ] **3. 处理外部资源** 只有 host 明确声明 materializer 能创建独立资源时允许复制；禁止直接复用源 domain 的 control block/raw pointer。引用语义的远端代理属于另一个明确协议，不自动启用。

- [ ] **4. 提交与失败恢复** 目标图完全初始化并验证后原子发布；取消/OOM/native materializer 异常清理全部目标临时资源，源保持可用。

## 核心算法与接口指导

### 2026-10-01 有界进度

目标字段 HashPair 分配及其 GC 后重试的真实 OOM 已补齐 Commit 恢复路径。
先修复 TryRun 的 GC scope 恢复，再在图解码器清理临时根，并在转抛原始状态前
释放提交窗口。两个用例分别验证 Abort/Free 后重新克隆和同一 claim 重试；
三套编译器均通过克隆 7 项、资源转移 24 项及竞争 5 项。
详见 [失败注入验收](../../../../tests/acceptance/ssa-cross-domain-clone-oom.md)。
随后补齐便利入口 Execute 的独立失败恢复：同一真实目标字段 OOM 曾留下源侧
信封分配 1、释放 0；内部受保护 Commit 清理后，三套编译器的克隆 8 项均通过，
失败信封分配/释放 1/1，再次成功调用后累计 2/2。
本结果仅覆盖目标字段分配失败；源 Prepare OOM、provider Throw 及完整深图/
并发销毁门禁仍待独立验证，上述任务和完整 06.04 保持未完成。

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
preflight(sourceGraph, limits, cloneContracts)
for sourceNode in graph:
    destination = allocateInTargetDomain()
    identityMap[sourceNode] = destination
for edge in graph: copyEdgeThroughIdentityMap(edge)
validateAllLayoutsAndOwnership()
commitDestinationGraph()
onFailure: destroyTransactionRootsAndResources(); leaveSourceUnchanged()
```

跨域复制与同域 unique handoff 不共用裸指针搬运实现。persistent 数据可带 token/offset，但不能把源进程地址当 relocation 目标。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| 带环/重复引用对象图 | 目标内部 alias 正确且与源独立 |
| 源域随后 GC/销毁 | 目标仍有效 |
| 第 N 个字段 OOM/取消 | 目标无泄漏，源未失效 |
| borrowed/native 不可复制资源 | 执行前精确拒绝 |

复用回归入口：`tests/core/test_resource_cross_domain_transfer.c`、`tests/core/test_resource_cross_domain_transfer_races.c`。历史计数只作为核对线索，实施时重跑并记录实际总数。

登记新 CTest 名 `ssa_cross_domain_clone` 和可执行目标 `zr_vm_ssa_cross_domain_clone_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_cross_domain_clone_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_cross_domain_clone$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** 跨域不出现共享 managed/control 指针；深图、循环、取消和并发销毁验证通过。

**失败恢复：** 不可复制对象明确报错，不退回共享 handle；保留现有源对象状态。

**文档交付：** 新增 docs/core-runtime/cross-domain-clone.md；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrDomainTransferRequest SZrDomainTransferRequest;
typedef struct SZrDomainTransferResult SZrDomainTransferResult;
TZrBool ZrCore_Domain_TransferValue(const SZrDomainTransferRequest *request,
    SZrDomainTransferResult *result, SZrDomainDiagnostic *diagnostic);
/* SZrDomainDiagnostic 为 06.03 定义的 core 运行期诊断类型 */
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| clone contract/limits | host/type metadata | 预检不可复制资源，不静默共享 |
| source→destination identity map | clone transaction | 保持图内 alias/cycle，目标指针独立 |
| temporary target roots | 目标 domain | 失败回收，成功发布后交给正常 GC |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 先只做 preflight 和错误路径定位，确保禁止值在任何目标分配前拒绝。

- [ ] **批次 2：** 实现有环对象图克隆和资源 materializer 事务，注入每个节点 OOM。

- [ ] **批次 3：** 做源域销毁、目标域 GC、取消/超时与并发请求，证明源内容未被隐式移动。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange source graph A->B->A plus two refs to B
assert destination preserves cycle/alias and shares no managed addresses
arrange clone fails at node N
assert source intact, target temporary roots/resources released
arrange source domain destroyed after successful copy
assert target graph remains usable
```

### 迁移结束检查

Transfer 名称仍可保留兼容 API，但文档和实现必须明确复制语义。任何 shared external handle 都需独立 materializer/协议，不可借名跨域共享 GC control block。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。
