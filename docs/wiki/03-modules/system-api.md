---
related_code:
  - zr_vm_lib_system/include/zr_vm_lib_system/module.h
  - zr_vm_lib_system/include/zr_vm_lib_system/console.h
  - zr_vm_lib_system/include/zr_vm_lib_system/env.h
  - zr_vm_lib_system/include/zr_vm_lib_system/process.h
  - zr_vm_lib_system/include/zr_vm_lib_system/fs_registry.h
  - zr_vm_lib_system/include/zr_vm_lib_system/gc_registry.h
  - zr_vm_lib_system/include/zr_vm_lib_system/exception_registry.h
  - zr_vm_lib_system/src/zr_vm_lib_system/module.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_registry.c
implementation_files:
  - zr_vm_lib_system/src/zr_vm_lib_system/module.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c
  - zr_vm_lib_system/src/zr_vm_lib_system/gc/gc.c
  - zr_vm_lib_system/src/zr_vm_lib_system/exception_registry.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/library-and-builtins/index.md
tests:
  - tests/system/test_system_fs_module.c
  - tests/module/test_module_system.c
  - tests/library/test_file_list.c
doc_type: api-reference
---

# `zr.system` API 参考

`zr.system` 是一个聚合根。`import("zr.system")` 返回的对象只包含叶子 module link；
控制台、文件系统、GC 和 VM 查询分别在 `console`、`fs`、`gc`、`vm` 等 descriptor 中注册。
直接导入 `zr.system.fs` 与从根字段取得的 provider 具有同一 module identity，不会创建第二
份全局句柄表。

## 导入和错误模型

```zr
let system = import("zr.system");
let console = system.console;
console.printLine("ready");
```

普通值 API 返回 `bool`、`int` 或 `null`；资源和 I/O 错误抛出
`zr.system.exception.IOException`。`read`/`readLine` 在 EOF 返回 null；这和 C API 的
`ZR_NULL` 指针失败不是同一语义。所有 path、stream 和 exception object 都由当前 state
管理，不能跨 state 保存裸地址。

## console

| 函数 | 精确签名 | 说明 |
| --- | --- | --- |
| `print` | `print(value: any): null` | 写普通输出，不自动换行。 |
| `printLine` | `printLine(value: any): null` | 写普通输出并追加换行。 |
| `printError` | `printError(value: any): null` | 写错误流，不自动换行。 |
| `printErrorLine` | `printErrorLine(value: any): null` | 写错误流并追加换行。 |
| `read` | `read(): string/null` | 读取一段输入；EOF 为 null。 |
| `readLine` | `readLine(): string/null` | 读取一行；保留 provider 的 UTF-8 解码规则。 |

`value:any` 会走 core formatter；对象格式化可能触发 `toString`/reflection，复杂对象不应在
异常处理路径中无限递归打印。宿主重定向 stdout/stderr 时仍保持两条流的区分。

## env、process、assembly

### 环境和进程

```zr
let env = import("zr.system.env");
let home = env.getVariable("HOME");
if (home != null) {
    import("zr.system.console").printLine(home);
}

let process = import("zr.system.process");
for (let argument in process.arguments) {
    import("zr.system.console").printLine(argument);
}
process.sleepMilliseconds(10);
```

| 模块 | 导出 | 行为 |
| --- | --- | --- |
| `env` | `getVariable(name: string): string/null` | 未定义变量返回 null，不修改环境。 |
| `process` | `arguments: string[]` | 启动参数快照；数组由 VM 管理。 |
| `process` | `sleepMilliseconds(milliseconds: int): null` | 阻塞当前执行线程；负值抛 TypeError。 |
| `process` | `exit(code: int): null` | 不可恢复控制转移；不会正常返回。 |
| `assembly` | `resourceExists(name: string): bool` | 查询当前 assembly 资源。 |
| `assembly` | `readResourceText(name: string): string` | 读取 UTF-8 资源；缺失/解码失败抛 IOException。 |
| `assembly` | `readResourceBytes(name: string): array` | 读取原始字节整数数组。 |

`exit` 不是异常；宿主若需要可测试退出路径，应在 C 层安装受控入口或使用独立进程。

## 文件系统

### 兼容函数

| 函数 | 签名 | 返回值 |
| --- | --- | --- |
| `currentDirectory` | `(): string` | 当前工作目录的归一化路径。 |
| `changeCurrentDirectory` | `(path: string): bool` | 成功 true；权限/不存在时 false 或抛 IOException（以 provider 配置为准）。 |
| `pathExists` | `(path: string): bool` | 文件或目录存在性。 |
| `isFile` / `isDirectory` | `(path: string): bool` | 类型判断，不跟随无效路径。 |
| `createDirectory` | `(path: string): bool` | 创建单层目录。 |
| `createDirectories` | `(path: string): bool` | 递归创建目录树。 |
| `removePath` | `(path: string): bool` | 删除文件或空目录。 |
| `readText` | `(path: string): string` | 读取整个 UTF-8 文件。 |
| `writeText` / `appendText` | `(path: string, text: string): bool` | 覆盖/追加文本。 |
| `getInfo` | `(path: string): SystemFileInfo` | 返回 metadata snapshot。 |

兼容函数适合一次性操作；需要多次读写、seek 或资源组合时使用对象 API。布尔 false 只表示
操作未完成，详细 I/O 原因仍应从当前异常/日志取得。

### 对象和字段

`FileSystemEntry(path: string)` 是基类；`File`、`Folder` 继承它。构造时计算 `path`、
`fullPath`、`name`、`extension`、`parent` 和 `fileInfo`，`exists()` 会先刷新 snapshot。

| 类型 | 字段 |
| --- | --- |
| `SystemFileInfo` | `path:string`、`size:int`、`isFile:bool`、`isDirectory:bool`、`modifiedMilliseconds:int`、`exists:bool`、`name:string`、`extension:string`、`parentPath:string`、`createdMilliseconds:int`、`accessedMilliseconds:int` |
| `FileSystemEntry` | `path:string`、`fullPath:string`、`name:string`、`extension:string`、`parent:Folder`、`fileInfo:SystemFileInfo` |
| `FileStream` | `path:string`、`mode:string`、`position:int`、`length:int`、`closed:bool` |

### File

| 方法 | 签名 | 细节 |
| --- | --- | --- |
| `open` | `open(mode: string = "r"): FileStream` | 支持 `r/r+/w/w+/a/a+/x/x+`，可带 `b` 别名。 |
| `create` | `create(recursively: bool = true): null` | 创建文件；按参数决定是否创建父目录。 |
| `readText` / `readBytes` | `(): string` / `(): array` | 从头读取整个文件。 |
| `writeText` / `appendText` | `(text: string): int` | 覆盖/追加 UTF-8，返回字节数。 |
| `writeBytes` / `appendBytes` | `(bytes: array): int` | 数组元素必须是 0..255 整数。 |
| `copyTo` / `moveTo` | `(targetPath: string, overwrite: bool = false): File` | 返回目标 wrapper。 |
| `delete` | `(): null` | 删除文件；已不存在通常视 provider 结果处理。 |

### Folder

| 方法 | 签名 | 细节 |
| --- | --- | --- |
| `create` | `(recursively: bool = true): null` | 创建目录或目录树。 |
| `entries` | `(): FileSystemEntry[]` | 只列直接子项，按 `fullPath` 排序。 |
| `files` / `folders` | `(): File[]` / `(): Folder[]` | 只列直接子项并过滤类型。 |
| `glob` | `(pattern: string, recursively: bool = false): FileSystemEntry[]` | `*`、`?` 通配；递归由参数开启。 |
| `copyTo` / `moveTo` | `(targetPath: string, overwrite: bool = false): Folder` | 复制/移动整棵树。 |
| `delete` | `(recursively: bool = false): null` | 非空目录必须显式允许递归删除。 |

### FileStream 和 using

`FileStream` 实现 `IStreamReader`、`IStreamWriter`，并注册 canonical `using` close meta
method：

```zr
let file = init import("zr.system.fs").File("notes.txt");
using (let stream = file.open("w+")) {
    stream.writeText("header\n");
    stream.seek(0, "begin");
    let text = stream.readText();
}
// stream.close 已由 guard 调用，重复 close 仍安全
```

`readBytes/readText(count = -1)` 从当前位置读取，`-1` 表示剩余全部；`write*` 返回实际写入
字节数；`flush` 刷新缓冲；`seek(offset, origin = "begin")` 返回新的绝对位置；`setLength`
截断或扩展文件；`close` 幂等。读写模式不匹配、负长度（除 -1）、越界 seek 和已关闭句柄
抛 IOException，不返回 errno sentinel。

## gc

| 函数 | 签名 | 作用 |
| --- | --- | --- |
| `enable` / `disable` | `(): null` | 开/关增量和调度式 GC。 |
| `collect` | `(kind: string = "full"): null` | `minor`、`major`、`full` 三种请求。 |
| `set_heap_limit` | `(bytes: int): null` | 设置堆上限；0 表示清除限制。 |
| `set_budget` | `(microseconds: int): null` | 设置安全 GC slice 的暂停预算。 |
| `get_stats` | `(): SystemGcStats` | 返回控制状态和 region 统计快照。 |

`SystemGcStats` 包含 enabled、heap/managed/debt/预算、worker/region 数、各 region used/live
bytes、最近一次 step/collection 信息以及 minor/major/full 次数和时长。快照是只读值；不要
保存其中的内部 region 指针。GC 操作可能触发 safepoint，native callback 必须先 root 临时值。

## vm 和 exception

`vm.loadedModules(): SystemLoadedModuleInfo[]` 返回模块名称、source kind/path、registration
kind、type hints、module/runtime ABI、required capabilities 和 descriptor-plugin 标志。
`vm.state(): SystemVmState` 返回 loadedModuleCount、GC 控制、debt/threshold、stackDepth 和
frameDepth。`vm.callModuleExport(moduleName, exportName, args)` 按已注册 descriptor 调用导出，
参数数组会进行完整 arity/type 检查。

`exception.registerUnhandledException(handler: (Error) -> bool): null` 注册未处理异常 handler。
handler 中应只读取 error snapshot 并快速返回；异常 handler 自身抛错会进入默认终止策略。

## C 注册和资源顺序

```c
const ZrLibModuleDescriptor *descriptor = ZrVmLibSystem_GetModuleDescriptor();
if (!ZrVmLibSystem_Register(global)) {
    /* read native-registry last error before changing global */
    return ZR_FALSE;
}
```

共享库入口是 `ZrVm_GetNativeModule_v1()`。宿主销毁 global 前应关闭 FileStream、资源读取
句柄和自定义 exception/debug callback；finalizer 会再次检查，但不应依赖 finalizer 作为唯一
关闭路径。详细 descriptor/callback helper 见 [Native API](native-api.md)，宿主阶段见
[C 宿主指南](../05-interop/c-host-guide.md)。
