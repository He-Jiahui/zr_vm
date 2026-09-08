---
related_code:
  - zr_vm_language_server_extension/package.json
  - zr_vm_language_server_extension/src/extension.ts
  - zr_vm_language_server_extension/server
  - zr_vm_language_server/include/zr_vm_language_server/lsp_interface.h
  - zr_vm_language_server/src/zr_vm_language_server
implementation_files:
  - zr_vm_language_server_extension/src/extension.ts
  - zr_vm_language_server_extension/server
  - zr_vm_language_server/src/zr_vm_language_server
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/plans/debug/05-dap-agent-enhancements.md
tests:
  - zr_vm_language_server_extension/test/extensionContributions.test.js
  - zr_vm_language_server_extension/test/languageClientLifecycle.test.js
  - zr_vm_language_server_extension/test/syncWasmServer.test.js
  - tests/debug/test_debug_agent_protocol.c
  - tests/cli/test_cli_debug_e2e.c
doc_type: module-detail
---

# VS Code 与调试器

extension 只负责语言客户端生命周期、语法高亮、native/WASM server 选择和调试适配；语义
解析仍在 `zr_vm_language_server`。桌面优先加载与平台匹配的 native server，Web/受限环境
使用 WASM response module。extension 不复制 parser 规则，语法 scope 来自
`syntaxes/zr.tmLanguage.json`，诊断和补全来自 LSP。

## 调试协议

CLI 以 `--debug`、`--debug-address host:port`、`--debug-wait` 启动 debug agent。agent 通过
canonical frame/snapshot 读取 stack、locals、upvalues、heap summary 和 TestManifest；断点、
step、evaluate、data breakpoint 和 DAP transport 失败都返回结构化错误。变量 handle 带
generation，snapshot 变化后旧 handle 必须拒绝，防止读取已释放 slot。

## 构建

```text
cmake -S . -B build -DBUILD_LANGUAGE_SERVER=ON
cmake --build build --target zr_vm_language_server
cd zr_vm_language_server_extension
npm run compile
npm run package
```

WASM 目标需要 Emscripten；extension 打包目标会先构建 WASM、安装 npm 依赖，再编译 TypeScript。
