---
related_code:
  - zr_vm_common/include/zr_vm_common/zr_instruction_conf.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_fusion.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_fusion_match.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_fusion_match_types.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_fusion_contract.c
implementation_files:
  - zr_vm_common/include/zr_vm_common/zr_instruction_conf.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - scripts/codegen/generate_execbc_patterns.py
  - zr_vm_parser/src/zr_vm_parser/exec_ir/execbc_patterns.def
  - zr_vm_parser/include/zr_vm_parser/execbc_fusion_patterns_generated.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_fusion.c
  - zr_vm_parser/include/zr_vm_parser/exec_ir_fusion.h
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_fusion_match.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_fusion_match_types.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_fusion_contract.c
  - zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_fusion_lifecycle.c
plan_sources:
  - docs/plans/ssa/index.md
  - "user: 2026-09-12 按方向拆解 SSA 计划并提供重构指导"
tests:
  - tests/parser/test_ssa_generated_fusion.c
  - tests/parser/test_ssa_generated_fusion_compare_branch.inc
  - tests/parser/test_ssa_generated_fusion_increment_branch.inc
  - tests/cmake/ssa-tests.cmake
  - tests/parser/test_semir_typed_opcode_guardrails.c
  - tests/parser/test_compiler_w2_performance_quickening.c
  - tests/parser/test_compiler_w2_quickening_array_add.inc
  - tests/acceptance/ssa-quickening-array-int-add.md
  - tests/acceptance/2026-09-29-ssa-compare-branch-mode.md
  - tests/acceptance/2026-09-29-ssa-increment-loop-branch-types.md
doc_type: milestone-detail
status: planned
---

# 03.04 自动组合指令与 ExecBC 编码投影

> 执行时使用 `executing-plans` 逐项推进；本文件是重构计划，未勾选项不代表已实现。代码段是算法/接口草案；新增符号由本任务实现，现有接口必须复用实际声明。

**Goal：** 从 ExecIR 自动选择高频组合指令，减少 dispatch 次数并保留精确语义映射。

**Architecture：** 小型模式规则生成 matcher/dispatch metadata/source-map 投影，使用固定宽度 SZrInstruction 与 side tables，不更改整条变长编码。

**Tech Stack：** C11、CMake、Unity/CTest；共享 ExecIR 与现有 ZR runtime。

## 依赖与交付范围

- 对应主计划：M3。
- 前置：[03.01 Dispatch 拆分、状态缓存与冷路径](../03-interpreter-binding/01-dispatch-boundaries.md)；[03.03 已解析目标、PIC 与 Guard 失效](../03-interpreter-binding/03-guarded-caches.md)；[02.01 Pass 管理、SCCP、复制传播与 DCE](../02-automatic-optimization/01-pass-manager-scalar.md)。
- 交付：模式描述、生成器、规则验证、融合前后差分和尺寸预算。
- 统一约束、测试命令与状态定义见 [00.02](../00-measurement-contracts/02-contract-freeze.md) 和 [00.03](../00-measurement-contracts/03-differential-harness.md)。每个子任务的编译依赖来自显式前置；未满足完整门禁时不得标记里程碑完成。

## 现状与代码落点

zr_instruction_conf.h 已有复杂 opcode；先列出现有组合指令的可复用语义，不新增近义 opcode 或按具体类名匹配。

| 类别 | 路径 | 责任与修改边界 |
| --- | --- | --- |
| 现有，修改/复用 | `zr_vm_common/include/zr_vm_common/zr_instruction_conf.h` | 保留现有编码并生成 metadata |
| 现有，修改/复用 | `zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c` | 逐步替换手写选择分支 |
| 现有，修改/复用 | `zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c` | 消费生成的 handler 对应表 |
| 计划新增 | `scripts/codegen/generate_execbc_patterns.py` | 从规则生成 matcher 和 opcode 元数据 |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/execbc_patterns.def` | 语义模式、前提和成本 |
| 计划新增 | `zr_vm_parser/src/zr_vm_parser/exec_ir/exec_ir_fusion.c` | 匹配、预算与 source/side table 投影 |
| 计划新增测试 | `tests/parser/test_ssa_generated_fusion.c` | 下述正向、失败与状态转换断言；复用既有 harness。 |

新增文件登记到所属模块 CMake；测试登记到计划新增的 `tests/cmake/ssa-tests.cmake`，由 `tests/CMakeLists.txt` 单点 include。先迁移职责并保持行为，再接入新 contract；不要把新分析或慢路径追加到巨型 dispatch/quickening 文件。

## 可逐项执行的重构任务

- [ ] **1. 建立六类模式** load+typed arithmetic、compare+branch、index+load/store、binding+call、increment+loop branch、call+return；每条规则声明类型/effect/异常/cleanup 前提。

- [ ] **2. 守住融合边界** 单入口直线片段先实现；不得跨 debug stop、safepoint、exception region 或可能重入点，除非组合 handler 内保留等价子边界。

- [ ] **3. 生成固定格式输出** operand 放不下时用 side table 或不融合，branch offset/source PC 重新映射；artifact 压缩只是存储压缩，加载后仍固定宽度。

- [ ] **4. 评估成本和生成稳定性** 生成结果可重复且有过期检查；代码尺寸和命中率决定启用，superinstruction 数量上限与 PIC 预算统一治理。

## 核心算法与接口指导

以下草案固定输入、处理顺序和失败行为；名称不是已经存在的 API。将其拆成上述文件中的私有 helper，错误使用项目诊断对象，不能通过布尔成功吞掉具体原因。

```text
match(pattern, irWindow):
    require typedOperands && compatibleEffects && matchingExceptionRegion
    require noUnrepresentedSafepointOrDebugBoundary
    require operandsFitEncodingOrSideTable
    require benefitEstimate > dispatchAndCodeSizeCost
    emit(existingOrGeneratedOpcode, operandTableIndex)
    mapEachObservableSuboperationToOriginalSourceAndResumeId()
otherwise: emitUnfusedOperations()
```

融合不能重复 native/getter 求值，不能省略 index/null/overflow 的规定错误。首阶段不引入 16/32 位 opcode 前缀改造。

## 测试设计与验收

先用最小 fixture 固定预期，再接入实现；失败测试必须断言错误种类和 source/IR 位置，不能只断言“返回失败”。

| 输入或触发 | 必须断言的结果 |
| --- | --- |
| 六类模式各一个命中和一个前提不满足 | 命中正确，其他保持原序列 |
| 最大 operand/index 与长跳转 | side table 正确或不融合，不截断 |
| 融合内异常/断点 | 定位原操作，恢复不重复副作用 |
| 同输入两次生成 | 字节和 metadata hash 相同 |

复用回归入口：`tests/parser/test_semir_typed_opcode_guardrails.c`。历史计数只作为核对线索，实施时重跑并记录实际总数。

登记新 CTest 名 `ssa_generated_fusion` 和可执行目标 `zr_vm_ssa_generated_fusion_test` 后，在 WSL 仓库根运行：

```bash
cmake --build build/ssa-gcc-debug --target zr_vm_ssa_generated_fusion_test -j 4
ctest --test-dir build/ssa-gcc-debug -R '^ssa_generated_fusion$' --output-on-failure --no-tests=error
```

预期：目标构建成功，至少一个匹配测试执行，全部断言通过、退出码 0。构建目录初始化、Clang/MSVC 和 sanitizer 扩展命令见 00.03；不得把“未找到测试”当作通过。

**退出门禁：** 生成规则覆盖正常/边界/失败条件，代表集测量达到 00.01 要求才宣称收益。

**失败恢复：** 逐规则禁用，统一 IR 与原非融合序列始终可用；不能通过减少语义检查伪造 dispatch 收益。

**文档交付：** 新增 docs/instruction-generation/execbc-pattern-lowering.md；在 [总索引](../index.md) 关联的验收矩阵记录命令、版本、环境、实际覆盖和未通过项。性能收益只按 00.01 的同口径门槛判定。

## 函数级设计与实现批次

### 接口草案

下面的 `SZr*` 入参/结果类型由本任务或显式前置任务定义；这是待实现的 C 接口设计，不是当前仓库 API 清单。实现时使用已有错误、分配器和容器约定，不把草案整体复制成新的平行框架。

```c
typedef struct SZrExecBcPatternOptions SZrExecBcPatternOptions;
TZrBool ZrParser_ExecIr_SelectExecBcPatterns(SZrExecIrFunction *function,
    const SZrExecBcPatternOptions *options, SZrExecIrDiagnostic *diagnostic);
```

### 数据生产与消费

| 数据/状态 | 生产责任 | 消费责任及不变量 |
| --- | --- | --- |
| 模式 schema | execbc_patterns.def | matcher/metadata/测试枚举同源 |
| fused operand side table | lowering | 固定宽度指令只持索引，界限验证 |
| suboperation source/resume map | 融合规划 | 异常/断点/GC 仍定位原语义操作 |

### 建议实施批次

以下批次分别形成可审查改动。每批先固定测试输入和失败预期，再实现；只完成前一批不能提前标记整份计划完成。

- [ ] **批次 1：** 只复用现有 opcode 建立模式规则和生成检查，确认没有改变编码。

- [ ] **批次 2：** 逐类加入六种融合，前提不满足的反例必须保持原序列。

- [ ] **批次 3：** 联动 code budget/profile，测量 dispatch 减少与 I-cache/code size 代价；淘汰无收益模式。

### 可直接转为测试的断言草案

草案中的 arrange/act/assert 表达 fixture 操作，落地到本任务的 Unity 测试时使用项目真实构造 API；事件序列必须逐项比较，不只检查函数最终返回。

```text
arrange compare followed by branch with debug boundary between
assert fusion rejected unless boundary represented inside handler
arrange operand larger than in-instruction field
assert side-table projection or unfused output, no truncation
arrange fused load triggers null error
assert original load source and no later arithmetic effect
```

### 迁移结束检查

生成文件只由生成器维护，测试检查 regenerate 无 diff；不保留两份手工 opcode size/dispatch/operand 元数据。

本任务的 acceptance 至少附上：上述断言对应的测试名称、实际执行后端/平台、失败注入位置、verifier 输入/输出摘要，以及涉及所有权时的分配/释放或 lease 平衡。新增入口的 OOM、取消、重复调用和部分初始化退出应有明确处理；不适用的状态写明原因。

## Scoped Progress — 2026-09-29

The `Array<int>.add` late-matcher slice is complete. It recognizes the direct
`KNOWN_NATIVE_MEMBER_CALL` form only when the same-site `MEMBER_GET` cache,
validated native provider contract, module relocation, and typed receiver/value
facts all agree. The rewrite retires the replaced callsite's binding contract
and relocation before final callsite linking.

The focused W2 quickening suite passed 20/20. GCC CTest passed
`call_binding_pipeline`, `call_binding_artifact`, and
`parser_span_inline_receiver` (3/3); the root agent also verified the direct
quickening suite and call-binding pipeline/artifact tests with MSVC. See
[`ssa-quickening-array-int-add.md`](../../../../tests/acceptance/ssa-quickening-array-int-add.md)
for the exact build commands and observed results.

The repository already has a separate six-pattern ExecIR fusion projection and
generated metadata. The `Array<int>.add` slice did not change that projection or
prove its execution through the runtime dispatch path. Remaining gates include executable
dispatch integration, end-to-end source/resume behavior, code-size/profile
budget evidence, and the plan-wide boundary and failure matrix. This entry
records one verified subtask; the plan status remains `planned`.

### Scoped Progress — 2026-09-29 — Compare branch mode projection

The `COMPARE_BRANCH_INT` projection now treats Compare `typeToken` as its
canonical selector (0=EQ, 1=LT, 2=LE, 3=GT, 4=GE, 5=NE), verifies signed i64
inputs and a BOOL result, and stores the selector in the plan side entry. The
selector participates in the deterministic generated hash and plan validation;
non-canonical modes and mismatched scalar types retain the original window.
The plan schema is version 2. The focused fixture lives in
`test_ssa_generated_fusion_compare_branch.inc`; the existing branch-remap
fixtures now use the same i64-to-BOOL contract while retaining their target
and source-map assertions.

The regression was first observed with mode 2 (LE): the D GCC target built,
then the direct test reported `fused=5` and failed its expected six-window
assertion. After the projection change, the focused direct test exited 0,
registered `ssa_generated_fusion` CTest passed 1/1, the generator `--check`
passed, and scoped `git diff --check` reported no whitespace errors. Exact
commands are in
[`2026-09-29-ssa-compare-branch-mode.md`](../../../../tests/acceptance/2026-09-29-ssa-compare-branch-mode.md).

This verifies parser-side plan projection only. Runtime handler dispatch,
execution of the generated fused word, and the plan-wide performance and
boundary matrix remain open; the 03.04 plan stays `planned`.

### Scoped Progress — 2026-09-29 — Increment loop branch type contract

The `INCREMENT_LOOP_BRANCH` candidate is semantically supported for a signed
i64 `ADD`: ExecIR verification accepts the fixture, and the core Oracle sends
4+1 to the truthy successor and 5+(-5) to the false successor. The matcher now
requires two signed i64 inputs, an i64 ADD result, and that result as the sole
conditional-branch input. A Verify-accepted object-typed version remains
unfused with `TYPE_MISMATCH`. The generated pattern row records
`SIGNED_I64_CONDITION`; the side entry retains that pattern identity and the
plan hashes the generated pattern schema, avoiding a redundant per-entry type
field or plan-layout version change.

The initial D GCC RED reached the intended type boundary: the Oracle paths and
Verify passed, then the direct test failed because the matcher fused the
object-typed window. A second focused RED caught an empty-pool-suffix off-by-one
in the operand/result helpers during their extraction from the 1,100-line
matcher; both helpers now reject that range before reading. The typed accessors
and compatibility checks are in `exec_ir_fusion_match_types.c`; the rest of
the matcher remains intact because its candidate evaluator shares several
file-local CFG, effect, and projection helpers. A future extraction should
move that evaluator as a complete unit with an explicit context.

In `/mnt/d/tmp/zr_vm/ssa-artifact-v6-gcc`, the focused target built, the direct
Unity suite passed, registered `ssa_generated_fusion` CTest passed 1/1, and the
pattern generator `--check` passed. The exact commands and observed counts are
in
[`2026-09-29-ssa-increment-loop-branch-types.md`](../../../../tests/acceptance/2026-09-29-ssa-increment-loop-branch-types.md).
This validates parser-side projection and Oracle semantics only; no fused
runtime handler was executed. Dispatcher integration, end-to-end source/resume
behavior, performance budgets, and the full boundary matrix remain open, so
the 03.04 plan stays `planned`.

### Scoped Progress — 2026-09-29 — LOAD_ADD_INT type contract

`LOAD_ADD_INT` now requires a signed i64 `LOAD` result and signed i64 `ADD`
inputs/result. The generated `SIGNED_I64_LOAD_ADD` constraint is part of the
pattern schema hash; side entries retain the pattern identity, so no extra
type field or plan-layout version change is needed. Verify-accepted DOUBLE,
int32, and uint64 cases stay unfused with a type-mismatch fallback. The DOUBLE
case runs in the Oracle and returns 41.75 for a 40.5 load plus 1.25.

The initial GCC RED passed Verify and Oracle, then failed at the expected
unfused assertion: the generic typed matcher emitted one `LOAD_ADD_INT` fusion
and one fallback. After the generated constraint and typed matcher guard, the
direct `ssa_generated_fusion` test passed, registered CTest passed 1/1, and the
pattern generator `--check` passed in the existing D-drive cache. Exact commands
are recorded in
[`2026-09-29-ssa-load-add-int-types.md`](../../../../tests/acceptance/2026-09-29-ssa-load-add-int-types.md).

This verifies the original operations and parser-side projection only; no fused
runtime handler was executed. The broader 03.04 plan remains open.

### Scoped Progress — 2026-09-29 — INDEX_LOAD_STORE address role

The STORE variant now requires the projected PLACE_PROJECT result to be
STORE operand 0, the address. The same-result and single-use constraints still
prove a direct dependency used only once, but a use only at STORE operand 1 is
the assigned value and remains as the original PLACE_PROJECT plus STORE with
a RESULT_MISMATCH fallback.

The regression fixture passes ExecIR Verify and the Oracle before checking
fusion. Its place provider returns 0xA0, while the STORE targets an independent
0xB0 address; the Oracle records address 0xB0, value 0xA0, and returns 0xB0.
Before the matcher guard, the focused test failed its expected-unfused
assertion after both semantic checks passed. With the guard, the focused GCC
target and direct test passed, registered ssa_generated_fusion CTest passed
1/1, and the pattern generator --check passed in the existing D cache.
Exact commands and limits are recorded in
[2026-09-29-ssa-index-store-address-role.md](../../../../tests/acceptance/2026-09-29-ssa-index-store-address-role.md).

This validates parser-side projection and STORE operand roles only; no fused
runtime handler was executed. The broader 03.04 plan remains open.

### Scoped Progress — 2026-09-29 — BINDING_CALL callee role

`BINDING_CALL` requires its `PLACE_PROJECT` result in CALL operand 0, the
canonical callee slot. The generic same-result check alone also accepts a
projection used only in an ordinary explicit argument, which must not be
reinterpreted as the call target. A Verify-accepted fixture runs through the
ExecIR Oracle with an independent callee (0xC0) and projected argument (0xA0),
then requires the original pair and a `RESULT_MISMATCH` fallback. The initial
GCC RED passed Verify and Oracle and failed only at the expected zero-fusion
assertion. The final matcher and validation evidence are recorded in
[`2026-09-29-ssa-binding-call-callee-role.md`](../../../../tests/acceptance/2026-09-29-ssa-binding-call-callee-role.md).

This validates parser-side call-role projection only. No generated runtime
handler is executed, and the broader 03.04 plan remains open.
