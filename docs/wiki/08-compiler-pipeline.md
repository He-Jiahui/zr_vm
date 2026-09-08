---
related_code:
  - zr_vm_parser/include/zr_vm_parser/lexer.h
  - zr_vm_parser/include/zr_vm_parser/parser.h
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/canonical_type.h
  - zr_vm_parser/include/zr_vm_parser/cfg.h
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/include/zr_vm_parser/writer.h
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/src/zr_vm_parser/lexer.c
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/semantic.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/lexer.c
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/semantic.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
tests:
  - tests/parser/test_parser.c
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_canonical_type_graph.c
  - tests/parser/test_semantic_facts.c
  - tests/parser/test_semantic_query.c
  - tests/parser/test_compiler_features.c
  - tests/parser/test_semir_pipeline.c
  - zr_vm_aot/tests/parser/test_execbc_aot_pipeline.c
  - tests/parser/test_artifact_schema.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/parser-and-semantics/index.md
  - docs/plans/syntax/2026-07-18-01-canonical-type-place-cfg-artifact-design.md
  - docs/plans/aot/03-instruction-set-refactor.md
doc_type: module-detail
---

# 编译器流水线

**状态：`current`；AOT/LLVM lowerings 的完整覆盖仍为 `experimental`。**

## 总体数据流

```text
.zr bytes
  -> Lexer tokens + source ranges
  -> Parser AST (syntax only)
  -> declaration binding / symbol table
  -> canonical Type graph + generic definitions
  -> CFG + dataflow facts (reachability, init, borrow, loans)
  -> bound expressions + Semantic IR
  -> final function assembly / ExecBC quickening
  -> .zrs / .zri / .zro / AOT C / AOT LLVM
```

AST 只记录语法和恢复范围；它不拥有最终 TypeId、borrow state、runtime ownership 或 artifact identity。后续阶段必须消费 canonical facts，不能重新从 token、显示类型字符串或具体类型名推断。

## Lexer

lexer 识别标识符、关键字、数字/字符串/字符 literal、运算符、注释和 source location。换行保留为位置 trivia，不执行 ASI。`%` 的 lexical token 只能表示 modulo/modulo-assignment；旧 percent-prefixed forms 被识别后立即生成 `legacy_syntax_removed`，没有 production AST。

## Parser 与 AST

parser 建立 module、declaration、expression、statement、property accessor、pattern、attribute、native extern 和 recovery node。所有节点带稳定范围，供 diagnostic/LSP 使用；AST node array 由 `ZrParser_AstNodeArray_*` 管理，生命周期绑定 state。

解析阶段只回答“语法是否合法”。例如 `init Point(...)` 先是 value-construction syntax，具体 Point 是否具备 construction capability 由 binding/type inference 决定。

## Binding 与 canonical Type

semantic context 为 nominal、generic、array、tuple、union、nullable、ref、owner、readonly 和 function types 分配稳定 `TZrTypeId`。definition/prototype、generic constraints、interface variance、constructor capability 和 GC scan kind 都登记到 canonical type index。

重要入口：

```c
TZrTypeId ZrParser_CanonicalType_InternPrimitive(...);
TZrTypeId ZrParser_CanonicalType_InternNominal(...);
TZrTypeId ZrParser_CanonicalType_InternGenericInstance(...);
TZrTypeId ZrParser_CanonicalType_InternArray(...);
TZrTypeId ZrParser_CanonicalType_InternUnion(...);
TZrTypeId ZrParser_CanonicalType_InternRef(...);
TZrTypeId ZrParser_CanonicalType_InternOwner(...);
TZrTypeId ZrParser_CanonicalType_InternFunction(...);
const SZrCanonicalTypeNode *ZrParser_CanonicalType_Find(...);
```

函数类型结构哈希包含 passing mode；不能用格式化签名作为 identity。

## CFG 与 dataflow

`ZrParser_Cfg_Build` 为每个 function 建 block/edge；`ZrParser_Cfg_EmitReachabilityFacts` 发布可达性。dataflow 计算：

- `INIT`/`UNINIT`/`MAYBE_INIT` definite assignment；
- reaching definition 与 source range；
- borrow/loan live region、conflict 和 escape；
- constant condition/switch exhaustiveness；
- pending cleanup/exception edge。

LSP 查询和 compiler diagnostics 都消费这些 producer facts，不能在 request-time 自己走 AST/name fallback。

## Semantic IR

Semantic IR 是 backend-neutral 的语义层，典型 opcode 包括 `VALUE_ADDR`、`FIELD_ADDR`、`LOAD_VALUE`、`STORE_VALUE`、`INIT_VALUE`、`COPY_VALUE`、`CALL_TYPED`、`RETURN_TYPED`、`META_GET/SET`、`DYN_CALL`、ownership transitions、property ref、dynamic index/iterator 和 typed arithmetic。它携带 TypeId、PlaceId、source range、effect 和 cleanup information。

ExecBC 可以把多个安全的 IR 指令 quicken 成 superinstruction（如 zero-arg call 或 dynamic iterator move-next/jump），但 `.zri` 的 SemIR 区段和 AOT artifact 保留可复现的原始语义。

## Compile-time 与 declaration transform

`comptime` evaluator 在受限 execution scope 中执行 typed expression、build feature predicate、attribute validation 和 declaration patch。source import ownership、comptime cache、patch transaction 和 diagnostics 都是 parser/compiler contract；运行时 module 不可读取 compile-time-only values。

面向使用者的 provider 入口分别见 [`zr.compile`](03-modules/compile.md) 和
[`zr.compile.declaration`](03-modules/compile-declaration.md)；本节只描述它们在流水线中的
阶段位置和数据流。

声明 transform 只可通过类型化 `DeclarationPatch` 添加 GeneratedField/Method/Property 等声明；不能改 token、已有成员或递归触发 transform。生成结果重新进入 canonical binding、metadata、AOT reachability 和 LSP。

## Compiler final assembly

compiler 将 bound expressions/statement lowering 到 `SZrFunction`：instruction stream、constant pool、local/closure metadata、frame layout、exception handlers、module effects、typed export symbols、callsite caches 和 debug ranges。final assembly 阶段验证 stack balance、label resolution、return arity、ownership effect、property accessor identity 和 function graph identity。

公开 API：

| API | 作用 |
|---|---|
| `ZrParser_CompilerState_Init/Free` | 初始化/释放编译上下文。 |
| `ZrParser_Source_Compile` | 编译 source bytes 为 entry function。 |
| `ZrParser_Source_CompileTest` | 以 test roots 编译。 |
| `ZrParser_Source_CompileSubmission` | REPL/增量 submission 编译。 |
| `ZrParser_Compiler_PreSemanticIr` | 获取 pre-semantic IR。 |
| `ZrParser_Compiler_ValidatePreSemanticIr` | 校验 IR invariants。 |
| `ZrParser_Compiler_Compile` | AST -> runtime function。 |
| `ZrParser_Expression_Compile` / `ZrParser_Statement_Compile` | 编译单个表达式/语句。 |
| `ZrParser_Compiler_MatchNamedArguments` | 生成参数映射。 |
| `ZrParser_Compiler_ValidateReferenceEscapes` | 检查 ref-like/borrow escape。 |
| `ZrParser_CompileTimeDeclaration_Execute` | 执行 compile-time declaration。 |

## Writer 与诊断

writer 提供 `.zrs`、`.zri`、`.zro`、AOT C 和 AOT LLVM 输出；`SZrAotWriterOptions` 控制 full-AOT、stripping、fallback warning、preserve roots 和 manifest exports。artifact writer 在写出前必须 join token/signature/layout/module identity；缺失 identity 直接返回 `EZrArtifactStatus` 和 `SZrArtifactDiagnostic`。
