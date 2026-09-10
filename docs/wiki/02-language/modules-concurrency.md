---
related_code:
  - zr_vm_library/include/zr_vm_library/project.h
  - zr_vm_library/include/zr_vm_library/zrm.h
  - zr_vm_library/include/zr_vm_library/native_registry.h
  - zr_vm_library/include/zr_vm_library/task_runtime.h
  - zr_vm_lib_task/include/zr_vm_lib_task/module.h
  - zr_vm_lib_thread/include/zr_vm_lib_thread/module.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_declarations.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_reserved_task.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_extern.c
  - tests/fixtures/projects/syntax_reference_v1/src/modules.zr
  - tests/fixtures/projects/syntax_reference_v1/src/async_jobs.zr
  - tests/fixtures/projects/syntax_reference_v1/src/iterators.zr
  - tests/fixtures/projects/syntax_reference_v1/src/native_ffi.zr
implementation_files:
  - zr_vm_library/src/zr_vm_library/project/project_manifest_v2.c
  - zr_vm_library/src/zr_vm_library/project/project_import_resolver.c
  - zr_vm_library/src/zr_vm_library/project/project.c
  - zr_vm_library/src/zr_vm_library/zrm.c
  - zr_vm_library/src/zr_vm_library/task_runtime.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_state.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_expression_primary.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_reserved_task.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_extern.c
tests:
  - tests/library/test_project_import_resolver.c
  - tests/library/test_project_module_specifier.c
  - tests/library/test_zrm_container.c
  - tests/task/test_task_runtime.c
  - tests/iterator/test_yield_syntax.c
  - tests/ffi/test_native_extern_contract.c
  - tests/fixtures/projects/syntax_reference_v1/golden/provider-locator.json
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/plans/syntax/2026-07-19-10-native-ffi-module-package-design.md
  - docs/plans/syntax/2026-07-20-12-async-task-job-scheduler-design.md
doc_type: language-reference
---

# 模块、并发与 FFI

模块系统把 source literal、project manifest、provider registry 和 artifact loader 连接起来；并发与 FFI 则在同一 module identity/descriptor contract 上运行。语言层只允许静态、可验证的 module path 和声明形状，路径解析、动态库加载、任务调度和线程 transfer 由 Library/Core provider 完成。

## import 表达式

```ebnf
import-expression = "import", "(", string-literal, ")" ;
```

```zr
let builtin = import("zr.system");
let workspace = import("engine.render");
let alias = import("#nativeRender");
let packageRoot = import("@fixturedep");
let packageTool = import("@fixturedep/tool");
let artifact = import("artifacts/fixturedep.zrm");
let file = import("file:${URI}");
```

`import` 是 contextual keyword，必须是 module-scope 静态表达式，括号内只能有一个 string literal。裸 `import path`、`import(variable)`、`zr.import(...)` 和 `%import` 都会得到明确诊断；动态加载应使用 provider/runtime API，而不是绕过静态 module graph。

### 解析 kind 与规范化

Library resolver 将 literal 归类为七种 kind：

| kind | 常见写法 | identity/来源 |
| --- | --- | --- |
| Official native | `zr.system`、`zr.task` | 内置 provider，registry 保留。 |
| Registered native | `native:engine.render` | manifest `nativeProviders` 或宿主 registry。 |
| Workspace | `engine.render`、`./math/vector` | 当前 `.zrp` source graph。 |
| Relative | `./child`、`../shared` | 相对 current module 规范化。 |
| Alias | `#nativeRender` | manifest `aliases` 展开后再解析。 |
| Package | `@fixturedep/tool` | dependency manifest/package export。 |
| File | `file:/.../lib.zrm` | file URI，按平台 path policy 定位。 |

点号和斜杠只影响 raw spelling；resolver 产生的 `SZrLibrary_ModuleIdentity` 才是缓存和 call binding 的 key。registered-native 与 workspace 即使显示名相同，也属于不同 domain，不能靠字符串拼接合并。

## `.zrp` 与模块初始化

`.zrp` manifest 至少声明 name/version/source/binary/entry，并可声明 aliases、package exports、dependencies、nativeProviders、features、testSource 和 capabilities。解析流程为：

1. 读取并校验 manifest schema。
2. 以 current module key 规范化 import literal，展开 alias/package/export。
3. 选择 source、`.zri`、`.zro` 或 `.zrm` provider location。
4. 校验 module signature、source/input hash、artifact schema、dependency version 和 provider phase。
5. 创建 module object，执行 entry 一次并发布 declaration-ready/public exports。

初始化状态通常经历 `UNINITIALIZED -> INITIALIZING -> READY`，失败进入 `FAILED`。循环 import 只能观察已经发布的 declaration-ready symbol，不能读取尚未初始化的值。缓存、reload 和 generation 细节见[模块、项目与产物](../07-modules-projects-artifacts.md)。

## 产物后缀

| 后缀 | 角色 | 典型 writer/reader |
| --- | --- | --- |
| `.zr` | source | file loader + parser。 |
| `.zrp` | project manifest | `ZrLibrary_Project_*`。 |
| `.zri` | canonical intermediate/metadata | parser writer、compiler、LSP。 |
| `.zro` | executable binary | VM/AOT loader。 |
| `.zrs` | syntax tree/source projection | IDE、诊断工具。 |
| `.zrm` | assembly/package ZIP-like container | package resolver/provider。 |

`.zrm` 使用 `zr.zrm/v1` 和 `META-INF/zrm.json`；模块、资源和 compile-tool executable 分别位于 `modules/`、`resources/`、`compile-tools/`。OpenBytes 形式借用 immutable bytes，必须在 Close 前保持存活；ReadEntry 返回的副本由 Library API 释放。

## async、Task 与 await

```zr
let task = import("zr.task");

async fn addOne(value: int): task.Task<int> {
    return value + 1;
}

fn caller(value: int): task.Task<int> {
    return addOne(value);
}

async fn consume(value: int): task.Task<int> {
    let result = await addOne(value);
    return result;
}
```

`async` 是 contextual function marker；函数声明必须仍以 `fn` 开头，并显式写 `Task<T>` 返回 TypeRef。`await` 是 unary expression，解析其后一个 unary operand，形成 suspension boundary。task runtime 负责 Job/Task/Scheduler 生命周期和 fault propagation；parser 不执行任务，也不保证 provider 已注册。

语义约束包括：borrow/loan、`ref` view、affine guard 和未完成的 Task 不能跨越不允许的 `await`；Send/Sync 与 scheduler domain 决定值能否转移到 worker。详见 [`zr.task` 与 `zr.thread`](../03-modules/task-thread.md)。

## iterator、yield 与异步迭代

```zr
let iteration = import("zr.iteration");

fn values(value: int): iteration.Iterator<int> {
    yield value;
    yield value + 1;
}
```

含 `yield` 的函数通常返回 `Iterator<T>`；异步协议使用 `AsyncIterator<T>` 和 `Task<bool>` 的 `moveNext`。yield frame 保存局部、program counter、GC root map 和可恢复 loan；再次驱动时恢复，终止/异常/close 分别进入对应终态。旧 `out` generator 与 `{{...}}` 已删除，参见[控制流与资源清理](control-flow.md)。

## native extern 与 C ABI

```zr
native extern("syntax_reference_native") {
    #zr.ffi.entry("syntax_reference_render_value")#
    fn nativeValue(): i32;
}

fn readNative(): i32 {
    return nativeValue();
}
```

`native extern` 的 library spec 必须是字符串 literal；块内函数必须以 `fn` 开头、写完整参数与返回 TypeRef，声明可附 `#zr.ffi.entry("symbol")#` decorator。parser 生成 native import AST，compiler 再固化参数 passing mode、调用约定、contract hash 和 source range；runtime 只在 registry/FFI contract 校验通过后做 marshalling。

scalar、canonical struct、`Ptr<T>`、`ref`/`out` view 和 callback 的 ABI 细节见 [`zr.ffi` 与 `native extern`](../03-modules/ffi.md)。裸地址、未声明 variadic 或隐式类型猜测都不属于稳定语言接口。

## C 宿主的模块/任务入口

```c
SZrLibrary_Project *project =
    ZrLibrary_Project_New(state, manifestText, manifestPath);
if (project == ZR_NULL) {
    /* read the global/project diagnostic before cleanup */
    return ZR_FALSE;
}

SZrLibrary_ModuleSpecifier specifier;
TZrChar errorBuffer[256];
if (!ZrLibrary_ModuleSpecifier_Parse("@fixturedep/tool",
                                    &specifier,
                                    errorBuffer,
                                    sizeof(errorBuffer))) {
    ZrLibrary_Project_Free(state, project);
    return ZR_FALSE;
}

const ZrLibModuleDescriptor *taskDescriptor = ZrVmTask_GetModuleDescriptor();
if (!ZrVmTask_Register(state->global)) {
    /* read ZrLibrary_NativeRegistry_GetLastError* before cleanup */
    ZrLibrary_Project_Free(state, project);
    return ZR_FALSE;
}
/* taskDescriptor is a borrowed static descriptor; never free it. */
(void)taskDescriptor;
```

宿主需要在同一 global 上注册 provider、保持 manifest/archive/path 的 owner 生命周期，并在关闭前释放 project、Task/Job、FFI handle 和 native registry。常用入口：

| C API | 用途 |
| --- | --- |
| `ZrLibrary_Project_New/Free` | 建立/释放 `.zrp` project。 |
| `ZrLibrary_ModuleSpecifier_Parse` | 解析 raw import literal。 |
| `ZrLibrary_Project_ResolveImportModuleKey` | 结合 current module 生成规范 key。 |
| `ZrLibrary_Project_ResolveImportProviderLocation` | 找 source/binary/package/native provider。 |
| `ZrLibrary_Zrm_Open/Close/FindModule/ReadEntry` | 读取 `.zrm`。 |
| `ZrLibrary_NativeRegistry_Attach/RegisterModule` | 安装并校验 native descriptor。 |
| `ZrLibrary_TaskRuntime_PrepareJob/ExecutePreparedJob/CompletePreparedJob` | 驱动 Task/Job bridge。 |
| `ZrVmTask_Register`、`ZrVmThread_Register` | 注册官方 task/thread provider。 |

上述函数的真实参数、返回枚举和借用规则以公共头文件为准；完整生命周期见 [Library C API](../05-interop/c-api-library.md) 和 [C API 通用约定](../05-interop/c-api.md)。
