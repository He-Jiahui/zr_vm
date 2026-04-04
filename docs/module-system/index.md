---
related_code:
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/module_loader.c
  - zr_vm_parser/src/zr_vm_parser/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/type_inference_import_metadata.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
  - zr_vm_parser/src/zr_vm_parser/writer_intermediate.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/module_loader.c
  - zr_vm_parser/src/zr_vm_parser/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/type_inference_import_metadata.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
  - zr_vm_parser/src/zr_vm_parser/writer_intermediate.c
plan_sources:
  - user: 2026-03-31 实现 “M6 强类型推断完整闭环计划”
  - .codex/plans/M6 强类型推断完整闭环计划.md
  - .codex/plans/zr_vm阶段化总计划.md
tests:
  - tests/parser/test_type_inference.c
  - tests/parser/test_compiler_features.c
  - tests/parser/test_instruction_execution.c
  - tests/scripts/test_artifact_golden.c
  - tests/cmake/run_projects_suite.cmake
doc_type: category-index
---

# Module System

本目录记录 `zr_vm` 模块导入、模块产物和模块级类型元数据的行为约束。

## 当前主题

- `typed-module-metadata.md`
  - `SZrFunction` / `SZrIoFunction` 的统一 typed metadata 载体
  - native / source / binary import 的编译期归一化装载
  - `.zri` / `.zro` 共用同一份 typed metadata 源
  - typed opcode 选择与 imported typed call 运行时约束

## 阅读顺序

1. 先看 `typed-module-metadata.md`，了解 M6 强类型推断闭环的元数据结构、导入路径和序列化边界。
2. 需要落代码时，再沿 frontmatter 的 `related_code` / `tests` 进入具体实现和验证入口。
