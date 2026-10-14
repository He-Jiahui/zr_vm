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
  - zr_vm_lib_system/src/zr_vm_lib_system/process/process.c
  - zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c
implementation_files:
  - zr_vm_lib_system/src/zr_vm_lib_system/module.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c
  - zr_vm_lib_system/src/zr_vm_lib_system/gc/gc.c
  - zr_vm_lib_system/src/zr_vm_lib_system/process/process.c
  - zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c
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
| `assembly` | `resourceExists`、`readResourceText`、`readResourceBytes` | 查询和读取当前项目包中的资源 |
| `vm` | `loadedModules`、`state`、`callModuleExport` | 全局已加载缓存快照、当前线程状态取样与模块导出转发 |

`exit` 直接终止宿主进程，是不可恢复控制转移；验证退出路径时应使用独立进程。
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
  和 `fileInfo` 快照；`exists()` 先刷新 `fileInfo` 再返回布尔值，`refresh()` 返回新的
  `SystemFileInfo`。刷新不重建 receiver 的其他路径字段。
- `File(path).open(mode = "r")` 返回 `FileStream`；`create(recursively = true)`、
  `readText()`、`writeText(text)`、`appendText(text)` 委托同一个 stream/handle 层。
- `Folder(path).entries()`、`files()`、`folders()` 和 `glob(pattern, recursively = true)`
  返回按 `fullPath` 排序的 `array`；`entries` 只列直接子项，`glob` 才按参数递归。
- `FileStream` 的隐藏 `__zr_fs_stream` 原生指针指向拥有宿主句柄的 `ZrSystemFsStreamData`；
  `__zr_ffi_handleId` 是供 FFI 绑定读取的整数视图，不承担资源所有权。脚本通过公开
  close/read/write 方法操作流。

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
| `Folder` | `glob(pattern, recursively = true)` | `*`/`?` 通配；返回匹配项数组 |
| `Folder` | `copyTo(targetPath, overwrite = false)` / `moveTo(targetPath, overwrite = false)` | 返回目标 `Folder` |
| `Folder` | `delete(recursively = false)` | 非空目录须显式传 `true` |
| `FileStream` | `readText(count = -1)` / `readBytes(count = -1)` | 从当前位置读取；`-1` 表示剩余全部 |
| `FileStream` | `writeText(text)` / `writeBytes(bytes)` | 从当前位置写入，返回写入字节数 |
| `FileStream` | `flush()` / `seek(offset, origin = "begin"): int` / `setLength(length)` | 刷新、定位、截断或扩展；`seek` 返回新的绝对位置；`origin` 为 `begin/current/end` |
| `FileStream` | `close()` | 关闭句柄；重复调用幂等 |

`FileStream` 同时实现 `IStreamReader` 和 `IStreamWriter`。读写模式不匹配、负长度（除
`-1` 外）、越界 seek 和已关闭句柄都会抛出 `IOException`；不会返回 errno 整数哨兵。

`File`、`Folder` 和 `FileStream` 的 I/O 操作会将相应失败转换为
`zr.system.exception.IOException`。兼容的路径探测和 `changeCurrentDirectory` 通过布尔值
报告结果，不保证附带异常。脚本 API 不返回裸指针或宿主 errno sentinel。首次成功 `close()`
释放句柄并标记关闭，后续关闭可成功返回 null；宿主关闭失败时保留私有句柄号与 `closed=false`，
底层句柄是否已释放待平台核验。终结器对私有状态仍未关闭的句柄尝试关闭后释放 `ZrSystemFsStreamData`，不保证关闭失败可重试。C 回调的普通 false 也可能在 VM
无异常时变为 null；逐接口约束见 [Fs 公共接口台账](../../code-review/coverage/zr_vm_lib_system_fs_public.tsv)。

## GC、VM 和异常子模块

`gc.enable()`、`gc.disable()`、`gc.collect(kind = "full")`、`gc.set_heap_limit(bytes)`、
`gc.set_budget(microseconds)` 和 `gc.get_stats()` 操作当前 state 所属 global 的共享 collector。
统计包括 GcDomain 指标及 eden/survivor/old/pinned/large/permanent region 的 used/live bytes。
Core getter 只对部分域指标加锁；脚本统计对象的后续分配可能触发 GC，整次调用没有
“不暂停其他 mutator”或线程安全保证。`disable()` 只设置 stop flag，保留 generational 模式，
不能据此保证所有后续自动推进均已停止。

`exception` 导出 `Error`、`RuntimeError`、`NullReferenceError`、`IOException`、
`TypeError`、`MemoryError`、`ExceptionError` 和 `StackFrame`，并提供
`registerUnhandledException`。异常对象的 stack frame 在抛出时捕获，格式化失败不能覆盖
原始异常。

`gc` 的公开签名为 `enable(): null`、`disable(): null`、`collect(kind?: string): null`、
`set_heap_limit(bytes: int): null`、`set_budget(microseconds: int): null` 和
`get_stats(): SystemGcStats`。`kind` 的接口约定为 `minor`、`major` 或 `full`，省略时使用
`full`；当前 C 字符串比较接受嵌入 NUL 前的合法前缀，已在 [GC 详细契约](system-api.md#gc-调用统计与失败边界)
记录为缺陷。`0` 作为 heap limit 表示清除软阈值；collection 的成功 null 不证明周期完成。
参数转换、预算、统计对象与并发限制见上述详细契约。

## C 接口与注册入口

```c
const ZrLibModuleDescriptor *d = ZrVmLibSystem_GetModuleDescriptor();
TZrBool ok = ZrVmLibSystem_Register(global);
```

`Register` 负责 descriptor 校验、叶子 module links 注册和全局缓存挂接。共享库导出
`ZrVm_GetNativeModule_v1()`。宿主应显式关闭仍打开的 `FileStream`；其隐藏文件句柄另有
finalizer 作为兜底。`assembly` 在单次资源读取中关闭归档，`vm` 查询不持有文件句柄。

### VM 快照与转发边界

`loadedModules` 只遍历 global 的已加载实例缓存，按哈希桶顺序投影注册元数据；它不是全部可用 descriptor 的枚举。`state` 的六字段混合缓存计数、全局 GC 与当前线程栈/帧取样，分配可能推进 GC，不保证原子一致；gcMode 不能替代 stopGC 启停标志。`callModuleExport` 通过共享调用器解析并转发模块导出，允许其惰性导入路径；malloc 参数区只浅拷贝 VM 值，正常返回负责 free。

void 字段写入失败、短串驻留失败后未初始化值读取、被忽略的数组追加失败分别记录为 BUG；GC 搬迁与局部异常恢复前的资源边界保留为 TODO。见 [system API 的 VM 契约](system-api.md#vm-快照与跨层调用契约) 和 [VM 逐单元台账](../../code-review/coverage/zr_vm_lib_system_vm.tsv)；一次 nested-native 类型比较不能扩大为快照、数值或故障路径保证。
