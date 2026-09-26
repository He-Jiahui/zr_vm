---
related_code:
  - zr_vm_core/src/zr_vm_core/hash_set.c
  - zr_vm_core/src/zr_vm_core/string.c
  - zr_vm_core/src/zr_vm_core/string_builder.c
  - zr_vm_core/src/zr_vm_core/hash.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/hash_set.c
  - zr_vm_core/src/zr_vm_core/string.c
  - zr_vm_core/src/zr_vm_core/string_builder.c
  - zr_vm_core/src/zr_vm_core/hash.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_container_specialize.c
  - zr_vm_core/src/zr_vm_core/object/container_storage_contract.c
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/core/test_ssa_maps_strings.c
  - tests/core/test_hash_set_dense_paths.c
  - tests/core/test_string_layout.c
  - tests/core/test_string_builder.c
doc_type: milestone-detail
status: planned
---

# 05.03 Map 与字符串的布局和分配

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 减少 map/string 热路径上的哈希、分配和复制，保持 equality、迭代和字符串语义。

**Architecture：** map 优先紧凑 entry/bucket 与缓存 hash；string 分别评估 intern、短串、builder/rope，通过通用 layout/effect 能力接入，按测量选择。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M2 容器优化；M6 自动变换。
- 前置：[05.01 对象 Shape 与内部布局映射](../05-data-layout/01-objects-layout-maps.md)；[02.03 逃逸、生命周期与分配消除](../02-automatic-optimization/03-escape-ownership.md)。
- 交付：map/string 成本基线、可复用存储快路、循环构建识别及限制。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

hash_set.c、string.c、string_builder.c 已存在，已有 dense-path/string layout 测试。先审计实现，避免再做一套 intern 或 builder。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/hash_set.c` | 缓存 hash/紧凑 bucket/迭代稳定性 |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/string.c` | 字符串 hash/intern/短串 |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/string_builder.c` | 容量复用和拼接 |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/hash.c` | 复用 hash 算法和 seed |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/passes/exec_ir_container_specialize.c` | 根据协议选择稳定 key/构建模式 |
| 计划新增 | `zr_vm_core/src/zr_vm_core/object/container_storage_contract.c` | 公开 layout/effect capability |
| 计划新增测试 | `tests/core/test_ssa_maps_strings.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 固定容器语义** 确认自定义 hash/equality 是否可抛/有副作用、迭代顺序、删除 tombstone、key mutation 与编码规则；不能因类型稳定就省略用户 equality 调用。

- [ ] **2. 优化存储与 hash** 稳定不可变 key 可缓存 hash，记录 seed/domain；紧凑 entries 扩容时保留 GC roots、迭代约定和删除语义，碰撞情况必须计入基准。

- [ ] **3. 减少字符串中间值** 只在中间串不逃逸且 identity/异常顺序不变的拼接循环用 builder；短串/intern 复用现有机制，rope 仅作为测得收益后的独立 storage variant。

- [ ] **4. 测量收益与成本** 分别测试短/长串、Unicode、collision、低容量、多次扩容；报告分配字节、hash/equality 次数、RSS 与延迟，不只吞吐。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
specializeMap only if keyHashAndEqualityContractStable
cachedHashKey = (immutableKeyIdentityOrValue, hashSeedVersion)
concatLoop -> Builder only if intermediatesDoNotEscape && effectsEquivalent
ropeCandidate requires measuredBenefit && boundedFlattenCost
unknownCustomEquality -> preserveGenericCallAndExceptionOrder
```

缓存 hash 不意味着绕过碰撞 equality；intern 表不能导致无限强引用保活。rope flatten 不能制造未预算的大帧停顿。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| 极端碰撞与自定义有副作用 equality | 结果/调用顺序正确 |
| 删除后重插与迭代 | 约定顺序不变 |
| Unicode、空串、很长拼接 | 内容/编码语义一致 |
| 中间串被保留或比较 identity | 不做破坏可观察性的 builder 变换 |

复用回归入口：`tests/core/test_hash_set_dense_paths.c`、`tests/core/test_string_layout.c`、`tests/core/test_string_builder.c`。历史计数只作为核对线索，实施时重跑并记录实际总数。

登记新 CTest 名 `ssa_maps_strings` 和可执行目标 `zr_vm_ssa_maps_strings_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_maps_strings_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_maps_strings$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** 各存储策略有真实收益证据与负测；rope/短串不是必然同时启用的要求，而是测量驱动的候选。

**失败恢复：** storage contract 保留通用实现；策略收益不足则不启用，不影响语义优化框架交付。

**文档交付：** 新增 docs/core-runtime/map-string-storage-optimization.md；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrContainerSpecializationFacts SZrContainerSpecializationFacts;
TZrBool ZrParser_ExecIr_SpecializeContainers(SZrExecIrFunction *function,
    const SZrContainerSpecializationFacts *facts,
    SZrExecIrDiagnostic *diagnostic);
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| hash/equality effects | type/protocol summary | 只有稳定无副作用 hash 才可安全缓存 |
| hash seed/version | domain/container runtime | 跨域复制不盲目复用源 seed hash |
| string escape/identity | SSA escape analysis | builder 替换不改变中间值可观察性 |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 分别审计 map dense path、string intern/SSO/builder 当前覆盖，不重新造轮子。

- [ ] **批次 2：** 实现 cache/compact 策略并覆盖 collision/tombstone/扩容；独立评估退化输入。

- [ ] **批次 3：** 用非逃逸 concat loop 试点 builder lowering，rope 只有测得实用收益且有 flatten 预算才启。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange two distinct keys with same hash
assert equality still runs and keys remain distinct
arrange cross-domain clone with different hash seed
assert destination hash recomputed or correctly rekeyed
arrange intermediate string escapes to list
assert concat-to-builder optimization rejected
```

### 迁移结束检查

迭代顺序、自定义 equality/hash 副作用是语言合同的一部分。容器 optimization remark 应指出具体阻断因素，不能给“类型不支持”笼统结论。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。

