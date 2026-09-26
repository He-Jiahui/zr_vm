---
related_code:
  - zr_vm_core/include/zr_vm_core/call_binding.h
  - zr_vm_core/include/zr_vm_core/artifact_schema.h
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  - zr_vm_parser/CMakeLists.txt
implementation_files:
  - zr_vm_core/include/zr_vm_core/call_binding.h
  - zr_vm_core/include/zr_vm_core/artifact_schema.h
  - zr_vm_common/include/zr_vm_common/zr_aot_abi.h
  - zr_vm_parser/CMakeLists.txt
  - zr_vm_core/include/zr_vm_core/exec_ir.h
  - zr_vm_core/include/zr_vm_core/execution_backend.h
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/parser/test_ssa_contract_freeze.c
doc_type: milestone-detail
status: planned
---

# 00.02 共享契约、语义边界与版本冻结

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 统一术语、数据所有权、能力安全边界和版本变更，消除跨子计划的歧义。

**Architecture：** ExecIR 是唯一执行语义源；ExecBC 与 AOTIR 是投影，直接 IR interpreter 只作 oracle。共享数据放 core 低层，构造/优化放 parser，后端不反向恢复语义。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M0 契约冻结；M1/M4/M5/M7 共同前置。
- 前置：[00.01 基线、性能指标与真实执行模式](../00-measurement-contracts/01-baseline-metrics.md)。
- 交付：共享契约目录、API 所属层、状态/错误枚举、序列化边界、兼容性规则和能力冻结方式。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

已存在 CallBinding schema 1、artifact schema 5、AOT ABI 16。compiler_semir.c 明确是 ExecBC 之后的兼容投影，而 compiler.h 的 preSemanticIr 位于 ExecBC 之前。zr_vm_parser/CMakeLists.txt 已显式编译 zr_vm_aot 下的后端源码，不能因为根 CMake 没有 add_subdirectory(zr_vm_aot) 就判定 AOT 未接入。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_core/include/zr_vm_core/call_binding.h` | 复用 contract/target 分离与结构化错误 |
| 现有，修改/复用 | `zr_vm_core/include/zr_vm_core/artifact_schema.h` | 登记序列化版本和必选 section |
| 现有，修改/复用 | `zr_vm_common/include/zr_vm_common/zr_aot_abi.h` | 冻结 frame/native/code registration ABI |
| 现有，修改/复用 | `zr_vm_parser/CMakeLists.txt` | 确认后端源码所有权与低层依赖 |
| 计划新增 | `zr_vm_core/include/zr_vm_core/exec_ir.h` | 低层无 AST 依赖的 IR 定义，细节由 01.01 实施 |
| 计划新增 | `zr_vm_core/include/zr_vm_core/execution_backend.h` | 后端状态与运行期 handle 接口，细节由 10.01 实施 |
| 计划新增测试 | `tests/parser/test_ssa_contract_freeze.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 建立字段所有权表** 每个 token/hash/id 指定唯一生产方、读者、生命周期和 hash 输入；TypeId 运行期映射与稳定 metadata token 明确区分。

- [ ] **2. 冻结语义术语** CoreIR 统一作为 ExecIR 的旧称；严格数值由当前语言规则逐项定义；优化推断失败只保留合法原语义，不把非法 borrow 或静态缺失 contract 放行。

- [ ] **3. 冻结域和热更规则** 隔离域只复制对象图，不共享 GC 指针或 ownership control；Transfer 是复制事务协议。同域共享必须证明访问和生命周期安全。公开 ABI 不变且可热更目标经过版本入口单元，冻结目标才可无条件内联。

- [ ] **4. 冻结版本与接口归属** schema/ABI 取实施时最新值的下一版本，v6/17 仅当前候选；磁盘 generation 是逻辑版本，加载分配本进程 epoch。返回状态区分 invalid IR/link/capability/deopt/unsupported/compile pending。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
PersistentContract = {token, signatureHash, layoutHash, moduleHash, logicalVersion}
RuntimeWitness = {loadEpoch, generation, taggedTarget, tracedContext}
ResolvedTarget = VM | Native | AOT | HostJit
StaticLinkFailure -> structured LinkError
ValidSpeculationFailure -> baseline operation using same token/slot
ExplicitDynamicOperation -> existing dynamic semantics
CrossDomain(value) -> validateGraph -> cloneIntoDestination -> publishCopy
EffectSummary != SecurityCapabilitySet
```

GC budget 是可观测的工作预算而非硬实时承诺；没有读屏障/句柄协议时不得在半完成 relocation 中恢复 mutator。同步 API 不能偷偷变 async，协程化只发生在已允许 suspend 的调用边界。fast-math 不默认开启。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| 同 token 但 signature/layout 不同 | 链接前拒绝并报告 expected/actual |
| Speculation guard miss 与 stale generation | 分别 deopt 与 link error，均无静态字符串查找 |
| 跨域 borrowed/native thread-affine handle | 复制预检拒绝或用显式 host materializer 复制独立资源 |
| 开启热更同时跨目标内联 | 缺少 invalidation/deopt 依赖时禁止内联 |

本任务新增测试先独立运行，再进入完整 SSA 差分矩阵。

登记新 CTest 名 `ssa_contract_freeze` 和可执行目标 `zr_vm_ssa_contract_freeze_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_contract_freeze_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_contract_freeze$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** 每个主计划接口族和持久化字段具有唯一所有者；边界歧义有明确策略；新增 API 先标 planned，禁止伪称已实现。

**失败恢复：** 契约未稳定前禁止发布新 schema；变更草案联动修改读写端和测试，不能靠接收任意版本解决兼容问题。

**文档交付：** 维护本目录的契约补充及 docs/core-runtime、docs/module-system 对应设计文档；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrExecIrDiagnostic {
    TZrUInt32 code, functionToken, blockId, instructionId, sourceId;
    TZrUInt64 expectedHash, actualHash;
} SZrExecIrDiagnostic;
typedef struct SZrExecutionContract SZrExecutionContract;
TZrBool ZrCore_ExecutionContract_Check(const SZrExecutionContract *expected,
    const SZrExecutionContract *actual, SZrExecIrDiagnostic *diagnostic);
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| Type/Layout/Signature hash | canonical metadata producer | parser/core/backend 不各算不同字段集合 |
| Runtime generation/target | loader/version manager | 只在运行期有效，不从磁盘恢复可信地址 |
| Capability allowlist | host/base manifest | effect 推导只能减少需求，不能增加授权 |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 写字段字典：类型、宽度、有效域、hash 编码顺序、生产/消费模块；为重复术语建立唯一名称。

- [ ] **批次 2：** 用现有 binding/artifact/AOT ABI 对照字段字典，明确下一版改动而非盲目固定 v6/17。

- [ ] **批次 3：** 完成兼容矩阵：源码重编、旧 artifact 拒绝、profile 失配忽略、patch contract 失配拒绝。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange identical token but different layoutHash
assert contract check fails before resolving pointer
arrange importedProfile.compilerABI differs
assert profile ignored; compilation remains legal
arrange old artifact schema
assert RECOMPILE_REQUIRED; no partial module published
```

### 迁移结束检查

公共接口归属必须写清：Build/Optimize/Lower/ReadWrite 的实现由 parser 驱动，低层结构/verifier/runtime resolution 在 core；不要让 core include parser AST。00.02 仅冻结 exec_ir.h/execution_backend.h 的所有权，正式建文件分别由 01.01/10.01 执行。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。

