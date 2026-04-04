---
related_code:
  - zr_vm_lib_system/include/zr_vm_lib_system/module.h
  - zr_vm_lib_system/src/zr_vm_lib_system/module.c
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/src/zr_vm_library/native_binding.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/compile_expression.c
implementation_files:
  - zr_vm_lib_system/src/zr_vm_lib_system/module.c
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/src/zr_vm_library/native_binding.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/compile_expression.c
plan_sources:
  - user: 2026-03-29 实现“zr.system 模块细分与子模块化方案”
tests:
  - tests/module/test_module_system.c
  - tests/parser/test_type_inference.c
  - tests/fixtures/projects/native_numeric_pipeline/src/main.zr
  - tests/fixtures/projects/native_math_export_probe/src/main.zr
doc_type: category-index
---

# Library And Builtins

本目录记录内建 native library、宿主暴露 API，以及这些 API 如何被 parser 和运行时识别。

## 当前主题

- `../parser-and-semantics/ffi-extern-declarations.md`
  - source-level `%extern` 声明如何 lower 到 `zr.ffi.loadLibrary(...)` / `getSymbol(...)`
  - extern signature descriptor、layout descriptor 和 callback delegate 的消费规则
- `zr-task-runtime.md`
  - `zr.task` 模块、`Async<T>` / `Scheduler` / `Shared<T>` / `Transfer<T>`、`supportMultithread` / `autoCoroutine` 当前如何接到 CLI、project config、parser sugar 和 runtime
  - `%mutex` / `%atomic`、`%async` 返回类型包装、`await` / borrowed effect 诊断，以及 native call 抛错如何回卷进 VM `try/catch/finally`
- `zr-system-submodules.md`
  - `zr.system` 从扁平模块拆成 6 个叶子模块和 1 个聚合根模块
  - native descriptor 新增 `moduleLinks`，根模块通过通用物化逻辑导出子模块对象
  - parser/type inference 读取 native module info 里的 `modules` 数组，把 `system.console` 这类访问识别成模块字段

## 阅读顺序

1. 先看 `zr-task-runtime.md`，了解当前并发 runtime 的模块面、配置开关、parser sugar 和已落地限制。
2. 再看 `../parser-and-semantics/ffi-extern-declarations.md`，了解 source-level FFI 如何接入 `zr.ffi`。
3. 然后看 `zr-system-submodules.md`，了解本仓库当前的 `zr.system` 结构、叶子 API 和元信息约束。
4. 需要追代码时，再用 frontmatter 里的 `related_code`、`implementation_files` 和 `tests` 反查实现与验证入口。
