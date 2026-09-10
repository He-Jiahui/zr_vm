---
related_code:
  - docs/zr_language_specification.md
  - zr_vm_parser/include/zr_vm_parser/lexer.h
  - zr_vm_parser/include/zr_vm_parser/parser.h
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/src/zr_vm_parser/lexer.c
  - zr_vm_parser/src/zr_vm_parser/parser
  - tests/fixtures/projects/syntax_reference_v1
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/lexer.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expressions.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_declarations.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_types.c
tests:
  - tests/parser/test_parser.c
  - tests/parser/test_syntax_reference_v1.c
  - tests/fixtures/projects/syntax_reference_v1/golden/coverage.json
  - tests/fixtures/projects/syntax_reference_v1/golden/diagnostics.json
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/zr_language_specification.md
  - docs/plans/syntax/README.md
doc_type: category-index
---

# ZR 语言手册

本目录是 ZrVm 当前源码语言的逐项参考。它按照“先看表面语法，再看语义边界，最后看宿主接口”的顺序组织，目标是让读者可以从一个 `.zr` 文件一路追踪到 lexer、AST、Semantic IR、运行时和 C API。页面描述的是当前 checkout 中 production parser 实际接受的语言，不把路线图或历史草案写成已经可用的特性。

## 适用范围

ZR 源文件通常以 `.zr` 结尾，并在 project manifest（`.zrp`）的 `src`、`generated` 或测试根目录中被解析。一个源文件可以有可选的 `module path;` 声明，然后是顶层声明、表达式语句和顶层 `return`。解析器先产生 AST；compiler 再把 AST 归一化为 canonical type、Place/Value/Loan、CFG 和 SemIR，最后交给 VM、AOT 或 LSP 消费。

下面这条链是阅读本手册时最重要的实现模型：

```text
UTF-8 source
    -> lexer (token + source range + literal payload)
    -> parser (AST + structured diagnostic)
    -> semantic/canonicalization (TypeId, Place, CFG, Loan, SemIR)
    -> backend (ExecBC / AOT C / AOT LLVM)
    -> runtime and host bindings
```

## 实现机制速览

| 阶段 | 主要产物 | 必须保持的契约 | 继续阅读 |
| --- | --- | --- | --- |
| Lexer | token、literal payload、byte/line/column range | 换行不插入分号；注释不改变范围；UTF-8 只在实现允许的位置进入值 | [词法与字面量](lexical-structure.md) |
| Parser | AST、声明/表达式 shape、结构化 diagnostic | 只判断语法和恢复边界，不猜类型、owner 或 provider | [声明与类型构造](declarations.md)、[表达式与运算符](expressions.md) |
| Binding | symbol table、module identity、canonical `TypeId` | 泛型实参、passing mode、module domain 必须参与 identity；不能用显示字符串代替 | [编译器流水线](../08-compiler-pipeline.md) |
| Flow/CFG | basic block、reachability、definite assignment、loan/cleanup edge | `return`、`throw`、`break`、`continue`、`yield` 和 `using` 共用 pending-control/cleanup 规则 | [控制流与资源清理](control-flow.md)、[控制流与错误](../06-control-flow-and-errors.md) |
| SemIR | Place/Value/Region/Loan、typed call/property/index、effect | backend 只消费已验证的语义事实；禁止从 AST token 在后端重新推断 ownership | [编译器流水线](../08-compiler-pipeline.md) |
| Backend | ExecBC、AOT C、AOT LLVM、`.zri/.zro/.zrs` | quickening/cache 只能优化已验证 contract，失配必须回到 checked path | [AOT 后端](../10-aot-backends.md)、[VM 运行时](../09-vm-runtime.md) |
| Runtime | stack/frame、GC、exception、module cache、native registry | managed 指针受 root/pin/generation 约束；跨线程/跨 domain 必须经过 provider contract | [类型、布局与内存模型](../04-types-and-memory.md)、[Core C API](../05-interop/c-api-core.md) |

这张表解释了为什么本手册会同时给出“能否解析”和“何时会失败”两种结论：例如
`own FileHandle()` 在 parser 阶段只是 construction AST，只有 canonicalization、ownership flow
和 resource descriptor 都通过后，compiler 才能生成 Drop/cleanup 语义；`import("...")` 的
字符串形状可以在 parser 验证，但具体 provider、版本和 artifact 要到 Library resolver 才确定。

### 一次调用的边界

一个嵌入式宿主通常按下面顺序推进状态；任一步失败都应保留该阶段的 diagnostic，并停止把
不完整产物交给下一阶段：

```text
Global/State
  -> Parser_State_Init + callback
  -> ParseWithState
  -> (hasError/hasFatalError ? stop : AST ownership transfer)
  -> Compiler_Compile / Writer_Write*
  -> Project resolver + native/provider admission
  -> VM Execute or AOT entry
  -> release AST, artifacts, handles, state, global in owner order
```

`hasError == false` 只说明 parser 没有报告普通语法错误；类型约束、借用逃逸、任务边界、
provider capability 和 ABI contract 仍可能在 semantic/compiler/runtime 阶段失败。反过来，
diagnostic 中的 `location`、`code` 和 `fixes` 在 parser state 有效期间才可靠；要跨 generation
或跨线程使用，必须调用复制 API。对应的 C 生命周期示例见[诊断与错误恢复](diagnostics.md)
和[Parser/Compiler C API](../05-interop/c-api-parser.md)。

### 证据优先级

当规范文字、示例和实现看起来不一致时，按以下顺序核对：公共头文件的函数/结构体签名，
production parser 的 token 分支，`syntax_reference_v1` fixture 的正负样例，最后才是旧文档或
路线图。页面中的 EBNF 是可读摘要；它不会覆盖 semantic phase 的所有约束，也不承诺某个
provider 在每个构建配置中都存在。

词法 token 名称、AST 节点和 C 结构体布局以公共头文件为准；页面中的 EBNF 是方便人读的摘要，不是另一份会自动生成代码的 grammar。遇到冲突时，编译器源码和对应的 fixture/golden 文件优先。

## 证据和状态

`tests/fixtures/projects/syntax_reference_v1` 是本手册的最小可执行语言样例。它同时覆盖 module/import、对象模型、ownership、callable、async、iterator、compile-time、native FFI、pooling 和错误样例。正向文件应能通过 fixture harness；`negative/` 下的文件刻意验证错误 code 和建议文本。页面若谈到尚未在该 fixture 中执行的能力，会明确标为“语法已解析”或“后端覆盖受限”，不会暗示所有 provider 都已安装。

## 页面地图

| 页面 | 用途 |
| --- | --- |
| [词法与字面量](lexical-structure.md) | 字符、注释、标识符、关键字、数字/字符串/模板和分号规则。 |
| [声明与类型构造](declarations.md) | module、绑定、函数、泛型、struct/class/interface/enum/union、property、attribute。 |
| [声明与语句完整参考](declaration-statement-reference.md) | 按 parser production 展开所有声明、控制语句、AST 节点、错误边界和 C 入口。 |
| [对象类型、成员与构造参考](object-model-member-reference.md) | class/struct/ref struct/interface/enum/union、字段、方法、property、constructor、AST 与 native object 边界。 |
| [类型表达式与参数契约参考](type-expression-reference.md) | TypeRef、数组/tuple、generic、ref/readonly/scoped、参数 direction、canonical TypeId 与 C 语义接口。 |
| [表达式与运算符](expressions.md) | 主表达式、postfix 链、调用、可选访问、构造、ownership intrinsic 和优先级。 |
| [表达式、构造与调用形状参考](expression-construction-reference.md) | primary/postfix/call、`init`/`new`/`own`、cast/type query、ownership intrinsic、Place 与宿主 AST 边界。 |
| [可调用对象、调用与异步迭代](callable-async-iterator-reference.md) | 命名函数、closure、命名/展开/ref/out 实参、Task/await、yield/Iterator、Task runtime C 入口。 |
| [控制流与资源清理](control-flow.md) | if/while/for/foreach/switch、异常、return/break/continue/yield、using。 |
| [控制流、模式与清理参考](control-flow-pattern-cleanup-reference.md) | block/if/loop/switch pattern、try/catch/finally、using guard、abrupt control、CFG cleanup 与异常 C 边界。 |
| [模块、并发与 FFI](modules-concurrency.md) | import 解析、产物后缀、async/await、Iterator、native extern 和 provider。 |
| [模块导入与 Native FFI](module-import-native-ffi-reference.md) | import/module key、native extern grammar、FFI decorator、ABI contract、native registry 与 C callback 边界。 |
| [作用域、名称绑定与可见性](scope-binding-reference.md) | value/type/module/generic namespace、overload、capture、import alias、scope cleanup 及 semantic C context。 |
| [诊断与错误恢复](diagnostics.md) | parser diagnostic 结构、常见错误、恢复边界和 C callback。 |
| [语法实用配方](cookbook.md) | 可复制的完整片段、预期结果、错误用法和 fixture 对照。 |
| [语义与执行模型](semantics-implementation.md) | TypeId、Place/Value、CFG、SemIR、所有权/GC、挂起帧和后端边界。 |
| [语法总表](grammar-reference.md) | 可复制的 EBNF、优先级、字面量、恢复边界和正反例。 |
| [运算符、元方法与类型转换](operators-conversions.md) | 运算符优先级、短路、meta 槽位、比较/哈希、数值转换与 C 调用映射。 |
| [类型、布局与所有权](types-ownership.md) | Value/Place/Loan、GC scan、resource、nullable 和跨 await 规则。 |
| [泛型、接口与协议](generics-protocols.md) | generic identity、约束、interface、Iterable、Task、Send/Sync。 |
| [泛型参数、约束与实例化参考](generic-parameter-instantiation-reference.md) | 当前 `<...>`/`where` grammar、方差/const/owner 约束、AST、推断、canonical TypeId、C 实例表与 AOT 边界。 |
| [Attribute 与编译期](attributes-comptime.md) | attribute role、comptime 阶段、effect、Patch、缓存和预算。 |
| [Attribute 与 Comptime 契约](attribute-comptime-contract-reference.md) | decorator schema/target/retention、conditional pruning、transform transaction、预算/确定性与 C tool API。 |
| [异常、诊断与失败传播](error-handling.md) | try/catch/finally、pending control、native callback 和 testing failure。 |
| [异常展开、finally 与清理运行时参考](exception-cleanup-runtime-reference.md) | catch metadata、handler phase、pending control、GC root、native recovery 与 AOT helper。 |
| [模式、所有权与资源配方](patterns-ownership-recipes.md) | 解构模式、var/let/const、ownership intrinsic、using、async 和线程边界的可复制用例。 |

旧的概览页 [语言语法参考](../03-language-syntax.md) 保留为短索引；本目录提供逐条规则和实现说明。类型布局、函数调用约定、控制流后端细节以及 C/Rust 宿主生命周期分别见 [类型与内存](../04-types-and-memory.md)、[函数与调用](../05-functions-and-calls.md)、[控制流与错误](../06-control-flow-and-errors.md) 和 [互操作目录](../05-interop/index.md)。

## 如何阅读语法块

本手册使用以下约定：

- `code` 表示必须按原样出现的 token 或 API 名称。
- `identifier`、`expr`、`TypeRef` 是可替换的语法变量。
- 方括号表示可选部分；花括号表示重复，不表示 ZR 源码中的 `{}`。
- 代码块标记为 `zr` 时可直接交给本仓库的高亮 lexer；`ebnf`、`c`、`text` 用于 grammar、宿主代码和诊断输出。
- “解析成功”只说明 parser 可以建立 AST；类型检查、provider 注册、运行时能力仍可能在后续阶段失败。

例如，函数声明的摘要写成：

```ebnf
function-declaration = [decorator], [access], [async], "fn", identifier,
                       [generic-parameters], "(", parameters, ")",
                       [":", TypeRef], [where-clauses], block ;
```

具体的参数 passing mode、默认值和 function type 见[声明与类型构造](declarations.md)；不要把函数声明中的 `:` 与 function type 中的 `->` 互换。

## 与 C API 的连接点

语言页面只解释源代码；宿主若要解析、编译或执行这些片段，应从 [Parser/Compiler C API](../05-interop/c-api-parser.md) 开始，再按需要阅读 [Library C API](../05-interop/c-api-library.md)、[Core C API](../05-interop/c-api-core.md) 和 [嵌入式生命周期](../05-interop/embedding-lifecycle.md)。最小宿主顺序是：创建 global/state -> 初始化 parser -> 处理 AST/diagnostic -> 编译或加载 artifact -> 执行入口 -> 释放同一 owner 下的资源。不要从文档示例推断结构体 padding、GC 对象地址稳定性或跨线程安全性；这些契约只由公共头文件和对应测试决定。
