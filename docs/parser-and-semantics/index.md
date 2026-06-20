---
related_code:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/cfg.h
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow.c
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/semantic.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_control_flow.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_loops.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_internal.h
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
  - zr_vm_parser/include/zr_vm_parser/cfg.h
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/semantic.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_facts.c
  - zr_vm_parser/src/zr_vm_parser/semantic/semantic_query.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_control_flow.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_loops.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/cfg_internal.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow.h
  - zr_vm_parser/src/zr_vm_parser/type_inference/dataflow.c
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
  - docs/plans/lsp/01-semantic-inference-core.md
  - docs/plans/lsp/05-implementation-blueprint.md
tests:
  - tests/parser/test_cfg_reachability.c
  - tests/parser/test_cfg_constant_conditions.c
  - tests/parser/test_cfg_switch_constants.c
  - tests/parser/test_cfg_finally_abrupt.c
  - tests/parser/test_dataflow_engine.c
  - tests/parser/test_semantic_query.c
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
  - tests/acceptance/2026-06-20-semantic-stage1-cfg.md
  - tests/acceptance/2026-06-20-semantic-stage1-dataflow.md
  - tests/acceptance/2026-06-20-semantic-stage1-semantic-query.md
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
  - `cfg-reachability-foundation.md`
  - parser CFG scaffold 构建 entry/statement/exit block graph
  - 顺序语句可达性传播覆盖 `return`/`throw`/`break`/`continue` 后的不可达事实
  - `if` statement 已建立 then/else/join 分支图，并覆盖 bool literal、unary `!`、logical `&&`/`||`、短路决定项以及 integer/string/char/float literal 比较条件折叠后的不可达事实
  - `switch` statement 已建立第一版 case body 图，并覆盖 case body 内 terminator 后不可达；bool/integer/string/char/float 常量 selector 会剪掉不匹配的同类常量 case，其中 bool selector/case 可复用 unary/logical 折叠，匹配常量 case 会让后续 default/无命中路径被标为常量分支不可达
  - CFG 构图实现已拆分出 `cfg_control_flow.c`、`cfg_loops.c` 和 `cfg_internal.h`，避免 `cfg.c` 接近 1000 行后继续堆叠控制结构逻辑
  - `while` statement 已建立 condition/body/back-edge/join 循环图，覆盖 `while(false)` 与 folded-false 条件循环体不可达，并把 body 内 `break` 接到 join、`continue` 接回 condition
  - 传统 `for` statement 已建立 init/condition/body/step/back-edge/join 基础图，覆盖 `for(false)` 与 folded-false 条件循环体不可达，并把 body 内 `break` 接到 join、`continue` 接到 step-entry 或 condition
  - `foreach` statement 已建立 entry/body/back-edge/join 基础图，覆盖 body 内 terminator 后不可达，并把 body 内 `break` 接到 join、`continue` 接回 foreach iteration block
  - `try` statement 已建立 try/catch/finally body 图，并覆盖 try/catch/finally body 内 terminator 后不可达；try/catch 内 `return`/`throw`/`break`/`continue` 现在也会进入 finally body，且只有 abrupt completion 时不会让 try 后续语句变可达
  - switch-local break 已审计为当前语言/编译器不支持：编译器只通过 loop label stack 解析 break/continue，switch 不提供 break label
  - 后续需扩展 break/continue 经 finally 后再到原 loop target 的精确目标重写、精确 catch 异常匹配/过滤边、union 穷尽分支、区间/符号值驱动等更广泛常量条件折叠和具体 dataflow 分析
- `dataflow-engine-foundation.md`
  - CFG 上的 forward/backward 工作队列执行框架
  - 每个 block 持有 in/out state，analysis 通过 init/join/transfer 回调定义半格语义
  - 当前引擎过滤 entry 不可达块，避免 unreachable 语句污染后向分析
- `semantic-query-api-foundation.md`
  - parser 侧公共 semantic query 查询面骨架
  - `TypeAt`、`DefinitionOf`、`FactsAt` 和 `Diagnostics` 的当前语义
  - module/node scope 过滤边界，以及后续局部重算和真实 diagnostics 来源
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
10. 再看 `cfg-reachability-foundation.md`，了解 Stage 1 CFG 可达性事实生产者的当前范围和后续边界。
11. 再看 `dataflow-engine-foundation.md`，了解 Stage 1 通用 dataflow 引擎骨架和当前验证边界。
12. 再看 `semantic-query-api-foundation.md`，了解 Stage 1 公共语义查询面骨架和当前限制。
13. 再看 `union-types.md`，了解 union 前端 slice、构造器 lowering 和后续模式匹配边界。
14. 最后看 `lsp-semantic-resolution-and-native-imports.md`，了解 language server 如何消费 parser/native import metadata 并稳定命中局部语义引用。
15. 需要落代码时，再对照 frontmatter 里的 `related_code` 和 `tests` 追踪实现与验证入口。
