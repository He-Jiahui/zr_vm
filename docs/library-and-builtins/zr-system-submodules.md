---
related_code:
  - zr_vm_lib_system/include/zr_vm_lib_system/module.h
  - zr_vm_lib_system/include/zr_vm_lib_system/console_registry.h
  - zr_vm_lib_system/include/zr_vm_lib_system/fs_registry.h
  - zr_vm_lib_system/include/zr_vm_lib_system/env_registry.h
  - zr_vm_lib_system/include/zr_vm_lib_system/process_registry.h
  - zr_vm_lib_system/include/zr_vm_lib_system/gc_registry.h
  - zr_vm_lib_system/include/zr_vm_lib_system/vm_registry.h
  - zr_vm_lib_system/src/zr_vm_lib_system/module.c
  - zr_vm_lib_system/src/zr_vm_lib_system/console_registry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs_registry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/env_registry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/process_registry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/gc_registry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/vm_registry.c
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/src/zr_vm_library/native_binding.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/compile_expression.c
implementation_files:
  - zr_vm_lib_system/src/zr_vm_lib_system/module.c
  - zr_vm_lib_system/src/zr_vm_lib_system/console_registry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs_registry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/env_registry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/process_registry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/gc_registry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/vm_registry.c
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
  - tests/fixtures/projects/native_numeric_pipeline/src/lin_alg.zr
  - tests/fixtures/projects/native_math_export_probe/src/main.zr
doc_type: module-detail
---

# zr.system Submodules

## Purpose

`zr.system` 现在是一个只负责聚合的 native 根模块，真正的行为全部落在叶子模块中。这个拆分解决了两个问题：

第一，system API 不再是一个持续膨胀的扁平命名空间。调用方必须显式选择 `console`、`fs`、`env`、`process`、`gc`、`vm` 之一，语义边界稳定，命名也能保持完整而不依赖缩写。

第二，根模块导出子模块对象不再靠 `zr.system` 专用硬编码，而是通过 `zr_vm_library` 的通用 native descriptor 和 materializer 机制完成。以后别的 native 模块如果也要聚合子模块，可以复用同一套 `moduleLinks` 路径。

## Related Files

`zr_vm_lib_system/src/zr_vm_lib_system/module.c` 定义根模块 `zr.system`，只保留 6 个 `moduleLinks`，并在注册时先注册 6 个叶子模块，再注册根模块。

`zr_vm_lib_system/src/zr_vm_lib_system/*_registry.c` 定义每个叶子模块的公开函数、常量、类型和 hints。这里是公开 API 与元信息的单一来源。

`zr_vm_library/include/zr_vm_library/native_binding.h` 扩展 native descriptor 结构，新增 `ZrLibModuleLinkDescriptor` 和 `ZrLibModuleDescriptor.moduleLinks/moduleLinkCount`。

`zr_vm_library/src/zr_vm_library/native_binding.c` 负责把 type、constant、function、module link 一起物化成运行时模块对象，并把 `modules` 数组写入 `__zr_native_module_info`。

`zr_vm_parser/src/zr_vm_parser/type_inference.c` 在编译期读取 `__zr_native_module_info` 的 `modules/functions/constants/types` 数组，生成模块原型与字段信息。

`zr_vm_parser/src/zr_vm_parser/compile_expression.c` 保证模块导出的 prototype 不能被普通函数调用语法误用，要求使用 `$target(...)` 或 `new target(...)`。

## Behavior Model

### Root Module Shape

`import("zr.system")` 返回的根模块只暴露这 6 个字段：

- `console: zr.system.console`
- `fs: zr.system.fs`
- `env: zr.system.env`
- `process: zr.system.process`
- `gc: zr.system.gc`
- `vm: zr.system.vm`

根模块不再保留旧的扁平函数，也不重导出 `SystemFileInfo`、`SystemVmState`、`SystemLoadedModuleInfo` 这类类型值。调用方必须走层级访问，例如：

```zr
var system = import("zr.system");
system.console.printLine("hello");
system.gc.stop();
var info = system.fs.getInfo(system.fs.currentDirectory());
```

### Leaf Module APIs

`zr.system.console` 提供标准输出和标准错误输出函数：

- `print`
- `printLine`
- `printError`
- `printErrorLine`

`zr.system.fs` 提供文件系统查询和文本 I/O：

- `currentDirectory`
- `changeCurrentDirectory`
- `pathExists`
- `isFile`
- `isDirectory`
- `createDirectory`
- `createDirectories`
- `removePath`
- `readText`
- `writeText`
- `appendText`
- `getInfo`

`zr.system.env` 只暴露 `getVariable`。

`zr.system.process` 暴露一个常量和两个过程控制函数：

- `arguments`
- `sleepMilliseconds`
- `exit`

`zr.system.gc` 暴露垃圾回收控制：

- `start`
- `stop`
- `step`
- `collect`

`zr.system.vm` 暴露运行时检查与跨模块调用：

- `loadedModules`
- `state`
- `callModuleExport`

### Public Types And Metadata

叶子模块拥有自己的公开类型，并且这些类型会同时出现在模块 export 和全局 type 空间中。当前固定的公开类型如下：

- `SystemFileInfo`
- `SystemVmState`
- `SystemLoadedModuleInfo`

字段名保持完整语义，不保留缩写：

- `SystemFileInfo { path, size, isFile, isDirectory, modifiedMilliseconds }`
- `SystemVmState { loadedModuleCount, garbageCollectionMode, garbageCollectionDebt, garbageCollectionThreshold, stackDepth, frameDepth }`
- `SystemLoadedModuleInfo { name, sourceKind, sourcePath, hasTypeHints }`

子模块对象本身没有额外的 `SystemConsoleModule`、`SystemFsModule` 之类别名类型。它们的类型身份直接使用模块路径名，例如 `zr.system.console`。

## Design And Rationale

### Generic Module Links Instead Of Root-Specific Hardcoding

`ZrLibModuleLinkDescriptor` 只描述三件事：

- 根模块中要导出的字段名
- 被链接的模块路径
- 文档说明

materializer 在处理一个 native 模块时，会先注册它自己的 types、constants、functions，再处理 `moduleLinks`。对于每个 link，它会确保目标模块已从 native registry 物化并进入 cache，然后把目标模块对象直接作为当前模块的 pub export。这样 `zr.system` 根模块与 `zr.system.console` 叶子模块共享同一个缓存实例，而不是拷贝出一层包装对象。

### Full Metadata Is Required For Compiler Access

`native_binding.c` 在 `__zr_native_module_info` 里新增了 `modules` 数组，与原来的 `functions`、`constants`、`types` 并列。这个数组里的每一项记录：

- `name`
- `moduleName`
- `documentation`

`type_inference.c` 在 `ensure_native_module_compile_info` 中先创建模块原型，再把 `modules` 数组里的每个 entry 追加成一个模块字段。字段的 `fieldTypeName` 直接使用目标模块路径，例如 `zr.system.console`。这样 `import("zr.system").console.printLine(...)` 在编译期就能沿着 `zr.system -> zr.system.console -> printLine` 继续推断，而不是退化成不透明 object。

### Prototype Access Rules Stay Explicit

模块导出的 type export 依然是 prototype 对象。为了避免 `math.Vector3(...)` 这种普通调用与 prototype 构造混淆：

- `compile_expression.c` 在 member-chain 和 type inference 两边都把 prototype reference 视为不可直接调用
- 诊断信息统一要求使用 `$target(...)` 或 `new target(...)`
- 老的 unary `$expr` 构造语法继续报错，不作为兼容入口保留

这条规则同样适用于通过模块导出的 native 类型，所以 `$math.Vector3(...)` 和 `$math.Vector4(...)` 这类写法仍是合法路径。

## Control Flow Or Data Flow

1. CLI 初始化时注册 native 库，包括 `zr.math` 与重构后的 `zr.system`。
2. `ZrVmLibSystem_Register` 先把 6 个叶子模块注册进 native registry，再注册根模块 `zr.system`。
3. 运行时 `import("zr.system")` 触发 root descriptor 物化。
4. materializer 先创建 root module，再按 descriptor 注册 type、constant、function、module link、`__zr_native_module_info`。
5. 当处理 `moduleLinks` 时，linked module 会被提前加载并加入 module cache，随后作为 root export 写入当前模块。
6. 编译期 `import("zr.system")` 触发 `ensure_native_module_compile_info`。
7. `type_inference.c` 从 `__zr_native_module_info` 读取 `modules`，把 `console/fs/env/process/gc/vm` 建成模块字段；再继续读取 `types`，把叶子模块里的 struct/class metadata 推入 compiler prototype 表。
8. 后续表达式推断与字节码发射就可以像处理普通模块和普通 prototype 一样处理 `system.console.printLine(...)`、`system.fs.getInfo(...).modifiedMilliseconds`、`system.vm.state().loadedModuleCount` 这些访问链。

## Edge Cases And Constraints

- 不保留旧 API 兼容层。`system.println`、`system.writeText`、`system.gcDisable` 这类旧名字不应再出现。
- `import("zr.system")` 和 `import("zr.system.fs")` 必须同时成立，并且两者命中的叶子模块对象必须共享缓存实例。
- 根模块不应该偷偷重导出叶子类型。类型属于叶子模块，但仍要进入全局类型空间，便于字段查找和构造路径复用。
- 所有公开 struct/class 都必须带完整 field/method/metaMethod 元信息；否则 parser/type inference 无法把 native 类型当成可推断对象来处理。
- 模块字段类型使用完整路径名而不是别名，这使 native module object 与普通类型原型在 compiler 内部都能用同一套 `typeName` 机制表示。

## Test Coverage

`tests/module/test_module_system.c` 覆盖了：

- 叶子模块直导入，例如 `import("zr.system.console")`
- 根聚合访问，例如 `import("zr.system").console`
- 根模块只导出 6 个子模块，不包含旧扁平 API
- `SystemFileInfo`、`SystemVmState`、`SystemLoadedModuleInfo` 的字段与元信息可见性
- `system.vm.callModuleExport(...)` 的嵌套 native 调用

`tests/parser/test_type_inference.c` 覆盖了：

- `system.console.printLine(...)`
- `system.fs.getInfo(...).modifiedMilliseconds`
- `system.process.arguments`
- `system.vm.state().loadedModuleCount`
- `system.vm.loadedModules()[0].sourcePath`

`tests/fixtures/projects/native_numeric_pipeline` 把项目脚本迁移到新层级 `system.console` 和 `system.vm` API，并在同一回归里继续覆盖 `$math.Vector3(...)`、`$math.Vector4(...)` 与 native module export 交互。

`tests/fixtures/projects/native_math_export_probe` 额外验证 `system.vm.callModuleExport(...)` 与 native static method 链路可以一起工作。

## Plan Sources

本实现直接来自 2026-03-29 的用户方案，关键约束包括：

- `zr.system` 必须拆成 6 个叶子模块和 1 个聚合根模块
- 命名统一使用完整 lowerCamelCase，不保留缩写式别名
- 根模块聚合必须走通用 descriptor/materializer 扩展，不允许对 `zr.system` 做专用硬编码
- 公开 struct/class 必须完整导出元信息

## Open Issues Or Follow-up

- `zr.system` 之外的 native 库目前还没有大规模采用 `moduleLinks`，后续如果继续拆分别的 built-in 模块，可以直接复用这套 descriptor 与 metadata 路径。
- 当前文档只覆盖 `zr.system` 和相关 compiler/runtime 支撑。更宽范围的 native module authoring 规范如果继续演进，建议单独补一篇通用文档，而不是继续把规则堆到本文件里。
