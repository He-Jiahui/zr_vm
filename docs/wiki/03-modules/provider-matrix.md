---
related_code:
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/include/zr_vm_library/native_registry.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_official_inventory.c
  - zr_vm_parser/include/zr_vm_parser/compile_tool.h
  - zr_vm_library/include/zr_vm_library/project.h
implementation_files:
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_official_inventory.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_registry.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_descriptor.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/library-and-builtins/index.md
tests:
  - tests/library/test_official_provider_convergence.c
  - tests/library/test_call_binding_native_registry.c
  - tests/library/test_project_import_resolver.c
  - tests/parser/test_compile_tool_project_import.c
doc_type: reference
---

# 官方 Provider 矩阵

本页是当前仓库中官方 module identity 的完整、可查表清单。来源是
native_binding_official_inventory.c，而不是 README 中的示例 import。一个条目存在表示
resolver/registry 识别这个名字；它不保证当前宿主已经注册了该 provider，也不保证每个
构建都包含其平台依赖。

## 1. 阅读方式

每个 descriptor 都至少携带 moduleName、ABI、moduleVersion、providerPhase、capability、
函数/类型表、module link、contract hash 和 canonical type role。registry admission 的顺序
可概括为：

~~~text
descriptor/plugin discovered
  -> ABI and min-runtime-ABI validation
  -> module name and official inventory lookup
  -> provider phase and required capability validation
  -> official contract-role/canonical-role validation
  -> duplicate provider/signature validation
  -> materialization on the consuming state
~~~

Runtime host 只能消费 Runtime provider；CompileTool host 可以消费 compile-only provider；
Test host 才能消费 zr.testing。一个插件即使能被操作系统动态加载，也可能在 registry 阶段
被拒绝，因此动态库加载成功不是可导入的充分条件。

## 2. N0: bootstrap surface

| module | phase | contract role | 内容 | 详细页 |
| --- | --- | --- | --- | --- |
| zr.builtin | Runtime | BUILTIN_TYPE_SURFACE | Object、TypeInfo、IArrayLike/IEquatable/IHashable/IComparable/IComparer、primitive wrapper | [Builtin API](builtin-api.md) |

N0 是其它 provider 的类型根。第三方不能用同名模块替换它；TypeInfo 和 metadata child
roles 由 registry 做唯一性约束。

## 3. N1: 运行时基础 provider

| module | phase | 主要导出/用途 | 相关文档 |
| --- | --- | --- | --- |
| zr.container | Runtime | Array、Map、Set、LinkedList、Span、pool 适配 | [Container API](container-api.md) |
| zr.iteration | Runtime | Iterable、Enumerator、Iterator、AsyncIterator protocol | [Iteration API](iteration-api.md) |
| zr.math | Runtime | 常量、标量、vector/matrix、complex/quaternion/tensor | [Math API](math-api.md) |
| zr.task | Runtime | Async/Scheduler/Channel/Shared/Atomic 与 canonical Task bridge | [Task API](task-api.md) |

N1 不意味着“无依赖”：container 仍依赖 builtin protocol，task 依赖 core frame 和 GC，
math 可能有本机平台实现。tier 是启动/契约分层，不是性能等级。

## 4. N2: 运行时扩展 provider

| module | phase | 作用 | 相关文档 |
| --- | --- | --- | --- |
| zr.debug | Runtime | 调试 agent、snapshot、evaluate、coverage/profile | [Debug/Testing API](debug-testing-api.md) |
| zr.ffi | Runtime | native extern、pointer、buffer、callback、dynamic library | [FFI API](ffi-api.md) |
| zr.network | Runtime | 网络聚合根 | [Network API](network-api.md) |
| zr.network.tcp | Runtime | TCP endpoint、listener、stream、framing | [Network API](network-api.md) |
| zr.network.udp | Runtime | UDP socket、datagram、endpoint | [Network API](network-api.md) |
| zr.pooling | Runtime | BufferPool、stable slot/generation pool | [Reflection/Pooling API](reflection-pooling-api.md) |
| zr.reflection | Runtime | TypeId/metadata token contract | [Reflection/Pooling API](reflection-pooling-api.md) |
| zr.system | Runtime | 系统服务聚合根 | [System API](system-api.md) |
| zr.system.assembly | Runtime | assembly resource 查找/读取 | [System API](system-api.md) |
| zr.system.console | Runtime | stdin/stdout/stderr | [System API](system-api.md) |
| zr.system.env | Runtime | environment snapshot/query | [System API](system-api.md) |
| zr.system.exception | Runtime | Error、exception contract | [System API](system-api.md) |
| zr.system.fs | Runtime | path、file/folder、stream | [System API](system-api.md) |
| zr.system.gc | Runtime | GC 查询、控制和统计 | [System API](system-api.md) |
| zr.system.process | Runtime | arguments、sleep、exit | [System API](system-api.md) |
| zr.system.vm | Runtime | VM state/query surface | [System API](system-api.md) |
| zr.thread | Runtime | worker、domain、lock、Send/Sync | [Thread API](thread-api.md) |

system 和 network 根模块通常只作为 module link 容器。直接导入 leaf（例如
import("zr.system.fs")）和通过根对象取得 leaf 具有同一 module identity；不要把根对象
字段名误当成新的 provider。

reflection 在官方清单中带 REFLECTION provider contract role。它可为 contract-only
descriptor：这意味着 resolver/metadata 可以消费类型角色，但 module 不一定以普通
runtime object 方式 materialize。pooling 则是可 materialize 的 Runtime provider，二者的
registry 生命周期不能互换。

## 5. N3: compile-only 和测试 provider

| module | phase | public contract | 用途 | 详细页 |
| --- | --- | --- | --- | --- |
| zr.compile | CompileTool | zr.compile/v2，hash fnv1a64:ca60a1b2107c893b | build feature、assert/error/warning、conditional role | [zr.compile](compile.md) |
| zr.compile.declaration | CompileTool | declaration provider hash fnv1a64:b4e4667f4100e100 | immutable declaration view、Patch、generated field | [zr.compile.declaration](compile-declaration.md) |
| zr.testing | Test | Test-phase descriptor | test/case/skip metadata、assert/equal/throws | [Debug/Testing API](debug-testing-api.md) |

CompileTool provider 是 compiler-owned execution scope 的能力，不会注入普通 runtime
module graph。Test provider 仅在 Test phase 注册；生产 artifact 不应把其测试 roots 当作
普通入口。

## 6. C 侧枚举和诊断

官方清单可直接遍历。调用方应复制需要长期保存的 name，因为返回 entry 是 registry/source
拥有的只读视图：

~~~c
for (TZrSize i = 0; i < ZrLibrary_OfficialModuleInventory_GetCount(); ++i) {
    const ZrLibOfficialModuleInventoryEntry *entry =
            ZrLibrary_OfficialModuleInventory_GetAt(i);
    if (entry != ZR_NULL) {
        printf("%s tier=%u phase=%u role=%u\n",
               entry->moduleName,
               (unsigned)entry->tier,
               (unsigned)entry->phase,
               (unsigned)entry->providerContractRole);
    }
}

const ZrLibOfficialModuleInventoryEntry *ffi =
        ZrLibrary_OfficialModuleInventory_Find("zr.ffi");
~~~

注册/加载失败后，读取同一 global 的 error code 和 message：

~~~c
if (!ZrLibrary_NativeRegistry_RegisterModule(global, descriptor)) {
    EZrLibNativeRegistryErrorCode code =
            ZrLibrary_NativeRegistry_GetLastErrorCode(global);
    const TZrChar *message =
            ZrLibrary_NativeRegistry_GetLastErrorMessage(global);
    fprintf(stderr, "provider error %u: %s\n", (unsigned)code,
            message != ZR_NULL ? message : "unknown");
}
~~~

| error code | 表示 | 宿主应做什么 |
| --- | --- | --- |
| LOAD / SYMBOL | 插件文件或 ZrVm_GetNativeModule_v1 不可用 | 修复路径、依赖或导出符号。 |
| ABI_MISMATCH / VERSION_MISMATCH | plugin ABI 或最低运行时 ABI 不兼容 | 重新编译 provider；不要强行加载。 |
| CAPABILITY_MISMATCH | descriptor 要求的 capability 未启用 | 修改 host/project capability，或不注册该模块。 |
| MODULE_NAME_MISMATCH | 请求名与 descriptor moduleName 不同 | 修复 manifest/import/插件声明。 |
| MODULE_IN_USE | provider 仍被 state 或 artifact 使用 | 先停止执行、释放引用，再 invalidate。 |
| PHASE_MISMATCH | host phase 无权消费该 provider | 在正确 CompileTool/Test/Runtime phase 使用。 |
| RESERVED_OFFICIAL_MODULE | 使用了保留/历史官方名称 | 使用规范的 zr.* 名称。 |
| DUPLICATE_OFFICIAL_PROVIDER | 官方名字已有不同 descriptor | 不替换官方 provider；使用自定义 moduleName。 |
| PROVIDER_CONTRACT_MISMATCH | official role/canonical role 不符合清单 | 让 descriptor 精确匹配，或去掉错误 role。 |
| INVALID_CANONICAL_TYPE_ROLE | type role/projection 非法 | 校验 parent role、name 和 surface flags。 |
| DUPLICATE_PROVIDER_CONTRACT | 同一 contract 被重复声明 | 合并 provider 或提升版本。 |

## 7. 注册顺序和关闭顺序

推荐初始化顺序：

~~~text
GlobalState_New
  -> NativeRegistry_Attach
  -> zr.builtin
  -> N1 dependencies
  -> N2 features selected by host/project
  -> set State provider phase
  -> CompileTool/Test provider only in their dedicated host
  -> create/load project and execute
~~~

关闭时反向处理：停止 debug/network/thread/task activity，释放 project/artifact/native
handles，确保 module refcount 降到零，invalidate descriptor plugin source（若适用），再
NativeRegistry_Free，最后销毁 state/global。不要在 registry free 后保留 descriptor、error
message 或 module export 的裸指针。

## 8. 选择 provider 的决策表

| 需要的能力 | 首选模块 | 不应误用 |
| --- | --- | --- |
| 可索引集合/稳定 pool | zr.container / zr.pooling | 不要把 native pointer 当 Array。 |
| 异步单域调度 | zr.task | 不要用 zr.thread 的 isolated worker 代替 cooperative task。 |
| 跨线程/隔离 transfer | zr.thread | 不要跳过 Send/Sync 与 quota。 |
| C ABI / DLL | zr.ffi | 不要用 descriptor callback 取代 native extern contract。 |
| 类型 token/成员查询 | zr.reflection | 不要只比较 type name string。 |
| 编译期 feature/诊断 | zr.compile | runtime import 无权消费 CompileTool provider。 |
| 声明生成/Patch | zr.compile.declaration | @decorate 已删除。 |
| 测试断言和 TestManifest | zr.testing | 生产 Runtime host 不应隐式注册。 |

清单是当前 checkout 的官方集合。自定义模块应使用非 zr.* 的独立 module identity，声明
准确 phase/capability/contract，并遵循[Native Module 编写](../05-interop/native-module-authoring.md)。
