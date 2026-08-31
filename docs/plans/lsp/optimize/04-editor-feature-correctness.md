# LSP 编辑功能正确性实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use `test-driven-development`, `zr-language-feature-design`, `support-first-regression-testing`, `modularize-large-files`, `code-module-docs-maintenance`, and `verification-before-completion` while executing this plan.

**Goal:** 在 canonical semantic query 和 snapshot 基础上实现真实的 navigation、hierarchy、rename、selection、formatting、code action、document link 和 semantic token 行为，并删除文本启发式伪实现。

**Architecture:** 语义功能按 SymbolId/TypeId/relation graph 实现；语法功能按 parser token/trivia/AST 实现；manifest 功能复用 library 的结构化 project parser。任何 provider 都不能用同名文本扫描替代缺失的 compiler fact。

**Tech Stack:** LSP 3.17/可选 3.18、parser AST/token/trivia、semantic relations、project manifest v2、C/Node protocol tests。

---

## Task 1：区分 declaration、definition、typeDefinition、implementation

**Files:**
- Modify: `zr_vm_language_server/src/zr_vm_language_server/lsp_editor_features.c`
- Create: `zr_vm_language_server/src/zr_vm_language_server/navigation/lsp_declaration.c`
- Create: `zr_vm_language_server/src/zr_vm_language_server/navigation/lsp_type_definition.c`
- Create: `zr_vm_language_server/src/zr_vm_language_server/navigation/lsp_implementation.c`
- Create: `tests/language_server/test_lsp_semantic_navigation.c`

- [ ] RED：同一 symbol 的 declaration 与 executable definition 可以不同；typeDefinition 跳到值的 TypeId 声明；implementation 返回 override/interface implementors，不得等于 definition 的机械别名。
- [ ] 测试 local/function/class/property/constructor/import alias/extern/virtual metadata；测试无实现、多个实现、closed generic 和 multi-root。
- [ ] 使用计划 03 relation query；删除三个 API 对 `GetDefinition` 的 alias。
- [ ] 只有全部运行时适配器与协议测试通过后重新声明 capability。

## Task 2：重写 call/type hierarchy

**Files:**
- Replace: `zr_vm_language_server/src/zr_vm_language_server/lsp_hierarchy.c`
- Create: `zr_vm_language_server/src/zr_vm_language_server/hierarchy/lsp_call_hierarchy.c`
- Create: `zr_vm_language_server/src/zr_vm_language_server/hierarchy/lsp_type_hierarchy.c`
- Create: `tests/language_server/test_lsp_call_hierarchy.c`
- Create: `tests/language_server/test_lsp_type_hierarchy.c`

- [ ] prepare item 的 `data` 保存 snapshot-safe SymbolId/TypeId/ModuleIdentity，不保存名称作为身份。
- [ ] incoming/outgoing 使用 call edge index；fromRanges 精确对应 caller 中每个 call site。
- [ ] type hierarchy 使用 base/interface relations；支持跨文件、跨模块、binary/native virtual declaration、generic definition/instantiation。
- [ ] unresolved/dynamic call 不产生错误目标；overload 与 receiver 同名方法必须区分。
- [ ] 删除注释/字符串跳过器、`name + '('` 和冒号后类型名扫描。

## Task 3：修复 references、highlight、rename 和 workspace edit

**Files:**
- Modify: `zr_vm_language_server/src/zr_vm_language_server/reference_tracker.c`
- Modify: `zr_vm_language_server/src/zr_vm_language_server/lsp_editor_features.c`
- Modify: `zr_vm_language_server/stdio/stdio_rename.c`
- Modify: `zr_vm_language_server/stdio/stdio_editing_json.c`
- Create: `tests/language_server/test_lsp_rename_semantics.c`

- [ ] reference role 使用 compiler read/write/call/type/member facts；document highlight 映射为 Text/Read/Write。
- [ ] rename 只编辑同一 SymbolId/alias relation，拒绝关键字、external no-source symbol、ambiguous/unresolved symbol、重叠 edits 和 invalid new name。
- [ ] workspace edit 根据 client capability 选择 versioned `documentChanges` 或 `changes`；支持 failureHandling/resourceOperations/changeAnnotations 时才输出对应字段。
- [ ] 在发送前验证所有 document snapshots；任何一个变化则整体 `ContentModified`，不得发送部分 edit。
- [ ] Web 与 native 使用同一 edit plan 与 serializer contract，避免 Web 自行从 Location[] 重建无版本 edit。

## Task 4：用 AST/token/trivia 实现 selection、folding 与 linked editing

**Files:**
- Create: `zr_vm_language_server/src/zr_vm_language_server/syntax/lsp_selection_ranges.c`
- Create: `zr_vm_language_server/src/zr_vm_language_server/syntax/lsp_folding_ranges.c`
- Modify: `zr_vm_language_server/src/zr_vm_language_server/lsp_editor_features.c`
- Modify: `zr_vm_language_server/stdio/stdio_linked_editing.c`
- Create: `tests/language_server/test_lsp_syntax_ranges.c`

- [ ] selection parent chain 为 token → expression/type → statement → declaration → block/module，且每个 parent 严格包含 child。
- [ ] 字符串、注释、template/interpolation 中的花括号不得成为代码 block；syntax error 使用 parser recovery tree，不回退 raw brace scan。
- [ ] folding 使用 AST declaration/block + trivia comments/regions/import groups；遵守 client lineFoldingOnly 与 rangeLimit。
- [ ] linked editing 只连接 compiler/syntax 明确配对的名称或标签，不按相同文本匹配。
- [ ] 覆盖 UTF-16、CRLF、EOF、空文件、嵌套注释、缺失闭括号和 last-good AST。

## Task 5：建立 parser-aware formatter

**Files:**
- Create: `zr_vm_language_server/include/zr_vm_language_server/lsp_formatting.h`
- Create: `zr_vm_language_server/src/zr_vm_language_server/formatting/lsp_formatter.c`
- Create: `zr_vm_language_server/src/zr_vm_language_server/formatting/lsp_formatting_options.c`
- Modify: `zr_vm_language_server/stdio/stdio_editor_features.c`
- Create: `tests/language_server/test_lsp_formatter.c`
- Create: `tests/language_server/stdio_formatting_options_smoke.js`

- [ ] 定义 options：tabSize、insertSpaces、trimTrailingWhitespace、insertFinalNewline、trimFinalNewlines；拒绝非法 tabSize。
- [ ] formatter 基于 token/trivia 和 AST nesting，不改变字符串、注释、literal、operator token 或语义内容。
- [ ] range formatting 只格式化覆盖的完整语法节点/行并返回最小 edit；on-type 限定当前语句/block，不格式化整个文档。
- [ ] willSaveWaitUntil 复用同一 formatter；超时或 syntax recovery 不安全时返回空 edits。
- [ ] property tests：format(format(x)) == format(x)，parse(format(x)) 的 semantic facts/public contract 与 parse(x) 等价。
- [ ] 3.18 rangesFormatting 仅在 registry 标为 optional 且客户端支持时声明；否则保留 3.17 rangeFormatting。

## Task 6：重写 code actions 与 document links

**Files:**
- Modify: `zr_vm_language_server/src/zr_vm_language_server/lsp_editor_features.c`
- Modify: `zr_vm_language_server/src/zr_vm_language_server/lsp_document_links.c`
- Create: `zr_vm_language_server/src/zr_vm_language_server/actions/lsp_diagnostic_actions.c`
- Create: `zr_vm_language_server/src/zr_vm_language_server/actions/lsp_import_actions.c`
- Create: `tests/language_server/test_lsp_code_actions.c`
- Create: `tests/language_server/test_lsp_document_links.c`

- [ ] quickfix 只消费 structured machine fix；每个 fix 带 diagnostic code、snapshot identity、expected old text/hash 和 edit safety class。
- [ ] organize/remove/missing import 使用 ModuleIdentity/import graph，不扫描 token 名称猜包；处理 alias、destructuring、重复/ambiguous provider。
- [ ] `.zrp` links 使用 `ZrLibrary_ProjectManifestV2`/JSON parser；正确处理 escape、相对路径、缺失文件和 non-file workspace URI。
- [ ] source import 与 manifest link target 复用 canonical URI API；virtual/native/binary target 由 metadata projection 提供。
- [ ] resolveProvider 仅在 resolve 阶段实际补齐 target/edit 时声明，否则初始响应完整返回。

## Task 7：semantic tokens、inlay、inline 与颜色能力

**Files:**
- Modify: `zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_tokens.c`
- Modify: `zr_vm_language_server/stdio/stdio_semantic_tokens*.c`
- Modify: `zr_vm_language_server/stdio/stdio_inline_value*.c`
- Modify: `zr_vm_language_server/stdio/stdio_inline_completion.c`
- Modify: `zr_vm_language_server/stdio/stdio_document_color.c`
- Create: `tests/language_server/test_lsp_semantic_presentations.c`

- [ ] semantic token type/modifier 由 syntax kind + canonical symbol role 产生；legend 与数据用同一 registry，并按客户端支持 token types/modifiers 映射。
- [ ] delta cache 按 URI + resultId 保留有界历史；找不到 previousResultId 时返回合法 full tokens，而不是解析不受信 resultId 长度猜 deleteCount。
- [ ] inlay hint resolve 只有延迟加载 tooltip/textEdits/data 时启用；identity resolve 关闭。
- [ ] inline value 绑定 debug context/range 与 semantic facts；inline completion 绑定 grammar/recovery context，不用整文件关键词扫描。
- [ ] document color 只识别语言规范与 compiler facts 明确定义的 typed color literal/constructor；如果语言没有该契约，则移除 colorProvider。字符串中的十六进制文本不得自动成为颜色。

## Task 8：验收门禁

- [ ] 每个重新声明的 provider 具有非平凡语义测试，不允许只断言“返回数组”。
- [ ] navigation/hierarchy 在 overload、receiver、alias、generic、跨文件、binary/native fixtures 上通过。
- [ ] formatting options 组合与 syntax recovery 通过，幂等和 semantic equivalence 成立。
- [ ] source contract 扫描不再发现 hierarchy/navigation/rename 的同名文本扫描。
- [ ] 触及的大文件按职责拆分，禁止继续向 `lsp_interface_support.c`、`lsp_signature_help.c` 或 monolithic tests 追加独立责任。
