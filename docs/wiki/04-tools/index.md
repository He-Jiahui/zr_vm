---
related_code:
  - zr_vm_cli/src/zr_vm_cli/command/command.h
  - zr_vm_cli/src/zr_vm_cli/app/app.c
  - zr_vm_language_server/include/zr_vm_language_server/lsp_interface.h
  - zr_vm_language_server/include/zr_vm_language_server/semantic_analyzer.h
  - zr_vm_language_server/stdio/stdio_server.h
  - zr_vm_language_server/wasm/wasm_exports.h
implementation_files:
  - zr_vm_cli/src/zr_vm_cli/command/command.c
  - zr_vm_cli/src/zr_vm_cli/app/app.c
  - zr_vm_cli/src/zr_vm_cli/repl/repl.c
  - zr_vm_cli/src/zr_vm_cli/testing/test_runner.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - zr_vm_language_server/stdio/stdio_server.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/plans/lsp/index.md
  - docs/cli-and-tooling/zr-vm-cli-command-system.md
tests:
  - tests/cli/test_cli_args.c
  - tests/cli/test_cli_repl_e2e.c
  - tests/cli/test_cli_zrp_metadata_dump.c
  - tests/language_server/test_lsp_interface.c
  - tests/cmake/zr_vm_lsp_stdio_handler_tests.cmake
doc_type: category-index
---

# 工具链

工具都建立在 parser 的 canonical semantic facts 之上。CLI 负责选择运行/编译模式，LSP
负责查询和编辑投影，test runner 消费 typed TestManifest，VS Code extension 只负责客户端
和 WASM/native 资产分发。

| 工具 | 页面 | 入口 |
| --- | --- | --- |
| CLI/REPL | [cli](cli.md) | `zr_vm_cli` |
| 测试命令 | [testing](testing.md) | `zr_vm_cli test`（帮助文本显示为 `zr test`） |
| Language Server | [language-server](language-server.md) | stdio JSON-RPC / WASM |
| VS Code/DAP | [editor-debugger](editor-debugger.md) | extension + debug agent |
| 迁移与元数据 | [migration](migration.md) | `migrate syntax`, `--dump-zrp-metadata` |
| 构建/验证 | [build-validation](build-validation.md) | CMake、CTest、sanitizer |

工具文档中的命令行返回码、JSON 字段和 diagnostic code 是稳定接口；文本排版可变，自动化
脚本应优先使用 JSON/manifest 输出而不是抓取人类可读行。
