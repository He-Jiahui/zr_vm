---
related_code:
  - zr_vm_core/src/zr_vm_core/module/module_loader.c
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
implementation_files:
  - zr_vm_core/src/zr_vm_core/module/module_loader.c
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_rollback.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_profile.c
  - tests/library/ssa_hotpatch_fault_cases.h
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/library/test_ssa_rollback_restricted.c
  - tests/acceptance/ssa-hotpatch-rollback-status-mapping.md
  - tests/acceptance/ssa-hotpatch-rollback-test-ndebug.md
doc_type: milestone-detail
status: planned
---

# 08.04 回滚、受限 Patch 与攻击面测试

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 完成回滚、幂等应用和 iOS/WASM 受限解释 Patch，并以对抗测试验证失败隔离。

**Architecture：** Patch 状态持久化的是 identity 与契约，运行期状态由 generation manager 管理；回滚是重新发布已验证内容，外部副作用不自动撤销。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M5。
- 前置：[08.03 Generation 发布、旧 Frame 与回收](../08-artifact-hotpatch/03-generation-publication.md)。
- 交付：rollback API、幂等 patch registry、受限 profile 与 fault injection。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

当前 artifact/binding 已有负测；新增 patch 必须覆盖加载事务而非只测试签名成功。平台 profile 是工程限制，不构成应用商店合规保证。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/module/module_loader.c` | patch 应用与卸载错误集成 |
| 现有，修改/复用 | `zr_vm_common/include/zr_vm_common/zr_aot_abi.h` | 冻结可调用 entry 能力 |
| 计划新增 | `zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_rollback.c` | 新 epoch 回滚与版本保留策略 |
| 计划新增 | `zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_profile.c` | iOS/WASM interpreter-only profile |
| 计划新增 | `tests/library/ssa_hotpatch_fault_cases.h` | 签名/内容/发布/回收故障注入用例 |
| 计划新增测试 | `tests/library/test_ssa_rollback_restricted.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

当前 `ZrCore_HotPatch_Rollback` 已对可达的 generation 失败保持分类：目标缺失为
`ROLLBACK_NOT_FOUND`，记录容量不足为 `APPLY_CAPACITY`，代际编号耗尽为
`APPLY_GENERATION_OVERFLOW`；公开入口的零目标仍为 `APPLY_INVALID_ARGUMENT`。
测试用两代真实 Prepare+Publish 记录构造 retained `RETIRED` 目标，并断言失败
不改 records、active、count、next-generation 或输出 handle。详见
[rollback status mapping acceptance](../../../../tests/acceptance/ssa-hotpatch-rollback-status-mapping.md)。
这仅覆盖返回状态映射，不完成并发 rollback 或其它加载/回收故障矩阵。

`test_ssa_rollback_restricted.c` 的检查已不依赖 `assert`：API 操作显式执行，
状态和结果由 `TEST_CHECK` 收集，所以 `NDEBUG` 下仍运行完整用例；初始化失败时
会安全退出而不 Deinit 未初始化 manager。临时 canary 曾在 `-DNDEBUG` 下确定性
失败，最终 Debug/NDEBUG 门禁及清理证据见
[NDEBUG test reliability acceptance](../../../../tests/acceptance/ssa-hotpatch-rollback-test-ndebug.md)。

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 定义幂等状态** patchId+contentHash 同内容重复应用返回 already-applied；同 ID 不同内容拒绝。记录 validated/prepared/published/retired，不用文件存在与否猜状态。

- [ ] **2. 实施回滚** 重新发布目标旧内容但分配新 epoch，重建 targets；明确不撤销外部 I/O、已提交状态变更。v1 禁公开 layout 变更，不支持自动持久状态迁移。

- [ ] **3. 限制平台 patch** 只带已验证 ExecIR/ExecBC/maps/profile/debug，排除 JIT/native code、新 import、新权限；target profile 与 base ABI 匹配后才能加载。

- [ ] **4. 做故障矩阵** 签名错、截断、超限、跨平台、未知 section、stale、重复、发布时 OOM/取消、并发 rollback、进程重启后重验；每个阶段注入失败并断言 active 不受损。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
apply(patchId, hash):
    if registry.sameIdDifferentHash: reject(ID_COLLISION)
    if registry.sameIdSameHashPublished: return ALREADY_APPLIED
    validate -> prepare -> atomicPublish
rollback(contentVersion):
    validateRetainedContentAgainstCurrentBaseAndPolicy()
    publishWithFreshEpoch(contentVersion)
restrictedProfile: rejectMachineCodeAndUndeclaredCapabilities()
```

旧版代码内容可回滚，业务数据和外部副作用未必可回滚，API/文档必须区分。签名 trust root 轮换由宿主 policy 控制，不允许 patch 自授新根。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| 同 patch 重复与同 ID 不同内容 | 幂等/拒绝 |
| rollback 时旧 callback 仍执行 | 旧 lease 安全，新入口取回滚版本 |
| iOS/WASM 包带 JIT blob | 拒绝 |
| 所有阶段故障注入 | 旧 active module 可继续使用 |

本任务新增测试先独立运行，再进入完整 SSA 差分矩阵。

登记新 CTest 名 `ssa_rollback_restricted` 和可执行目标 `zr_vm_ssa_rollback_restricted_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_rollback_restricted_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_rollback_restricted$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** 所有负测与 rollback race 通过；artifact 无运行期地址，受限 profile 不依赖机器码 JIT。

**失败恢复：** 保留最后 accepted base，损坏 patch 不修改 active；回滚失败时明确保持当前版本而非半切换。

**文档交付：** 新增 docs/module-system/hotpatch-recovery-restricted-profiles.md；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrHotPatchRollbackRequest SZrHotPatchRollbackRequest;
TZrBool ZrCore_HotPatch_Rollback(const SZrHotPatchRollbackRequest *request,
    SZrHotPatchApplyResult *result, SZrHotPatchDiagnostic *diagnostic);
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| patchId/content hash registry | apply transaction | 幂等判定不能只用 id |
| rollback content/new epoch | generation manager | 旧内容重新发布，epoch 不复用 |
| restricted target profile | host/base contract | 机器码、新 import、新权限执行前拒绝 |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 实现 patch registry 和重复/冲突行为，测试失败准备不会留下已应用状态。

- [ ] **批次 2：** 实现新 epoch rollback，分离代码回滚与业务状态/I/O 不可逆效果。

- [ ] **批次 3：** 补每个阶段 fault injection 和 mobile/WASM restricted fixture；跨平台包拒绝必须早于执行。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange same patchId same hash already published
assert ALREADY_APPLIED and generation unchanged
arrange same patchId different hash
assert ID_COLLISION
arrange rollback after external I/O
assert code version changes but API does not claim I/O undone
```

### 迁移结束检查

不支持公开 layout/state migration 的 v1 要明确拒绝，而不是试图字节复制旧堆到新布局。rollback 仍做当前 policy 验证，不能复活已撤销权限。

第一版还须拒绝会改变**存活状态所依赖的私有 layout** 的 patch：私有名称不等于没有活跃实例。函数新版本的局部 frame 可以使用新内部布局，因为旧 frame 保留旧 descriptor；跨调用存活的 heap object、闭包捕获、task frame 和 module global 则必须保持各自 version/layout 兼容，或完整隔离为新版本独立对象。未实现状态迁移时不得重新解释旧字节。新增测试覆盖旧 closure/task 挂起期间应用 patch 再恢复。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。
