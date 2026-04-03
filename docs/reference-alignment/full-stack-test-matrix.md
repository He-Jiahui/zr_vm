---
related_code:
  - tests/parser/test_char_and_type_cast.c
  - tests/function/test_named_arguments.c
  - tests/module/test_module_system.c
  - tests/harness/reference_support.h
  - tests/harness/reference_support.c
  - tests/harness/path_support.h
  - tests/harness/path_support.c
  - tests/fixtures/reference/core_semantics/lexing_literals_diagnostics/manifest.json
  - tests/fixtures/reference/core_semantics/expressions_precedence_chains/manifest.json
  - tests/fixtures/reference/core_semantics/calls_named_default_varargs/manifest.json
  - tests/fixtures/reference/core_semantics/types_casts_const/manifest.json
  - tests/fixtures/reference/core_semantics/object_member_index_construct_target/manifest.json
  - tests/fixtures/reference/core_semantics/protocols_iteration_comparable/manifest.json
  - tests/fixtures/reference/core_semantics/modules_imports_artifacts/manifest.json
  - tests/fixtures/reference/core_semantics/oop_inheritance_descriptors/manifest.json
  - tests/fixtures/reference/core_semantics/ownership_using_resource_lifecycle/manifest.json
  - tests/fixtures/reference/core_semantics/exceptions_gc_native_stress/manifest.json
implementation_files:
  - tests/parser/test_char_and_type_cast.c
  - tests/harness/reference_support.h
  - tests/harness/reference_support.c
  - tests/harness/path_support.h
  - tests/harness/path_support.c
  - tests/CMakeLists.txt
plan_sources:
  - user: 2026-04-03 implement the full-stack reference-language-driven test matrix
  - .codex/plans/ZR 全栈参考语言共性测试矩阵设计.md
  - docs/testing-and-validation/core-semantics-reference-alignment.md
tests:
  - tests/parser/test_char_and_type_cast.c
  - tests/function/test_named_arguments.c
  - tests/module/test_module_system.c
  - tests/fixtures/reference/core_semantics/lexing_literals_diagnostics/manifest.json
  - tests/fixtures/reference/core_semantics/expressions_precedence_chains/manifest.json
  - tests/fixtures/reference/core_semantics/calls_named_default_varargs/manifest.json
  - tests/fixtures/reference/core_semantics/types_casts_const/manifest.json
  - tests/fixtures/reference/core_semantics/object_member_index_construct_target/manifest.json
  - tests/fixtures/reference/core_semantics/protocols_iteration_comparable/manifest.json
  - tests/fixtures/reference/core_semantics/modules_imports_artifacts/manifest.json
  - tests/fixtures/reference/core_semantics/oop_inheritance_descriptors/manifest.json
  - tests/fixtures/reference/core_semantics/ownership_using_resource_lifecycle/manifest.json
  - tests/fixtures/reference/core_semantics/exceptions_gc_native_stress/manifest.json
doc_type: testing-guide
---

# ZR 全栈参考语言共性测试矩阵

## 目标

这份文档把 `ZR` 的参考语言测试策略从第一阶段 `core_semantics` 小清单，提升成长期有效的主矩阵。它回答三件事：

1. `ZR` 当前要守住的全栈语义合同是什么。
2. 这些合同分别参考了哪几类上游语言能力。
3. 每条合同落在哪些验证层和哪些现有 executable 上。

本轮落地采用一个硬约束：`tests/fixtures/reference/core_semantics/` 继续作为总入口，不再新开平行 reference 根目录。旧的 phase1 小目录仍保留为 seed fixture，新的 10 个全栈域 manifest 也直接挂在这个根下。

## 资产布局

- 主文档：`docs/reference-alignment/full-stack-test-matrix.md`
- 主目录：`tests/fixtures/reference/core_semantics/`
- 主 helper：
  - `reference manifest` 校验：`tests/harness/reference_support.h` / `tests/harness/reference_support.c`
  - 分层断言 helper：`ZrTests_Reference_TextContainsAll`、`ZrTests_Reference_TextContainsInOrder`
  - `artifact/diagnostic` 正则断言 helper：`ZrTests_Reference_TextMatchesRegex`
- 现有 seed fixture 仍由：
  - `tests/parser/test_char_and_type_cast.c`
  - `tests/function/test_named_arguments.c`
  - `tests/module/test_module_system.c`
  继续消费

## 10 个固定语义域

| 语义域 | 上游证据主角 | 最低层覆盖 | 当前状态 |
| --- | --- | --- | --- |
| `Lexing/Literals/Diagnostics` | Lua, CPython, Rust | `parser` + `semantic/compiler` + `artifact` | 12 条 inventory 已落地，含 phase1 seed fixture |
| `Expressions/Precedence/Chains` | Lua, QuickJS, CPython | `parser` + `semantic/compiler` + `runtime` | 12 条 inventory 已落地 |
| `Calls/Named/Default/Varargs` | CPython, JDK, Mono | `parser` + `semantic/compiler` + `runtime` | 12 条 inventory 已落地，命名参数 seed fixture 可执行 |
| `Types/Casts/Const` | Rust, JDK, CPython | `semantic/compiler` + `runtime` + `artifact` | 12 条 inventory 已落地，const seed fixture 可执行 |
| `Object/Member/Index/ConstructTarget` | Lua, CPython, Rust | `semantic/compiler` + `runtime` + `artifact` | 12 条 inventory 已落地 |
| `Protocols/Iteration/Comparable` | JDK, Rust, CPython | `semantic/compiler` + `runtime` + `stress` | 12 条 inventory 已落地 |
| `Modules/Imports/Artifacts` | QuickJS, CPython, JDK | `module/project` + `artifact` + `runtime` | 12 条 inventory 已落地，root-member-chain seed fixture 可执行 |
| `OOP/Inheritance/Descriptors` | CPython, QuickJS, JDK | `semantic/compiler` + `runtime` + `artifact` | 12 条 inventory 已落地 |
| `Ownership/Using/ResourceLifecycle` | Rust, Mono | `semantic/compiler` + `runtime` + `module/project` | 12 条 inventory 已落地 |
| `Exceptions/GC/NativeStress` | Lua, Mono, CPython, QuickJS | `runtime` + `module/project` + `stress` | 12 条 inventory 已落地 |

这 10 组域是当前唯一允许新增 reference case 的一级归类。后续补测必须先选域，再落 manifest 和 executable。

## 配额与门槛

每个语义域首轮固定 `12 条核心 case`，配额为：

- `2 happy`
- `2 negative`
- `2 boundary`
- `2 combination`
- `2 regression`
- `2 divergence`

因此当前主矩阵首轮最低配额固定为 `120` 条。高风险域还必须补充 `stress_class`，并明确是 `approximate` 还是 `full`。本轮仓库内已经把这 120 条 case 的 inventory 全部写入 `tests/fixtures/reference/core_semantics/`。

## Manifest 合同

每条 case 的固定字段为：

- `id`
- `domain`
- `feature_group`
- `upstream_language`
- `upstream_file`
- `upstream_intent`
- `zr_decision`
- `zr_contract`
- `layer_targets`
- `case_kind`
- `expected_outcome`
- `assertions`
- `stress_class`
- `tiers`
- `execution_modes`
- `artifact_targets`
- `probe_targets`
- `backend_requirements`
- `oracles`

字段含义约束如下：

- `zr_decision` 只允许 `adopt`、`adapt`、`reject`。
- `layer_targets` 至少覆盖 3 层；高风险域默认覆盖 `stress`。
- `assertions` 不能只写“能编译/能执行”，必须包含至少一个更低层证据，例如 AST 形态、diagnostic 范围、opcode、artifact 文本、metadata roundtrip、cleanup 次数。
- `stress_class` 即使不是压力 case 也必须出现；非压力 case 固定为 `none`。
- `tiers` 当前固定只允许 `smoke`、`core`、`stress`，并且每条 AOT case 至少落入 `core`。
- `execution_modes` 当前统一声明 `interp`、`aot_c`、`aot_llvm`；是否真正需要可执行 parity 由 `backend_requirements` 决定。
- `backend_requirements.require_aot_path` 用来表达“必须证明真的走了 AOT 路径”；`executed_via` 则是运行态可观测标记，当前允许值固定为 `interp|aot_c|aot_llvm`。
- `oracles` 固定拆成 `source`、`artifact`、`parity`、`probe` 四类；任何正式 AOT case 都不能只保留前端断言或只保留后端断言。

## AOT 分层矩阵

新的 AOT 回归不是平行于 reference matrix 的第二棵目录树，而是 reference matrix 上叠加的第二维。当前固定五层：

- `L0 Source Semantics`
  - 用户可见行为：stdout、返回值、异常类型、cleanup trace
- `L1 Artifact Contract`
  - `.zri`、`.zro`、AOT-C、AOT-LLVM 文本产物中的 opcode、runtime contract、`DEOPT_MAP`、`EH_TABLE`、`GC_MAP`、`CALLSITE_CACHE_TABLE`
- `L2 Executable AOT Parity`
  - `interp`、`aot_c`、`aot_llvm` 结果一致性；若 backend 还只有文本产物，必须显式记录 skip reason，不能静默退回解释器
- `L3 Runtime Path Probes`
  - cache hit、PIC 宽度、deopt / requickening 次数、`max_callInfo_depth`、drop 顺序、`executed_via`
- `L4 Project Fixtures`
  - 多文件项目、source/binary import、CLI compile/run/incremental、大常量池、深 CFG、native/container/FFI 组合

## AOT Oracle 合同

`oracles` 字段统一表达同一条 case 在前后两侧都要担保什么：

- `source`
  - 记录 stdout、返回值、异常或 cleanup 顺序这类用户可见行为
- `artifact`
  - 记录 must-contain、must-not-contain、ordered-fragments、regex
- `parity`
  - 记录 `interp vs aot_c vs aot_llvm` 的对齐要求与显式 skip reason
- `probe`
  - 记录 cache/PIC/deopt/frame reuse/drop order/`executed_via` 之类的内部合同

## 分档与运行

当前分档固定为：

- `smoke`
  - 每次必跑；每个覆盖带至少 1 个小夹具；当前优先覆盖 `.zri/.zro + AOT-C` artifact contract、AOT path proof contract、meta cache、dynamic PIC、tail reuse、weak upgrade、`%using` exact-once drop
- `core`
  - PR 必跑；展开全部 8 个覆盖带；`aot_c` 至少要覆盖 artifact contract，能执行的路径再做 parity
- `stress`
  - 夜间或手动；跑 megamorphic PIC、重复 deopt / requickening、深尾递归、nested finally、弱引用失效、ownership-heavy 微基准、深模块图、大常量池、超深 CFG

当前仓库不新增长期第 6 个顶层 suite。`smoke/core/stress` 作为标签与 runner filter 叠加在现有 `core_runtime`、`language_pipeline`、`projects`、`golden_regression`、CLI suite 之上。

## 超大集成 Fixture

这轮 AOT 测试矩阵固定保留 3 个超大集成 fixture 兜底：

- `aot_module_graph_pipeline`
  - 多文件工程、source/binary 混合 import、incremental compile、CLI compile/run、module cache identity、artifact roundtrip
- `aot_dynamic_meta_ownership_lab`
  - property / `META_GET` / `META_SET`、dynamic/meta call-site cache、slot cache / PIC、prototype 变更导致 deopt、ownership/weak/%using/drop、container/native receiver 组合
- `aot_eh_tail_gc_stress`
  - nested try/finally、direct/meta/dyn tail call、frame reuse 与 handler/%using 下的合法拒绝复用、weak expiry、长循环、GC checkpoint、反复 deopt / requickening

## 可复用 helper 合同

本轮新增的 helper 不是为了省几行代码，而是为了把 reference 资产约束变成共享基础设施：

- `ZrTests_Reference_ReadFixture`
  - 读取 `tests/fixtures/reference/core_semantics/` 下的 reference 资产
- `ZrTests_Reference_ReadDoc`
  - 读取 `docs/reference-alignment/` 下的矩阵文档
- `ZrTests_Reference_AssertManifestShape`
  - 校验 manifest 的固定字段与最小 case 数
- `ZrTests_Reference_AssertCaseKindsCovered`
  - 校验 `happy/negative/boundary/combination/regression/divergence` 的覆盖分布
- `ZrTests_Reference_TextContainsAll`
  - 做分层断言 helper，适合检查文档、artifact、manifest 的关键短语
- `ZrTests_Reference_TextContainsInOrder`
  - 做 ordered fragment 断言，适合 artifact 文本顺序约束
- `ZrTests_Reference_TextMatchesRegex`
  - 做简化正则断言，锁“错误类别 + 范围 + 关键短语”而不是整段全文

## Executable 映射

主矩阵不新开平行 test executable，而是复用现有入口：

- `tests/parser/test_parser.c`, `tests/parser/test_char_and_type_cast.c`, `tests/parser/test_compiler_features.c`, `tests/parser/test_type_inference.c`
  - 承担 `parser` / `semantic/compiler`
- `tests/parser/test_semir_pipeline.c`, `tests/parser/test_execbc_aot_pipeline.c`, `tests/parser/test_meta_call_pipeline.c`, `tests/parser/test_tail_call_pipeline.c`
  - 承担 `artifact`
- `tests/parser/test_instruction_execution.c`, `tests/instructions/test_instructions.c`, `tests/exceptions/test_exceptions.c`
  - 承担 `runtime`
- `tests/module/test_module_system.c`, `tests/function/test_named_arguments.c`, `tests/container/test_container_runtime.c`, `tests/container/test_container_type_inference.c`
  - 承担 `module/project` 与 `native/container` 集成
- `tests/gc/gc_tests.c`, `tests/scripts/test_artifact_golden.c`, `tests/cmake/run_cli_suite.cmake`, `tests/fixtures/projects/*`
  - 承担 `stress` / `golden` / `CLI`

## 首轮 30 条高风险优先 case

当前 inventory 已固定以下优先 id，后续 runnable fixture 必须优先从这里补：

### 字面量 / 诊断 6 条

- `lexing-unclosed-string`
- `lexing-invalid-hex-escape`
- `lexing-unicode-boundary`
- `lexing-invalid-char-width`
- `lexing-multiline-literal-reject`
- `lexing-location-recovery`

### 调用 / 参数 6 条

- `calls-named-default-reorder-pass`
- `calls-duplicate-named-fail`
- `calls-unexpected-named-fail`
- `calls-positional-after-named-fail`
- `calls-varargs-arity-boundary`
- `calls-overload-ambiguity-fail`

### 导入 / artifact 6 条

- `modules-root-member-chain-pass`
- `modules-duplicate-import-identity`
- `modules-cyclic-import-reject`
- `modules-binary-metadata-roundtrip`
- `modules-source-binary-same-logical-path`
- `modules-hidden-internal-import-api-reject`

### 成员 / 索引 / 协议 6 条

- `object-member-vs-string-index-split`
- `object-array-map-plain-index-split`
- `object-property-getter-setter-precedence`
- `protocols-foreach-contract-lowering`
- `protocols-iterator-invalidation`
- `protocols-comparable-hashable-consistency`

### ownership / 异常 / GC 6 条

- `ownership-shared-owner-consume`
- `ownership-shared-new-wrapper`
- `ownership-using-cleanup-on-return`
- `exceptions-cleanup-on-throw`
- `ownership-weak-expiry-after-last-shared-release`
- `exceptions-gc-with-active-owned-native-object`

## 当前落地边界

这次提交完成的是主矩阵基础设施，而不是所有 120 条 case 都已经变成 runnable fixture：

- 已完成
  - 10 个域 manifest 全部入库
  - `tests/parser/test_char_and_type_cast.c` 现在会校验 10 个域、120 条 case 和主矩阵文档
  - `tests/harness/reference_support.*` 已成为共享 helper
  - `tests/function/test_named_arguments.c` 与 `tests/module/test_module_system.c` 继续消费现有 phase1 seed fixture
- 仍待继续
  - 把首轮 30 条优先 id 逐步变成真实 `.zr` fixture、artifact golden 或 stress runner case
  - 按域把 `assertions` 中记录的 lower-level evidence 逐条接入现有 executable
  - 把更多 `module/project`、`artifact`、`stress` 断言转成自动化执行

## 设计默认值

- 类型名、协议名、成员名字面量都不能成为共享 VM 或 compiler path 的分发依据。
- 参考语言只提供 precedent，不做一比一镜像目录复制。
- 诊断默认锁 `错误类别 + 行列/范围 + 关键短语`，不默认锁全文。
- `tests/fixtures/reference/core_semantics/` 是长期 reference 根目录；phase1 seed fixture 继续可用，但新增 case 一律按 10 域归类。
- rollout 顺序固定为：`核心语义 → 成员/索引/协议/OOP → 模块/native/artifact → ownership/异常 → GC/压力`。
