---
related_code:
  - zr_vm_parser/include/zr_vm_parser/lexer.h
  - zr_vm_parser/include/zr_vm_parser/parser.h
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/include/zr_vm_parser/canonical_type.h
  - zr_vm_parser/include/zr_vm_parser/cfg.h
  - zr_vm_parser/include/zr_vm_parser/semantic_ir.h
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/include/zr_vm_parser/writer.h
  - zr_vm_parser/include/zr_vm_parser/test_contract.h
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/lexer.c
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/semantic.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/parser-and-semantics/index.md
  - docs/plans/syntax/2026-07-18-01-canonical-type-place-cfg-artifact-design.md
tests:
  - tests/parser/test_parser.c
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_canonical_type_graph.c
  - tests/parser/test_semantic_query.c
  - tests/parser/test_semir_pipeline.c
  - tests/parser/test_artifact_schema.c
doc_type: api-reference
---

# Parser/Compiler C API

## 解析

```c
SZrParserState ps;
ZrParser_State_Init(&ps, state, source, sourceLength, sourceName);
SZrAstNode *ast = ZrParser_ParseWithState(&ps);
/* ast belongs to caller */
ZrParser_Ast_Free(state, ast);
ZrParser_State_Free(&ps);
```

快捷函数 `ZrParser_Parse` 创建并销毁临时 parser state；增量工具使用
`State_SeekToTokenStart`、`ParseTopLevelStatementWithState` 或
`ParseExpressionWithState`。错误 callback 可同时接收旧文本和 `SZrStructuredDiagnostic`；
production parser 默认 `enableLegacyMigrationParsing=false`，legacy token 会产生 fatal
diagnostic，迁移器才显式开启该开关。

## Compiler state

`ZrParser_CompilerState_Init/Free` 管理常量池、local/closure slots、type environment、CFG、
SemIR、compile-time cache、child functions、instruction array、exception metadata 和 test
manifest。常用入口：

```c
SZrFunction *fn = ZrParser_Compiler_Compile(state, ast);
SZrFunction *testFn = ZrParser_Compiler_CompileTest(state, ast);
const SZrSemanticIrFunction *ir = ZrParser_Compiler_PreSemanticIr(&compilerState);
ZrParser_Compiler_ValidatePreSemanticIr(&compilerState);
```

`CompileWithCurrentModuleKey` 用于 project/module identity 正确传播。`Compile` 失败时不要
读取部分 function；通过 compiler structured error 获取 code、cause、suggestion 和 range。

## Canonical type、CFG 与 SemIR

canonical type API 将 source TypeRef 归一化为 TypeId/owner module/generic arguments；CFG API
`ZrParser_Cfg_Init/Build/Connect/EmitReachabilityFacts` 产生 block/edge/reachability。SemIR
API `SemanticIrFunction_Init/AddLocal/AddValue/AddRegion/AddLoan/Emit/Validate` 记录 Place、
Value、Region、Loan、contiguous-view/bounds/cleanup facts。flow API
`ZrParser_SemanticFlow_Analyze`、`LoanIsLiveAt`、`LoanIsActiveAt` 输出 NLL/definite-assignment
结论。AOT、LSP、reflection 应读取这些 facts，不从 AST token 重新推断。

## Semantic query

semantic query 是 snapshot-borrowed 查询，输出类型不可用通配名称代替：
`TypeAt` 写入 `SZrInferredType`，`CanonicalTypeAt` 写入
`SZrParserSemanticTypeQuery`，`CallAt` 写入 `SZrParserSemanticCallQuery`，`SymbolAt` 写入
`SZrParserSemanticSymbolQuery`。`CallEdgesAt`、`OutgoingCalls` 和 `IncomingCalls` 向调用方提供
的 `SZrArray` 追加 `SZrParserSemanticCallEdgeQuery` 值；`CallCandidatesAt` 追加
`SZrParserSemanticCallCandidateQuery` 值。

这些值本身可以复制，但其中的 AST 指针、显示字符串、module URI 和部分 relation 字段借用自
semantic snapshot。它们只能在对应 `SZrSemanticContext` 的当前 generation 内使用；跨
generation 必须复制所需文本/ID 并重新查询。

## Writer 与 manifest

| API | 输出 |
| --- | --- |
| `ZrParser_Writer_WriteBinaryFile` | `.zro` executable binary |
| `ZrParser_Writer_WriteIntermediateFile` | `.zri` canonical intermediate |
| `ZrParser_Writer_WriteSyntaxTreeFile` | `.zrs` AST projection |
| `ZrParser_Writer_WriteSchedulerArtifactFile` | scheduler/task contract artifact |
| `ZrParser_Writer_WriteAotCFile` | generated C |
| `ZrParser_Writer_WriteAotLlvmFile` | generated LLVM IR |
| `ZrParser_TestManifest_Encode/Decode/Validate` | versioned TestManifest |

writer 在写入前校验 artifact schema、module signature、provider hashes 和 call-binding rows；
调用方必须检查 `TZrBool/EZrArtifactStatus`，失败时删除不完整输出。
