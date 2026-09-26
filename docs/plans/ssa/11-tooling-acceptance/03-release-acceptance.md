---
related_code:
  - tests/CMakeLists.txt
  - docs/plans/ssa/index.md
  - tests/acceptance/2026-09-11-call-binding-m1-current-head.md
implementation_files:
  - tests/CMakeLists.txt
  - docs/plans/ssa/index.md
  - tests/acceptance/2026-09-11-call-binding-m1-current-head.md
  - tests/acceptance/ssa-milestone-acceptance.md
  - tests/acceptance/ssa-requirement-matrix.md
  - tests/cmake/ssa-coverage-manifest.cmake
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/core/test_ssa_release_acceptance.c
doc_type: milestone-detail
status: planned
---

# 11.03 完整覆盖、文档与阶段收口

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 逐项收口原索引所有需求，确保局部完成不被错误标记为整条 SSA 主线完成。

**Architecture：** 需求→子计划→测试→证据→平台构成验收矩阵；阶段按依赖推进，历史失败、缺失后端和测量噪声保留可见。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M0–M7 最终验收。
- 前置：[07.03 泛型、LTO、PGO 与裁剪](../07-aot-backends/03-generics-lto-pgo.md)；[07.04 AOT Runner、覆盖率与发布门禁](../07-aot-backends/04-aot-runner-coverage.md)；[08.04 回滚、受限 Patch 与攻击面测试](../08-artifact-hotpatch/04-rollback-restricted.md)；[09.03 Batch、矩阵与自动向量化](../09-language-simd/03-batch-vectorization.md)；[10.03 桌面、Android、iOS 与 WASM 平台验收](../10-jit-platforms/03-platform-matrix.md)；[11.02 优化说明 CLI、JSON 与 LSP](../11-tooling-acceptance/02-optimization-remarks.md)。
- 交付：全需求追踪、分层回归、性能/平台记录、维护文档与旧路径退出清单。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

当前仓库已有多条活跃工作与未提交改动；验收必须绑定实际 HEAD/dirty digest，不能沿用历史 member/native/GC 数字作为本次结果。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `tests/CMakeLists.txt` | 全部 suite/新测试注册清单 |
| 现有，修改/复用 | `docs/plans/ssa/index.md` | 保留主需求并维护导航 |
| 现有，修改/复用 | `tests/acceptance/2026-09-11-call-binding-m1-current-head.md` | 只作为历史证据，不当最新通过结论 |
| 计划新增 | `tests/acceptance/ssa-milestone-acceptance.md` | 按阶段/平台记录真实结果 |
| 计划新增 | `tests/acceptance/ssa-requirement-matrix.md` | 需求、case、测试名、状态、证据 |
| 计划新增 | `tests/cmake/ssa-coverage-manifest.cmake` | 声明应执行的语义/后端组合 |
| 计划新增测试 | `tests/core/test_ssa_release_acceptance.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 建立明确清单** 覆盖 parser 调用形态、IR CFG/effects、runtime GC/ownership/async/native、多后端、artifact/hotpatch、platform/perf；每项要求正向、边界和失败。

- [ ] **2. 先下层后整体** 先 model/verifier，再 lowering/ABI/runtime，再 library/CLI/LSP；上层失败追溯下层，不先在 CLI 加 workaround。已有 baseline failure 标明复现与影响。

- [ ] **3. 按矩阵跑完整证据** GCC/Clang/MSVC、ASan/UBSan/LSan/TSan、Valgrind/Helgrind 和目标平台各自记录命令/总数/失败/限制；性能 Release 单独计时。

- [ ] **4. 退出旧路径与同步文档** 统计 legacy SemIR projection/AOT opcode decoder 的剩余消费者，迁完再删除；更新 docs/wiki、API/schema/CLI/reference matrix。只有完整 scope 通过才 accepted。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
for requirement in originalIndex:
    require ownedSubplan && namedPositiveCase && namedNegativeCase
    require evidenceBoundToCurrentRevisionAndEnvironment
for milestone in M0..M7:
    accepted = allInScopeRequirementsPassed && allPrerequisitesAccepted
    if missingBackendOrPlatformOrKnownFailure: keepOpenWithReason
performanceClaim requires validComparableEvidenceAndThreshold
legacyRemoval requires zeroRemainingProductionConsumers
```

本次交付是计划，不勾选实施完成，不运行尚未存在的目标来制造通过记录。未来任何暂停项仍占据矩阵，不被删除。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| 遗漏一种 virtual/interface/accessor 形态 | 覆盖门禁失败 |
| 只有 smoke 通过但 sanitizer 失败 | 里程碑不完成 |
| native coverage 数据缺失 | M4 不通过 |
| 旧 decoder 仍被 AOT 消费 | 不能声明同源后端迁移完成 |

本任务新增测试先独立运行，再进入完整 SSA 差分矩阵。

登记新 CTest 名 `ssa_release_acceptance` 和可执行目标 `zr_vm_ssa_release_acceptance_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_release_acceptance_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_release_acceptance$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** 原索引每项均可追踪到实现和实际证据；全部完整门禁满足后才收口，性能收益遵守相对 3% 与帧预算规则。

**失败恢复：** 下层回归重开对应里程碑，保留先前结果但标失效；不通过删除测试/缩窄分母收口。

**文档交付：** 维护 docs/wiki、各模块 API/IR/artifact 文档及完整 acceptance record；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrSsaAcceptanceManifest SZrSsaAcceptanceManifest;
typedef struct SZrSsaAcceptanceResult SZrSsaAcceptanceResult;
TZrBool ZrTests_Ssa_ValidateAcceptance(const SZrSsaAcceptanceManifest *manifest,
    SZrSsaAcceptanceResult *result);
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| requirement mapping | 原 index 与各叶子 owner | 每项至少有正常/边界/失败验证 |
| actual evidence | 当前构建/运行/测量 | 绑定 revision、dirty digest、环境、工具链 |
| accepted state | 全范围门禁聚合 | focused-passed 不能冒充整里程碑 accepted |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 从原 index 逐段建立需求矩阵，登记所有 backend/平台/语义变体。

- [ ] **批次 2：** 运行完整底层到上层回归与工具矩阵，记录 existing failure 和新回归。

- [ ] **批次 3：** 复核性能口径、90% coverage、旧路径消费者清零、文档/API/schema 同步后收口。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange one required interface accessor variant missing
assert milestone remains open
arrange all smoke passes but TSan finds race
assert M2 acceptance fails
arrange performance baseline from different compiler/options
assert comparison rejected even if percentage looks positive
```

### 迁移结束检查

最终记录应能从任何需求跳到实际测试和日志，不能仅链接到计划文本。未完成项保留 owner、下一个验证步骤和依赖，不从清单移除。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。

