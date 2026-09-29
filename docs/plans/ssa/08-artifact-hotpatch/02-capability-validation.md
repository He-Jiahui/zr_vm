---
related_code:
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_core/src/zr_vm_core/metadata_runtime_method_binding.c
  - zr_vm_core/src/zr_vm_core/module/module_loader.c
implementation_files:
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_core/src/zr_vm_core/metadata_runtime_method_binding.c
  - zr_vm_core/src/zr_vm_core/module/module_loader.c
  - zr_vm_core/include/zr_vm_core/capability_manifest.h
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_validate.c
  - zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_capability.c
  - zr_vm_core/include/zr_vm_core/hotpatch.h
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/library/test_ssa_capability_validation.c
doc_type: milestone-detail
status: planned
---

# 08.02 Capability manifest 与 Patch 验证

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 建立宿主冻结的 AOT capability allowlist，并在 Patch 执行前验证权限、ABI 和完整性。

**Architecture：** 安全能力独立于 effect 优化摘要；patch 必须是宿主授权、基包已发布能力的子集，验证覆盖直接/间接/反射/native/import 调用。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M5。
- 前置：[08.01 版本化 Artifact 与无地址 Relocation](../08-artifact-hotpatch/01-schema-relocation.md)；[07.01 共享 AOTIR 与后端 ABI 收敛](../07-aot-backends/01-aotir-contract.md)。
- 交付：manifest schema、签名信任接口、闭包能力分析与 precise rejection。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

native_binding.h 现有功能 flags 不能当作安全白名单；必须新增独立的宿主授权模型。符号名称相同不能证明相同权限或 ABI。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_library/include/zr_vm_library/native_binding.h` | 标明功能元数据与安全授权的分界 |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/metadata_runtime_method_binding.c` | 将目标 token 对应已授权能力 |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/module/module_loader.c` | 接入验证，不先发布 |
| 计划新增 | `zr_vm_core/include/zr_vm_core/capability_manifest.h` | 冻结 allowlist 和 native/import/type/dispatch 范围 |
| 计划新增 | `zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_validate.c` | base/hash/signature/ABI/capability 验证 |
| 计划新增 | `zr_vm_core/src/zr_vm_core/hotpatch/hotpatch_capability.c` | 跨函数调用能力闭包 |
| 计划新增 | `zr_vm_core/include/zr_vm_core/hotpatch.h` | Validate/Apply/Rollback 状态 API |
| 计划新增测试 | `tests/library/test_ssa_capability_validation.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 定义宿主权威** host config 确定 allowlist，构建器只能证明程序需求属于 allowlist；自动发现需求不能自动扩大授权。manifest 绑定 base artifact、target 和 ABI。

- [ ] **2. 接入签名信任根** 使用现有可信密码库/宿主验签回调，不自写加密；签名覆盖 canonical header、section hashes、base identity、patch id 和数值 profile。验签前限额解析，验签后深度验证。

- [ ] **3. 计算实际需求闭包** 检查所有函数体和间接调用上界；typed function signature 不是权限许可。native callbacks、reflection、dynamic target 都经受控入口检查。

- [ ] **4. 执行子集规则** 先求 required ∩ allowed 并要求等于 required；超出即拒绝，不能静默裁掉能力后运行。公开 signature/layout/native import/scheduler/domain 权限不得新增。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
allowed = hostAllowlist ∩ baseAotManifest
required = deriveTransitiveRequirements(patchVerifiedIR, bindingGraph)
if !verifySignature(trustRoot, signedCanonicalContent): reject(SIGNATURE)
if patch.baseIdentity != loadedBase: reject(BASE_IDENTITY)
if required - allowed != empty: reject(CAPABILITY_ESCALATION)
if changesPublicABIOrAddsImport: reject(CONTRACT_CHANGE)
validatedPatch = immutableOwnedCopy(bytes, verificationResult)
```

防 TOCTOU：验证结果绑定不可变内容/hash，Apply 不接受验证后可被修改的 buffer。签名认证来源不代替语义/资源/能力验证。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| 新增 native/FFI/reflection/domain 权限 | 执行前拒绝 |
| 通过已允许 wrapper 间接访问禁能力 | 闭包或入口检查拒绝 |
| 有效签名但 base/layout 不匹配 | 拒绝 |
| 验证后修改 buffer/伪造摘要 | hash/不可变对象约束拦截 |
| 4097 项 requirement、实际仅提供一个槽 | 签名 callback 与逐项扫描前返回 `ZR_HOT_PATCH_LIMIT`，expected=4096、actual=4097，validated 清零；完整 4096 项边界继续接受 |

当前已完成的窄切片只统一主 `Validate` 与能力闭包的 requirement 数量上限，证据见 [requirement limit acceptance](../../../../tests/acceptance/ssa-hotpatch-requirement-limit.md)。其余签名内容不可变副本、TOCTOU 防护、完整调用图能力分析和发布隔离门禁仍属于本计划未完成项。

本任务新增测试先独立运行，再进入完整 SSA 差分矩阵。

登记新 CTest 名 `ssa_capability_validation` 和可执行目标 `zr_vm_ssa_capability_validation_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_capability_validation_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_capability_validation$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** 受限解释器不能绕过宿主 allowlist；每个拒绝包含 capability/token/source，未授权代码不执行。

**失败恢复：** 验证失败不创建可调用 generation，不改变运行中模块；保留明确失败记录。

**文档交付：** 新增 docs/module-system/hotpatch-capability-contract.md；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrHotPatchInput SZrHotPatchInput;
typedef struct SZrValidatedHotPatch SZrValidatedHotPatch;
typedef struct SZrHotPatchDiagnostic SZrHotPatchDiagnostic;
TZrBool ZrCore_HotPatch_Validate(const SZrHotPatchInput *input,
    SZrValidatedHotPatch *validated, SZrHotPatchDiagnostic *diagnostic);
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| trust roots/allowlist | 宿主和已签 base manifest | patch 不能修改自己的授权上界 |
| transitive required capability | verified IR/call graph | 间接调用未知上界不能忽略 |
| validated immutable bytes | validation transaction | Apply 绑定内容身份防 TOCTOU |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 实现 manifest codec 与 host allowlist，区分优化 effect bits 与安全 capability。

- [ ] **批次 2：** 接可信验签、base/target/ABI/hash 校验和能力闭包；错误含 token 与 source。

- [ ] **批次 3：** 覆盖 direct/virtual/interface/delegate/reflection/native 路径，验证 cached lane 同样不可绕过授权。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange signed patch uses allowed wrapper that calls forbidden native import
assert capability closure or protected import gate rejects before forbidden effect
arrange required={A,B},allowed={A}
assert rejected; never silently truncate to {A}
arrange change validated input bytes before apply
assert immutable copy/hash binding prevents installation
```

### 迁移结束检查

签名有效不代表语义安全或能力合法。reflective construction、dynamic invoke 和 native callback 是独立攻击入口，必须通过同一 allowlist。

验证完成后交给 Apply 的对象包含内容所有权、验证策略版本、base identity、required capability digest 和可执行投影身份。缓存的 ExecBC 按 08.01 的派生关系验证或重新生成；能力分析的对象必须就是最终执行的代码。不得对一份 IR 做 allowlist 校验后执行另一份未验证 bytecode。

资源限制同样在执行前明确：函数/block/operand 数量、递归验证深度、解压字节、签名算法与 key-id 集合、IR 分析工作预算均有上限。它们是 loader 的健壮性约束，不是由 patch 声明可关闭的优化选项。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。
