---
related_code:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/semantic.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function_assembly.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_local_semantic_query.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_local_semantic_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_constant_condition.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_reachability.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_expression_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_references.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_support.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
  - zr_vm_parser/src/zr_vm_parser/semantic.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_function_assembly.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_local_semantic_query.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_local_semantic_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_internal.h
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_constant_condition.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_reachability.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_expression_query.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_references.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_support.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_symbols.c
  - zr_vm_language_server/src/zr_vm_language_server/reference_tracker.c
plan_sources:
  - user: 2026-03-28 实现“ZR 全目标回归强化与 Field-Scoped using 语义计划”
  - user: 2026-04-04 实现“ZR LSP 语义内核与元信息推断增强计划”
  - user: 2026-04-04 拆分边界“final function assembly + invariant validation”独立出去
  - user: 2026-04-08 Rust-First Ownership / GC 分层设计
  - user: 2026-05-16 Rust-First using / Ownership 语义收敛计划
  - .codex/plans/ZR 全目标回归强化与 Field-Scoped using 语义计划.md
  - .codex/plans/Rust-First Ownership  GC 分层设计.md
  - .codex/plans/Rust-First using  Ownership 语义收敛计划.md
  - docs/superpowers/specs/2026-06-03-zr-vm-semantic-inference-design.md
  - docs/superpowers/plans/2026-06-03-zr-vm-semantic-inference-fact-layer.md
tests:
  - tests/parser/test_semantic_facts.c
  - tests/parser/test_type_inference.c
  - tests/parser/test_parser.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_lsp_local_semantic_hover.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/stdio_smoke.js
  - tests/acceptance/2026-06-03-semantic-inference-fact-layer.md
  - tests/language_server/test_lsp_project_features.c
  - tests/parser/test_compiler_features.c
  - tests/module/test_module_system.c
  - tests/parser/test_union.c
  - tests/acceptance/2026-06-17-union-types.md
doc_type: category-index
---

# Parser And Semantics

本目录记录 parser、semantic analyzer 和编译期前端共享的语言行为。

## Shared Semantic Facts

parser semantic context 现在持有表达式、引用、数值、可达性、逻辑和所有权事实数组。type inference 和 semantic analyzer 在同一次分析中追加事实，LSP 局部查询消费这些事实，并在语法不完整时返回精确事实、诊断阻断结果或显式 unknown。

诊断应包含 stable code、具体原因和建议动作。parser 仍保留 legacy error callback，但当 LSP 或工具可消费结构化诊断时，优先使用 structured diagnostic，再通过标准 LSP message 展示 cause 和 suggestion。

## 当前主题

- `ffi-extern-declarations.md`
  - `%extern("lib") decl` 与 `%extern("lib") { decls }` 源级 FFI 语法
  - extern function / struct / enum / delegate 的 declaration metadata 和 lowering 规则
  - `compileTimeTypeEnv` 与真正 compile-time callable 的边界
- `dynamic-iteration-semir-execbc-aot.md`
  - dynamic foreach 在 `SemIR -> ExecBC -> AOT` 三层中的职责边界
  - `DYN_ITER_INIT` / `DYN_ITER_MOVE_NEXT` 的稳定 runtime contract
  - `SUPER_DYN_ITER_MOVE_NEXT_JUMP_IF_FALSE` 的 quickening 约束
- `dynamic-meta-tail-call-semir-execbc-aot.md`
  - `META_CALL`、`DYN_TAIL_CALL`、`META_TAIL_CALL` 的稳定语义边界
  - tail-call site 上 dynamic/meta dispatch 不再退化成普通 `FUNCTION_TAIL_CALL`
  - 安全条件下的真实 `callInfo` frame reuse 与异常 / `%using` 回退边界
  - AOT backend 继续只依赖共享的 `FUNCTION_PRECALL` runtime contract
- `call-site-quickening-meta-access-semir-aot.md`
  - 零参数 call-site superinstruction、cached `META_CALL/DYN_CALL`、以及 cached tail-call 的 ExecBC quickening 边界
  - property getter/setter 现在直接落成 ExecBC `META_GET` / `META_SET`，并保持同名 `SemIR` / AOT 契约
  - meta access 与 cached dynamic/meta call site 现在都带显式 `CALLSITE_CACHE_TABLE`，并使用固定 2-slot PIC
  - child function 的 `.zri`/AOT metadata 会递归输出，constant function/closure 会重绑到 quickened child function tree
  - quickened ExecBC 与稳定语义层继续解耦
- `ownership-builtins-semir-aot.md`
  - `%borrow/%loan/%upgrade/%release/%detach` 的 parser、ExecBC、SemIR、AOT 契约
  - ownership expression 与 statement `%using` 的边界
  - legacy ownership using helper 的 artifact 收口
- `csharp-value-type-semir-aot.md`
  - C#-style `struct` value-place SemIR contract
  - inline struct field address/load/store and by-value copy metadata
  - AOT ExecIR runtime-contract boundary for typed value operations
- `compiler-final-function-assembly.md`
  - `compiler.c` 只保留 orchestration，最终 `SZrFunction` 装配沉到 `compiler_function_assembly.c`
  - script wrapper / top-level function declaration 共用同一套 final assembly 逻辑
  - `CREATE_CLOSURE -> childFunctions` child graph 不变量在装配期统一校验
- `owned-field-lifecycle.md`
  - direct `%unique/%shared` field 的字段生命周期语义
  - legacy field-scoped `%using` 的迁移边界
  - owner 值跨入 plain GC world 必须显式 `%detach` 或 bridge
  - `%weak` 使用目标前必须显式 `%upgrade`/check，不能直接流入 `%borrowed`
  - cleanup plan 与 prototype metadata 的传播路径
- `semantic-fact-layer.md`
  - `SZrSemanticContext` 统一持有表达式、引用、数值、可达性、逻辑和所有权事实
  - 事实层提供 append-by-copy、reset/free 和按节点/位置查询契约
  - type inference 已开始写入字面量和二元数值表达式事实
  - LSP semantic analyzer 已开始写入声明和使用引用事实，并保留完整 token 范围
  - semantic analyzer 已在不可达语句、常量分支和确定性短路处写入 reachability/logical facts
  - semantic analyzer 已能从同作用域 `var const flag = true/false` 的布尔初始化推断局部分支不可达、常量真分支退出后的后续不可达，并通过 LSP hover/rich hover 暴露具体原因
  - semantic analyzer 已为代表性所有权违规写入 ownership facts，并用 stable code/cause/suggestion 替代泛化 `type_mismatch`
  - parser 已为代表性语法错误提供结构化诊断，LSP 会展示 stable code、cause 和 suggestion
  - LSP 局部语义查询已返回 fact、diagnostic-backed failure 或 explicit unknown，并让 hover 避免在语法阻断位置继续误导性推断
  - Debug、REPL 后续消费共享事实，避免重复局部推断
- `lsp-semantic-resolution-and-native-imports.md`
  - `this` / `super` / `%compileTime` / `%test` / lambda 的局部符号命中规则
  - reference tracker 的“最窄范围优先”策略
  - `%import("zr.math")` 如何在语义分析阶段预热 native metadata，支撑 `$math.Vector3(...).y`
  - imported type 只允许 `module.Type` 或 `var {Type} = %import(...)` 两种显式绑定路径
  - nested native module lookup 与 compile-only imported stub 如何避免递归和 runtime prototype 污染
- `union-types.md`
  - Rust-like `union` 声明、unit/tuple/struct variant AST 和泛型声明解析
  - `Shape.Circle(...)` / `Option<int>.Some(...)` 构造器解析、类型推断和 object carrier lowering
  - LSP type prototype / symbol 支持，以及后续 pattern matching、tagged layout、owner drop 分派边界

## 阅读顺序

1. 先看 `ffi-extern-declarations.md`，了解 `%extern` 语法、descriptor schema 和 `zr.ffi` lowering 路径。
2. 再看 `dynamic-iteration-semir-execbc-aot.md`，了解动态迭代在 SemIR、ExecBC quickening 与 AOT 契约之间的边界。
3. 然后看 `dynamic-meta-tail-call-semir-execbc-aot.md`，了解 dynamic/meta 调用在 tail-site 上的稳定语义契约。
4. 再看 `call-site-quickening-meta-access-semir-aot.md`，了解 zero-arg/cached call-site quickening、meta access PIC、以及 child artifact 对齐规则。
5. 再看 `ownership-builtins-semir-aot.md`，了解 ownership builtin 与 statement `%using` 的稳定语义边界。
6. 再看 `csharp-value-type-semir-aot.md`，了解 C#-style struct value-place SemIR 和 AOT 边界。
7. 再看 `compiler-final-function-assembly.md`，了解 parser orchestration 与 final function assembly 的边界。
8. 再看 `owned-field-lifecycle.md`，了解 direct owner field 的生命周期语义。
9. 再看 `semantic-fact-layer.md`，了解 parser 侧共享语义事实容器、LSP 局部查询三态结果和查询契约。
10. 再看 `union-types.md`，了解 union 前端 slice、构造器 lowering 和后续模式匹配边界。
11. 最后看 `lsp-semantic-resolution-and-native-imports.md`，了解 language server 如何消费 parser/native import metadata 并稳定命中局部语义引用。
12. 需要落代码时，再对照 frontmatter 里的 `related_code` 和 `tests` 追踪实现与验证入口。
