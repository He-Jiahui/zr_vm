---
related_code:
  - zr_vm_core/src/zr_vm_core/call_binding.c
  - zr_vm_core/src/zr_vm_core/call_binding_link.c
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/metadata_runtime_method_binding.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/call_binding.c
  - zr_vm_core/src/zr_vm_core/call_binding_link.c
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/src/zr_vm_core/metadata_runtime_method_binding.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_generation.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_publish.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_retire.c
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/core/test_ssa_generation_publication.c
  - tests/core/test_call_binding_runtime.c
  - tests/acceptance/ssa-hotpatch-generation-handle-ownership.md
  - tests/library/test_ssa_capability_validation.c
  - tests/acceptance/ssa-hotpatch-prepare-status-mapping.md
doc_type: milestone-detail
status: planned
---

# 08.03 Generation 发布、旧 Frame 与回收

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 以原子 generation 发布支持热更旧 frame/new frame 共存，并安全回收过期 targets/code。

**Architecture：** 每个 module version record 不可变，frame 持 epoch/lease；可 patch 方法走稳定入口单元，发布指针原子切换，旧代码到所有使用者退出后回收。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M5。
- 前置：[08.02 Capability manifest 与 Patch 验证](../08-artifact-hotpatch/02-capability-validation.md)；[03.03 已解析目标、PIC 与 Guard 失效](../03-interpreter-binding/03-guarded-caches.md)；[04.02 调用返回、结果转发与尾调用复用](../04-frame-native/02-call-return-tail.md)。
- 交付：version records、entry cell、发布事务、in-flight 引用/epoch 回收和 stale binding 规则。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

CallBinding 已有 AdvanceGeneration 和 invalidate；该机制必须扩展为版本归属，不能直接把旧 frame 仍在使用的 code/metadata 释放。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/call_binding.c` | generation/validation 接缝 |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/call_binding_link.c` | 版本目标表 |
| 现有，修改/复用 | `zr_vm_core/include/zr_vm_core/function.h` | frame/function version 的引用 |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/metadata_runtime_method_binding.c` | entry cell 绑定 |
| 计划新增 | `zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_generation.c` | 版本记录和 acquire/release |
| 计划新增 | `zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_publish.c` | 单事务发布 |
| 计划新增 | `zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_retire.c` | retired 版本等待与回收 |
| 计划新增测试 | `tests/core/test_ssa_generation_publication.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

当前已验证一个 manager 所有权边界：`GenerationHandle` 只能用于创建它的
manager。跨 manager 的 Publish、Resolve、Release 返回结构化错误且不读外部
record、不改变记录或 lease；回滚按 manager 内 generation 编号查找，不接收
handle。实现与复杂度、测试证据见
[generation handle ownership acceptance](../../../../tests/acceptance/ssa-hotpatch-generation-handle-ownership.md)。
这只完成 manager 归属检查，不完成 frame 集成、并发 Resolve/Publish 安全或
本文件的发布与回收门禁。

`ApplyValidated` 现在保留 `Generation_Prepare` 当前可返回的失败类别：无效模块
身份、manager 槽位耗尽和 generation 溢出都能由 host 区分。Apply overflow
状态追加到旧枚举末尾，既有枚举值不变。focused capability-validation fixture
验证三类状态、文字名称、结构化诊断以及 manager/registry/handle 无发布副作用；
证据见
[the Prepare status mapping acceptance](../../../../tests/acceptance/ssa-hotpatch-prepare-status-mapping.md).
这只关闭 Prepare 到 Apply 的状态映射边界。Rollback 状态映射、并发发布、frame
集成和 executable installation 仍是独立计划工作。

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 定义版本亲和性** 活动 frame 持所进入的 version，内部静态依赖在该版本闭包解析；从 host/新 task 进入取最新 active version。跨边界 callback/delegate 明确版本 pin 或入口转发策略。

- [ ] **2. 构建新版本** 离线完成所有 token relocation、capability、frame/maps 与 entry table；准备失败不改 active。可 patch 目标禁止被未登记依赖的直接 AOT call/inlining 绕过。

- [ ] **3. 原子发布** 对 module bundle 使用一个 commit record 发布，release/acquire 保证表完整可见；多 worker 在安全点观察新 epoch，发布过程中不混读新旧布局。

- [ ] **4. 回收与失效** frame、callback、delegate、native in-flight、JIT code handle 都计入 lease；retired 不等于 invalid。对已失效外部 witness 返回 stale link error，合法旧 frame 可继续原版本。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
prepare(newVersion) -> validate -> link -> freeze
commit: atomicStoreRelease(activeVersion, newVersion)
enterFromHost: version = acquireActiveVersionWithLease()
oldFrameCall: resolveWithin(frame.version)
exitFrame: unpin(frame.version)
retire(version) only if noFrames && noCallbacks && noDelegates && noCodeLeases
staleExternalBinding -> structured LinkError, never nameLookup
```

generation 不复用，防 ABA；回滚也创建新的发布 epoch。仅递增一个全局整数不能实现 old/new 共存。

`acquireActiveVersionWithLease()` 必须把“取得 active 指针”和“防止其被回收”放在同一保护协议内。首版可在 version registry 锁内读取并增加引用，退出锁后执行；后续若优化为 epoch/hazard，则 reader 必须先宣布进入读临界区再读取 active，retirer 等待相关 reader 退出。禁止裸 `load(active); refcount++`，因为两条操作之间旧版本可能已经释放。增加发布/回收恰好发生在读指针与取 lease 之间的确定性 race 测试。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| 旧 frame 长循环期间替换方法 | 旧 frame 规则明确，新入口执行新版本 |
| 并发 host calls 和原子 bundle 发布 | 不出现混合表 |
| callback/delegate 仍引用旧版本 | 不提前释放 |
| 另一个 manager 的 prepared 或 leased handle | 拒绝且不读取外部 record、不改变任一 manager 的槽位或 lease |
| 重复失效/epoch 溢出/卸载竞争 | 确定错误或等待策略，无悬空 target |

复用回归入口：`tests/core/test_call_binding_runtime.c`。历史计数只作为核对线索，实施时重跑并记录实际总数。

登记新 CTest 名 `ssa_generation_publication` 和可执行目标 `zr_vm_ssa_generation_publication_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_generation_publication_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_generation_publication$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** 旧 frame、native callback、虚/接口缓存及 AOT direct-call 策略一致，ASan/TSan 回收压力验证通过。

**失败恢复：** 发布前失败直接丢 staging；发布后回滚走 08.04 新 epoch，不就地覆盖版本表。

**文档交付：** 新增 docs/core-runtime/module-generation-lifetime.md；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrHotPatchApplyResult SZrHotPatchApplyResult;
TZrBool ZrCore_HotPatch_Apply(const SZrValidatedHotPatch *patch,
    SZrHotPatchApplyResult *result, SZrHotPatchDiagnostic *diagnostic);
TZrBool ZrCore_HotPatch_CollectRetired(struct SZrState *state,
    SZrHotPatchDiagnostic *diagnostic);
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| staging version graph | loader/linker | 完整不可变后才 single-commit 发布 |
| frame/callback/code leases | 所有运行入口 | retired graph 到最后 lease 退出才释放 |
| active generation cell | atomic publisher | 新入口 acquire，旧 frame 用自身 version |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 先实现 version record 与 lease 计数/epoch，测试不发布情况下安全构建/销毁。

- [ ] **批次 2：** 接 entry cell 和原子 commit，明确 host 入口、旧 frame 内调用、delegate/callback 的 generation 选择。

- [ ] **批次 3：** 实现 retired reclaim、多 worker/GC/native/JIT 竞争，验证没有 ABA 和混合表。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange V1 frame active, publish V2, enter new host call
assert old frame resolves V1 contract; new call uses V2
arrange V1 callback still in-flight after frame exit
assert V1 code and metadata not freed
arrange publication fails before commit record
assert active pointer unchanged and staging fully released
```

### 迁移结束检查

所有可 patch AOT 调用必须可被入口单元或完整失效协议控制。若旧 AOT 内联体无法撤销，不能声称该方法可透明热更。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。
