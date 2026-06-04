---
related_code:
  - zr_vm_language_server/include/zr_vm_language_server/conf.h
  - zr_vm_language_server/include/zr_vm_language_server/lsp_interface.h
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_local_semantic_query.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_document_links.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_editor_features.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_folding_ranges.c
  - zr_vm_language_server/stdio/stdio_editor_features.c
  - zr_vm_language_server/stdio/stdio_inline_value.c
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_language_server/stdio/zr_vm_language_server_stdio_internal.h
  - zr_vm_language_server/CMakeLists.txt
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_local_semantic_query.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_document_links.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_editor_features.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_folding_ranges.c
  - zr_vm_language_server/stdio/stdio_editor_features.c
  - zr_vm_language_server/stdio/stdio_inline_value.c
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_language_server/include/zr_vm_language_server/lsp_interface.h
  - zr_vm_language_server/include/zr_vm_language_server/conf.h
plan_sources:
  - user: 2026-04-24 Zr LSP 现代能力对齐计划
tests:
  - tests/language_server/test_lsp_advanced_editor_features.c
  - tests/language_server/test_lsp_local_semantic_hover.c
  - tests/language_server/test_lsp_computed_member_hover.c
  - tests/language_server/stdio_smoke.js
  - tests/language_server/stdio_inline_value_semantic_smoke.js
  - tests/CMakeLists.txt
doc_type: module-detail
---

# LSP Advanced Editor Features

## 范围

这份文档记录 Zr language server 第一阶段现代 LSP 能力对齐：补齐编辑器高频交互能力，同时保持语义事实仍来自现有 `lsp_semantic_query`、project index、metadata provider 和 incremental parser。

本阶段实现的协议能力包括：

- `textDocument/codeAction`
- `textDocument/formatting`
- `textDocument/rangeFormatting`
- `textDocument/onTypeFormatting`
- `textDocument/foldingRange`
- `textDocument/selectionRange`
- `textDocument/documentLink`
- `textDocument/declaration`
- `textDocument/typeDefinition`
- `textDocument/implementation`
- `textDocument/codeLens`
- `textDocument/prepareCallHierarchy`
- `callHierarchy/incomingCalls`
- `callHierarchy/outgoingCalls`
- `textDocument/prepareTypeHierarchy`
- `typeHierarchy/supertypes`
- `typeHierarchy/subtypes`
- `textDocument/diagnostic`
- `workspace/diagnostic`
- `completionItem/resolve`
- `textDocument/inlineValue`

## 设计边界

`lsp_editor_features.c` / `lsp_code_actions.c` 是新的公共接口实现文件，避免继续扩大 `lsp_interface.c`。它们只负责编辑器形态的结果建模：

- formatting 生成 `SZrLspTextEdit`，当前使用基于 brace/block 的保守缩进。
- code action 使用 `SZrLspCodeAction`，第一阶段提供 `source.organizeImports` 和缺失分号 `quickfix`，返回最小 `TextEdit`。
- folding / selection range 从当前文档文本结构生成轻量结构范围，包含 block、连续 import/comment region，以及 word -> line -> block selection chain。
- document link 扫描 `%import("...")` 字面量、`.zrp` 的 `source` / `binary` / `entry` 路径，以及 native virtual declaration 里的 module link；import 优先复用 definition 查询结果作为跳转目标。
- declaration / typeDefinition / implementation 当前复用 definition 查询，保证和已有导航结果一致。
- code lens 当前为函数/类型提供引用计数，并为 `%test(...)` 提供可运行入口。
- call hierarchy 当前实现 prepare 和同文件直接 incoming/outgoing calls；type hierarchy 当前返回同文件直接继承/派生关系。
- pull diagnostics 复用现有 diagnostics 生成逻辑，不替代 `publishDiagnostics` 推送模型。
- completion resolve 通过 completion item `data` 复用原始 URI/position，按 label 回填 detail/documentation。

`stdio_editor_features.c` 是协议层 glue：

- 序列化 `TextEdit`、`WorkspaceEdit`、`CodeAction`、`FoldingRange`、`SelectionRange`、`DocumentLink`、`CodeLens` 和 pull diagnostic report。
- 处理新增 request，并在失败或空结果时返回符合 LSP 习惯的空数组/空 report。
- 保持 stdio request 分发文件只做 capability 广告和 method routing。

`stdio_inline_value.c` 是 stdio 层的 inline value consumer。它继续返回调试器可用的 `InlineValueVariableLookup`，同时对 local initializer、单行 `return` 表达式，以及缩进后位于行首的简单 expression statement 调用 `ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt`。当 parser/type inference 已证明 numeric/logical fact 时，stdio 会追加 `InlineValueText`，例如变量名范围上的 `range 20..20` / `logical true, short-circuits`，return 表达式范围上的 `range 3..3`，或 `1 + 2;` / `true || false;` / `seed + 3;` statement expression 范围上的 `range 3..3` / `logical true, short-circuits` / `range 5..5`。当 editor request 只从 operator-split expression statement 的 continuation line 开始，例如 `1 +\n 2;` 的第二行，stdio 会回溯 owner statement 并仍返回覆盖完整表达式的 fact-backed inline value。当 shared expression fact 已有 call/member payload 时，同一路径也能在 `pick(42);` / `seed.value;` statement expression 范围上显示 `call pick args=1` / `member value`；当 local semantic query 同时返回 reference fact 时，stdio 会追加 `reference ...`，例如 `seed[index];` 显示 `member index, reference member access`。这条路径复用语义事实层，不在 stdio request handler 内重新实现数值范围、变量范围传播、短路推断、reference 分类或 call/member payload 推断。

`lsp_local_semantic_query.c` 也是 hover/rich-hover 的局部事实 formatter。它在传统 symbol hover 之外追加 numeric/logical/reachability/reference/ownership facts，并且现在直接渲染 parser expression fact 的 call/member payload：`pick(42)` 显示 `Call: pick args=1`，`seed.value` 显示 `Member: value`。对 `seed[index]` 这类 computed member，`[` 位置会用 member-access reference fact 找回 member payload，所以 hover/rich-hover 可以同时显示 `Reference: member access`、`Symbol: index` 和 `Member: index`。`lsp_interface.c` 的 rich-hover section parser 把这些 label 映射成稳定 `reference`、`symbol`、`call`、`member` roles，方便 VSCode 端结构化展示而不需要重新解析源码。

## Capability 广告

`initialize` 现在会额外声明：

- `completionProvider.resolveProvider = true`
- `codeActionProvider.codeActionKinds = ["quickfix", "source.organizeImports"]`
- `documentFormattingProvider = true`
- `documentRangeFormattingProvider = true`
- `documentOnTypeFormattingProvider.firstTriggerCharacter = "}"`
- `foldingRangeProvider = true`
- `selectionRangeProvider = true`
- `documentLinkProvider.resolveProvider = false`
- `declarationProvider = true`
- `typeDefinitionProvider = true`
- `implementationProvider = true`
- `codeLensProvider.resolveProvider = false`
- `callHierarchyProvider = true`
- `typeHierarchyProvider = true`
- `diagnosticProvider.interFileDependencies = true`
- `diagnosticProvider.workspaceDiagnostics = true`

VS Code desktop/native stdio 模式会自动消费这些 standard providers；extension 侧不需要为 formatting、links、CodeLens 或 organize imports 增加专有命令。

## 当前限制

- formatting 仍是保守文本缩进器，不是 AST pretty-printer；它不会重排表达式、参数列表或注释。
- code action 第一阶段稳定输出 organize imports 和缺失分号 quickfix；后续可按 diagnostics code/data 增加缺失 import、未解析成员等修复。
- declaration / typeDefinition / implementation 目前复用 definition 查询；后续如果 class/interface/extern 语义拆分，需要在统一语义查询层细化，不要在 stdio 层分叉。
- workspace diagnostic 会为当前增量解析器已知文档返回 full report；document pull diagnostics 复用现有单文档诊断。
- inline value 的语义 fact-backed 文本目前覆盖 local initializer、单行 return 表达式，以及缩进后行首的简单 expression statement 的 numeric/logical/reference facts，包括 identifier-led 算术表达式如 `seed + 3;`。多行支持仍是保守的：已覆盖多行 return、return-next-line、multi-line initializer，以及 request range 从 continuation line 开始的简单 operator-split expression statement；call/member statement 只在共享 expression/reference payload fact 已存在时显示 `call ... args=N`、`member ...` 或 `reference ...`，不会由 stdio 自己解析源码来伪造展示。运行时 debugger value 展示和更复杂的控制流 fact 仍应通过共享 local semantic query 继续扩展。
- hover/rich-hover 的 call/member 展示只消费已经存在的 expression/reference payload facts，不执行调用、不解析成员链类型，也不为没有共享 payload 的表达式合成 UI 文本；后续更复杂的 call/member UI 仍应先扩展共享 fact 层。

## 验证证据

新增覆盖：

- `tests/language_server/test_lsp_advanced_editor_features.c`
  - formatting full-document edit
  - folding range 和 selection range
  - `%import(...)` document link
  - organize imports code action
  - reference count 和 `%test(...)` CodeLens command
  - call hierarchy / type hierarchy prepare item
- `tests/language_server/stdio_smoke.js`
  - initialize capability wire shape
  - formatting / folding / selection / documentLink / codeAction / CodeLens / hierarchy request arrays
  - declaration provider
  - pull diagnostics full report
  - inline value variable lookup 和 semantic fact-backed inline text
- `tests/language_server/stdio_inline_value_semantic_smoke.js`
  - identifier-led expression statement `seed + 3;`
  - fact-backed inline text `range 5..5` on the expression range
  - call/member expression statements `pick(42);` / `seed.value;`
  - fact-backed inline text `call pick args=1` / `member value` on the expression range
  - computed member expression statement `seed[index];`
  - fact-backed inline text `member index, reference member access` on the expression range
  - continuation-only request range for `1 +` / `2;`
  - fact-backed inline text `range 3..3` over the full multi-line expression range
- `tests/language_server/test_lsp_local_semantic_hover.c`
  - call/member expression payload hover text
  - rich-hover `call` / `member` roles
- `tests/language_server/test_lsp_computed_member_hover.c`
  - computed member-access hover at `[` includes `Reference: member access`, `Symbol: index`, and `Member: index`
  - rich-hover `reference` / `symbol` / `member` roles for the same bracket-position query

2026-04-24 验证命令：

```sh
cmake -S . -B build/codex-wsl-gcc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ -DBUILD_TESTS=ON
cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_lsp_advanced_editor_features_test zr_vm_language_server_stdio -j 8
ctest --test-dir build/codex-wsl-gcc-debug -R 'language_server|language_server_stdio_smoke' --output-on-failure
cmake -S . -B build/codex-wsl-clang-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ -DBUILD_TESTS=ON
cmake --build build/codex-wsl-clang-debug -j 8
ctest --test-dir build/codex-wsl-clang-debug -R 'language_server|language_server_stdio_smoke' --output-on-failure
```

2026-06-04 inline value semantic fact 聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-wsl-gcc-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_CXX_COMPILER=g++ && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_stdio -j 8 && ctest --test-dir build/codex-wsl-gcc-debug -R language_server_stdio_smoke --output-on-failure"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake -S . -B build/codex-wsl-clang-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=clang -DCMAKE_CXX_COMPILER=clang++ && cmake --build build/codex-wsl-clang-debug --target zr_vm_language_server_stdio -j 8 && ctest --test-dir build/codex-wsl-clang-debug -R language_server_stdio_smoke --output-on-failure"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake -S . -B build\codex-msvc-lsp-smoke -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTS=OFF -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF; cmake --build build\codex-msvc-lsp-smoke --config Debug --target zr_vm_language_server_shared zr_vm_language_server_stdio --parallel 8; cmake -S . -B build\codex-msvc-cli-debug -G "Visual Studio 17 2022" -A x64 -DBUILD_TESTS=OFF -DBUILD_LANGUAGE_SERVER_EXTENSION=OFF; cmake --build build\codex-msvc-cli-debug --config Debug --target zr_vm_cli_executable --parallel 8; .\build\codex-msvc-cli-debug\bin\Debug\zr_vm_cli.exe .\tests\fixtures\projects\hello_world\hello_world.zrp
```

结果：WSL gcc 和 WSL clang 的 `language_server_stdio_smoke` 均通过，覆盖 `textDocument/inlineValue` 对 `var x = 20;` 输出 `range 20..20`，对 `var flag = true || false;` 输出 `logical true, short-circuits`。MSVC smoke 构建 `zr_vm_language_server_shared`、`zr_vm_language_server_stdio` 和 CLI 通过，`hello_world.zrp` 输出 `hello world`。

2026-06-04 return expression inline value semantic fact 聚焦验证：

```powershell
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && node tests/language_server/stdio_smoke.js build/codex-wsl-clang-debug/bin/zr_vm_language_server_stdio build/codex-wsl-clang-debug/bin/zr_vm_cli"
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_language_server_stdio zr_vm_cli_executable -j 4 && node tests/language_server/stdio_smoke.js build/codex-wsl-clang-debug/bin/zr_vm_language_server_stdio build/codex-wsl-clang-debug/bin/zr_vm_cli"
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_stdio zr_vm_cli_executable -j 4 && node tests/language_server/stdio_smoke.js build/codex-wsl-gcc-debug/bin/zr_vm_language_server_stdio build/codex-wsl-gcc-debug/bin/zr_vm_cli"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-msvc-lsp-smoke --config Debug --target zr_vm_language_server_shared zr_vm_language_server_stdio --parallel 4; node tests\language_server\stdio_smoke.js build\codex-msvc-lsp-smoke\bin\Debug\zr_vm_language_server_stdio.exe build\codex-msvc-cli-debug\bin\Debug\zr_vm_cli.exe
```

RED：新增 `return 1 + 2;` 的 `textDocument/inlineValue` 断言后，旧 server 返回响应但没有 `range 3..3` inline text。GREEN：`stdio_inline_value.c` 对 `return` 表达式范围调用同一 local semantic query，并在表达式范围 `1 + 2` 上返回 `InlineValueText`。WSL clang、WSL gcc 和 MSVC stdio smoke 均通过；gcc/clang/MSVC 仍有既有 core/parser/LSP warning，本轮没有运行全仓库 ctest。

2026-06-04 expression-statement inline value semantic fact 聚焦验证：

```powershell
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_language_server_stdio zr_vm_cli_executable -j 4 > /tmp/zr_inline_expr_clang_build.out 2>&1; status=$?; tail -80 /tmp/zr_inline_expr_clang_build.out; exit $status"
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && node tests/language_server/stdio_smoke.js build/codex-wsl-clang-debug/bin/zr_vm_language_server_stdio build/codex-wsl-clang-debug/bin/zr_vm_cli"
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_stdio zr_vm_cli_executable -j 4 > /tmp/zr_inline_expr_gcc_build.out 2>&1; status=$?; tail -80 /tmp/zr_inline_expr_gcc_build.out; exit $status"
wsl -e bash -lc "cd /mnt/e/Git/zr_vm && node tests/language_server/stdio_smoke.js build/codex-wsl-gcc-debug/bin/zr_vm_language_server_stdio build/codex-wsl-gcc-debug/bin/zr_vm_cli"
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"; where.exe cl; cmake --build build\codex-msvc-lsp-smoke --config Debug --target zr_vm_language_server_shared zr_vm_language_server_stdio --parallel 4; node tests\language_server\stdio_smoke.js build\codex-msvc-lsp-smoke\bin\Debug\zr_vm_language_server_stdio.exe build\codex-msvc-cli-debug\bin\Debug\zr_vm_cli.exe
```

RED：新增 `1 + 2;` / `true || false;` 的 `textDocument/inlineValue` 断言后，旧 server 返回响应但没有 expression-statement 范围上的 `range 3..3` 或 `logical true, short-circuits` inline text。GREEN：`stdio_inline_value.c` 对缩进后行首的简单 literal/boolean expression statement 复用同一 local semantic query，在表达式范围上返回 fact-backed `InlineValueText`。WSL clang、WSL gcc 和 MSVC stdio smoke 均通过；gcc/MSVC 仍有既有 core/parser/CLI warning，本轮没有运行全仓库 ctest。

2026-06-04 call/member expression-statement inline value semantic payload 聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_stdio -j 8 && node tests/language_server/stdio_inline_value_semantic_smoke.js build/codex-wsl-gcc-debug/bin/zr_vm_language_server_stdio && ctest --test-dir build/codex-wsl-gcc-debug -R '^language_server_stdio(_inline_value_semantic)?_smoke$' --output-on-failure"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_language_server_stdio -j 8 && node tests/language_server/stdio_inline_value_semantic_smoke.js build/codex-wsl-clang-debug/bin/zr_vm_language_server_stdio && ctest --test-dir build/codex-wsl-clang-debug -R '^language_server_stdio(_inline_value_semantic)?_smoke$' --output-on-failure"
cmake --build build\codex-msvc-lsp-smoke --config Debug --target zr_vm_language_server_stdio --parallel 8; if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }; node tests\language_server\stdio_inline_value_semantic_smoke.js build\codex-msvc-lsp-smoke\bin\Debug\zr_vm_language_server_stdio.exe
node tests\language_server\stdio_smoke.js build\codex-msvc-lsp-smoke\bin\Debug\zr_vm_language_server_stdio.exe build\codex-msvc-cli-debug\bin\Debug\zr_vm_cli.exe
```

RED：新增 `pick(42);` / `seed.value;` 的 `textDocument/inlineValue` 断言后，旧 server 返回响应但没有 call/member payload inline text。GREEN：`stdio_inline_value.c` 格式化共享 `SZrSemanticExpressionFact` 的 call/member payload，分别输出 `call pick args=1` 和 `member value`。WSL gcc、WSL clang registered stdio smokes 均通过；Windows MSVC dedicated inline-value smoke 和 broader stdio smoke 通过。本轮仍不声明全仓库 ctest 绿色。

2026-06-04 continuation-range expression-statement inline value 聚焦验证：

```powershell
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-gcc-debug --target zr_vm_language_server_stdio -j 8 && node tests/language_server/stdio_inline_value_semantic_smoke.js build/codex-wsl-gcc-debug/bin/zr_vm_language_server_stdio && ctest --test-dir build/codex-wsl-gcc-debug -R '^language_server_stdio(_inline_value_semantic)?_smoke$' --output-on-failure"
wsl bash -lc "cd /mnt/e/Git/zr_vm && cmake --build build/codex-wsl-clang-debug --target zr_vm_language_server_stdio -j 8 && node tests/language_server/stdio_inline_value_semantic_smoke.js build/codex-wsl-clang-debug/bin/zr_vm_language_server_stdio && ctest --test-dir build/codex-wsl-clang-debug -R '^language_server_stdio(_inline_value_semantic)?_smoke$' --output-on-failure"
cmake --build build\codex-msvc-lsp-smoke --config Debug --target zr_vm_language_server_stdio --parallel 8
node tests\language_server\stdio_inline_value_semantic_smoke.js build\codex-msvc-lsp-smoke\bin\Debug\zr_vm_language_server_stdio.exe
node tests\language_server\stdio_smoke.js build\codex-msvc-lsp-smoke\bin\Debug\zr_vm_language_server_stdio.exe build\codex-msvc-cli-debug\bin\Debug\zr_vm_cli.exe
```

RED：新增只请求 `1 +\n 2;` 第二行的 inline-value 断言后，旧 server 返回空数组。GREEN：`stdio_inline_value.c` 在 request 从 continuation line 开始且 owner line 不在请求范围内时回溯到前一行 expression statement，返回完整表达式范围上的 `range 3..3`。WSL gcc、WSL clang registered stdio smokes 均通过；Windows MSVC dedicated inline-value smoke 和 broader stdio smoke 通过。本轮仍不声明全仓库 ctest 绿色。

2026-06-05 computed-member inline value reference fact 聚焦验证：

```powershell
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-gcc-debug --target zr_vm_language_server_stdio -j 6 && node tests/language_server/stdio_inline_value_semantic_smoke.js ./build/codex-semantic-wsl-gcc-debug/bin/zr_vm_language_server_stdio'
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build/codex-semantic-wsl-clang-debug --target zr_vm_language_server_stdio -j 6 && node tests/language_server/stdio_inline_value_semantic_smoke.js ./build/codex-semantic-wsl-clang-debug/bin/zr_vm_language_server_stdio'
. "C:\Users\HeJiahui\.codex\skills\using-vsdevcmd\scripts\Import-VsDevCmdEnvironment.ps1"
where.exe cl
cmake --build build\agent-msvc-tests --config Debug --target zr_vm_language_server_stdio --parallel 6
node tests\language_server\stdio_inline_value_semantic_smoke.js build\agent-msvc-tests\bin\Debug\zr_vm_language_server_stdio.exe
```

RED：新增 `seed[index];` 的 `textDocument/inlineValue` reference 断言后，旧 server 返回 `text:"member index"`，缺少 `reference member access`。GREEN：`stdio_inline_value.c` 继续通过 `ZrLanguageServer_LspLocalSemanticQuery_ExpressionAt` 取事实，并把 `SZrSemanticReferenceFact` 格式化为 compact `reference ...` inline text；computed member expression statement 现在返回 `member index, reference member access`。WSL gcc、WSL clang 和 Windows MSVC dedicated inline-value smoke 均通过；MSVC 仍有当前 dirty checkout 的既有 core/parser/library warning，本轮仍不声明全仓库绿色。
