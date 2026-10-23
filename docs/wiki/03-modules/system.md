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
| `File` | `create(recursively = true)` | 缺失时创建空文件，已存在普通文件保留内容，返回 `null`；父目录按参数创建 |
| `File` | `readText()` / `readBytes()` | 读取整个文件，返回 `string` / `array` |
| `File` | `writeText(text)` / `appendText(text)` | 按 C-string 覆盖/追加文本，不校验 UTF-8且首 NUL 截断，返回写入字节数 |
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

`FileStream` 同时实现 `IStreamReader` 和 `IStreamWriter`。读写模式不匹配、宿主 I/O 失败和已关闭句柄的保护路径
会抛 `IOException`；读取的负 count 当前均按读至 EOF 处理（缺省 -1），seek 的失败由宿主 helper 报告。

`File`、`Folder` 和 `FileStream` 的 I/O 操作会将相应失败转换为
`zr.system.exception.IOException`。兼容的路径探测和 `changeCurrentDirectory` 通过布尔值
报告结果，不保证附带异常。脚本 API 不返回裸指针或宿主 errno sentinel。首次成功 `close()`
释放句柄并标记关闭，后续关闭可成功返回 null；宿主关闭失败时保留私有句柄号与 `closed=false`，
底层句柄是否已释放待平台核验。终结器仅在能取回隐藏 owner 时，对私有状态仍未关闭的句柄尝试关闭后释放 `ZrSystemFsStreamData`，不保证关闭失败可重试。C 回调的普通 false 也可能在 VM
无异常时变为 null；逐接口约束见 [Fs 公共接口台账](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/coverage/zr_vm_lib_system_fs_public.tsv)。

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
`full`；当前 C 字符串比较接受嵌入 NUL 前的合法前缀，已在 [GC 详细契约](system-api.md#gc-statistics)
记录为缺陷。`0` 作为 heap limit 表示清除软阈值；collection 的成功 null 不证明周期完成。
参数转换、预算、统计对象与并发限制见上述详细契约。

## C 接口与注册入口

```c
const ZrLibModuleDescriptor *d = ZrVmLibSystem_GetModuleDescriptor();
TZrBool ok = ZrVmLibSystem_Register(global);
```

`Register` 负责 descriptor 校验、叶子 module links 注册和全局缓存挂接。共享库导出
`ZrVm_GetNativeModule_v1()`。宿主应显式关闭仍打开的 `FileStream`；只有隐藏 owner 字段成功发布，
finalizer 才能找到记录并尝试关闭句柄、释放记录。`assembly` 在单次资源读取中关闭归档，`vm` 查询不持有文件句柄。

### VM 快照与转发边界

`loadedModules` 只遍历 global 的已加载实例缓存，按哈希桶顺序投影注册元数据；它不是全部可用 descriptor 的枚举。`state` 的六字段混合缓存计数、全局 GC 与当前线程栈/帧取样，分配可能推进 GC，不保证原子一致；gcMode 不能替代 stopGC 启停标志。`callModuleExport` 通过共享调用器解析并转发模块导出，允许其惰性导入路径；malloc 参数区只浅拷贝 VM 值，正常返回负责 free。

void 字段写入失败、短串驻留失败后未初始化值读取、被忽略的数组追加失败分别记录为 BUG；GC 搬迁与局部异常恢复前的资源边界保留为 TODO。见 [system API 的 VM 契约](system-api.md#vm) 和 [VM 逐单元台账](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/coverage/zr_vm_lib_system_vm.tsv)；一次 nested-native 类型比较不能扩大为快照、数值或故障路径保证。


### FileStream 结果与生命周期边界

position/length 是尽力写入的快照；初始查询失败被构造器忽略，读写或 seek 后的查询失败则报异常且不回滚已发生的 I/O。setLength 尝试把超出末尾的游标移回，但该次 seek 的失败被忽略。读取 count 按字节计，0 不消费输入，负值均读至 EOF。

readText 的首 NUL 截断、指定短串 HASH_BUCKET 扩容软失败下的 true/null，以及隐藏 owner 的 ARRAY pin 扩容软失败丢失，均有静态闭合路径。File 一次性 readText 需临时关闭成功且回调正常返回；File 刷新的普通 false 被忽略，Stream readText 还需后续快照查询成功。终结器只有能取回 owner 时才能尝试关闭/释放原生记录，关闭失败后的宿主状态仍待平台核实。详见 [Stream 逐单元台账](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/coverage/zr_vm_lib_system_fs_stream.tsv) 与 [批次证据](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/batches/20261015-system-fs-stream-contracts.md)。

### File、Folder 的副作用与一次性资源

File.create不截断已存在的普通文件；文本读写不验证UTF-8并在首NUL处截断。File copy/move尝试刷新源receiver却忽略普通false，Folder copy/move不刷新原receiver；move后的原对象仍指旧路径，目标包装失败不回滚宿主副作用。

已取得C缓冲、一次性句柄或目录列表后，相关VM分配异常非局部退出会跳过这些资源的free/close；WriteBytes限定合法RAW_CANONICAL物化需新增pairpool，WriteText helper没有该VM分配路径。普通失败清理仍执行；TryRun只恢复VM roots/scopes。这些BUG及共享字段setter的未初始化值BUG都有静态闭链，本批无故障注入。借用/保活保留具体TODO，Glob省略recursively默认true已确认，详见[FsEntry台账](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/coverage/zr_vm_lib_system_fs_entry.tsv)与[验收记录](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/batches/20261015-system-fs-entry-contracts.md)。

写入、创建、复制、移动和递归删除都不承诺事务回滚。短写后报错可能已写出前缀；File.writeBytes虽先完整校验元素才写字节，却先以w打开，非法元素也可能已截断旧文件（appendBytes不追加非法内容，但可先创建缺失文件）。递归创建失败保留已建父目录，File创建后的close失败保留新文件；递归删除失败不恢复已删子项。overwrite复制/移动可先删除旧目标，随后失败留下部分目标；移动的复制后删源回退若删除失败，也可能两边仍有内容。这些是现有失败状态契约，本批没有故障注入或专项执行。

File.open的w/a/x在宿主open成功时已可截断或创建文件，后续append seek失败或wrapper构造失败也不恢复文件内容；普通wrapper失败只尝试关闭句柄。Folder.glob省略recursively时确定默认true，现有显式true测试不提供省略参数的专项运行覆盖。

复制的普通文件分支会忽略close返回值，成功不保证关闭成功；overwrite可先删除旧目标，失败时目标可能仍不存在或只留下已建父目录。BUG：File/Folder.moveTo覆盖同一现存路径会先删源，再以IOException失败；Folder.copyTo把目录复制到自身子目录会不断纳入新建目标，moveTo在rename失败后的复制回退也可触发。调用时避开这些路径组合；本轮仅记录静态已证缺陷，未改行为，也未运行这两类破坏性场景。

路径重叠的另一已证BUG：overwrite=true且目标是源的现存祖先目录，Copy/Move会先递归删除该祖先树，连源及兄弟项一起删除，再报IOException；目录Copy还可能留下重建的空目标。File/Folder包装名称不校验实际源类型，实际源为目录的File.copyTo也可能触发前述子目录递归；moveTo在rename失败后的复制回退适用。上述结论按NormalizePath实际输出的组件关系说明，不承诺文件系统别名或并发操作安全。
