---
related_code:
  - zr_vm_lib_system/include/zr_vm_lib_system/module.h
  - zr_vm_lib_system/include/zr_vm_lib_system/gc.h
  - zr_vm_lib_system/include/zr_vm_lib_system/fs_registry.h
  - zr_vm_lib_system/include/zr_vm_lib_system/exception_registry.h
  - zr_vm_lib_system/src/zr_vm_lib_system/module.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_registry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c
implementation_files:
  - zr_vm_lib_system/src/zr_vm_lib_system/module.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c
  - zr_vm_lib_system/src/zr_vm_lib_system/gc/gc.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/plans/syntax/README.md
tests:
  - tests/system/test_system_fs_module.c
  - tests/library/test_file_list.c
  - tests/module/test_module_system.c
doc_type: module-detail
---

# `zr.system`

**状态：`current`；根 descriptor 版本 `1.0.0`，叶子模块按同一 Runtime ABI 注册。根模块
只聚合 link，不铺平叶子导出。**

`zr.system` 是 Runtime provider 的聚合根。根模块只暴露叶子模块字段，不把叶子
模块的函数和类型重新铺平，因此模块身份、权限和文档可以独立演进。

## 导入和叶子模块

```zr
module app.main;
let system = import("zr.system");
let fs = system.fs;
let text = fs.readText("config.zr");
system.console.printLine(text);
```

当前根字段为 `console`、`fs`、`assembly`、`env`、`process`、`gc`、`exception` 和
`vm`。直接导入 `zr.system.fs` 等价于取得同一 descriptor identity，不会创建第二
份 provider。

## Console、environment 和 process

| 模块 | 导出 | 语义 |
| --- | --- | --- |
| `console` | `print`、`printLine`、`printError`、`printErrorLine`、`read`、`readLine` | UTF-8 文本 I/O；错误流和普通流分开，EOF 返回 `null` |
| `env` | `getVariable(name)` | 读取宿主环境；未定义变量返回 `null` |
| `process` | `arguments`、`sleepMilliseconds`、`exit` | 启动参数、阻塞休眠和进程退出 |
| `assembly` | assembly/package 元数据查询 | 读取当前 module/assembly identity |
| `vm` | `loadedModules`、`state`、`callModuleExport` | 运行时模块快照和受控导出调用 |

`exit` 是不可恢复控制转移，宿主可在 C 层拦截；库函数不得把它当普通错误返回。
环境和参数字符串由 VM 创建并受 GC 管理，C callback 只在 `ZrLibCallContext` 生命周期内
借用指针。

### 叶子函数签名

| 叶子 | 函数签名 |
| --- | --- |
| `console` | `print(value: any): null`；`printLine(value: any): null`；`printError(value: any): null`；`printErrorLine(value: any): null`；`read(): string/null`；`readLine(): string/null` |
| `env` | `getVariable(name: string): string/null` |
| `process` | `arguments: array`；`sleepMilliseconds(milliseconds: int): null`；`exit(code: int): null`（不返回） |
| `assembly` | `resourceExists(name: string): bool`；`readResourceText(name: string): string`；`readResourceBytes(name: string): array` |
| `vm` | `loadedModules(): SystemLoadedModuleInfo[]`；`state(): SystemVmState`；`callModuleExport(moduleName: string, exportName: string, args: array): value` |

## 文件系统对象模型

`zr.system.fs` 同时保留兼容函数和对象 API。兼容函数包括：
`currentDirectory`、`changeCurrentDirectory`、`pathExists`、`isFile`、
`isDirectory`、`createDirectory`、`createDirectories`、`removePath`、
`readText`、`writeText`、`appendText`、`getInfo`。

对象层由 `SystemFileInfo`、`FileSystemEntry`、`File`、`Folder`、
`IStreamReader`、`IStreamWriter` 和 `FileStream` 组成：

- `FileSystemEntry(path)` 立即计算 `path`、`fullPath`、`name`、`extension`、`parent`
  和 `fileInfo` 快照；`exists()` 先刷新快照再返回布尔值，`refresh()` 返回本轮对象视图。
- `File(path).open(mode = "r")` 返回 `FileStream`；`create(recursively = true)`、
  `readText()`、`writeText(text)`、`appendText(text)` 委托同一个 stream/handle 层。
- `Folder(path).entries()`、`files()`、`folders()` 和 `glob(pattern, recursively = false)`
  返回按 `fullPath` 排序的 `array`；`entries` 只列直接子项，`glob` 才按参数递归。
- `FileStream` 的宿主句柄存放在隐藏 `handle_id` wrapper 中。脚本只能通过公开 close/read/write
  方法操作，不能把 `Ptr` 直接当文件句柄。

### 对象 API 速查

| 对象 | 方法 | 参数与返回值 |
| --- | --- | --- |
| `File` | `open(mode = "r")` | 返回 `FileStream`；支持 `r/r+/w/w+/a/a+/x/x+`，可带 `b` 别名 |
| `File` | `create(recursively = true)` | 创建文件，返回 `null`；父目录按参数创建 |
| `File` | `readText()` / `readBytes()` | 读取整个文件，返回 `string` / `array` |
| `File` | `writeText(text)` / `appendText(text)` | 覆盖/追加 UTF-8 文本，返回写入字节数 |
| `File` | `writeBytes(bytes)` / `appendBytes(bytes)` | 覆盖/追加 `0..255` 整数数组，返回写入字节数 |
| `File` | `copyTo(targetPath, overwrite = false)` / `moveTo(targetPath, overwrite = false)` | 返回目标 `File` |
| `File` | `delete()` | 删除文件，返回 `null` |
| `Folder` | `create(recursively = true)` | 创建目录，返回 `null` |
| `Folder` | `entries()` / `files()` / `folders()` | 返回直接子项数组 |
| `Folder` | `glob(pattern, recursively = false)` | `*`/`?` 通配；返回匹配项数组 |
| `Folder` | `copyTo(targetPath, overwrite = false)` / `moveTo(targetPath, overwrite = false)` | 返回目标 `Folder` |
| `Folder` | `delete(recursively = false)` | 非空目录须显式传 `true` |
| `FileStream` | `readText(count = -1)` / `readBytes(count = -1)` | 从当前位置读取；`-1` 表示剩余全部 |
| `FileStream` | `writeText(text)` / `writeBytes(bytes)` | 从当前位置写入，返回写入字节数 |
| `FileStream` | `flush()` / `seek(offset, origin = "begin"): int` / `setLength(length)` | 刷新、定位、截断或扩展；`seek` 返回新的绝对位置；`origin` 为 `begin/current/end` |
| `FileStream` | `close()` | 关闭句柄；重复调用幂等 |

`FileStream` 同时实现 `IStreamReader` 和 `IStreamWriter`。读写模式不匹配、负长度（除
`-1` 外）、越界 seek 和已关闭句柄都会抛出 `IOException`；不会返回 errno 整数哨兵。

所有路径先经过平台归一化和权限检查。失败统一抛出 `zr.system.exception.IOException`；
不会返回裸指针、负整数或宿主 errno sentinel。`close()` 可重复调用，第一次释放句柄，
后续调用无副作用。

## GC、VM 和异常子模块

`gc.enable()`、`gc.disable()`、`gc.collect(kind = "full")`、`gc.set_heap_limit(bytes)`、
`gc.set_budget(microseconds)` 和 `gc.get_stats()` 操作当前 `GcDomain`。统计快照区分
eden/survivor/old/pinned/large/permanent region 的 used/live bytes；读快照不会暂停
其他 mutator。

`exception` 导出 `Error`、`RuntimeError`、`NullReferenceError`、`IOException`、
`TypeError`、`MemoryError`、`ExceptionError` 和 `StackFrame`，并提供
`registerUnhandledException`。异常对象的 stack frame 在抛出时捕获，格式化失败不能覆盖
原始异常。

`gc` 的公开签名为 `enable(): null`、`disable(): null`、`collect(kind?: string): null`、
`set_heap_limit(bytes: int): null`、`set_budget(microseconds: int): null` 和
`get_stats(): SystemGcStats`。`kind` 只能是 `minor`、`major` 或 `full`，省略时使用 `full`；
`0` 作为 heap limit 表示清除限制。

## C 接口与注册入口

```c
const ZrLibModuleDescriptor *d = ZrVmLibSystem_GetModuleDescriptor();
TZrBool ok = ZrVmLibSystem_Register(global);
```

`Register` 负责 descriptor 校验、叶子 module links 注册和全局缓存挂接。共享库导出
`ZrVm_GetNativeModule_v1()`。宿主销毁 global 前必须先关闭仍由 fs/assembly/vm 模块持有的
native handles；最终 GC 会再次调用 finalizer，但 finalizer 设计为幂等。
