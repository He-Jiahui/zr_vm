---
related_code:
  - zr_vm_parser/include/zr_vm_parser/parser.h
  - zr_vm_parser/include/zr_vm_parser/lexer.h
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/include/zr_vm_parser/diagnostic_builder.h
  - zr_vm_parser/include/zr_vm_parser/semantic_query.h
  - zr_vm_parser/include/zr_vm_parser/writer.h
  - zr_vm_parser/include/zr_vm_parser/test_contract.h
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_parser/src/zr_vm_parser/semantic.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type.c
  - zr_vm_parser/src/zr_vm_parser/semantic_ir.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
plan_sources:
  - user: 2026-09-10 继续细化 Wiki，要求补充语法、用例、C native 库调用和内置接口
  - docs/parser-and-semantics/index.md
  - docs/plans/syntax/2026-07-18-01-canonical-type-place-cfg-artifact-design.md
tests:
  - tests/parser/test_parser.c
  - tests/parser/test_pre_semantic_ir.c
  - tests/parser/test_canonical_type_graph.c
  - tests/parser/test_semantic_query.c
  - tests/parser/test_semir_pipeline.c
  - tests/parser/test_artifact_schema.c
  - tests/cli/test_cli_repl_e2e.c
doc_type: api-reference
---

# Parser、Compiler 与 Semantic C API

本页给出从 UTF-8 字节串得到可执行 `SZrFunction` 的 C 调用方案，并说明每个中间对象的
owner、错误边界和可查询事实。它比 [Parser C API](c-api-parser.md) 更偏向嵌入式实现者：
需要做 IDE 增量解析、REPL、代码生成、静态分析或自定义 artifact writer 时，可以按本页
的阶段拆开调用；只想运行一段源码时，使用文末的 `ZrParser_Source_Compile` 封装即可。

## 1. 总体流水线

Parser 层不负责类型推断或模块加载，Compiler 也不会替宿主创建 global/state。一个完整
调用链如下：

```text
SZrGlobalState / SZrState（宿主创建）
    -> SZrParserState + Lexer
    -> token / AST / structured diagnostics
    -> import canonicalization + build facts
    -> canonical type / symbol / ownership / CFG
    -> PreSemanticIr validation
    -> ExecBC function or AOT/writer artifact
    -> module cache / VM execute
```

每一层都有自己的成功条件：

| 层 | 成功表示 | 仍未保证的内容 |
| --- | --- | --- |
| lexer | token 可读、范围可定位 | token 组合未必是合法声明 |
| parser | AST shape 合法，普通 syntax error 未报告 | 类型、import、ownership、provider |
| semantic | symbol/type/flow facts 可用 | 后端指令、AOT 入口、动态 provider |
| compiler | `SZrFunction *` 完整且可链接 | writer 输出、运行时资源和外部插件 |
| runtime | function 在当前 state 可执行 | 另一个 state、另一个 generation 的借用视图 |

不要把 `parserState.hasError == false` 当作“程序正确”；也不要把 compiler 返回的非空
指针当作可以跨 global 保存的稳定 ABI。所有 pointer 的有效期都以公共 header 和对应
owner 为准。

## 2. Parser state 和 lexer

### 2.1 初始化和关闭

`ZrParser_State_Init` 为 parser 分配 lexer，记录 source、长度、source name，并把游标置于
第一个 token。`source` 由调用方持有，解析完成前必须保持不可变；`sourceName` 是借用的
`SZrString *`，用于每个 `SZrFileRange` 的文件标识。

```c
#include "zr_vm_parser/parser.h"
#include "zr_vm_core/string.h"

SZrParserState parser;
const TZrChar source[] = "module demo; let answer: int = 42;";
SZrString *sourceName = ZrCore_String_Create(state, "demo.zr");

ZrParser_State_Init(&parser, state, source, sizeof(source) - 1u, sourceName);
if (parser.hasError) {
    /* Allocation failure or lexer initialization failure. */
    ZrParser_State_Free(&parser);
    return ZR_FALSE;
}

SZrAstNode *ast = ZrParser_ParseWithState(&parser);
ZrParser_State_Free(&parser);
if (ast == ZR_NULL) {
    return ZR_FALSE;
}
/* Consume or transfer ast, then release it exactly once. */
ZrParser_Ast_Free(state, ast);
```

`ZrParser_State_Free` 只释放 lexer 和 parser-owned storage，不释放返回的 AST。AST 由调用
方拥有，必须在同一个 `SZrState` 上调用 `ZrParser_Ast_Free`；即使 parser 报错后返回了
部分树，也不能把它交给另一个 state。

### 2.2 三个解析粒度

| 函数 | 前置条件 | 返回节点 | 典型使用 |
| --- | --- | --- | --- |
| `ZrParser_Parse` | 有效 state、非空 source | 新建并释放临时 state 的 `ZR_AST_SCRIPT` | 一次性文件 |
| `ZrParser_ParseWithState` | 已初始化 state，游标在任意 token | 完整 script AST | 编译、批量分析 |
| `ZrParser_ParseTopLevelStatementWithState` | 游标不是 EOS | 一个顶层声明/语句 | REPL、增量编辑 |
| `ZrParser_ParseExpressionWithState` | 游标开始于表达式 | 一个表达式 AST | LSP hover、表达式评估 |

增量入口不会自动跳过任意字符。`ZrParser_State_SeekToTokenStart` 只有在
`sourceOffset` 恰好等于 token 起点时才返回 true；空白、注释、EOS 和半个 UTF-8 字符偏移
都会返回 false。调用方应先用编辑器的 byte offset 映射到 lexer 的 token boundary。

### 2.3 callback 和错误模式

`SZrParserState` 同时保留旧的文本 callback 和结构化 callback：

```c
static void on_structured_error(
    TZrPtr userData,
    const SZrStructuredDiagnostic *diagnostic,
    EZrToken token) {
    ErrorSink *sink = (ErrorSink *)userData;
    if (sink != ZR_NULL && diagnostic != ZR_NULL) {
        sink->count++;
        error_sink_copy(sink, diagnostic, token);
    }
}

parser.structuredErrorCallback = on_structured_error;
parser.errorUserData = (TZrPtr)&sink;
parser.suppressErrorOutput = ZR_TRUE; /* LSP/IDE usually owns the output channel. */
```

callback 收到的 `diagnostic` 及其 `message/code/fixes` 字段在 callback 返回后不应被直接
保存。需要跨线程或跨解析 generation 使用时，调用
`ZrParser_StructuredDiagnostic_Copy(state, &owned, diagnostic)`，并在 owner 结束时调用
`ZrParser_StructuredDiagnostic_Free`。`token` 只说明触发错误的 lexer token，不是稳定的
AST node id。

production parser 遇到可恢复错误会尝试跳到分号、声明关键字或下一个安全边界；连续错误
达到内部上限会停止。删除的 legacy syntax 会设置 `hasFatalError`，而
`enableLegacyMigrationParsing` 默认 false，因而 `ParseWithState` 会释放部分 AST 并返回
空指针。只有迁移前端显式开启该字段，才能取得用于自动修复的旧 shape。

## 3. AST 的读取和所有权

AST 结构定义在 `ast.h`，节点以 `EZrAstNodeType` 区分。常见根节点是
`ZR_AST_SCRIPT`，其 `data.script.moduleName` 和 `data.script.statements` 指向 parser
分配的子节点/数组。数组元素、字符串和类型节点都由 `ZrParser_Ast_Free` 递归释放。

读取 AST 时应采用只读遍历，不要修改 parser 生成的数组长度或把内部字符串替换为宿主
分配的指针：

```c
static void visit_script(const SZrAstNode *script) {
    if (script == ZR_NULL || script->type != ZR_AST_SCRIPT ||
        script->data.script.statements == ZR_NULL) {
        return;
    }
    for (TZrSize i = 0; i < script->data.script.statements->length; ++i) {
        const SZrAstNode *statement = script->data.script.statements->nodes[i];
        inspect_statement(statement); /* read-only */
    }
}
```

AST 地址不等于 `TZrSymbolId`、`TZrTypeId` 或 metadata token。需要持久化索引时，应在
semantic/compiler 阶段取得稳定 id，并复制 source range 和显示文本；不要把节点地址写入
数据库、LSP cache 或 AOT artifact。

## 4. 一次性 source-to-function API

### 4.1 `ZrParser_Source_Compile`

```c
SZrFunction *ZrParser_Source_Compile(
    SZrState *state,
    const TZrChar *source,
    TZrSize sourceLength,
    SZrString *sourceName);
```

该封装按 production 顺序执行：初始化 parser -> 捕获 parser error -> 解析 AST -> 准备
compile-time build facts -> canonicalize project imports -> 准备 current source module ->
调用 compiler -> 清除 module-init AST identity -> 释放 AST。`sourceLength == 0`、state/source
为空或任一步失败都返回 `ZR_NULL`。成功后 AST 已经释放，返回的 function 由 VM/function
owner 管理，不要再次用 `ZrParser_Ast_Free`。

`ZrParser_Source_CompileTest` 与它相同，但临时把 state provider phase 设为
`ZR_LIBRARY_PROVIDER_PHASE_TEST` 并生成 test manifest；函数返回前会恢复原 phase。测试
runner 不应把 test function 当生产入口。

### 4.2 module key 和 submission

当宿主已经知道当前模块 canonical key，应使用：

```c
SZrString *moduleKey = ZrCore_String_Create(state, "demo.entry");
SZrFunction *function = ZrParser_Compiler_CompileWithCurrentModuleKey(
    state, ast, moduleKey);
```

这样可以让 imported symbol、public contract 和 call-binding module hash 使用同一 identity。
不传 key 的 `Compile` 适合没有 project resolver 的单元测试，但不能用来伪造跨文件模块
身份。

REPL/增量编译使用 `ZrParser_Source_CompileSubmission`。其
`SZrParserSubmissionContext` 是只读快照，包含先前 binding、callable signature、module/
environment/cell generation；成功时 `SZrParserSubmissionResult` 发布新 binding 和
signature：

```c
SZrParserSubmissionResult result;
memset(&result, 0, sizeof(result));
SZrFunction *cell = ZrParser_Source_CompileSubmission(
    state, source, sourceLength, sourceName, &snapshot, &result);
if (cell != ZR_NULL) {
    publish_submission_bindings(&result);
}
ZrParser_SubmissionResult_Free(state, &result);
```

发布必须是原子动作：编译失败时不要把 `result.bindings` 的部分元素合并到旧环境。
`ZrParser_CompilerState_SeedSubmissionContext` 只把验证过的只读快照接入 compiler state，
不会复制宿主拥有的 source/module 字符串；快照必须活到该 compiler state 释放。

## 5. 拆分 compiler state

需要观察中间事实或实现自定义 pipeline 时，直接管理 `SZrCompilerState`：

```c
SZrCompilerState compiler;
ZrParser_CompilerState_Init(&compiler, state);

/* `ast` remains alive for the entire compiler state lifetime. */
ZrParser_Statement_Compile(&compiler, statement);
const SZrSemanticIrFunction *ir =
    ZrParser_Compiler_PreSemanticIr(&compiler);
if (ir != ZR_NULL && !ZrParser_Compiler_PreSemanticIrIsValidated(&compiler)) {
    if (!ZrParser_Compiler_ValidatePreSemanticIr(&compiler)) {
        consume_compiler_diagnostic(&compiler);
    }
}

ZrParser_CompilerState_Free(&compiler);
```

初始化会建立以下 compiler-owned 结构：

| 结构 | 内容 | 为什么重要 |
| --- | --- | --- |
| `constants` | `SZrTypeValue` 常量池 | 指令和 AOT 引用共享稳定索引 |
| `localVars`、`closureVars` | 栈槽、捕获和 alias | 生成 load/store/closure layout |
| `scopeStack`、`loopLabelStack` | 作用域和 break/continue 目标 | 清理 edge 必须在跳转前落地 |
| `tryContextStack`、handler arrays | catch/finally 元数据 | abrupt control 经过 cleanup |
| `typeEnv`、`typePrototypes` | canonical type 和 prototype | member/constructor contract |
| `preSemanticIr` | Place/Value/Region/Loan/CFG facts | 后端只消费已验证事实 |
| compile-time arrays/cache | build facts、provider binding、digest | compile-only effect 隔离 |
| `testManifestEntries` | typed test metadata | Test phase artifact |

`ZrParser_Expression_Compile` 和 `ZrParser_Statement_Compile` 是低层入口，适合 parser/
semantic 单元测试；它们可能只产生部分状态，不应单独当作完整 module compiler。完成后
必须检查 `compiler.hasError`、`hasFatalError`、`hasStructuredError`，并在失败时丢弃整份
function/IR，而不是继续 writer。

## 6. Semantic、CFG 和 SemIR 查询

compiler 的事实接口把“源码长什么样”和“程序做什么”分开：

1. canonical type 把 `TypeRef` 归一化为 `TZrTypeId`、owner module、generic arguments 和
   ownership qualifier。
2. symbol/semantic context 建立 declaration/reference/call facts。
3. CFG 计算 reachability、definite assignment、pending control 和 cleanup edge。
4. SemIR 记录 `Place`、`Value`、`Region`、`Loan`、typed call/property/index、effect。
5. `ValidatePreSemanticIr` 确认 backend 不会看到未绑定的 place、悬空 loan 或未解析的
   call target。

语义查询返回的是 snapshot-borrowed view：

```c
SZrParserSemanticTypeQuery typeQuery;
if (ZrParser_SemanticQuery_TypeAt(context, sourceOffset, &typeQuery)) {
    if (typeQuery.typeId != ZR_TYPE_ID_INVALID && typeQuery.expression != ZR_NULL) {
        show_type(typeQuery.typeId, typeQuery.expression->exactness);
    }
}
```

`SZrParserSemanticCallQuery`、`SZrParserSemanticSymbolQuery`、relation/call-edge 数组中的
AST 指针、`SZrString *`、external origin 和 fact pointer 都只能在同一 semantic context
generation 使用。跨 generation 只保留 `TZrSymbolId`、`TZrTypeId`、metadata token、复制的
range/text；然后重新查询。若 fact exactness 是 `UNKNOWN` 或 `APPROXIMATE`，调用方必须
fail closed，不能根据源代码字符串猜类型或重载。

## 7. Structured diagnostic 的生命周期

parser/compiler 的诊断结构包含：

| 字段 | 作用 | ownership |
| --- | --- | --- |
| `severity` | error/warning/info/hint | 值字段 |
| `location` | source、byte offset、line、column | source 指针随 snapshot 借用 |
| `code`、`message`、`cause` | 机器码和人类说明 | `SZrString *`，由诊断 owner 管理 |
| `suggestion` | 简短修复建议 | 同上 |
| `relatedInformation` | 相关声明/调用位置 | 数组由 diagnostic 管理 |
| `fixes` | edit range、text、applicability | 只能在明确适用性时自动应用 |
| `descriptorId` | provider/descriptor 关联 | 稳定整数 |
| `noFixReason` | 无法生成修复的原因 | 枚举 |

初始化、复制和释放应成对出现：

```c
SZrStructuredDiagnostic owned;
ZrParser_StructuredDiagnostic_Init(&owned);
if (compiler.hasStructuredError) {
    ZrParser_StructuredDiagnostic_Copy(state, &owned, &compiler.structuredError);
    diagnostics_store(&owned); /* store makes another copy or takes ownership */
}
ZrParser_StructuredDiagnostic_Free(state, &owned);
```

不要在 `CompilerState_Free` 或 semantic snapshot 释放后访问原诊断。一个普通
`hasError == true` 但没有 structured error 的路径仍可能只有 `errorMessage`；宿主应同时
记录文本和结构化字段，不能只依赖其中一个。

## 8. Writer 和 artifact 边界

function/AST 成功后，可以选择 writer 输出：

| API | 产物 | 输入 owner |
| --- | --- | --- |
| `ZrParser_Writer_WriteBinaryFile` | `.zro` 可执行 binary | 完整 `SZrFunction` |
| `ZrParser_Writer_WriteIntermediateFile` | `.zri` canonical intermediate | 完整 function/semantic facts |
| `ZrParser_Writer_WriteSyntaxTreeFile` | `.zrs` AST projection | 仍存活的 AST |
| `ZrParser_Writer_WriteSchedulerArtifactFile` | scheduler/task contract | validated task metadata |
| `ZrParser_Writer_WriteAotCFile` | generated C | 已链接 function |
| `ZrParser_Writer_WriteAotLlvmFile` | LLVM IR | 已链接 function |

writer 返回 `TZrBool` 或 `EZrArtifactStatus`；失败时输出文件可能已经创建，调用方应关闭
文件并删除不完整产物。写入前 writer 会检查 artifact schema、module signature、provider
hash、call-binding rows 和 generation；不能通过跳过 `ValidatePreSemanticIr` 来获得“部分
可用”的文件。

Test 编译额外生成 `SZrParserTestManifest`。它使用
`ZrParser_TestManifest_Encode/Decode/Validate/Free` 的版本化格式；manifest bytes 的
owner 是调用方，decode 后的字符串/数组必须由 `Free` 释放。

## 9. 一个可复制的宿主函数

下面的函数展示“解析、编译、写出、清理”的安全顺序。真实工程还要先创建 global/state
并注册 core/library provider，相关顺序见 [C 宿主集成指南](c-host-guide.md)。

```c
TZrBool compile_file_to_binary(
    SZrState *state,
    const TZrChar *source,
    TZrSize sourceLength,
    SZrString *sourceName,
    const TZrChar *outputPath) {
    SZrAstNode *ast = ZR_NULL;
    SZrFunction *function = ZR_NULL;
    SZrParserState parser;
    TZrBool success = ZR_FALSE;

    ZrParser_State_Init(&parser, state, source, sourceLength, sourceName);
    if (!parser.hasError) {
        ast = ZrParser_ParseWithState(&parser);
    }
    if (ast == ZR_NULL || parser.hasError || parser.hasFatalError) {
        collect_parser_error(&parser);
        ZrParser_State_Free(&parser);
        return ZR_FALSE;
    }
    ZrParser_State_Free(&parser);

    function = ZrParser_Compiler_Compile(state, ast);
    if (function != ZR_NULL) {
        success = ZrParser_Writer_WriteBinaryFile(state, function, outputPath);
    }
    if (!success) {
        remove_incomplete_artifact(outputPath);
    }

    ZrParser_Ast_Free(state, ast);
    /* function is owned by state/function graph; release it through the runtime owner. */
    return success;
}
```

这里的 `function` 注释是有意保守的：当前 public API 没有一个通用的
`ZrParser_Function_Free`，因此宿主不能擅自 `free(function)`。它通常挂在 state/global 的
function graph，关闭顺序由 [嵌入式宿主生命周期](embedding-lifecycle.md) 规定。

## 10. 常见错误和排查顺序

| 现象 | 先检查 | 不要做的事 |
| --- | --- | --- |
| `ParseWithState` 返回空 | `hasError`、`hasFatalError`、structured callback | 把部分 AST 当成功结果 |
| 编译返回空但 parser 成功 | canonical type/import/flow/compiler diagnostic | 只打印“syntax ok”就继续 writer |
| LSP 查询偶发乱码 | semantic generation 和 borrowed `SZrString` | 跨请求保存 fact pointer |
| REPL 新 binding 污染旧 cell | submission snapshot generation 和原子 publish | 逐个推送未完成 result |
| `.zro` 无法加载 | writer status、schema、module/provider hash | 保留失败生成的半文件 |
| test provider 在 runtime 出现 | `ZrParser_Source_CompileTest` 的 phase 恢复 | 永久修改 state phase |
| legacy 代码无 AST | `enableLegacyMigrationParsing` 是否只在迁移 frontend 开启 | 在 production parser 中放宽 fatal gate |

诊断链建议保留 `sourceName`、byte range、diagnostic code、module key、provider phase、
compiler phase 和 artifact path。这些信息足够把问题归类到 parser、semantic、compiler、
writer 或 runtime，而不是在 C API 边界猜测。

## 11. API 速查

```text
Parser:
  State_Init / State_Free / State_SeekToTokenStart
  Parse / ParseWithState / ParseTopLevelStatementWithState / ParseExpressionWithState
  Ast_Free

Compiler:
  CompilerState_Init / CompilerState_Free
  Compiler_Compile / Compiler_CompileTest / Compiler_CompileWithCurrentModuleKey
  Expression_Compile / Statement_Compile
  PreSemanticIr / PreSemanticIrIsValidated / ValidatePreSemanticIr
  Source_Compile / Source_CompileTest / Source_CompileSubmission
  SubmissionResult_Free / CompilerState_SeedSubmissionContext

Diagnostics:
  StructuredDiagnostic_Init / Copy / Free / AddFix / AddRelatedInformation

Output:
  Writer_WriteBinaryFile / WriteIntermediateFile / WriteSyntaxTreeFile
  Writer_WriteAotCFile / Writer_WriteAotLlvmFile
  TestManifest_Encode / Decode / Validate / Free
```

所有函数的参数类型、导出宏和结构体布局以当前 checkout 的 `include/` 头文件为准；本页
的代码片段用于说明顺序，不承诺不同 ABI 版本之间可以直接链接。
