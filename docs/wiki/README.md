---
related_code:
  - CMakeLists.txt
  - README.md
  - zr_vm_common/include/zr_vm_common.h
  - zr_vm_core/include/zr_vm_core.h
  - zr_vm_parser/include/zr_vm_parser.h
  - zr_vm_library/include/zr_vm_library.h
  - zr_vm_cli/include/zr_vm_cli.h
implementation_files:
  - CMakeLists.txt
  - README.md
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/zr_language_specification.md
tests:
  - tests/fixtures/projects/syntax_reference_v1/syntax_reference_v1.zrp
  - tests/fixtures/projects/hello_world/hello_world.zrp
  - tests/CMakeLists.txt
doc_type: category-index
---

# ZrVm Wiki

这套 Wiki 是当前仓库的工程说明书入口。它同时服务三类读者：

1. **ZR 使用者**：编写 `.zr` 源码、组织项目、使用标准库和 CLI。
2. **运行时/工具开发者**：理解 parser、Semantic IR、ExecBC、VM、AOT 和 LSP 的边界。
3. **宿主集成者**：从 C 或 Rust 创建 VM、注册 native module、加载项目并处理错误与生命周期。

## 文档状态

文档以当前 checkout 的源码、公共头文件、测试和已提交设计为依据。每页使用状态标签：

| 标签 | 含义 |
|---|---|
| `current` | 当前源码和至少一个仓库测试路径已经提供的行为；仍应以具体 API/测试为准。 |
| `experimental` | 源码或测试已经存在，但 ABI、语义或跨后端覆盖仍在演进。 |
| `planned` | 设计/路线图中的目标，不应当当作可用接口。 |
| `migration` | 只用于解释已删除或迁移中的旧形式。 |

Wiki 不替代代码中的公共头文件。签名、枚举值、结构体布局和编译选项以对应 `include/` 文件为准；本文档负责解释调用顺序、所有权、错误和实现上下文。

## 阅读路径

### 初次使用

1. [系统概览](01-overview.md)
2. [构建与第一个项目](02-getting-started.md)
3. [语言语法参考](03-language-syntax.md)
   - [ZR 语言手册（逐条语法、用例与实现）](02-language/index.md)
     - [语义与执行模型](02-language/semantics-implementation.md)
     - [运算符、元方法与类型转换](02-language/operators-conversions.md)
     - [声明与语句完整参考](02-language/declaration-statement-reference.md)
     - [对象类型、成员与构造参考](02-language/object-model-member-reference.md)
     - [类型表达式与参数契约参考](02-language/type-expression-reference.md)
     - [表达式、构造与调用形状参考](02-language/expression-construction-reference.md)
     - [可调用对象、调用与异步迭代](02-language/callable-async-iterator-reference.md)
     - [模块导入与 Native FFI](02-language/module-import-native-ffi-reference.md)
     - [控制流、模式与清理参考](02-language/control-flow-pattern-cleanup-reference.md)
     - [作用域、名称绑定与可见性](02-language/scope-binding-reference.md)
     - [Attribute 与 Comptime 契约](02-language/attribute-comptime-contract-reference.md)
     - [模式、所有权与资源配方](02-language/patterns-ownership-recipes.md)
     - [泛型参数、约束与实例化参考](02-language/generic-parameter-instantiation-reference.md)
     - [异常展开、finally 与清理运行时参考](02-language/exception-cleanup-runtime-reference.md)
4. [类型、布局与内存](04-types-and-memory.md)
5. [函数、闭包与调用](05-functions-and-calls.md)

### 深入实现

- [控制流、异常与清理](06-control-flow-and-errors.md)
- [模块、项目与产物](07-modules-projects-artifacts.md)
- [编译器流水线](08-compiler-pipeline.md)
  - [Semantic IR、CFG 与数据流事实](08-compiler-semantic-ir-facts-reference.md)
- [VM 运行时](09-vm-runtime.md)
  - [执行栈、调用帧与解释器](09-runtime-stack-execution-reference.md)
- [AOT C/LLVM 后端](10-aot-backends.md)
  - [AOT Lowering、运行时 Helper 与注册](10-aot-lowering-registration-reference.md)

### 标准库与工具

- [标准库总览](03-modules/index.md)
  - [官方 Provider 矩阵](03-modules/provider-matrix.md)
  - [`zr.builtin` API](03-modules/builtin-api.md)
  - [官方库 API 总目录](03-modules/official-library-api-catalog.md)
  - [Task Runtime 状态机与 Scheduler bridge](03-modules/task-scheduler-runtime-reference.md)
  - [`zr.ffi` Handle、Callback 与 Pin](03-modules/ffi-runtime-handle-reference.md)
  - [Native Provider Descriptor 与注册协议](03-modules/native-provider-descriptor-reference.md)
  - [系统、容器、反射与稳定槽池运行时参考](03-modules/system-container-reflection-runtime-reference.md)
- [CLI 与构建工具](04-tools/cli.md)
  - [CLI 完整命令参考](04-tools/cli-command-reference.md)
- [语言服务器与 VS Code](04-tools/language-server.md)
  - [Language Server 协议与嵌入式接口](04-tools/language-server-protocol-reference.md)
- [测试与验证](04-tools/testing.md)
  - [TestManifest 与 Runner](04-tools/test-manifest-runner-reference.md)

### 宿主互操作

- [C API 总览](05-interop/c-api.md)
- [C 宿主集成指南](05-interop/c-host-guide.md)
- [Core 宿主状态、执行与预算](05-interop/core-runtime-host-api.md)
- [Core 值、对象与模块](05-interop/core-value-object-api.md)
- [GC、异常与执行安全](05-interop/core-gc-exception-api.md)
- [C 值与 GC 生命周期](05-interop/c-api-value-lifecycle.md)
- [Native Module 编写](05-interop/native-module-authoring.md)
- [Native Callback 实战配方](05-interop/native-callback-recipes.md)
- [Native Call Context 与回调 C API](05-interop/native-call-context-reference.md)
- [Native 插件加载](05-interop/native-plugin-loading.md)
- [FFI Contract](05-interop/ffi-contract.md)
- [项目、文件与 ZRM](05-interop/project-file-zrm-api.md)
- [Artifact Writer/Loader](05-interop/artifact-writer-loader-api.md)
- [Core C API](05-interop/c-api-core.md)
- [Parser/Compiler C API](05-interop/c-api-parser.md)
- [Parser/Compiler 深度 C API](05-interop/parser-compiler-c-api.md)
- [Library/Native Registry C API](05-interop/c-api-library.md)
- [AOT Runtime ABI](05-interop/aot-abi.md)
- [Rust Binding](05-interop/rust-binding.md)
- [嵌入式宿主生命周期](05-interop/embedding-lifecycle.md)

### 查阅索引

- [API 与公共头文件索引](06-reference/api-index.md)
- [文件格式与产物](06-reference/artifacts.md)
- [Canonical Artifact 二进制 Schema](06-reference/artifact-binary-schema-reference.md)
- [ABI 与兼容性参考](06-reference/abi-compatibility-reference.md)
- [能力状态矩阵](06-reference/status-matrix.md)
- [诊断生命周期与修复契约](06-reference/diagnostic-lifecycle-reference.md)
- [术语表](06-reference/glossary.md)

## 页面地图

网页生成器可按下表建立稳定路由；`manifest.json` 同时提供机器可读的 `sections` 和
`pages` 数组。编号只表达推荐阅读顺序，不是运行时 ABI 版本。

| 区域 | 页面 |
|---|---|
| 概览与语言 | [overview](01-overview.md)、[getting-started](02-getting-started.md)、[syntax](03-language-syntax.md)、[language handbook](02-language/index.md)、[declaration reference](02-language/declaration-statement-reference.md)、[object/member](02-language/object-model-member-reference.md)、[type contracts](02-language/type-expression-reference.md)、[expression/construction](02-language/expression-construction-reference.md)、[callables/async](02-language/callable-async-iterator-reference.md)、[control/patterns](02-language/control-flow-pattern-cleanup-reference.md)、[generic instantiation](02-language/generic-parameter-instantiation-reference.md)、[exception runtime](02-language/exception-cleanup-runtime-reference.md)、[scope/binding](02-language/scope-binding-reference.md)、[module/FFI](02-language/module-import-native-ffi-reference.md)、[attribute/comptime](02-language/attribute-comptime-contract-reference.md)、[ownership recipes](02-language/patterns-ownership-recipes.md)、[types](04-types-and-memory.md)、[functions](05-functions-and-calls.md)、[control-flow](06-control-flow-and-errors.md) |
| 编译与运行时 | [modules](07-modules-projects-artifacts.md)、[compiler](08-compiler-pipeline.md)、[SemIR/CFG](08-compiler-semantic-ir-facts-reference.md)、[vm](09-vm-runtime.md)、[stack/execution](09-runtime-stack-execution-reference.md)、[aot](10-aot-backends.md)、[AOT lowering](10-aot-lowering-registration-reference.md) |
| 标准库与编译期 provider | [module index](03-modules/index.md)、[official catalog](03-modules/official-library-api-catalog.md)、[builtin](03-modules/builtin.md)、[system](03-modules/system.md)、[system API](03-modules/system-api.md)、[container](03-modules/container.md)、[container API](03-modules/container-api.md)、[system/container/reflection runtime](03-modules/system-container-reflection-runtime-reference.md)、[math](03-modules/math.md)、[math API](03-modules/math-api.md)、[network](03-modules/network.md)、[network API](03-modules/network-api.md)、[iteration](03-modules/iteration.md)、[iteration API](03-modules/iteration-api.md)、[task/thread](03-modules/task-thread.md)、[task API](03-modules/task-api.md)、[task runtime](03-modules/task-scheduler-runtime-reference.md)、[thread API](03-modules/thread-api.md)、[ffi](03-modules/ffi.md)、[ffi API](03-modules/ffi-api.md)、[ffi runtime](03-modules/ffi-runtime-handle-reference.md)、[native API](03-modules/native-api.md)、[registry binding](03-modules/native-registry-call-binding.md)、[provider descriptor](03-modules/native-provider-descriptor-reference.md)、[debug/testing](03-modules/debug-testing.md)、[debug/testing API](03-modules/debug-testing-api.md)、[reflection/pooling](03-modules/reflection-pooling.md)、[reflection/pooling API](03-modules/reflection-pooling-api.md)、[compile](03-modules/compile.md)、[compile-declaration](03-modules/compile-declaration.md) |
| 工具链 | [tool index](04-tools/index.md)、[cli](04-tools/cli.md)、[CLI reference](04-tools/cli-command-reference.md)、[testing](04-tools/testing.md)、[TestManifest runner](04-tools/test-manifest-runner-reference.md)、[language-server](04-tools/language-server.md)、[LSP protocol](04-tools/language-server-protocol-reference.md)、[editor/debugger](04-tools/editor-debugger.md)、[migration](04-tools/migration.md)、[build-validation](04-tools/build-validation.md)、[wiki-authoring](04-tools/wiki-authoring.md) |
| 互操作 | [interop index](05-interop/index.md)、[C API](05-interop/c-api.md)、[Core C API](05-interop/c-api-core.md)、[Parser C API](05-interop/c-api-parser.md)、[Parser/Compiler deep API](05-interop/parser-compiler-c-api.md)、[Library C API](05-interop/c-api-library.md)、[Native callbacks](05-interop/native-callback-recipes.md)、[Native call context](05-interop/native-call-context-reference.md)、[Native plugins](05-interop/native-plugin-loading.md)、[AOT ABI](05-interop/aot-abi.md)、[Rust](05-interop/rust-binding.md)、[embedding](05-interop/embedding-lifecycle.md) |
| 参考 | [reference index](06-reference/index.md)、[API index](06-reference/api-index.md)、[artifacts](06-reference/artifacts.md)、[artifact schema](06-reference/artifact-binary-schema-reference.md)、[ABI compatibility](06-reference/abi-compatibility-reference.md)、[status](06-reference/status-matrix.md)、[errors](06-reference/error-catalog.md)、[diagnostic lifecycle](06-reference/diagnostic-lifecycle-reference.md)、[glossary](06-reference/glossary.md) |

## 版本与证据

仓库顶层 CMake 项目当前声明版本 `0.0.1`；源码还通过 `zr_vm_common/zr_version_info.h` 暴露构建版本信息。Wiki 不人为创建另一个版本号。页面中的“当前”表示本次文档生成时 checkout 的代码状态，而不是承诺未来 ABI 永不变化。

最小可运行证据是 `tests/fixtures/projects/syntax_reference_v1`：它包含正/负语法清单，入口工程在解释器和 binary-first 路径返回 checksum `7`。更宽的 AOT、反射、池化、异步、迭代和 LSP 能力分别由各自测试矩阵验证；详见[状态矩阵](06-reference/status-matrix.md)。

## 贡献文档

新增代码能力时，同时更新对应 Wiki leaf page 的 `related_code`、`implementation_files`、`plan_sources` 和 `tests`。不要把一次性调试日志堆进 Wiki；可复用的验证结论应放在 `docs/acceptance/` 或 `tests/acceptance/`，Wiki 只链接并解释其长期契约。
