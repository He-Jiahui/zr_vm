---
related_code:
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/module/module_loader.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_metadata.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_intermediate.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/function.h
  - zr_vm_core/include/zr_vm_core/io.h
  - zr_vm_core/src/zr_vm_core/io.c
  - zr_vm_core/src/zr_vm_core/module/module_loader.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_metadata.c
  - zr_vm_parser/src/zr_vm_parser/writer.c
  - zr_vm_parser/src/zr_vm_parser/writer/writer_intermediate.c
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
  - `.zro` 作为正式 typed metadata 语义源，`.zri` 仅保留 debug / intermediate 职责
  - typed opcode 选择与 imported typed call 运行时约束

## Import Path Contract

项目模块导入现在统一走 canonical module key 解析：

- bare absolute `%import("foo")` / `%import("zr.system")` 继续保留现有语义。
- 显式相对导入只接受 leading-dot：
  - `.x.y` => 当前模块目录下的 `x/y`
  - `..x.y` => 父目录下的 `x/y`
  - 更多前导点继续逐级上溯
- `.zrp.pathAliases` 可以把 `@alias` 映射到 sourceRoot 下的 module prefix；`@alias.foo.bar` 会继续拼到该 prefix 后面。
- canonical key 始终是 slash-separated project module key，不是绝对文件系统路径。
- 解析失败后仍然拒绝隐式相对猜测；`./foo`、`../foo`、bare `.` / `..`、`@alias/foo`、unknown alias 和越过 `sourceRoot` 的父目录逃逸都属于非法输入。

## 阅读顺序

1. 先看 `typed-module-metadata.md`，了解 M6 强类型推断闭环的元数据结构、导入路径和序列化边界。
2. 需要落代码时，再沿 frontmatter 的 `related_code` / `tests` 进入具体实现和验证入口。
