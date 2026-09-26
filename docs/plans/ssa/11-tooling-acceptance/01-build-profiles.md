---
related_code:
  - zr_vm_cli/src/zr_vm_cli/project/project.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_content_hash.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_project_provider.c
  - scripts/benchmark/benchmark_task4_contract.py
implementation_files:
  - zr_vm_cli/src/zr_vm_cli/project/project.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_content_hash.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_project_provider.c
  - scripts/benchmark/benchmark_task4_contract.py
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_optimization_profile.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_ir_cache.c
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/parser/test_ssa_build_profiles.c
doc_type: milestone-detail
status: planned
---

# 11.01 构建 Profile、增量缓存与脚本治理

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 以构建 profile 和增量缓存自动选择优化强度，建立可复现脚本治理，避免业务人员逐函数调参。

**Architecture：** 配置先规范化为 immutable compile policy，再作为 IR/profile/artifact/cache key 的一部分；开发低延迟，发布深优化，帧预算是可组合约束。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M0/M6/M7 工程治理。
- 前置：[00.02 共享契约、语义边界与版本冻结](../00-measurement-contracts/02-contract-freeze.md)；[02.01 Pass 管理、SCCP、复制传播与 DCE](../02-automatic-optimization/01-pass-manager-scalar.md)；[08.01 版本化 Artifact 与无地址 Relocation](../08-artifact-hotpatch/01-schema-relocation.md)。
- 交付：dev/interactive/release/LTO/PGO/fast-math/frame-safe/平台/host-jit profiles 和缓存依赖图。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

CLI project 和 compile_tool 已有项目 provider/content hash，应扩展既有配置，不创建旁路脚本通过环境变量偷偷开启优化。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_cli/src/zr_vm_cli/project/project.c` | 统一配置解析和默认值 |
| 现有，修改/复用 | `zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_content_hash.c` | 复用内容 hash |
| 现有，修改/复用 | `zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_project_provider.c` | 增量构建依赖/项目环境 |
| 现有，修改/复用 | `scripts/benchmark/benchmark_task4_contract.py` | 复用基准环境与发布合同 |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/compiler/compile_optimization_profile.c` | 规范化策略和冲突诊断 |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/compiler/compile_ir_cache.c` | IR/摘要/backend cache 的依赖失效 |
| 计划新增测试 | `tests/parser/test_ssa_build_profiles.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 拆分配置维度** build mode、numeric permission、frame budget、target/backend 是独立维度；便捷 profile 展开后给出 effective config，不能让 release-fast-math 隐式允许新增能力。

- [ ] **2. 设计缓存 key** source/dependency content、canonical contract、compiler ABI、pass pipeline、numeric/layout/target、profile hash 一并进入 key；运行期地址/generation witness 不入可移植缓存。

- [ ] **3. 实现失效和并发写入** 按函数/模块摘要决定增量范围，缺失依赖则扩大重编；并发构建用临时产物+原子 rename，取消不留下 accepted cache，损坏缓存删除单项后重建。

- [ ] **4. 建立治理门禁** dry-run 显示将运行的 passes/backends；脚本记录环境、返回码、产物 hash、baseline，避免 silent fallback。CI 分 focused/full/perf/platform，完整失败不可被 smoke 覆盖。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
EffectivePolicy = normalize(buildMode, numericPermission, frameBudget, target)
CacheKey = hash(source, dependencies, contracts, compilerABI, passPipeline,
                target, numericPolicy, importedProfileHash)
cacheHit only if allDependenciesValidated
build -> writeTemporary -> validate -> atomicPublishCacheEntry
cancel/failure -> noPublishedEntry
```

编译时间/内存本身受预算；自动优化收益不能依赖开发环境每次全量 LTO。缓存命中必须可解释，旧 profile 失效不影响语义正确性。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| 任一 signature/layout/compiler/profile 变化 | 对应 cache 失效 |
| dev+host-jit 与 mobile+host-jit | 前者按能力处理，后者明确冲突 |
| 并发写缓存、取消/损坏 | 无半产物命中 |
| 无任何函数标注的项目 | profiles 能驱动优化并显示 effective policy |

本任务新增测试先独立运行，再进入完整 SSA 差分矩阵。

登记新 CTest 名 `ssa_build_profiles` 和可执行目标 `zr_vm_ssa_build_profiles_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_build_profiles_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_build_profiles$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** 所有配置有默认/冲突/继承规则；重复构建可复现，增量不漏重编，pipeline mode 与报告一致。

**失败恢复：** 单项缓存可安全忽略重建；不降级 hash 校验以维持命中率。

**文档交付：** 新增 docs/cli-and-tooling/optimization-build-profiles.md；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrCompileOptimizationProfile SZrCompileOptimizationProfile;
typedef struct SZrCompileCacheKey SZrCompileCacheKey;
TZrBool ZrParser_Compile_NormalizeOptimizationProfile(
    SZrCompileOptimizationProfile *profile, SZrExecIrDiagnostic *diagnostic);
TZrBool ZrParser_Compile_BuildIrCacheKey(const SZrCompileOptimizationProfile *profile,
    SZrCompileCacheKey *key, SZrExecIrDiagnostic *diagnostic);
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| effective profile | CLI/project normalization | parser/backend 不重复解释配置 |
| dependency hashes | content/contract graph | 增量缓存不遗漏 imported summary |
| cache publish state | build transaction | 只有验证成功的完整产物可命中 |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 定义 profile 展开和冲突规则，CLI dry-run 显示 effective settings。

- [ ] **批次 2：** 实现分层 key 与依赖失效，先比全量构建结果一致，再启并发写 cache。

- [ ] **批次 3：** 接 CI focused/full/perf/platform 和工程报告，治理 flaky/noisy 与 silent fallback。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange change imported signature without changing caller source
assert caller cached IR invalidated as required
arrange two builds race to publish same key
assert accepted cache is complete and deterministic
arrange project fast-math permission absent
assert compiler cannot enable it through hidden environment defaults
```

### 迁移结束检查

构建脚本只编排正式接口，不在脚本里重建类型或 capability 逻辑。缓存损坏只影响该 key，不能递归清空用户工作区。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。

