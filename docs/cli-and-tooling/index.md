---
related_code:
  - zr_vm_cli/CMakeLists.txt
  - zr_vm_cli/src/zr_vm_cli.c
  - zr_vm_cli/src/zr_vm_cli/app.c
  - zr_vm_cli/src/zr_vm_cli/command.h
  - zr_vm_cli/src/zr_vm_cli/command.c
  - zr_vm_cli/src/zr_vm_cli/project.h
  - zr_vm_cli/src/zr_vm_cli/project.c
  - zr_vm_cli/src/zr_vm_cli/compiler.h
  - zr_vm_cli/src/zr_vm_cli/compiler.c
  - zr_vm_cli/src/zr_vm_cli/runtime.h
  - zr_vm_cli/src/zr_vm_cli/runtime.c
  - zr_vm_cli/src/zr_vm_cli/repl.h
  - zr_vm_cli/src/zr_vm_cli/repl.c
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/include/zr_vm_core/module.h
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_core/src/zr_vm_core/module_loader.c
  - zr_vm_parser/src/zr_vm_parser/compiler_bindings.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
  - zr_vm_parser/src/zr_vm_parser/writer_intermediate.c
implementation_files:
  - zr_vm_cli/src/zr_vm_cli/app.c
  - zr_vm_cli/src/zr_vm_cli/command.c
  - zr_vm_cli/src/zr_vm_cli/project.c
  - zr_vm_cli/src/zr_vm_cli/compiler.c
  - zr_vm_cli/src/zr_vm_cli/runtime.c
  - zr_vm_cli/src/zr_vm_cli/repl.c
  - zr_vm_core/src/zr_vm_core/io_runtime.c
  - zr_vm_core/src/zr_vm_core/module_loader.c
  - zr_vm_parser/src/zr_vm_parser/compiler_bindings.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
  - zr_vm_parser/src/zr_vm_parser/writer_intermediate.c
plan_sources:
  - user: 2026-03-31 实现 ZR VM CLI 命令系统与 compile/run/REPL 计划
tests:
  - tests/cli/test_cli_args.c
  - tests/cmake/run_cli_suite.cmake
  - tests/CMakeLists.txt
doc_type: category-index
---

# CLI And Tooling

本目录记录 `zr_vm_cli` 的命令系统、项目编译工作流、binary-first 运行路径和 REPL 约束。

## 当前主题

- `zr-vm-cli-command-system.md`
  - flag 模式与位置参数兼容规则
  - 项目上下文、输出目录和增量 manifest
  - compile / compile+run / REPL handler 的职责分离
  - `.zro` 里内部 native helper 的稳定序列化与运行时恢复
- `zrp-editor-schema-and-lsp-refresh.md`
  - `.zrp` 作为 JSON 文档的 VS Code 识别路径
  - schema 字段覆盖、必填项与基础校验
  - `.zrp` 文档更新与 language server project refresh 的连接方式

## 阅读顺序

1. 先看 `zr-vm-cli-command-system.md`，了解 CLI 第一版对用户暴露的模式和内部模块边界。
2. 需要修改 `.zrp` 编辑体验或 project config 刷新路径时，再看 `zrp-editor-schema-and-lsp-refresh.md`。
3. 需要修改实现时，再沿 frontmatter 里的 `related_code` 和 `tests` 进入具体文件。
