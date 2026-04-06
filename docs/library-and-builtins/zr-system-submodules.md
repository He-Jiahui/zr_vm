---
related_code:
  - zr_vm_lib_system/include/zr_vm_lib_system/module.h
  - zr_vm_lib_system/include/zr_vm_lib_system/fs_registry.h
  - zr_vm_lib_system/include/zr_vm_lib_system/exception_registry.h
  - zr_vm_lib_system/src/zr_vm_lib_system/module.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs_registry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs_common.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs_entry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs_stream.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs_internal.h
  - zr_vm_lib_system/src/zr_vm_lib_system/exception_registry.c
  - zr_vm_library/include/zr_vm_library/file.h
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/src/zr_vm_library/file.c
  - zr_vm_library/src/zr_vm_library/native_binding.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/compile_expression.c
implementation_files:
  - zr_vm_lib_system/src/zr_vm_lib_system/module.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs_registry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs_common.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs_entry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs_stream.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs_internal.h
  - zr_vm_lib_system/src/zr_vm_lib_system/exception_registry.c
  - zr_vm_library/include/zr_vm_library/file.h
  - zr_vm_library/src/zr_vm_library/file.c
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/src/zr_vm_library/native_binding.c
  - zr_vm_parser/src/zr_vm_parser/type_inference.c
  - zr_vm_parser/src/zr_vm_parser/compile_expression.c
plan_sources:
  - user: 2026-03-29 实现“zr.system 模块细分与子模块化方案”
  - .codex/plans/zr struct 值类型与 native wrapper 分层方案.md
  - .codex/plans/zr.system.fs 对象化文件系统与 FileStream handle_id wrapper 验证.md
tests:
  - tests/module/test_module_system.c
  - tests/parser/test_type_inference.c
  - tests/system/test_system_fs_module.c
  - tests/ffi/ffi_fixture.c
  - tests/fixtures/projects/native_numeric_pipeline/src/main.zr
  - tests/fixtures/projects/native_math_export_probe/src/main.zr
doc_type: module-detail
---

# zr.system Submodules

## Purpose

`zr.system` 现在仍然是一个聚合根模块，但文件系统部分已经从“扁平函数集合”升级成“路径对象 + 打开流对象 + native wrapper metadata”三层结构。

这个文档同时约束两件事：

- 根模块和 6 个叶子模块之间的聚合关系必须继续走通用 `moduleLinks` / native materializer 路径，而不是对 `zr.system` 做专用硬编码。
- `zr.system.fs` 必须同时提供对象化路径 API、兼容的旧函数 API，以及 `FileStream` 的 `handle_id` wrapper 语义，且这些语义只能在 FFI/native 边界自动 lowering。

## Root Module Shape

`%import("zr.system")` 返回的根模块只导出这 6 个字段：

- `console: zr.system.console`
- `fs: zr.system.fs`
- `env: zr.system.env`
- `process: zr.system.process`
- `gc: zr.system.gc`
- `vm: zr.system.vm`

根模块不再重导出旧的扁平文件系统函数，也不重导出 `SystemFileInfo`、`SystemVmState`、`SystemLoadedModuleInfo` 这类类型值。类型仍然属于各自叶子模块，但会进入全局 type 空间，所以既可以写 `var fs = %import("zr.system.fs"); new fs.File("a.txt");`，也可以在类型推断阶段通过模块字段拿到原型和元信息。

`zr.system.exception` 仍然是独立模块，不属于根模块这 6 个字段之一。文件系统相关失败会抛这个模块里的 `IOException`。

## Leaf Modules At A Glance

`zr.system.console` 暴露：

- `print`
- `printLine`
- `printError`
- `printErrorLine`

`zr.system.fs` 暴露：

- 兼容函数：`currentDirectory`、`changeCurrentDirectory`、`pathExists`、`isFile`、`isDirectory`、`createDirectory`、`createDirectories`、`removePath`、`readText`、`writeText`、`appendText`、`getInfo`
- 对象类型：`SystemFileInfo`、`FileSystemEntry`、`File`、`Folder`、`IStreamReader`、`IStreamWriter`、`FileStream`

`zr.system.env` 暴露：

- `getVariable`

`zr.system.process` 暴露：

- `arguments`
- `sleepMilliseconds`
- `exit`

`zr.system.gc` 暴露：

- `start`
- `stop`
- `step`
- `collect`

`zr.system.vm` 暴露：

- `loadedModules`
- `state`
- `callModuleExport`

`zr.system.exception` 暴露：

- `registerUnhandledException`
- `Error`
- `StackFrame`
- `RuntimeError`
- `IOException`
- `TypeError`
- `MemoryError`
- `ExceptionError`

## zr.system.fs Public Surface

### Compatibility Functions

旧的模块级函数还保留在 `zr.system.fs`，用于兼容既有脚本和测试：

- `currentDirectory(): string`
- `changeCurrentDirectory(path: string): bool`
- `pathExists(path: string): bool`
- `isFile(path: string): bool`
- `isDirectory(path: string): bool`
- `createDirectory(path: string): bool`
- `createDirectories(path: string): bool`
- `removePath(path: string): bool`
- `readText(path: string): string`
- `writeText(path: string, text: string): bool`
- `appendText(path: string, text: string): bool`
- `getInfo(path: string): SystemFileInfo`

这些函数现在只是对象模型之上的兼容薄封装：

- 查询型 API 继续返回 `bool` 或 `SystemFileInfo`
- 文本 I/O helper 继续保留原返回类型
- 对会实际执行文件创建、删除、打开或读写的 helper，失败时不再退回裸 `Ptr` 或宿主侧特殊 sentinel，而是统一映射为 `zr.system.exception.IOException`

### SystemFileInfo

`SystemFileInfo` 是 native 导出的 `struct` 值类型，用来承载一次文件系统快照。当前字段固定为：

- `path`
- `size`
- `isFile`
- `isDirectory`
- `modifiedMilliseconds`
- `exists`
- `name`
- `extension`
- `parentPath`
- `createdMilliseconds`
- `accessedMilliseconds`

`fileInfo` 字段和 `refresh()` 返回值都保存这个 struct 的快照副本，而不是懒引用宿主查询结果。

### FileSystemEntry

`FileSystemEntry` 是路径包装基类，`File` 和 `Folder` 都继承它。构造签名是 `FileSystemEntry(path: string)`，公开字段为：

- `path`: 构造时传入的原始路径
- `fullPath`: 归一化后的绝对路径
- `name`: 最终路径段
- `extension`: 最终路径段的扩展名，包含前导点
- `parent`: 归一化后的父目录 `Folder` 对象；如果已经是文件系统根，则为 `null`
- `fileInfo`: 最近一次刷新得到的 `SystemFileInfo` 快照

公开方法只有两个：

- `exists(): bool`
- `refresh(): SystemFileInfo`

构造函数和 `refresh()` 都会立即做一次宿主文件系统查询。`exists()` 也会先刷新快照，再返回 `fileInfo.exists`。

### File

`File` 继承 `FileSystemEntry`，构造签名是 `File(path: string)`，公开方法为：

- `open(mode: string = "r"): FileStream`
- `create(recursively: bool = true): null`
- `readText(): string`
- `writeText(text: string): int`
- `appendText(text: string): int`
- `readBytes(): array`
- `writeBytes(bytes: array): int`
- `appendBytes(bytes: array): int`
- `copyTo(targetPath: string, overwrite: bool = false): File`
- `moveTo(targetPath: string, overwrite: bool = false): File`
- `delete(): null`

行为约束：

- `create(true)` 会在需要时递归创建父目录
- 如果同一路径已经存在同类文件，`create` 视为成功
- 如果同一路径存在异类对象，相关操作会抛 `IOException`
- `copyTo` 和 `moveTo` 返回新的 `File` 包装对象，原对象不会自动改写为目标路径

### Folder

`Folder` 继承 `FileSystemEntry`，构造签名是 `Folder(path: string)`，公开方法为：

- `create(recursively: bool = true): null`
- `entries(): array`
- `files(): array`
- `folders(): array`
- `glob(pattern: string, recursively: bool = true): array`
- `copyTo(targetPath: string, overwrite: bool = false): Folder`
- `moveTo(targetPath: string, overwrite: bool = false): Folder`
- `delete(recursively: bool = false): null`

行为约束：

- `entries`、`files`、`folders`、`glob` 的返回结果按 `fullPath` 词法排序，保证测试稳定
- `glob` 当前支持 `*` 和 `?`
- `delete(false)` 只允许删除空目录；递归删除必须显式传 `true`

### IStreamReader And IStreamWriter

`IStreamReader` 只描述读接口：

- `readBytes(count: int = -1): array`
- `readText(count: int = -1): string`

`IStreamWriter` 只描述写接口：

- `writeBytes(bytes: array): int`
- `writeText(text: string): int`
- `flush(): null`

`FileStream` 明确实现这两个接口，因此可以在普通 zr 代码里把打开后的流传给接口形参，但这不等价于把它自动转成整数句柄。

### FileStream

`FileStream` 是第一个落地的 native wrapper class。它不是路径对象，而是实际持有宿主文件描述符的资源对象。公开字段为：

- `path`
- `mode`
- `position`
- `length`
- `closed`

公开方法为：

- `readBytes(count: int = -1): array`
- `readText(count: int = -1): string`
- `writeBytes(bytes: array): int`
- `writeText(text: string): int`
- `flush(): null`
- `seek(offset: int, origin: string = "begin"): int`
- `setLength(length: int): null`
- `close(): null`

额外还有一个 `@close` 元方法，复用同一个 runtime close 入口，供 `using` / `%using` 自动释放时调用。

`FileStream` 目前在 native metadata 中固定注册为：

- `ffiLoweringKind = "handle_id"`
- `ffiUnderlyingTypeName = "i32"`
- `ffiOwnerMode = "owned"`
- `ffiReleaseHook = "close"`

这意味着：

- 普通 zr 赋值、普通函数调用、数组写入都不会把 `FileStream` 隐式转成整数
- 只有 extern/native 边界在目标参数是 `i32` handle 时，才会读取 wrapper 里的 handle id 做 lowering
- wrapper 已经关闭后，再参与 lowering 必须报错，防止把失效 fd 透传给 native

## Stream Modes And Data Conventions

`File.open(mode)` 支持的 canonical mode 是：

- `r`
- `r+`
- `w`
- `w+`
- `a`
- `a+`
- `x`
- `x+`

可以额外附带 `b` 作为二进制别名，例如 `rb`、`wb+`、`ab`。`FileStream.mode` 字段里保存的是去掉可选 `b` 之后的 canonical 形式。

接口能力不受 `b` 影响：

- `readText` / `writeText` 始终存在，文本按 UTF-8 处理
- `readBytes` / `writeBytes` 始终存在，字节数组用 `array` 承载 `0..255` 的整数
- 真正决定是否可读、可写、是否截断、是否追加、是否独占创建的是打开模式本身

`seek` 的 `origin` 只接受：

- `begin`
- `current`
- `end`

其它字符串会抛 `IOException`。

## Lifecycle And Error Model

路径对象 `File` / `Folder` 不持有 native 资源，只保存规范化路径和最近一次快照。

`FileStream` 才是真正的 owner：

- 底层保存稳定的宿主文件句柄 id
- `close()` 幂等
- `using` / `%using` 会走 `@close`
- finalizer 只做一次兜底关闭，不会重复释放已经关闭过的句柄
- 多个 zr 变量复制的是同一个 wrapper 引用，不会复制底层 fd

错误模型统一如下：

- `exists()`、`pathExists()`、`isFile()`、`isDirectory()` 这类探测接口返回 `bool`
- `open`、`create`、`delete`、`copyTo`、`moveTo`、`entries`、`glob`、`read*`、`write*`、`flush`、`seek`、`setLength` 失败时抛 `zr.system.exception.IOException`
- 模式不匹配，例如对只读流写入，也抛 `IOException`

## FFI Boundary Rules

`FileStream` 的 lowering 只在 FFI/native binding helper 边界生效。典型例子如下：

```zr
%extern("ffi_fixture") {
  #zr.ffi.entry("zr_ffi_tell_fd")# tellFd(fd:i32): i32;
}

var fs = %import("zr.system.fs");
var file = new fs.File("sample.txt");
file.parent.create(true);

var stream = file.open("w+");
using stream;
stream.writeText("abc");

if (tellFd(stream) != 3) {
  throw "unexpected fd position";
}
```

这段代码是合法的，因为 `tellFd` 是 extern 入口，目标参数就是 `i32`。

相反，普通 zr 调用点不会触发 lowering：

```zr
var fs = %import("zr.system.fs");

func acceptFd(fd:i32): i32 {
  return fd;
}

var stream = new fs.File("sample.txt").open("w+");
acceptFd(stream); // 编译期报错
```

这个约束把 wrapper 生命周期语义和普通类型系统隔开了。上层 API 可以一直使用 `FileStream`，不必退回到裸 `Ptr` 或手工整数句柄。

## Metadata, Compiler Access, And LSP Visibility

`zr_vm_library` 的 native descriptor/materializer 现在不仅要把 `File`、`Folder`、`FileStream` 物化成运行时 prototype，也要把这些元信息写进 `__zr_native_module_info`。

对于 `zr.system.fs`，编译器和 LSP 侧至少可以看到这些事实：

- `File extends FileSystemEntry`
- `Folder extends FileSystemEntry`
- `FileStream implements IStreamReader, IStreamWriter`
- `FileStream` 的 wrapper metadata 是 `handle_id/i32/owned/close`
- `SystemFileInfo` 是公开 struct
- `close` 同时存在普通方法与 `@close` 生命周期入口

`type_inference.c` 会读取这些类型、字段、接口和 FFI metadata，把 `new fs.File(...)`、`file.parent.fullPath`、`takeReader(stream)`、以及 extern handle lowering 场景都保留在可推断路径内。

## Control Flow Or Data Flow

1. CLI 初始化时注册 `zr.system` 根模块、各个叶子模块，以及独立的 `zr.system.exception` 模块。
2. `%import("zr.system")` 通过 `moduleLinks` 物化根模块，并把 6 个叶子模块对象直接作为 export 暴露出去。
3. `%import("zr.system.fs")` 直接物化 `zr.system.fs`，其中类型描述符注册 `File`、`Folder`、`SystemFileInfo`、`FileStream` 和两个 stream interface。
4. `File` / `Folder` 构造时调用底层 `zr_vm_library/file.c` 查询宿主路径信息，填充 `fullPath`、`parent` 和 `fileInfo`。
5. `File.open(...)` 通过平台文件句柄接口打开底层资源，再创建 `FileStream` wrapper 对象，并把 handle id 与隐藏 native 指针写入对象字段。
6. 普通 zr 调用里，`FileStream` 只是一个 class 实例引用。
7. 遇到 extern/native 边界时，FFI lowering 根据 prototype 上的 wrapper metadata 读取 handle id，把它按 `i32` ABI 参数传给宿主函数。
8. `close()`、`using` 和 finalizer 最终都收敛到同一条关闭路径，负责置 `closed = true`、把隐藏 handle id 更新为 `-1`，并避免二次释放。

## Edge Cases And Constraints

- 根模块仍然只导出 6 个叶子模块，不应把 `IOException` 或 `FileStream` 再次平铺到 `zr.system`。
- `parent` 基于 `fullPath` 计算，而不是保留相对路径词法语义；因此 `new fs.File("a.txt").parent` 会直接得到归一化后的所属目录对象。
- 目录对象也允许拥有扩展名，因为 `name` / `extension` 是按最终路径段抽取，不区分文件还是目录。
- `refresh()` 返回的是当前 `fileInfo` 快照值，不是懒句柄或 view。
- `FileStream` 的关闭状态必须参与 lowering 诊断；关闭后不得再把它当成有效 `i32` handle 传出。
- 兼容函数 API 仍然存在，但新的推荐上层写法是 `File` / `Folder` / `FileStream`，而不是字符串路径 + 裸 `Ptr`。

## Test Coverage

`tests/module/test_module_system.c` 继续覆盖：

- `zr.system` 根模块的 6 个叶子字段
- 根模块聚合与叶子直导入共享缓存实例

`tests/parser/test_type_inference.c` 继续覆盖：

- `system.console`、`system.fs`、`system.vm` 这类模块字段访问链仍可推断

`tests/system/test_system_fs_module.c` 现在专门覆盖：

- `File`、`Folder`、`FileStream`、`IStreamReader`、`IStreamWriter` 的 metadata 暴露
- `SystemFileInfo` 扩展字段与 `refresh()` 行为
- `File` / `Folder` 的 `create`、`copyTo`、`moveTo`、`delete`、`entries`、`files`、`folders`、`glob`
- `FileStream` 的 mode、`read*`、`write*`、`seek`、`setLength`、`close`
- `using` 自动关闭
- `IOException` 抛出路径
- `handle_id` lowering 只在 extern 边界生效，且关闭后的流会被拒绝

`tests/ffi/ffi_fixture.c` 提供 `zr_ffi_tell_fd` 等宿主入口，用于验证 `FileStream -> i32` 的实际 lowering 与消费路径。

## Plan Sources

当前行为来自三条叠加约束：

- 2026-03-29 的 `zr.system` 子模块化计划，要求根模块聚合必须走通用 descriptor/materializer
- `struct` 值类型与 native wrapper 分层方案，要求值语义聚合和 native resource wrapper 分层实现
- `zr.system.fs` 对象化文件系统与 `FileStream handle_id wrapper` 计划，要求用文件流把 wrapper metadata/runtime 真正跑通

## Open Issues Or Follow-up

- 当前文件系统实现仍然只覆盖同步 I/O，不包含 async I/O、watcher、ACL 或 memory map。
- `FileStream` 目前是第一个 `handle_id` wrapper；如果后续把更多系统资源迁移到同一 descriptor 体系，建议继续沿用“路径对象或业务对象 + owned wrapper + FFI boundary lowering”这一分层，而不是重新引入裸 `Ptr` 风格上层 API。
