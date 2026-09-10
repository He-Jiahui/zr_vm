---
related_code:
  - zr_vm_common/include/zr_vm_common/zr_common_conf.h
  - zr_vm_core/include/zr_vm_core/global.h
  - zr_vm_core/include/zr_vm_core/state.h
  - zr_vm_parser/include/zr_vm_parser/parser.h
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_rust_binding/include/zr_vm_rust_binding.h
implementation_files:
  - zr_vm_core/src/zr_vm_core/global.c
  - zr_vm_core/src/zr_vm_core/state.c
  - zr_vm_parser/src/zr_vm_parser/parser.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/core-runtime/index.md
  - docs/parser-and-semantics/index.md
  - docs/library-and-builtins/index.md
tests:
  - tests/core/test_session_checkpoint.c
  - tests/ffi/test_native_extern_contract.c
  - tests/library/test_official_provider_convergence.c
  - tests/rust_binding
doc_type: category-index
---

# C/Rust 互操作

ZrVm 的宿主 ABI 分为四层：

1. `zr_vm_core`：global/state、值、对象、执行、GC、异常和模块基础设施。
2. `zr_vm_parser`：词法/语法、语义查询、CFG/SemIR、编译和产物写入。
3. `zr_vm_library`：项目/文件、native registry、`.zrm`、AOT runtime 和任务桥接。
4. `zr_vm_rust_binding`：为 Rust/其它 FFI 宿主提供稳定 opaque-handle API。

| 页面 | 适用对象 |
| --- | --- |
| [C API 约定](c-api.md) | 所有 C 调用方 |
| [Core C API](c-api-core.md) | 嵌入 VM、管理值/对象/GC |
| [Core 宿主状态、执行与预算 C API](core-runtime-host-api.md) | global/state、mutator、loader hook、execution budget、取消和 checkpoint |
| [Core 值、字符串、对象与模块 C API](core-value-object-api.md) | `SZrTypeValue`、ownership、object/prototype、module export/cache |
| [Core GC、异常与执行安全 API](core-gc-exception-api.md) | write barrier、pin/root、GC domain、try/throw、取消 unwind |
| [C 值与 GC 生命周期](c-api-value-lifecycle.md) | `SZrTypeValue`、ownership、root/pin、写屏障和异常边界 |
| [Parser C API](c-api-parser.md) | 编译器、IDE、代码生成器 |
| [Parser/Compiler 深度 API](parser-compiler-c-api.md) | 分阶段解析、Semantic/CFG/SemIR、submission 和 writer |
| [Library C API](c-api-library.md) | 项目加载、provider、文件和任务 |
| [AOT ABI](aot-abi.md) | 生成 C/LLVM、动态加载 AOT 模块 |
| [Rust binding](rust-binding.md) | Rust workspace/runtime/native module |
| [嵌入生命周期](embedding-lifecycle.md) | 从创建到关闭的完整顺序 |
| [C 宿主集成指南](c-host-guide.md) | 从 global/provider 到执行、异常和关闭的可复制流程 |
| [Native Module 编写](native-module-authoring.md) | descriptor、callback、GC root、注册和升级 |
| [Native Callback 实战配方](native-callback-recipes.md) | 参数、对象/数组、out/ref、inline value、异常和线程边界 |
| [Native Call Context 与回调 C API](native-call-context-reference.md) | `ZrLibCallContext`、参数/结果、temp root、inline view、写回和重入的完整生命周期 |
| [Native 插件加载](native-plugin-loading.md) | 动态库发现、ABI、shadow cache、phase、失效和重载 |
| [FFI Contract](ffi-contract.md) | native extern、类型 lowering、pointer/callback ABI |
| [项目、文件与 ZRM 包 API](project-file-zrm-api.md) | `.zrp`、specifier、import resolver、文件 handle 和 `.zrm` 容器 |
| [Artifact Writer、Loader 与元数据投影 API](artifact-writer-loader-api.md) | `.zro/.zri/.zrs`、scheduler artifact、AOT writer、loader 校验 |

接口声明以仓库头文件为准。文档中的指针若未明确标注 `owned`，默认只在当前调用或
当前 state/revision 内借用；跨调用保存必须复制、注册 root 或使用专门 handle。
