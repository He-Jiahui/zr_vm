---
doc_type: plan-detail
related_code:
  - zr_vm_parser/include/zr_vm_parser/type_inference.h
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_semantic_facts.c
  - zr_vm_parser/include/zr_vm_parser/semantic_facts.h
  - zr_vm_parser/include/zr_vm_parser/diagnostic_builder.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_diagnostics.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_position.c
  - zr_vm_language_server/src/zr_vm_language_server/incremental_parser.c
  - zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c
  - zr_vm_cli/src/zr_vm_cli/repl/repl.c
---

# 00 · 现状盘点

本篇给后续设计提供事实基线：语义推断、诊断、LSP、debug、REPL 五块各自落在哪些文件、关键 struct/函数、完成度，以及与目标形态的差距。引用以 `file:line` 为准（行号为调研时快照，会随提交漂移，定位以函数名为准）。

参考资料：`lua/` 下放置了若干成熟语言实现作为设计借鉴源——CPython（`lua/cpython`）、C#/Roslyn 与 .NET runtime（`lua/csharplang`、`lua/runtime`）、Rust 编译器与 rust-analyzer（`lua/rust`）、JDK javac（`lua/jdk`）、Mono 调试代理（`lua/mono`）、QuickJS（`lua/QuickJS-master`）。后续各篇会标注「借鉴点」。

---

## 1. 类型推断引擎 — 表达式级已可用，流分析缺位

入口与分发集中在 `type_inference.c`，子模块在 `type_inference/` 目录。

- 主入口 `ZrParser_ExpressionType_Infer`（`type_inference.c:2301` 附近）：按 AST 节点种类分发到字面量 / 标识符 / 二元 / 一元 / 调用 / lambda 等。
- 字面量推断 `ZrParser_LiteralType_Infer`（`type_inference.c:904`）：bool / int64 / double / string / char / null；**未加后缀整数统一按 int64**，浮点默认 double。范围约束经 `type_inference_apply_literal_numeric_range()`。
- 标识符推断 `ZrParser_IdentifierType_Infer`（`type_inference.c:1000`）：查 `typeEnv` / `compileTimeTypeEnv` / 类型原型；推断时经 `record_identifier_reference_fact`（`type_inference.c:973`）登记引用事实。
- 二元 / 一元 `ZrParser_BinaryExpressionType_Infer`（`type_inference.c:1104`）、`ZrParser_UnaryExpressionType_Infer`（`type_inference.c:1058`）：比较/逻辑 -> bool，算术 -> 类型提升（`ZrParser_InferredType_GetCommonType`）。
- 函数调用 `ZrParser_FunctionCallType_Infer`（`type_inference.c:1216`）：**实现简陋，默认回退 OBJECT**，真实签名解析在 primary expression 与泛型调用模块里。
- 泛型调用 `type_inference/type_inference_generic_calls.c`（~1425 行）：`resolve_generic_function_call_signature_detailed` / `validate_generic_call_bindings_constraints`。
- 导入元数据 `type_inference_import_metadata.c`、原生绑定 `type_inference_native.c`：把跨模块 / FFI 签名转成本地推断类型。

**差距**：
- 调用返回类型推断不统一（核心入口回退 OBJECT，真实逻辑分散）。
- 没有**控制流图（CFG）**，不可达分析只在表达式层（短路、常量分支），语句级（穷尽 if/else、循环后续）缺失。
- 数值推断只有字面量范围，没有跨语句区间传播 / 溢出推断。
- 所有权推断只覆盖泛型约束层，没有 move-后-use、借用越界、生命周期 region 的流式追踪（`TZrLifetimeRegionId` 已定义但未充分使用）。

> 借鉴点：CFG + predecessor 计数删不可达块见 `lua/cpython/Python/flowgraph.c`（`remove_unreachable`）；数据流半格 + 工作队列定点迭代见 `lua/rust/compiler/rustc_mir_dataflow/src/framework/mod.rs`；借用三域见 `lua/rust/compiler/rustc_borrowck/src/dataflow.rs`。

---

## 2. 语义事实层 — 结构齐全，生产端薄

`semantic_facts.h` 已定义六类事实，记录端在 `type_inference/type_inference_semantic_facts.c`：

| Fact | 结构 | 现状 |
| --- | --- | --- |
| Expression | `SZrSemanticExpressionFact` | 类型/常值/调用目标/成员访问，已用 |
| Reference | `SZrSemanticReferenceFact` | DECLARATION/READ/WRITE/CALL/MEMBER_ACCESS/MEMBER_WRITE，已用 |
| Numeric | `SZrSemanticNumericFact` | 范围约束/溢出，仅字面量层 |
| Reachability | `SZrSemanticReachabilityFact` | AFTER_RETURN/THROW/BREAK/CONTINUE、SHORT_CIRCUIT、CONSTANT_BRANCH、AFTER_EXHAUSTIVE_BRANCH，**仅表达式级** |
| Logical | `SZrSemanticLogicalFact` | 恒真/恒假/短路 |
| Ownership | `SZrSemanticOwnershipFact` | DECLARATION/BORROW/MOVE/COPY/RELEASE/ERROR，仅泛型约束触发 |

ID 系统在 `semantic.h:14` 附近：`TZrTypeId / TZrSymbolId / TZrOverloadSetId / TZrLifetimeRegionId`（`TZrUInt32`，0 = 无效）。

**差距**：事实**模型**已完整，但**生产端**（谁在编译流程里写这些事实）覆盖窄；事实之间没有形成可供 LSP / debug 复用的统一查询面（查询接口零散）。

---

## 3. 诊断系统 — 结构化诊断已成熟，缺「关联位置链」与「机器可应用修复」

核心结构 `SZrStructuredDiagnostic`（`diagnostic_builder.h:16-23`）：`severity` + `location`(行/列/offset) + `code` + `message` + `cause` + `suggestion`。

- `diagnostic_builder.h` 已有 **40+ 个专用 builder**（`BuildMissingRightOperand` / `BuildPatternUnknownField` / `BuildOwnershipMismatch` / `BuildWeakUpgrade` / `BuildBorrowEscape` …），多数已带 cause + suggestion，**并非只说「预期 token」**。
- 发送入口 `ZrParser_Compiler_StructuredError`（`compiler_diagnostics.c:59`），输出 `line:column`。
- 类型错误仍偏模板化：`compiler_diagnostics.c:249` 附近对 `"Type mismatch"` / `"Incompatible types"` 串匹配后套用通用建议。

**差距**（错误表达能力的真正缺口，而非「有没有 suggestion」）：
1. **没有 relatedInformation**：诊断只有单个 `location`，无法表达「在此处声明 / 在彼处冲突」的多位置链。
2. **没有机器可应用 fix-it**：`suggestion` 是自由文本，不是结构化 `TextEdit`，IDE 无法一键修复。
3. **类型不匹配诊断粗糙**：缺「期望 T 实得 U + 为什么不兼容 + 最近可行转换」。
4. **缺诊断码注册表**：code 是裸字符串，没有 ID->标题/严重级/文档链接的规范表，难本地化（消息目前全英文）。

> 借鉴点：Roslyn `DiagnosticDescriptor` 见 `lua/runtime/.../Roslyn/DiagnosticDescriptorHelper.cs`；Rust `span_label` 多位置 + `Applicability` 可修复等级见 `lua/rust/compiler/rustc_borrowck/src/borrowck_errors.rs`；javac `DiagnosticFlag` / `LintCategory` 见 `lua/jdk/.../javac/util/JCDiagnostic.java`。

---

## 4. LSP 服务器 — 方法覆盖面很广，但「行列定位」有 bug、推断是全量

C 端在 `zr_vm_language_server/`，stdio 入口在 `stdio/`，VSCode 扩展在 `zr_vm_language_server_extension/`。

### 4.1 已实现能力（覆盖面已很全）

`stdio/stdio_request_dispatch.c` 注册了 hover、completion(+resolve)、signatureHelp、definition/declaration/typeDefinition/implementation/references、callHierarchy、typeHierarchy、documentSymbol、workspace/symbol、semanticTokens(full/delta/range)、codeAction(+resolve)、rename(+prepare)、formatting(全/范围/onType)、foldingRange、selectionRange、inlayHint(+resolve)、documentHighlight、codeLens、documentLink、inlineValue、diagnostic(文档/工作区)。能力声明在 `stdio/stdio_initialize.c` 与 `stdio/stdio_initialize_capabilities.c`（含 `textDocumentSync = INCREMENTAL`）。

### 4.2 行列定位 bug（用户点名，已确认）

`lsp_interface_position.c`：
- 行/列 1<->0 索引转换 `file_line_to_lsp_line` / `file_column_to_lsp_character`（`:55-69`）只做 +-1，**把 LSP `character` 当成「文件列」**。
- 偏移计算 `ZrLanguageServer_Lsp_CalculateOffsetFromLineColumn`（`:82-117`）**按字节逐个计列**（`currentColumn++` 每字节一次）：LSP 规范默认 `character` 是 **UTF-16 code unit** 计数，不是字节也不是码点。含多字节 UTF-8（λ 2 字节、中文 3 字节）或 BMP 外字符（emoji 4 字节 / UTF-16 两个 surrogate）时，**列与偏移全部错位**。对应测试 `tests/language_server/test_lsp_position_mapping.c:40-52`。
- `ZrLanguageServer_LspRange_ToFileRange`（`:195`）无 content 版直接 `offset = 0`，列映射同样错误。

### 4.3 推断是「整文件全量」，增量是 TODO

- 诊断两路：解析诊断 `incremental_parser.c:57`（`collect_parser_diagnostic`）、语义诊断 `semantic/semantic_analyzer*.c`（typecheck / union_patterns / reachability / references）。
- 聚合发布 `lsp_interface.c:1225`（`GetDiagnostics`）：解析诊断 -> fallback AST 早退 -> 语义诊断 -> 项目诊断。
- 增量解析 `incremental_parser.c:400` 附近基本 **TODO**：变更 > 文件 10% 全量重解析，否则「尝试增量」但未实现。语义分析始终全量。

### 4.4 健壮性

- 入口普遍有 NULL 检查，错误用 return code，无异常机制。
- 隐患：`CalculateOffsetFromLineColumn`（越界假设）、`lsp_interface_support_file_position_from_offset`（`content[index]` 无越界检查，并发改内容可能段错误）、`FileVersion_UpdateContent`（`incremental_parser.c:189` 自赋值 UAF 已部分防护但依赖调用方守约）。

### 4.5 扩展侧

`extension.ts` 通过 stdio 启动原生服务器（`native`/`auto`/`web`，web 未实现）；`nativeAssets.ts` 按平台 + 最新构建时间挑二进制。**扩展侧不做独立推断**，全部委托服务器。

**差距**：(a) 行列 UTF-16 必须修；(b) 局部 / 增量推断缺失导致大文件卡顿；(c) 健壮性隐患（越界 / 并发）需补防护，做到「不该推断时不崩」。

---

## 5. Debug — 条件断点 / 表达式求值已相当完整，缺 DAP 与 AOT 路径

`zr_vm_lib_debug/`（约 7500 行）：`debug.c`（代理）、`debug_protocol.c`（JSON-RPC `zrdbg/1`）、`debug_eval.c`（表达式求值）、`debug_snapshot.c`（栈帧快照）、`debug_semantic_facts.c`（语义增强）。

- 断点：行断点 + 函数断点（`debug.h:30`）。
- **条件断点已支持**：条件求值 `debug.c:592`；hit condition `%N` / `>=N` / `<=N` / `==N`（`debug.c:531`）；logpoint `"... {expr} ..."`（`debug.c:626`）。
- **表达式求值已支持**：`ZrDebug_Evaluate`（`debug.h:151`），引擎 `debug_eval.c:907`，覆盖逻辑/比较/算术/三元/成员/索引/typeof。结果含 `type_name / value_text / semantic_summary / reference_summary`（`debug.h:108`）。
- 数据检查：`ZrDebug_ReadScopes` / `ReadVariables`，作用域含 ARGUMENTS/LOCALS/CLOSURES/GLOBALS/PROTOTYPE/STATICS/EXCEPTION（`debug.h:20`）；支持 `variables_reference` 递归展开。
- 步进 stepInto/Over/Out、异常断点齐全。

**差距**：
- 非标准 DAP（自定义 `zrdbg/1`），与通用 DAP 客户端不互通。
- 仅 loopback，无远程；AOT（`aot_c`/`aot_llvm`）路径不支持调试。
- 条件断点表达式与主编译器、LSP、REPL 各有一套求值/类型逻辑，**未共享语义推断**，能力与诊断不一致。

> 借鉴点：调试事件/上下文分离见 `lua/mono/mono/mini/debugger-agent.h`；表达式编译为字节码沙箱求值见 `lua/QuickJS-master/repl.c`。

---

## 6. REPL — 能跑表达式，但执行状态不持久

`zr_vm_cli/src/zr_vm_cli/repl/repl.c`（~926 行）+ `repl_input_scan.c` + `repl_semantic_facts.c`：

- 表达式求值：自动判定表达式 vs 语句，表达式包成 `return expr;` 求值并回显（`repl.c:807`）。
- 多行输入：空行提交，缓冲自动扩容。
- `:type <expr>` 展示推断类型 + 数值/逻辑/所有权/引用/可达性语义事实（复用语义事实层）。
- 元命令 `:help` / `:reset` / `:quit` / `:type`。

**核心限制**：每次提交都 `ZrCli_Project_CreateBareGlobal()` 新建全局态 + 主线程态（`repl.c:736`），把会话源代码当前缀重编译（`repl.c:307-335` 决定哪些语句进 `session.source`）。后果：
- 表达式求值**不进**会话状态，无法引用上一条表达式的运行期对象。
- 类/函数定义只以源文本保留，运行期对象、变量值跨提交不保留。
- 每次提交重编译全部会话源 + 重载标准库，性能差。

---

## 7. 小结：可复用基建 vs 需新建

**可直接复用 / 扩展**：
- 语义事实模型 `semantic_facts.h`（六类事实结构齐全）——做成统一查询面，供编译器/LSP/debug/REPL 共享。
- 结构化诊断 `SZrStructuredDiagnostic` + 40+ builder——叠加 relatedInformation 与 fix-it。
- debug 的条件断点 / 表达式求值引擎——把求值统一到共享语义层。
- LSP 已注册的全套方法骨架——补实现质量而非补方法。

**需新建 / 重做**：
- CFG + 数据流框架（不可达 / definite-assignment / 所有权流 / 数值区间）。
- UTF-16 安全的位置编解码层（替换按字节计列）。
- 局部 / 增量推断 + 健壮性隔离（推断失败降级不崩）。
- 诊断码注册表 + 关联位置 + 机器可应用修复。
- REPL 持久执行上下文（跨提交保留全局态与运行期值）。
- 一套被编译器 / LSP / debug / REPL 共享的「语义查询 API」。
