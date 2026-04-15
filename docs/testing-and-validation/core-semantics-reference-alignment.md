---
related_code:
  - tests/parser/test_char_and_type_cast.c
  - tests/parser/test_const_keyword.c
  - tests/function/test_named_arguments.c
  - tests/module/test_module_system.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class_support.c
  - docs/reference-alignment/core-semantics-matrix.md
  - tests/fixtures/reference/core_semantics/literals/manifest.json
  - tests/fixtures/reference/core_semantics/expressions/manifest.json
  - tests/fixtures/reference/core_semantics/imports/manifest.json
  - tests/fixtures/reference/core_semantics/calls/manifest.json
  - tests/fixtures/reference/core_semantics/casts-and-const/manifest.json
  - tests/fixtures/reference/core_semantics/diagnostics/manifest.json
  - tests/fixtures/reference/core_semantics/literals/unclosed_string_fail.zr
  - tests/fixtures/reference/core_semantics/casts-and-const/const_reassign_fail.zr
  - tests/fixtures/reference/core_semantics/casts-and-const/const_if_missing_branch_fail.zr
  - tests/fixtures/reference/core_semantics/casts-and-const/const_switch_paths_pass.zr
  - tests/fixtures/reference/core_semantics/casts-and-const/const_switch_missing_default_fail.zr
  - tests/fixtures/reference/core_semantics/casts-and-const/const_ternary_paths_pass.zr
  - tests/fixtures/reference/core_semantics/casts-and-const/const_ternary_missing_branch_fail.zr
  - tests/fixtures/reference/core_semantics/calls/named_arguments_defaults_pass.zr
  - tests/fixtures/reference/core_semantics/imports/native_root_member_chain_pass.zr
implementation_files:
  - tests/parser/test_char_and_type_cast.c
  - tests/parser/test_const_keyword.c
  - tests/function/test_named_arguments.c
  - tests/module/test_module_system.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class_support.c
  - tests/fixtures/reference/core_semantics/literals/manifest.json
  - tests/fixtures/reference/core_semantics/expressions/manifest.json
  - tests/fixtures/reference/core_semantics/imports/manifest.json
  - tests/fixtures/reference/core_semantics/calls/manifest.json
  - tests/fixtures/reference/core_semantics/casts-and-const/manifest.json
  - tests/fixtures/reference/core_semantics/diagnostics/manifest.json
  - tests/fixtures/reference/core_semantics/literals/unclosed_string_fail.zr
  - tests/fixtures/reference/core_semantics/casts-and-const/const_reassign_fail.zr
  - tests/fixtures/reference/core_semantics/casts-and-const/const_if_missing_branch_fail.zr
  - tests/fixtures/reference/core_semantics/casts-and-const/const_switch_paths_pass.zr
  - tests/fixtures/reference/core_semantics/casts-and-const/const_switch_missing_default_fail.zr
  - tests/fixtures/reference/core_semantics/casts-and-const/const_ternary_paths_pass.zr
  - tests/fixtures/reference/core_semantics/casts-and-const/const_ternary_missing_branch_fail.zr
  - tests/fixtures/reference/core_semantics/calls/named_arguments_defaults_pass.zr
  - tests/fixtures/reference/core_semantics/imports/native_root_member_chain_pass.zr
plan_sources:
  - user: 2026-04-03 实现 ZR 核心语义外部对齐第一阶段
  - docs/zr_language_specification.md
  - docs/zr_language_test_requirements.md
tests:
  - tests/parser/test_char_and_type_cast.c
  - tests/parser/test_const_keyword.c
  - tests/function/test_named_arguments.c
  - tests/module/test_module_system.c
  - tests/fixtures/reference/core_semantics/literals/unclosed_string_fail.zr
  - tests/fixtures/reference/core_semantics/casts-and-const/const_reassign_fail.zr
  - tests/fixtures/reference/core_semantics/casts-and-const/const_if_missing_branch_fail.zr
  - tests/fixtures/reference/core_semantics/casts-and-const/const_switch_paths_pass.zr
  - tests/fixtures/reference/core_semantics/casts-and-const/const_switch_missing_default_fail.zr
  - tests/fixtures/reference/core_semantics/casts-and-const/const_ternary_paths_pass.zr
  - tests/fixtures/reference/core_semantics/casts-and-const/const_ternary_missing_branch_fail.zr
  - tests/fixtures/reference/core_semantics/calls/named_arguments_defaults_pass.zr
  - tests/fixtures/reference/core_semantics/imports/native_root_member_chain_pass.zr
doc_type: testing-guide
---

# Core Semantics Reference Alignment

## Purpose

这份文档定义第一阶段“核心语义外部对齐”的落地形态。目标不是一次把 Lua、Mono、QuickJS、CPython、JDK、Rust 的所有语言语义全部抄成 ZR，而是先把外部参考收敛成可执行、可扩展、可追踪的资产：

- 用 manifest 把上游 precedent 固定到仓库内的具体文件路径
- 用最小 fixture 把其中一小段语义真正接到当前 ZR 测试入口
- 用现有 test target 先跑通 parser、compiler、runtime 三层的最短闭环

这样后续每加一个 ZR 语言特性，不需要重新从 6 个参考树里散搜，而是可以直接在 `tests/fixtures/reference/core_semantics/` 下增量扩展。

当前跨阶段主入口已经升级为 [docs/reference-alignment/full-stack-test-matrix.md](D:/Git/Github/zr_vm_mig/zr_vm/docs/reference-alignment/full-stack-test-matrix.md)。第一阶段自己的 6 主题状态表则落在 [docs/reference-alignment/core-semantics-matrix.md](D:/Git/Github/zr_vm_mig/zr_vm/docs/reference-alignment/core-semantics-matrix.md)。这份文档继续负责 phase1 seed fixture 的来历和最小 runnable baseline，而全栈域划分、120 条首轮 inventory 和后续 rollout 顺序统一以主矩阵为准。

## Phase 1 Scope

第一阶段只做 6 个主题的 reference inventory：

- `literals`
- `expressions`
- `imports`
- `calls`
- `casts-and-const`
- `diagnostics`

每个主题都用一个 `manifest.json` 保存至少 6 个上游 case。manifest 不是为了替代真实测试，而是为了把“参考了哪个语言、哪份上游测试、ZR 选择了什么规则、期望是什么”固定成机器可读结构。

当前三个已有 test target 会消费这些资产：

- `tests/parser/test_char_and_type_cast.c`
  - 校验 6 份 manifest 的基本形状和总 case 数
  - 校验 `core-semantics-matrix.md` 与 `full-stack-test-matrix.md` 两份主文档存在且包含关键短语
  - 运行 `unclosed_string_fail.zr`
  - 运行 `const_reassign_fail.zr`
  - 运行 constructor const control-flow fixture matrix：`else-if`、单臂 `if`、`switch`、`ternary`
- `tests/function/test_named_arguments.c`
  - 运行 `named_arguments_defaults_pass.zr`
  - 运行 `calls_named_default_varargs/duplicate_named_fail.zr`
  - 运行 `calls_named_default_varargs/unexpected_named_fail.zr`
  - 运行 `calls_named_default_varargs/positional_after_named_fail.zr`
  - 运行 `calls_named_default_varargs/overload_ambiguity_fail.zr`
- `tests/module/test_module_system.c`
  - 读取 import manifest
  - 运行 `native_root_member_chain_pass.zr`

## Why The Fixtures Are Small

当前工作区仍然存在大量未提交修改，所以这个阶段继续坚持“先把 reference 资产和最小可运行基线接进现有测试目标”，而不是同步铺开大批 parser/runtime 改写。

不过 phase1 已经不再停留在最初的 4 个 seed fixture。当前最小 runnable baseline 仍然围绕 4 个入口主题构建，但 `casts-and-const` 已经扩成一组控制流夹具：

- `literals/unclosed_string_fail.zr`
  - 目标是 parser 必须直接失败
- `casts-and-const/const_reassign_fail.zr`
  - 目标是 local const 重赋值在 compiler 阶段被拒绝
- `casts-and-const/const_if_missing_branch_fail.zr`
  - 目标是单臂 `if` 不能让 constructor const 字段视为 definite-assigned
- `casts-and-const/const_switch_paths_pass.zr`
  - 目标是 `switch` 所有 continuing path 都初始化时允许通过
- `casts-and-const/const_switch_missing_default_fail.zr`
  - 目标是缺少 `default` 时保留未命中路径为未初始化
- `casts-and-const/const_ternary_paths_pass.zr`
  - 目标是 ternary 两支按 definite-assignment 交集收敛
- `casts-and-const/const_ternary_missing_branch_fail.zr`
  - 目标是 ternary 只初始化一支时必须拒绝
- `calls/named_arguments_defaults_pass.zr`
  - 目标是命名参数、默认值、重排三者同时成立，并返回稳定值 `128`
- `imports/native_root_member_chain_pass.zr`
  - 目标是 `%import("zr.system").vm.state().loadedModuleCount` 这条 root-member-chain 可编译且可执行

## Chosen ZR Baselines

### Literals

第一阶段优先固定一个最容易退化的负例：未闭合字符串。

原因很直接：

- 这是 parser 前置语义，不该拖到 compiler/runtime
- Lua、CPython、Rust 都有稳定 precedent
- 它能快速暴露 lexer、token span、错误恢复三类基础问题

因此当前基线是：

- 读到未闭合字符串时，`ZrParser_Parse(...)` 必须直接返回 `NULL`
- 不允许生成残缺 AST 再由后续阶段兜底

### Calls

当前 runnable baseline 选的是“命名参数 + 默认值 + 参数重排”。

`named_arguments_defaults_pass.zr` 里故意组合了：

- 省略中间参数时使用默认值
- 通过命名参数覆盖尾部默认值
- 全部参数命名重排

这比单纯 `func(c: 3, a: 1, b: 2)` 更接近真实语言设计压力，因为它同时覆盖：

- 默认值投影
- 参数名解析
- positional 与 named 混合规则

除此之外，调用面的 full-stack negative fixture 也已经入库并接到 `tests/function/test_named_arguments.c`：

- `calls_named_default_varargs/duplicate_named_fail.zr`
- `calls_named_default_varargs/unexpected_named_fail.zr`
- `calls_named_default_varargs/positional_after_named_fail.zr`
- `calls_named_default_varargs/overload_ambiguity_fail.zr`

因此调用面当前不再只是“单个 pass fixture + manifest inventory”，而是已经具备一组围绕 parser/semantic/compiler 诊断的负例基线。

### Imports

这次 import 基线没有使用额外 probe 模块，而是选择 `zr.system` 根模块。

原因是当前 `test_module_system.c` 的 `create_test_state()` 默认已经注册：

- `zr.system`
- `zr.container`

这意味着 `%import("zr.system").vm.state().loadedModuleCount` 可以在不引入额外初始化噪音的前提下，稳定验证：

- root native module import
- submodule member chain
- submodule method call
- 返回对象字段访问

也就是说，这个 fixture 测到的是 ZR 自己现在最需要保住的 import/member-chain 核心路径，而不是一个临时搭建的测试专用模块。

### Const

`const` 已经从单一 local-reassign baseline 扩成一个更接近 JDK definite-assignment 的 seed 子矩阵。

当前已落地的行为包括：

- local const 重赋值
  - `const_reassign_fail.zr`
  - 编译器会报出 `Cannot assign to const variable ...`
- constructor const 单臂 `if` 失败
  - `const_if_missing_branch_fail.zr`
- constructor const `switch` 全路径初始化成功
  - `const_switch_paths_pass.zr`
- constructor const `switch` 缺 `default` 失败
  - `const_switch_missing_default_fail.zr`
- constructor const ternary 双分支初始化成功
  - `const_ternary_paths_pass.zr`
- constructor const ternary 单分支初始化失败
  - `const_ternary_missing_branch_fail.zr`

对应实现已经下沉到 `zr_vm_parser/src/zr_vm_parser/compiler/compiler_class_support.c`，规则固定为：

- 构造器 const 字段必须在所有 continuing path 上 exactly once 完成初始化
- `if`、`switch`、`ternary` 都按分支交集收敛 definite-assignment 状态
- 缺少 `default`、单臂分支、重复赋值都必须在 compiler 阶段拒绝

这意味着 `casts-and-const` 在 phase1 中已经不再只有 smoke 级 local const 断言，而是具备了真正的控制流回归。

## Manifest Structure

每份 manifest 至少包含以下字段：

- `feature_group`
- `cases[].id`
- `cases[].upstream_language`
- `cases[].zr_decision`
- `cases[].expected_outcome`

推荐继续保留但不是当前测试强制的字段：

- `phase_note`
- `cases[].upstream_path`
- `cases[].upstream_focus`

这样 C 测试只需要验证最小形状，而文档和后续脚本仍能读到足够多的设计信息。

## Upstream Evidence Strategy

本阶段 manifest 主要引用以下上游树中的测试文件：

- Lua: `lua/testes/errors.lua`, `lua/testes/calls.lua`, `lua/testes/closure.lua`
- QuickJS: `lua/QuickJS-master/tests/test_language.js`, `fixture_cyclic_import.js`, `test_cyclic_import.js`
- CPython: `test_eof.py`, `test_syntax.py`, `test_string_literals.py`, `test_grammar.py`, `test_call.py`, `test_extcall.py`, `test_keywordonlyarg.py`, `test_import/__init__.py`
- JDK: `importscope/*.java`, `importChecks/*.java`, `diags/examples/CantAssignToFinal.java`, `FinalParamCantBeAssigned.java`
- Rust: `ui/codemap_tests/tab_2.rs`, `ui/fmt/ifmt-bad-arg.rs`, `ui/macros/format-parse-errors.rs`, `ui/borrowck/assign-imm-local-twice.rs`, `borrowck-assign-to-constants.rs`
- Mono: `params.cs`, `custom-modifiers-inheritance.cs`

这些引用在第一阶段主要承担两个职责：

- 给 ZR 当前已实现能力提供对照来源
- 给还没落 runnable fixture 的主题先建立“之后要补什么”的有边界清单

## Next Expansion Order

基于当前资产，后续建议按下面顺序扩展：

1. `calls`
   - 增加 duplicate named argument、unexpected named argument、positional-after-named 的 compile-fail fixture
2. `imports`
   - 增加 duplicate import、cyclic import、cache identity 的 source fixture
3. `literals`
   - 增加 escape-sequence success cases 和 triple-quoted/long-string negative cases
4. `casts-and-const`
   - 在现有 constructor const control-flow baseline 之上，继续补 const parameter、explicit cast failure 与 narrowing diagnostics
5. `expressions`
   - 增加 closure capture、member-chain precedence、nested conditional 和 interpolation-boundary tests
6. `diagnostics`
   - 把 parser/compiler/runtime error taxonomy 统一整理成可断言文本或错误码

## Acceptance Bar For This Phase

这一阶段完成的标准不是“ZR 已经和所有参考语言完全一致”，而是：

- 6 个核心主题都有 manifest
- 当前三个测试入口能读到这些资产
- 至少 4 个 fixture 形成 parse/compiler/runtime 的实际断言
- 文档明确说明了为什么先这样切，而不是继续只写零散示例

只要这个骨架稳住，后续把更复杂的上游测试映射进来就会变成可持续工作，而不是每次从头整理参考资料。
