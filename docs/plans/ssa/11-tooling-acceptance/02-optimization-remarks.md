---
related_code:
  - zr_vm_cli/src/zr_vm_cli/command/command.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_diagnostic_projection.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_registry.c
implementation_files:
  - zr_vm_cli/src/zr_vm_cli/command/command.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_diagnostic_projection.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_registry.c
  - zr_vm_core/include/zr_vm_core/optimization_remark.h
  - zr_vm_core/src/zr_vm_core/optimization_remark.c
  - zr_vm_cli/src/zr_vm_cli/commands/explain_optimize_command.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_optimization_remarks.c
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/parser/test_ssa_optimization_remarks.c
doc_type: milestone-detail
status: planned
---

# 11.02 优化说明 CLI、JSON 与 LSP

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 提供可定位、可区分证明与测量的优化说明，帮助用户理解自动优化及其阻断原因。

**Architecture：** pass/codegen/runtime 共用结构化 remark schema；CLI/JSON/LSP 只是查询投影，源事实由 parser/IR 持有，不在 IDE 重新推断。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M6。
- 前置：[02.05 循环优化、PGO 与专门化预算](../02-automatic-optimization/05-loops-specialization.md)；[09.01 自动语义摘要与通用能力协议](../09-language-simd/01-inferred-protocols.md)；[11.01 构建 Profile、增量缓存与脚本治理](../11-tooling-acceptance/01-build-profiles.md)。
- 交付：SZrOptimizationRemark、zr explain optimize 与 --json、LSP 查询/缓存失效。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

LSP 已有 semantic/project/diagnostic projection，其他任务正在维护；这里只新增优化查询接口并复用 canonical source ranges，不能拷贝一套 AST 分析。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_cli/src/zr_vm_cli/command/command.c` | 增加 explain optimize 子命令路由 |
| 现有，修改/复用 | `zr_vm_language_server/src/zr_vm_language_server/interface/lsp_diagnostic_projection.c` | 关联 canonical source 范围 |
| 现有，修改/复用 | `zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_registry.c` | 统一 reason code 登记 |
| 计划新增 | `zr_vm_core/include/zr_vm_core/optimization_remark.h` | 跨编译/runtime 的稳定记录 |
| 计划新增 | `zr_vm_core/src/zr_vm_core/optimization_remark.c` | Query、分页、版本和生命周期 |
| 计划新增 | `zr_vm_cli/src/zr_vm_cli/commands/explain_optimize_command.c` | 文本/JSON 命令 |
| 计划新增 | `zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_optimization_remarks.c` | LSP 投影，不重复推断 |
| 计划新增测试 | `tests/parser/test_ssa_optimization_remarks.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 固定 schema** pass/source/status/reason/before/after/backend/profileCount/cost/proof/boxing/allocation/cache/deopt 字段；增加 evidenceKind=proven/estimated/measured 与 module/IR/source version。

- [ ] **2. 接入 producers** pass 成功和失败都发记录；runtime miss 通过 stable site key 关联 profile，区别软件 IC miss 与 PMU hardware cache miss，不混用原因。

- [ ] **3. 实现 CLI/JSON** 支持模块筛选、reason/backend/范围过滤、分页和稳定排序；JSON version 固定，文本说明原因及影响，不要求用户增加注解。

- [ ] **4. 实现 LSP/IDE** 请求携文档版本，取消与增量重编清除旧 remark；跳转到 source range，布局变换可展示 logical/physical map，unknown 数据不填估计真值。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
Remark = {schemaVersion, moduleHash, irHash, sourceVersion, sourceRange,
          pass, status, reasonCode, before, after, backendMask,
          evidenceKind, proofId, profileCount, estimatedCost, measuredCounters}
status = success | missed | blocked
reason = alias_unknown | escapes | abi_visible | effect_order | code_budget
         | profile_stale | target_unsupported | capability_denied
query(sourceVersion) returns only matching records
```

静态工具不能声称知道具体 hardware cache miss 的动态原因；可以说明布局阻断和已测计数，推论必须标 inferred。用户源码中的性能解释不夹入内部实现任务。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| boxing/bounds/vector/barrier/inlining/AOT/deopt/layout | 均有具体原因和源位置 |
| 没有 PMU/profile | estimated/unavailable，不伪造 measured |
| 编辑文件后旧编译完成 | LSP 丢弃过期结果 |
| CLI text 与 JSON/LSP | 相同 reason/status/source identity |

本任务新增测试先独立运行，再进入完整 SSA 差分矩阵。

登记新 CTest 名 `ssa_optimization_remarks` 和可执行目标 `zr_vm_ssa_optimization_remarks_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_optimization_remarks_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_optimization_remarks$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** 主要成功和拒绝变换可解释；自动优化不以 IDE 重新分析为条件；JSON 消费者有 schema 兼容测试。

**失败恢复：** 收集可按预算限量，计数给 truncated 状态；关闭展示不影响编译语义和诊断错误。

**文档交付：** 新增 docs/cli-and-tooling/optimization-remarks.md；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrOptimizationRemarkQuery SZrOptimizationRemarkQuery;
typedef struct SZrOptimizationRemarkPage SZrOptimizationRemarkPage;
TZrBool ZrCore_OptimizationRemarks_Query(const SZrOptimizationRemarkQuery *query,
    SZrOptimizationRemarkPage *page, SZrExecIrDiagnostic *diagnostic);
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| success/missed/blocked | pass/codegen/runtime 生产者 | UI 不重新推断优化结论 |
| source/document version | canonical source map | 过期 LSP 结果不发布 |
| proven/estimated/measured | 证据生产源 | 软件 IC miss 和硬件 cache miss 分开 |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 先固定 schema/reason codes 与 JSON golden fixtures，代码消费者用稳定 enum。

- [ ] **批次 2：** 接所有 pass 和 runtime site 计数，测试 missing profile/unsupported target/no evidence。

- [ ] **批次 3：** 实现 CLI 文本/JSON 与 LSP 分页、取消、版本校验，验证三个消费者一致。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange no PMU data but estimated SoA locality benefit
assert evidenceKind=estimated and hardwareCacheMiss=unavailable
arrange LSP document version changes before result arrives
assert stale result discarded
arrange same query through text/JSON/LSP
assert identical reason/source/backend identity
```

### 迁移结束检查

优化说明不是普通编译错误；unknown optimization fact 不能作为红色错误阻止合法代码。性能提示预算溢出应标 truncated，不能静默丢失导致用户误以为无问题。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。

