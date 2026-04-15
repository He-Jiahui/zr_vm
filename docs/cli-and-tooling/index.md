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
plan_sources:
  - user: 2026-03-31 实现 ZR VM CLI 命令系统与 compile/run/REPL 计划
  - user: 2026-04-06 扩展 zr_vm_cli 入口参数、透传参数与 process.arguments 契约
  - user: 2026-04-06 扩成 CLI 覆盖矩阵，列出入口模式、合法组合、非法组合和 process.arguments 契约
tests:
  - tests/cli/test_cli_args.c
  - tests/cli/test_cli_repl_e2e.c
  - tests/cli/test_cli_debug_e2e.c
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
- `zr-debugger-v1-launch-workflow.md`
  - `launch-under-debug` 作为 v1 主路线的模块分层
  - `zr_vm_debug` / `zr_vm_network` / CLI runtime 的职责边界
  - `zrdbg/1` 请求与事件最小集
  - source / binary 断点解析、step 语义和 AOT 边界
- `zrp-editor-schema-and-lsp-refresh.md`
  - `.zrp` 作为 JSON 文档的 VS Code 识别路径
  - schema 字段覆盖、必填项与基础校验
  - `.zrp` 文档更新与 language server project refresh 的连接方式

## 阅读顺序

1. 先看 `zr-vm-cli-command-system.md`，了解当前 CLI 主模式、入口参数契约和内部模块边界。
2. 需要快速确认某个入口模式、flag 组合、报错文案或 `process.arguments` 行为时，再看 `zr-vm-cli-coverage-matrix.md`。
3. 需要修改调试 launch、断点解析、`zrdbg/1` 协议或 loopback transport 时，再看 `zr-debugger-v1-launch-workflow.md`。
4. 需要修改 `.zrp` 编辑体验或 project config 刷新路径时，再看 `zrp-editor-schema-and-lsp-refresh.md`。
5. 需要修改实现时，再沿 frontmatter 里的 `related_code` 和 `tests` 进入具体文件。
