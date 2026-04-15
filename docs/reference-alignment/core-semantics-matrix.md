---
related_code:
  - tests/parser/test_char_and_type_cast.c
  - tests/parser/test_const_keyword.c
  - tests/function/test_named_arguments.c
  - tests/module/test_module_system.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class_support.c
  - tests/fixtures/reference/core_semantics/calls_named_default_varargs/duplicate_named_fail.zr
  - tests/fixtures/reference/core_semantics/calls_named_default_varargs/unexpected_named_fail.zr
  - tests/fixtures/reference/core_semantics/calls_named_default_varargs/positional_after_named_fail.zr
  - tests/fixtures/reference/core_semantics/calls_named_default_varargs/overload_ambiguity_fail.zr
  - tests/fixtures/reference/core_semantics/literals/manifest.json
  - tests/fixtures/reference/core_semantics/expressions/manifest.json
  - tests/fixtures/reference/core_semantics/imports/manifest.json
  - tests/fixtures/reference/core_semantics/calls/manifest.json
  - tests/fixtures/reference/core_semantics/casts-and-const/manifest.json
  - tests/fixtures/reference/core_semantics/diagnostics/manifest.json
  - tests/fixtures/reference/core_semantics/casts-and-const/const_if_missing_branch_fail.zr
  - tests/fixtures/reference/core_semantics/casts-and-const/const_switch_paths_pass.zr
  - tests/fixtures/reference/core_semantics/casts-and-const/const_switch_missing_default_fail.zr
  - tests/fixtures/reference/core_semantics/casts-and-const/const_ternary_paths_pass.zr
  - tests/fixtures/reference/core_semantics/casts-and-const/const_ternary_missing_branch_fail.zr
implementation_files:
  - tests/parser/test_char_and_type_cast.c
  - tests/parser/test_const_keyword.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_class_support.c
  - tests/function/test_named_arguments.c
  - tests/fixtures/reference/core_semantics/casts-and-const/manifest.json
  - tests/fixtures/reference/core_semantics/casts-and-const/const_if_missing_branch_fail.zr
  - tests/fixtures/reference/core_semantics/casts-and-const/const_switch_paths_pass.zr
  - tests/fixtures/reference/core_semantics/casts-and-const/const_switch_missing_default_fail.zr
  - tests/fixtures/reference/core_semantics/casts-and-const/const_ternary_paths_pass.zr
  - tests/fixtures/reference/core_semantics/casts-and-const/const_ternary_missing_branch_fail.zr
plan_sources:
  - user: 2026-04-03 实现 ZR 核心语义外部对齐第一阶段
  - .codex/plans/ZR 核心语义外部对标与首批落地计划.md
  - docs/testing-and-validation/core-semantics-reference-alignment.md
tests:
  - tests/parser/test_char_and_type_cast.c
  - tests/parser/test_const_keyword.c
  - tests/function/test_named_arguments.c
  - tests/module/test_module_system.c
  - tests/fixtures/reference/core_semantics/calls_named_default_varargs/duplicate_named_fail.zr
  - tests/fixtures/reference/core_semantics/calls_named_default_varargs/unexpected_named_fail.zr
  - tests/fixtures/reference/core_semantics/calls_named_default_varargs/positional_after_named_fail.zr
  - tests/fixtures/reference/core_semantics/calls_named_default_varargs/overload_ambiguity_fail.zr
doc_type: capability-matrix
---

# ZR 核心语义 capability matrix

## Purpose

这份矩阵把 `.codex/plans/ZR 核心语义外部对标与首批落地计划.md` 里第一阶段的 6 个主题，压成一张可以直接回指代码、测试和 reference fixture 的工作表。

这里的判断规则固定为：

- 先看当前代码和稳定测试，不先看旧文档承诺
- 外部语言只提供 precedent，不直接覆盖 `ZR` 自身设计
- 每个主题都同时看 `parser / semantic / compiler / runtime / project / golden` 六层，而不是只写“支持/不支持”

第一阶段固定只覆盖以下 6 个主题：

- 字面量与转义
- 表达式与优先级
- %module/%import 与成员链
- 调用面：位置参数、命名参数、默认值、变参、重载/错误 arity
- <Type> 转换、prototype/new 误用、`const` 赋值规则
- 诊断与错误恢复

## 结论标签

- `已符合 ZR 合同`
  - 当前层已经有可执行证据，且行为和当前 `ZR` 合同一致
- `需要补测试`
  - 语义方向基本确定，但还缺 runnable fixture、项目夹具或更低层断言
- `需要补诊断/实现`
  - 已知缺口仍在代码或诊断层，继续补测试前需要先修行为
- `明确与外部语言不同并保留差异`
  - 外部语言存在 precedent，但 `ZR` 明确选择不跟随，并需要稳定拒绝或保持现状

## 六层视图

本矩阵固定检查这 6 层：

- `parser`
- `semantic`
- `compiler`
- `runtime`
- `project`
- `golden`

其中 `semantic` 和 `compiler` 在当前仓库里通常一起由 parser/compiler test executable 承担，但矩阵仍分开写，避免把“能 parse”误记成“有稳定 lowering/diagnostic 合同”。

## Matrix

| 主题 | parser | semantic | compiler | runtime | project | golden | 当前结论 | 主要证据 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 字面量与转义 | 已符合 ZR 合同 | 需要补测试 | 需要补测试 | 当前无直接 phase1 合同 | 需要补测试 | 需要补测试 | 需要补测试 | `tests/parser/test_char_and_type_cast.c` 已覆盖未闭合字符串、非法 hex escape、多字符 char、multiline reject 的 seed fixture；manifest 见 `tests/fixtures/reference/core_semantics/literals/manifest.json` |
| 表达式与优先级 | 需要补测试 | 需要补测试 | 需要补测试 | 需要补测试 | 需要补测试 | 需要补测试 | 需要补测试 | 目前已有 `tests/fixtures/reference/core_semantics/expressions/manifest.json` 和全栈 `expressions_precedence_chains` inventory，但缺少 phase1 级 runnable fixture 把 precedence/member-chain 直接钉进 reference 入口 |
| `%module/%import` 与成员链 | 已符合 ZR 合同 | 已符合 ZR 合同 | 已符合 ZR 合同 | 已符合 ZR 合同 | 已符合 ZR 合同 | 需要补测试 | 需要补测试 | `tests/module/test_module_system.c` 已执行 `imports/native_root_member_chain_pass.zr`，验证 root import、成员链、方法调用与对象字段读取；duplicate import / cyclic import 仍停在 manifest inventory |
| 调用面：位置参数、命名参数、默认值、变参、重载/错误 arity | 已符合 ZR 合同 | 已符合 ZR 合同 | 已符合 ZR 合同 | 已符合 ZR 合同 | 需要补测试 | 需要补测试 | 需要补测试 | `tests/function/test_named_arguments.c` 已执行 `calls/named_arguments_defaults_pass.zr`，并覆盖 `calls_named_default_varargs/duplicate_named_fail.zr`、`unexpected_named_fail.zr`、`positional_after_named_fail.zr`、`overload_ambiguity_fail.zr`；project/golden 层仍待补 |
| `<Type> 转换、prototype/new 误用、`const` 赋值规则 | 已符合 ZR 合同 | 需要补测试 | 已符合 ZR 合同 | 需要补测试 | 需要补测试 | 需要补测试 | 需要补测试 | `tests/parser/test_char_and_type_cast.c` 覆盖 type cast 基本 parse/compile 和 `casts-and-const` manifest；`tests/parser/test_const_keyword.c` 与 `zr_vm_parser/src/zr_vm_parser/compiler/compiler_class_support.c` 已把 `const` 扩到 `if/switch/ternary` definite-assignment，并新增 `const_if_missing_branch_fail.zr`、`const_switch_paths_pass.zr`、`const_switch_missing_default_fail.zr`、`const_ternary_paths_pass.zr`、`const_ternary_missing_branch_fail.zr` |
| 诊断与错误恢复 | 已符合 ZR 合同 | 需要补测试 | 需要补测试 | 需要补测试 | 需要补测试 | 需要补测试 | 需要补测试 | phase1 入口已经断言 invalid literal 会保留错误节点或直接拒绝编译，但 parser/compiler/runtime taxonomy 仍缺统一错误码或稳定分类文本基线 |

## Current Notes By Topic

### 字面量与转义

当前最稳的基线是 parser 前置负例：

- `unclosed_string_fail.zr`
- `invalid_hex_escape_fail.zr`
- `multiline_literal_reject_fail.zr`
- `invalid_char_width_fail.zr`

这些 case 已经把“必须在词法或 parser 阶段暴露”的合同固定住，但 escape success、CRLF/注释混排和数值边界仍主要停在 inventory。

### 表达式与优先级

这个主题的 reference 资产已经有了，但可执行证据还弱于其他主题。当前更接近：

- inventory 已建立
- parser/compiler 的普通单测存在
- reference fixture 还没有系统接进 phase1 入口

因此这里仍应标记为 `需要补测试`，不是 `需要补诊断/实现`。

### `%module/%import` 与成员链

当前可以明确保住的基线是：

- root native module import
- submodule member chain
- 子模块方法调用
- 返回对象字段读取

这个方向已经形成 runnable 证据，但 duplicate import identity、source/binary 同路径、hidden internal import rejection 还没有在 phase1 入口全部变成 fixture。

当前合同还新增两条公开路径规则：

- project import 支持显式 relative-dot（`.x.y`、`..x.y`、`...x.y`）和 `.zrp.pathAliases` 的 `@alias(.foo.bar)` 展开。
- canonical 解析失败后仍然拒绝隐式相对 guessing / 名称回退；`./foo`、`../foo`、bare `.` / `..`、`@alias/foo` 和 sourceRoot 逃逸都属于非法形式。

### 调用面：位置参数、命名参数、默认值、变参、重载/错误 arity

当前最可信的证据来自：

- `tests/function/test_named_arguments.c`
- `calls/named_arguments_defaults_pass.zr`
- `calls_named_default_varargs/duplicate_named_fail.zr`
- `calls_named_default_varargs/unexpected_named_fail.zr`
- `calls_named_default_varargs/positional_after_named_fail.zr`
- `calls_named_default_varargs/overload_ambiguity_fail.zr`

它已经证明命名参数、默认值、参数重排和几类核心负例都不是只靠文档描述存在，但 project/golden 层和更多 vararg/overload 边界 case 还没有按计划全部 runnable 化。

### `<Type> 转换、prototype/new 误用、`const` 赋值规则

这是当前批次 C 里最实的子主题，尤其是 `const`：

- local const 重赋值继续由 `const_reassign_fail.zr` 守住
- 构造器 const definite-assignment 已扩到 `if`、`switch`、`ternary`
- `tests/parser/test_const_keyword.c` 直接覆盖 success/fail 单测
- `tests/parser/test_char_and_type_cast.c` 现在会跑项目级 reference fixture matrix

当前已落地的控制流 fixture 包括：

- `const_else_if_paths_pass.zr`
- `const_if_missing_branch_fail.zr`
- `const_switch_paths_pass.zr`
- `const_switch_missing_default_fail.zr`
- `const_ternary_paths_pass.zr`
- `const_ternary_missing_branch_fail.zr`

语义选择与 JDK definite-assignment precedent 一致：

- continuing path 必须全部完成初始化
- 分支结果按交集收敛
- 缺少 `default` 或单臂分支都不能被当成“已 definite”

但这一主题整体还不能写成“完全完成”，因为 `<Type>` narrowing failure、prototype/new misuse、project/golden 层对齐仍待继续。

### 诊断与错误恢复

当前诊断层已经有一些稳定片段，但还没有统一成完整 taxonomy。也就是说：

- 现在已经能稳定报出一批关键错误
- 但还不能把所有 parser/compiler/runtime 失败都归到统一、受测的分类面

因此这个主题现在更适合记作 `需要补测试`，而不是直接声称诊断系统已经封口。

## Intentional Differences

第一阶段仍保留“有 precedent 但不跟随”的语义位点。也就是说，矩阵里必须允许出现 `明确与外部语言不同并保留差异`，而不是把所有外部 case 都硬翻译成 `ZR` 行为。当前已知差异主要来自：

- Lua/QuickJS 某些 sugar 或 host-specific 行为只作为反例收录
- `ZR` 对 import/internal API、调用参数顺序、部分构造误用会优先选择显式拒绝
- 这些 reject case 必须继续保留在 manifest 中，而不是因为当前还没 runnable 化就从矩阵里删除

## Next Gaps To Close

按这张矩阵继续推进，下一组最值得补的空洞是：

1. 给“表达式与优先级”补 phase1 级 runnable reference fixture，而不只停在 manifest inventory。
2. 给“调用面”补 duplicate named、unexpected named、positional-after-named 的 compile-fail fixture。
3. 给“`<Type>` 转换、prototype/new 误用、`const` 赋值规则”补 explicit cast failure、narrowing diagnostics 和 project 层 fixture。
4. 给“诊断与错误恢复”补统一的 taxonomy 断言，减少只看错误短句的脆弱性。

这四项做完后，计划文件里“6 个主题都要有真实状态和证据来源”的要求才算真正闭环。
