---
related_code:
  - zr_vm_cli/CMakeLists.txt
  - zr_vm_cli/src/zr_vm_cli.c
  - zr_vm_cli/src/zr_vm_cli/app/app.c
  - zr_vm_cli/src/zr_vm_cli/command/command.h
  - zr_vm_cli/src/zr_vm_cli/command/command.c
  - zr_vm_cli/src/zr_vm_cli/project/project.h
  - zr_vm_cli/src/zr_vm_cli/project/project.c
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler.h
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler.c
  - zr_vm_cli/src/zr_vm_cli/runtime/runtime.h
  - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c
  - zr_vm_cli/src/zr_vm_cli/repl/repl.h
  - zr_vm_cli/src/zr_vm_cli/repl/repl.c
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/include/zr_vm_core/module.h
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_core/src/zr_vm_core/module/module_loader.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_bindings.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_intermediate.c
  - zr_vm_language_server_extension/package.json
  - zr_vm_language_server_extension/syntaxes/zr.tmLanguage.json
  - zr_vm_language_server_extension/src/extension.ts
  - zr_vm_language_server_extension/src/debug/dapSession.ts
implementation_files:
  - zr_vm_cli/src/zr_vm_cli/app/app.c
  - zr_vm_cli/src/zr_vm_cli/command/command.c
  - zr_vm_cli/src/zr_vm_cli/project/project.c
  - zr_vm_cli/src/zr_vm_cli/compiler/compiler.c
  - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c
  - zr_vm_cli/src/zr_vm_cli/repl/repl.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_core/src/zr_vm_core/module/module_loader.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_bindings.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_intermediate.c
  - zr_vm_language_server_extension/package.json
  - zr_vm_language_server_extension/syntaxes/zr.tmLanguage.json
  - zr_vm_language_server_extension/src/extension.ts
  - zr_vm_language_server_extension/src/debug/dapSession.ts
plan_sources:
  - user: 2026-03-31 实现 ZR VM CLI 命令系统与 compile/run/REPL 计划
  - user: 2026-04-06 扩展 zr_vm_cli 入口参数、透传参数与 process.arguments 契约
  - user: 2026-04-06 扩成 CLI 覆盖矩阵，列出入口模式、合法组合、非法组合和 process.arguments 契约
tests:
  - tests/cli/test_cli_args.c
  - tests/cli/test_cli_repl_e2e.c
  - tests/cli/test_cli_debug_e2e.c
  - zr_vm_language_server_extension/test/syntaxGrammar.test.js
  - zr_vm_language_server_extension/test/dapSessionBreakpoints.test.js
  - zr_vm_language_server_extension/test/extensionContributions.test.js
  - tests/cmake/run_cli_suite.cmake
  - tests/CMakeLists.txt
doc_type: category-index
---

# CLI And Tooling

本目录记录 `zr_vm_cli` 的命令系统、项目编译工作流、binary-first 运行路径和 REPL 约束。

## 当前主题

- `zr-vm-cli-command-system.md`
  - 主模式、修饰符、别名和非法组合规则
  - `.zrp` 直跑、`--compile --run`、`-e/-c`、`--project -m` 的入口语义
  - `--` 透传与 `zr.system.process.arguments` 注入契约
  - REPL / post-run `-i` / 增量 manifest / 运行层边界
- `zr-vm-cli-coverage-matrix.md`
  - 入口模式、合法组合、非法组合和 `zr.system.process.arguments` 的查表矩阵
  - parser / runtime 拒绝边界与稳定错误片段
  - 当前单测、集成用例和专用 fixture 的覆盖映射
- `syntax-migration-command.md`
  - `migrate syntax` 的 check/write、JSON/text report 和固定 language direction
  - machine-only write、hash/parser/compiler/atomic replacement guard 与目录 exclusion
- `zr-vm-test-command.md`
  - typed TestManifest discovery、stable case id、filter/list/jobs/timeout/isolation contract
  - structured result、exit code、test project/module boundary
- `zr-debugger-v1-launch-workflow.md`
  - `launch-under-debug` 作为 v1 主路线的模块分层
  - `zr_vm_debug` / `zr_vm_network` / CLI runtime 的职责边界
  - `zrdbg/1` 请求与事件最小集
  - source / binary 断点解析与 step 语义
- `debug-variable-handle-generations.md`
  - paused-state `variablesReference` 的单调分配、generation 失效与 handle-only 数值区间
  - resume 后旧 children handle 的 `-32002` fail-closed 协议边界
- `zrp-editor-schema-and-lsp-refresh.md`
  - `.zrp` 作为 JSON 文档的 VS Code 识别路径
  - schema 字段覆盖、必填项与基础校验
  - `.zrp` 文档更新与 language server project refresh 的连接方式
- `vscode-extension-language-support.md`
  - VSCode extension 的 TextMate grammar、ZR debug commands、inline DAP adapter 和 VSIX/native asset 打包边界
  - union/default variant/variant member 语法高亮覆盖
  - native CLI、debug DLL、WASM server 和 schema 在 VSIX 中的验证方式
- `lsp-advanced-editor-features.md`
  - LSP code action、formatting、folding、selection、document link、CodeLens 和 pull diagnostics
  - stdio capability 广告、request wiring 与 JSON wire shape
  - 新增 C 单测和 stdio smoke 覆盖
- `lsp-diagnostic-safe-fixes.md`
  - parser structured fix的primary/edit range与applicability ownership
  - LSP machine-applicable projection、placeholder负边界与diagnostic数组释放
  - code-action snapshot resolve、stale拒绝和apply-edit-rebind验证
- `lsp-workspace-edit-snapshot-provenance.md`
  - opened overlay 与 disk cache 的显式 provenance，含合法 client version 0
  - workspace edit 的 URI/version/generation/open-state/length/hash 捕获与提交前复验
  - 普通 rename 与 source-file rename 的 captured-version 序列化和整批失败边界
- `lsp-stdio-validation.md`
  - stdio server child process 的 OS peak working-set budget 与跨平台采集
  - request/cancel/change/close 的 reader-thread 线性化和精确 snapshot 验证
  - server-owned reader stop/join、ordered teardown 与启动故障注入
- [lsp-capability-resolve-contract.md](lsp-capability-resolve-contract.md)
  - native/WASM resolve runtime masks 与初始响应完整性
  - identity-only resolve 撤销、MethodNotFound 与 code-action snapshot 复验
- [lsp-capability-registry-metadata.md](lsp-capability-registry-metadata.md)
  - core/native adapter 实现归属、runtime 字段与静态元数据的验证边界
- [lsp-navigation-capability-boundary.md](lsp-navigation-capability-boundary.md)
  - declaration/typeDefinition alias 撤销与四类查询的语义边界
  - canonical implementation 准确目标集合、范围与未完成 provider 门槛
- [lsp-workspace-file-operation-contract.md](lsp-workspace-file-operation-contract.md)
  - file operation 注册、版本化重命名编辑和过期磁盘快照拒绝
- `lsp-pull-push-diagnostics.md`
  - native/WASM 共享的 structured diagnostic resultId 与 dependency identity
  - workspace indexed-source coverage、invalid params 和 push/pull coexistence
  - browser worker 只消费 WASM bridge，不维护 TypeScript diagnostics hash
- `lsp-uri-native-path-boundary.md`
  - LSP `file:` URI 和 native path 的唯一双向转换、URI equivalence 与 platform normalization
  - project/navigation direct consumer 与 stdio native-I/O fail-closed boundary
- `zr-vm-rust-binding.md`
  - `zr_vm_rust_binding` 稳定 C ABI、Rust sys/safe crate 与 opaque handle 设计
  - project scaffold/open/compile/run 与 `callModuleExport` 的 host lifecycle
  - owned/live value mirror、array/object 访问、Cargo/CMake 校验集成
  - 主仓不再暴露 AOT；历史 AOT 资产已分离到 `zr_vm_aot/`

## 阅读顺序

1. 先看 `zr-vm-cli-command-system.md`，了解当前 CLI 主模式、入口参数契约和内部模块边界。
2. 需要快速确认某个入口模式、flag 组合、报错文案或 `process.arguments` 行为时，再看 `zr-vm-cli-coverage-matrix.md`。
3. 需要修改 `zr_vm test`、TestManifest discovery、隔离 worker 或退出码时，看 `zr-vm-test-command.md`。
4. 需要修改调试 launch、断点解析、`zrdbg/1` 协议或 loopback transport 时，再看 `zr-debugger-v1-launch-workflow.md`。
5. 需要修改 paused-state variables、children handle 或 resume 后失效边界时，再看 `debug-variable-handle-generations.md`。
6. 需要修改 `.zrp` 编辑体验或 project config 刷新路径时，再看 `zrp-editor-schema-and-lsp-refresh.md`。
7. 需要修改 VSCode extension 的 grammar、debug commands、native asset sync 或 VSIX 打包时，再看 `vscode-extension-language-support.md`。
8. 需要修改 language server 的现代编辑器能力、stdio request wiring 或 provider capability 时，再看 `lsp-advanced-editor-features.md`。
9. 需要修改diagnostic fix的parser ownership、applicability或LSP projection时，再看`lsp-diagnostic-safe-fixes.md`。
10. 需要修改 rename/code action/fix 的 workspace edit 快照、document version 或 disk/open provenance 时，再看 `lsp-workspace-edit-snapshot-provenance.md`。
11. 需要修改 stdio smoke 的性能、process memory 或 request lifecycle race 时，再看 `lsp-stdio-validation.md`。
12. 需要修改 pull/push diagnostics、workspace report coverage、resultId 或 browser diagnostics bridge 时，再看 `lsp-pull-push-diagnostics.md`。
13. 需要修改 document URI、native filesystem path、project path discovery 或 virtual document I/O boundary 时，再看 `lsp-uri-native-path-boundary.md`。
14. 需要修改 Rust 绑定 ABI、Rust workspace、host runtime lifecycle 或 cargo/CMake 集成时，再看 `zr-vm-rust-binding.md`。
15. 需要修改实现时，再沿 frontmatter 里的 `related_code` 和 `tests` 进入具体文件。
