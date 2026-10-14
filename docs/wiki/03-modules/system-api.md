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
  - zr_vm_lib_system/src/zr_vm_lib_system/exception/exception_registry.c
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

普通值 API 返回 `bool`、`int` 或 `null`；`File`、`Folder` 和 `FileStream` 的 I/O 失败
按各自接口抛出 `zr.system.exception.IOException`，兼容路径函数可能只返回 `false`。
`read`/`readLine` 在 EOF 返回 null；这和 C API 的
`ZR_NULL` 指针失败不是同一语义。所有 path、stream 和 exception object 都由当前 state
管理，不能跨 state 保存裸地址。

## console

| 函数 | 精确签名 | 说明 |
| --- | --- | --- |
| `print` | `print(value: any): null` | 写普通输出，不自动换行。 |
| `printLine` | `printLine(value: any): null` | 写普通输出并追加换行。 |
| `printError` | `printError(value: any): null` | 写错误流，不自动换行。 |
| `printErrorLine` | `printErrorLine(value: any): null` | 写错误流并追加换行。 |
| `read` | `read(): string/null` | 读取恰好一个 UTF-8 码点；EOF 为 null。 |
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
| `process` | `arguments: string[]` | VM 管理的数组；CLI 在执行前写入启动参数，单独注册 descriptor 不注入 argv。 |
| `process` | `sleepMilliseconds(milliseconds: int): null` | Windows/POSIX 等待当前 OS 线程；Emscripten 配置异步转换后让出 event loop；负值按 0 处理。 |
| `process` | `exit(code: int): null` | 不可恢复控制转移；不会正常返回。 |
| `assembly` | `resourceExists(name: string): bool` | 查询当前 assembly 资源。 |
| `assembly` | `readResourceText(name: string): string` | 将资源字节作为文本构造字符串，当前不验证 UTF-8；资源缺失或读取失败触发运行错误，内嵌 NUL 会截断。 |
| `assembly` | `readResourceBytes(name: string): array` | 读取原始字节整数数组。 |

`exit` 不是异常，当前回调直接调用宿主 `exit()`；测试退出路径应使用独立进程。

Process getter 借出提供者映像中的静态 descriptor 和嵌套表；注册方必须使该映像驻留到
所有借用结束。注册分派先验证恰好一个参数，数量不符在 callback 前抛元数异常；
callback 内的 `ReadInt` 接受有符号整数、无符号整数及浮点数值，非数值抛类型异常；
合法直接 C 调用绕过分派器时，缺参由 helper 自身抛元数异常。hint 的 `int` 文本不充当运行时类型门禁。

**BUG:** 大等待值窄化为 Windows `DWORD` 或 Emscripten `unsigned int` 后会改变时长；
Windows 的 `0xFFFFFFFF` 被解释为 `INFINITE`。POSIX 收到信号使 `nanosleep` 提前返回时，
当前代码不续睡，仍写 null 并报告成功。这些结论基于完整静态 callback 路径和平台 API，
本批未执行无限等待、信号注入或 Emscripten 运行验证。

**TODO:** Windows/POSIX 长等待的默认 GC-aware 策略、Emscripten Asyncify/JSPI 配置及
重入/挂起期借用有效性、支持 ABI 的 `time_t` 宽度、退出码范围和 function hint 的消费
仍需沿各自入口核实。调用链、平台资料与测试边界见
[Process 审查台账](../../code-review/coverage/zr_vm_lib_system_process.tsv)。

## 文件系统

### 兼容函数

| 函数 | 签名 | 返回值 |
| --- | --- | --- |
| `currentDirectory` | `(): string` | 当前工作目录的归一化路径。 |
| `changeCurrentDirectory` | `(path: string): bool` | 成功返回 true；系统调用失败返回 false，不附带 I/O 异常。 |
| `pathExists` | `(path: string): bool` | 文件或目录存在性。 |
| `isFile` / `isDirectory` | `(path: string): bool` | 类型判断，不跟随无效路径。 |
| `createDirectory` | `(path: string): bool` | 创建单层目录。 |
| `createDirectories` | `(path: string): bool` | 递归创建目录树。 |
| `removePath` | `(path: string): bool` | 删除文件或空目录。 |
| `readText` | `(path: string): string` | 读取整个 UTF-8 文件。 |
| `writeText` / `appendText` | `(path: string, text: string): bool` | 覆盖/追加文本。 |
| `getInfo` | `(path: string): SystemFileInfo` | 返回 metadata snapshot。 |

兼容函数适合一次性操作；需要多次读写、seek 或资源组合时使用对象 API。布尔 false 表示
操作未完成，其中 `changeCurrentDirectory` 不提供结构化 I/O 错误详情。

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
| `glob` | `(pattern: string, recursively: bool = true): FileSystemEntry[]` | `*`、`?` 通配；省略参数时回调按递归处理。 |
| `copyTo` / `moveTo` | `(targetPath: string, overwrite: bool = false): Folder` | 复制/移动整棵树。 |
| `delete` | `(recursively: bool = false): null` | 非空目录必须显式允许递归删除。 |

`glob` 的 descriptor 只声明接受 1 到 2 个参数，不携带默认值；上表的 `true` 来自回调
实现。现有脚本测试显式传入 `true`，省略参数的实际调用路径仍待测试确认。

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
bytes、最近一次 step/collection 信息以及 minor/major/full 次数和时长。快照不暴露
collector 内部结构；修改脚本快照字段不会改变 collector。GC 操作可能触发 safepoint，
native callback 必须先 root 临时值。

## vm 和 exception

`vm.loadedModules(): SystemLoadedModuleInfo[]` 返回模块名称、source kind/path、registration
kind、type hints、module/runtime ABI、required capabilities 和 descriptor-plugin 标志。
`vm.state(): SystemVmState` 返回 loadedModuleCount、GC 控制、debt/threshold、stackDepth 和
frameDepth。`vm.callModuleExport(moduleName, exportName, args)` 按已注册 descriptor 调用导出；
参数数组由目标导出的实际调用约束处理，native descriptor 会检查参数数量。

`exception.registerUnhandledException(handler: (Error) -> bool): null` 注册未处理异常 handler。
生产宿主的未处理异常分发路径尚待核查，目前不承诺 handler 的调用时机或其自身抛错时的策略。

## C 注册和资源顺序

```c
const ZrLibModuleDescriptor *descriptor = ZrVmLibSystem_GetModuleDescriptor();
if (!ZrVmLibSystem_Register(global)) {
    /* read native-registry last error before changing global */
    return ZR_FALSE;
}
```

共享库入口是 `ZrVm_GetNativeModule_v1()`。宿主销毁 global 前应显式关闭仍打开的
`FileStream`；其文件句柄由 finalizer 兜底释放。资源读取归档在单次调用内关闭；注册的
exception handler 由 VM global 持有。详细 descriptor/callback helper 见 [Native API](native-api.md)，宿主阶段见
[C 宿主指南](../05-interop/c-host-guide.md)。

## Env 调用与交付边界

注册的 `getVariable` 先由分派器验证恰好一个参数，再由 `ReadString` 验证字符串类型；
两种参数异常均通过 VM 异常传播。缺失变量正常返回 null，存在值经 `SetString` 交给 VM。
名称包含嵌入 NUL 时只查询该字节之前的前缀，完整名称查询应传入不含 NUL 的字符串。
宿主环境值只借用到复制完成；callback 不加锁，宿主须按目标 CRT 的约定协调并发环境修改。
静态 Env descriptor 及嵌套表、字符串和 callback 依赖 provider image 驻留至所有借用结束。

**BUG:** `kFunctions` 的返回声明是 `string`，而缺失变量可正常返回 null，独立 hint 为 `string?`。
当前 runtime typed metadata、parser 和 LSP 均有直接消费 descriptor 返回类型的路径；hint 不会覆盖该声明。
另在合法的新短环境值触发字符串驻留哈希桶扩容且该桶分配失败时，void `SetString` 可保留预置 null，
callback 仍成功，导致已有值被交付成缺失标记。这两项有完整静态证据，本轮未修复或运行故障注入。

**TODO:** 字符串对象分配失败后的完整 GC 恢复可能首次通过 `ZR_VM_TRACE_GC_REWRITE_SLOTS` 再次调用
`getenv`；需按支持的 CRT 核查旧宿主指针在该嵌套查询后是否有效，并以受控 GC 验证复制窗口。
任意长值还进入 Core API 字符串缓存，缓存比较已有短串转换宏用于长串的缺陷记录；
长环境值后续同桶查询的可观察影响须从 `CreateTryHitCache` 和 `ZR_CAST_STRING_TO_NATIVE` 核查。
当前只登记具体待核入口，不把上述风险当作已验证的 Env 行为缺陷。

相关 module 测试只核导出、根模块缓存身份和 module links；本轮 provider 9/9 只验证提供者契约。
环境查询、nullable metadata、长值同桶读取和分配失败没有执行信用。逐单元依据见
[Env 审查批次](../../code-review/batches/20261013-system-env-contracts.md)和
[Env 台账](../../code-review/coverage/zr_vm_lib_system_env.tsv)。


## Console 调用、资源与失败边界

`print` / `printLine` / `printError` / `printErrorLine` 注册元数均为 1，`read` / `readLine` 为 0；数量错误在 callback 前由 native dispatcher 抛 `ArityError`。非空 context 必须关联有效 VM state，result 必须是当前调用的可写槽；回调只同步借用它们。打印会转换第一个参数，可能执行用户 `toString` 并重入或抛出 VM 异常；异常发生后不继续日志提交。正常 true 仅表示日志 API 调用完成，返回值仍为 null，不保证操作系统写入或持久化成功。

日志默认 FILE sink 先按 channel 写 stdout/stderr，随后同步通知宿主 observer，print/printLine 传递 INFO/STDOUT/RESULT，printError/printErrorLine 传递 ERROR/STDERR/DIAGNOSTIC；observer 不取代默认输出。observer 的消息指针仅在本次同步通知内有效，需保留时自行复制。输入函数开始前只刷新默认 stdout/stderr，不触发 observer 的 flush。

`read` 按首字节候选长度消费一个 UTF-8 序列，严格 decoder 再验证完整码点；非法前缀、截断或非法编码会抛 VM 错误，已经消费的输入不回滚。首字节 EOF 返回 null。`readLine` 消费到 LF 或 EOF，删除 LF，并对两种结束都删除一个尾 CR；空行返回空字符串，未消费任何字节的 EOF 返回 null。输入来自进程共享 stdin，本模块没有覆盖整个码点/行的锁，宿主需要串行化并发读取；fgetc 可能无限等待。

**BUG:** 打印对显式字符串字节长度验证 UTF-8，但 `Log_Write` 默认 sink 用 strlen、换行路径以 `%s` 格式化；合法嵌入 NUL 会使其后文本从默认输出中消失。合法脚本入口为 lexer.c:262-275 的 `\x00` 转义，:51-78 的 save_char 保留零字节并增加显式长度，:302 把 bytes/length 交给 parser/parser_ownership.c:463-505 的 helper，后者在 :488 原样调用 String_Create。输出消费证据为 Console 共用 print helper、`value/value.c:319-363` 的转换路径及 `base/log.c:75-160` 的日志消费；非换行 observer 仍收到原 C 指针，不能据默认 sink 的截断推断宿主 observer 的全部处理行为。

**BUG:** 读取并分配 realloc buffer 后（剥除尾 CR 后的文本可为空），如果字符串尚需创建对象，`String_Create` 的 RawObject/GcMalloc 首次分配及 full-GC 后重试均失败会抛 MemoryError；活动 `TryRun` 的 C 异常入口执行 longjmp，跳过其后的 free(buffer)。realloc 失败和 UTF-8 校验失败均先释放 buffer，初始空行或行首 EOF 没有已分配 buffer，均不属于该触发条件。完整静态链见 `gc/gc_object.c:1069-1074`、`base/memory.c:39-47`、`exception/exception.c:543-574` 与 `exception_internal.h:18-21`；本轮不修复，不赋予 OOM 执行证据。

**BUG:** `read` 的新码点或 `readLine` 的新短行若尚未驻留，且 short-intern 表需要 HASH_BUCKET rehash，raw allocator 失败可让 String_Create 软返回 null 而不设置 VM 异常。SetStringObject 对 null 不写槽，callback 仍返回 true，dispatcher 交付预置 null，因而与正常 EOF 混淆。该分支见 `string/string.c:802-808`、`hash_set.h:525-537`、`base/hash_set.c:170-181`、`memory.h:19-22` 和 `native_binding_dispatch.c:362,1311-1316`；readLine 在此正常返回路径仍释放 C buffer，与硬异常泄漏分开。

**TODO:** `ferror` 尚未区分：首字节流错误与 EOF 返回同样 null，中途 read 错误表现为截断 UTF-8，readLine 的部分输入遇到错误可照常交付；须明确并测试 I/O 错误策略。末尾孤立 CR 加 EOF 的裁剪是否符合行契约也须核实。descriptor 默认 GC_AWARE 模式下，阻塞 fgetc/同步日志仍持有 RUNNING mutator；须从 `native_binding_dispatch_lanes.h:240-278` 和 `gc_domain_mutator.c:630-693` 检查 STW 等待与 native 模式策略，现有测试没有该时序覆盖。

System 的现有 module 测试检查 console 导出/hint，provider 测试检查官方归属；它们没有执行 Console 读输入或验证边界。CLI suite 的 `repl_native_import` 用例（`tests/cmake/run_cli_suite.cmake:1130-1149`）调用 print("xxx") 并断言 stdout 含 xxx 且 stderr 为空，其他项目 marker 断言只能提供整体流程信用。完整逐单元审查见 [Console 台账](../../code-review/coverage/zr_vm_lib_system_console.tsv)；本节不声称本批已执行整套 CLI 或 OOM/UTF-8/线程故障测试。
