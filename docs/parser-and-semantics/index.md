---
related_code:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic_analyzer.c
implementation_files:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/semantic.h
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_parser/src/zr_vm_parser/compile_statement.c
  - zr_vm_parser/src/zr_vm_parser/compiler.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic_analyzer.c
plan_sources:
  - user: 2026-03-28 实现“ZR 全目标回归强化与 Field-Scoped using 语义计划”
  - .codex/plans/ZR 全目标回归强化与 Field-Scoped using 语义计划.md
tests:
  - tests/parser/test_parser.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/parser/test_compiler_features.c
  - tests/module/test_module_system.c
doc_type: category-index
---

# Parser And Semantics

本目录记录 parser、semantic analyzer 和编译期前端共享的语言行为。

## 当前主题

- `ffi-extern-declarations.md`
  - `%extern("lib") decl` 与 `%extern("lib") { decls }` 源级 FFI 语法
  - extern function / struct / enum / delegate 的 declaration metadata 和 lowering 规则
  - `compileTimeTypeEnv` 与真正 compile-time callable 的边界
- `field-scoped-using.md`
  - `class`/`struct` 字段级 `using var` 语法
  - `static using` 非法诊断
  - cleanup plan 与 prototype metadata 的传播路径

## 阅读顺序

1. 先看 `ffi-extern-declarations.md`，了解 `%extern` 语法、descriptor schema 和 `zr.ffi` lowering 路径。
2. 再看 `field-scoped-using.md`，了解字段生命周期语义。
3. 需要落代码时，再对照 frontmatter 里的 `related_code` 和 `tests` 追踪实现与验证入口。
