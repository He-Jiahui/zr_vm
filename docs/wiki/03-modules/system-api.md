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
  - zr_vm_lib_system/include/zr_vm_lib_system/exception.h
  - zr_vm_lib_system/src/zr_vm_lib_system/exception/exception.c
  - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c
  - zr_vm_cli/src/zr_vm_cli/repl/repl_session.c
  - tests/harness/runtime_support.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_core/include/zr_vm_core/value.h
  - zr_vm_core/src/zr_vm_core/function/function.c
implementation_files:
  - zr_vm_lib_system/src/zr_vm_lib_system/module.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c
  - zr_vm_lib_system/src/zr_vm_lib_system/gc/gc.c
  - zr_vm_lib_system/src/zr_vm_lib_system/process/process.c
  - zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c
  - zr_vm_lib_system/src/zr_vm_lib_system/exception/exception_registry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/exception/exception.c
  - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c
  - zr_vm_cli/src/zr_vm_cli/repl/repl_session.c
  - tests/harness/runtime_support.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_core/src/zr_vm_core/function/function.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/library-and-builtins/index.md
tests:
  - tests/system/test_system_fs_module.c
  - tests/module/test_module_system.c
  - tests/library/test_file_list.c
  - tests/exceptions/test_exceptions.c
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
[Process 审查台账](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/coverage/zr_vm_lib_system_process.tsv)。

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
| `readText` | `(path: string): string` | 读取文件字节并构造文本 string，不验证 UTF-8，首 NUL 截断。 |
| `writeText` / `appendText` | `(path: string, text: string): bool` | 覆盖/追加文本。 |
| `getInfo` | `(path: string): SystemFileInfo` | 返回 metadata snapshot。 |

兼容函数适合一次性操作；需要多次读写、seek 或资源组合时使用对象 API。
`changeCurrentDirectory` 和路径探测可成功执行 callback 并返回脚本布尔 false；
`createDirectory/createDirectories/removePath/writeText/appendText` 的正常脚本结果为 true，
宿主操作失败则构造 `IOException` 并返回 native false。`currentDirectory` 的路径归一化失败、
`getInfo` 的查询或包装对象失败也可返回 native false，却不保证设置 I/O 异常；VM 仍正常时
dispatcher 交付 null。不能把 C 回调的 false 统一解释为脚本布尔 false。

文本接口按宿主 C 字符串处理内容：四个 `readText` 导出入口复用三个 C callback，虽然读取显式字节数，
返回 VM string 时仍在首个 NUL 截断；合法内容 `A\0B` 会返回 `A`。FileStream 的
读取位置按已消费字节推进，`count` 也是字节数，可能截开 UTF-8 多字节序列，
当前路径不保证校验 UTF-8。`writeText`／`appendText` 按 `strlen` 取得写入长度，
嵌入 NUL 后的内容不会写入。需要保留任意字节时使用 `readBytes`／`writeBytes`。
字符串结果和 metadata 字段的 void setter 还存在软失败边界；native true 不证明
所有结果或字段均已写入。File/Folder I/O 完成后的快照刷新只是尝试，失败不会
回滚已经完成的宿主操作。逐入口的失败、借用与测试边界见
[Fs 公共契约台账](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/coverage/zr_vm_lib_system_fs_public.tsv)。

### 对象和字段

`FileSystemEntry(path: string)` 是基类；`File`、`Folder` 继承它。构造时计算 `path`、
`fullPath`、`name`、`extension`、`parent` 和 `fileInfo`。`exists()` 和 `refresh()` 重新查询并替换
`fileInfo` 快照；`refresh()` 返回这份 `SystemFileInfo`，不返回 receiver，也不重新计算其他路径字段。

| 类型 | 字段 |
| --- | --- |
| `SystemFileInfo` | `path:string`、`size:int`、`isFile:bool`、`isDirectory:bool`、`modifiedMilliseconds:int`、`exists:bool`、`name:string`、`extension:string`、`parentPath:string`、`createdMilliseconds:int`、`accessedMilliseconds:int` |
| `FileSystemEntry` | `path:string`、`fullPath:string`、`name:string`、`extension:string`、`parent:Folder`、`fileInfo:SystemFileInfo` |
| `FileStream` | `path:string`、`mode:string`、`position:int`、`length:int`、`closed:bool` |

### File

| 方法 | 签名 | 细节 |
| --- | --- | --- |
| `open` | `open(mode: string = "r"): FileStream` | 支持 `r/r+/w/w+/a/a+/x/x+`，可带 `b` 别名。 |
| `create` | `create(recursively: bool = true): null` | 缺失时创建空文件；已存在普通文件保留内容，按参数决定是否创建父目录。 |
| `readText` / `readBytes` | `(): string` / `(): array` | 从头读取整个文件。 |
| `writeText` / `appendText` | `(text: string): int` | 按 C-string 覆盖/追加文本，首 NUL 截断且不验证 UTF-8，返回字节数。 |
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
// guard 成功 close 后，再次 close 幂等
```

`readBytes/readText(count = -1)` 从当前位置读取，`-1` 表示剩余全部；`write*` 返回实际写入
字节数；`flush` 刷新缓冲；`seek(offset, origin = "begin")` 返回新的绝对位置；`setLength`
截断或扩展文件。首次成功 `close` 后再次关闭可成功返回 null；宿主关闭返回失败时，
包装对象的私有句柄号与 `closed=false` 保持原值，后续仍可进入关闭路径；底层句柄是否已释放需按平台核验。读写模式不匹配、宿主长度/seek 操作失败和已关闭句柄的 I/O 保护路径
抛 `IOException`；参数 helper 的普通 false、缓冲或对象构造失败不一定设置异常，需按具体回调判断。

公共声明与注册表的逐单元契约见 [Fs 公共接口台账](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/coverage/zr_vm_lib_system_fs_public.tsv)。
现有 Fs 八例基线为 7 PASS / 1 FAIL，流模式用例在 `takeReader` 的 cached native callable 检查处提前失败；
其后的 read、setLength、append、using 等语句未执行。省略可选参数、appendText 和 flush 没有本批运行覆盖；
默认值来自回调实现，descriptor 只记录参数区间与类型。

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

`vm.loadedModules(): SystemLoadedModuleInfo[]` 遍历 global 的已加载模块实例缓存，再投影原生注册元数据；仅注册而尚未导入的 descriptor 不在列表中。sourceKind 由 moduleName/fullPath 文本相等与否推断，registrationKind 则另取注册来源。moduleVersion 是 descriptor 版本文本，runtimeAbiVersion 取 descriptor.minRuntimeAbi，零值或无 descriptor 时回退当前 runtime ABI，并非测量插件正在执行的 ABI。
`vm.state(): SystemVmState` 返回 loadedModuleCount、garbageCollectionMode、garbageCollectionDebt、garbageCollectionThreshold、stackDepth 和 frameDepth，组合已加载缓存计数、全局 GC 标量及当前 VM 线程状态。
`vm.callModuleExport(moduleName, exportName, args)` 将数组元素转发给 ZrLib_CallModuleExport；目标解析可包含该共享调用器的惰性导入路径。目标实际调用约束由共享调用器和 native dispatcher 处理。

`exception.registerUnhandledException(handler: (Error) -> bool): null` 将函数值复制到当前 VM global
的单一槽，后续注册覆盖旧值；GC 追踪该槽，session 仅在 handler 值及可达图均可恢复时保存/恢复它。此调用只登记函数。
当前 CLI/REPL 失败出口没有调用它，测试宿主才显式读取并同步分发；hint 的 bool 签名不规定生产宿主的
调用时机、重入或 handler 自身抛错策略。接入意图仍为 TODO，核查入口见文末 Exception 契约。

## C 注册和资源顺序

```c
const ZrLibModuleDescriptor *descriptor = ZrVmLibSystem_GetModuleDescriptor();
if (!ZrVmLibSystem_Register(global)) {
    /* read native-registry last error before changing global */
    return ZR_FALSE;
}
```

共享库入口是 `ZrVm_GetNativeModule_v1()`。宿主销毁 global 前应显式关闭仍打开的
`FileStream`；仅在隐藏 owner 字段成功发布时，finalizer 才能找到记录并尝试关闭句柄、释放记录。资源读取归档在单次调用内关闭；注册的
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
[Env 审查批次](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/batches/20261013-system-env-contracts.md)和
[Env 台账](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/coverage/zr_vm_lib_system_env.tsv)。


## Console 调用、资源与失败边界

`print` / `printLine` / `printError` / `printErrorLine` 注册元数均为 1，`read` / `readLine` 为 0；数量错误在 callback 前由 native dispatcher 抛 `ArityError`。非空 context 必须关联有效 VM state，result 必须是当前调用的可写槽；回调只同步借用它们。打印会转换第一个参数，可能执行用户 `toString` 并重入或抛出 VM 异常；异常发生后不继续日志提交。正常 true 仅表示日志 API 调用完成，返回值仍为 null，不保证操作系统写入或持久化成功。

日志默认 FILE sink 先按 channel 写 stdout/stderr，随后同步通知宿主 observer，print/printLine 传递 INFO/STDOUT/RESULT，printError/printErrorLine 传递 ERROR/STDERR/DIAGNOSTIC；observer 不取代默认输出。observer 的消息指针仅在本次同步通知内有效，需保留时自行复制。输入函数开始前只刷新默认 stdout/stderr，不触发 observer 的 flush。

`read` 按首字节候选长度消费一个 UTF-8 序列，严格 decoder 再验证完整码点；非法前缀、截断或非法编码会抛 VM 错误，已经消费的输入不回滚。首字节 EOF 返回 null。`readLine` 消费到 LF 或 EOF，删除 LF，并对两种结束都删除一个尾 CR；空行返回空字符串，未消费任何字节的 EOF 返回 null。输入来自进程共享 stdin，本模块没有覆盖整个码点/行的锁，宿主需要串行化并发读取；fgetc 可能无限等待。

**BUG:** 打印对显式字符串字节长度验证 UTF-8，但 `Log_Write` 默认 sink 用 strlen、换行路径以 `%s` 格式化；合法嵌入 NUL 会使其后文本从默认输出中消失。合法脚本入口为 lexer.c:262-275 的 `\x00` 转义，:51-78 的 save_char 保留零字节并增加显式长度，:302 把 bytes/length 交给 parser/parser_ownership.c:463-505 的 helper，后者在 :488 原样调用 String_Create。输出消费证据为 Console 共用 print helper、`value/value.c:319-363` 的转换路径及 `base/log.c:75-160` 的日志消费；非换行 observer 仍收到原 C 指针，不能据默认 sink 的截断推断宿主 observer 的全部处理行为。

**BUG:** 读取并分配 realloc buffer 后（剥除尾 CR 后的文本可为空），如果字符串尚需创建对象，`String_Create` 的 RawObject/GcMalloc 首次分配及 full-GC 后重试均失败会抛 MemoryError；活动 `TryRun` 的 C 异常入口执行 longjmp，跳过其后的 free(buffer)。realloc 失败和 UTF-8 校验失败均先释放 buffer，初始空行或行首 EOF 没有已分配 buffer，均不属于该触发条件。完整静态链见 `gc/gc_object.c:1069-1074`、`base/memory.c:39-47`、`exception/exception.c:543-574` 与 `exception_internal.h:18-21`；本轮不修复，不赋予 OOM 执行证据。

**BUG:** `read` 的新码点或 `readLine` 的新短行若尚未驻留，且 short-intern 表需要 HASH_BUCKET rehash，raw allocator 失败可让 String_Create 软返回 null 而不设置 VM 异常。SetStringObject 对 null 不写槽，callback 仍返回 true，dispatcher 交付预置 null，因而与正常 EOF 混淆。该分支见 `string/string.c:802-808`、`hash_set.h:525-537`、`base/hash_set.c:170-181`、`memory.h:19-22` 和 `native_binding_dispatch.c:362,1311-1316`；readLine 在此正常返回路径仍释放 C buffer，与硬异常泄漏分开。

**TODO:** `ferror` 尚未区分：首字节流错误与 EOF 返回同样 null，中途 read 错误表现为截断 UTF-8，readLine 的部分输入遇到错误可照常交付；须明确并测试 I/O 错误策略。末尾孤立 CR 加 EOF 的裁剪是否符合行契约也须核实。descriptor 默认 GC_AWARE 模式下，阻塞 fgetc/同步日志仍持有 RUNNING mutator；须从 `native_binding_dispatch_lanes.h:240-278` 和 `gc_domain_mutator.c:630-693` 检查 STW 等待与 native 模式策略，现有测试没有该时序覆盖。

System 的现有 module 测试检查 console 导出/hint，provider 测试检查官方归属；它们没有执行 Console 读输入或验证边界。CLI suite 的 `repl_native_import` 用例（`tests/cmake/run_cli_suite.cmake:1130-1149`）调用 print("xxx") 并断言 stdout 含 xxx 且 stderr 为空，其他项目 marker 断言只能提供整体流程信用。完整逐单元审查见 [Console 台账](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/coverage/zr_vm_lib_system_console.tsv)；本节不声称本批已执行整套 CLI 或 OOM/UTF-8/线程故障测试。


## GC 调用、统计与失败边界（Statistics）

这六个 callback 使用当前 state 所属 global 的共享 collector；多个 state 可以共享它。GcDomain 指标是统计内容，不代表每次调用拥有独立 collector。context、state、collector 与 result 都只借用至本次同步调用结束；宿主须协调同一 global 的并发控制和读取。注册元数分别为 enable/disable/get_stats 0、collect 0..1、set_heap_limit/set_budget 1，数量错误在 callback 前抛 ArityError。

`enable` 设置 generational 模式并清除 stop flag；`disable` 只设置 stop flag，保留模式，不终止当前 step。Core 仅在部分推进循环检查该标志，generational safe-point 分支仍可执行回收；enabled 字段只表示 `!stopGcFlag`，不证明后台线程存在、回收正在运行或所有自动推进均已停止。此接口的禁用范围仍需从 `CheckGc`、`GcStep` 与 generational step 明确设计意图和断言。

`collect` 省略参数时走 full。minor/major 安排对应请求后调用一次 GcStep，成功返回 null 不保证整个周期完成。full 调用同步 GcFull 入口，但 Core 在 StopTheWorldBegin 超时后会 void 早退，callback 无完成状态可检查；成功 null 同样不是完成证明。kind 参数错误标签由 ReadString 抛 TypeError，未知文本正常 false 可被 FINE dispatcher 折叠为预置 null。**BUG:** 合法脚本字符串如 `"full\x00tail"` 在 lexer 的显式长度路径保留 NUL，ReadString 的 C 指针随后由 strcmp 比较，后缀被忽略；接口约定的精确 kind 被当作前缀接受。这是完整静态路径结论，未执行 NUL 测试。

两个 setter 共用 ReadInt 后检查转换结果非负；可靠输入是非负且可表示的 int64。有限浮点先向零截断，`-0.5` 可变成 0；大 UINT64 转 int64 依赖实现。**BUG:** 当前共享 Reader 对 NaN、无穷或超 int64 范围的 FLOAT/DOUBLE 直接转换，触发 C 未定义行为，GC 注册入口可达该既有共享缺陷。**TODO:** heap limit 再转 ptrdiff_t，pause budget 在 Core 中转 TZrSize，需核实支持的 32 位配置与越界策略。heap limit 0 清除软阈值；set_budget 同时写 pause/remark 配置，0 在 Core 中仍将 gcPauseBudget 设置为 1。预算不构成整个 collection、最终 remark、sweep 或 finalizer 的时间上限。

`get_stats` 先取得无指针的栈快照，再分配 VM 管理的 SystemGcStats 对象并逐个写入 42 个字段；enabled 在对象分配后单独读取。它没有发布栈指针，但字段并非同一瞬间的原子快照，对象分配还可能触发 GC。Core getter 对域指标局部加锁，压力汇总仍有 regions 数组并发访问的既有缺陷；当前入口不应被当作任意线程安全的宿主快照 API。返回对象按普通 VM 根与有效期规则持有，调用者不手动 free。

**BUG:** 42 次字段写入使用 void setter，三条软失败路径均可留下缺字段对象：新实例的初始 nodeMap 桶申请失败，首个 setter 内重试初始化仍失败时，只记录日志并跳过 enabled；ignored-root pin 表扩容的 ARRAY RawMalloc 失败会跳过对应字段；后续整数项使 map 跨负载阈值时，HASH_BUCKET rehash 分配失败也会让 Object_SetValue 静默返回。首字段初始化失败与后续扩容分别记录，不将 enabled 的正常首写归于负载阈值。投影 helper 不检测这些状态，GetStats 仍把非空对象作为成功结果。证据涵盖 native instance helper、Object_Init/SetValue、HashSet_Init/Rehash、dispatcher pin 和成功返回链；没有故障注入信用。抛出 VM 异常的分配失败属于另一条传播路径，不归为正常成功。

现有两项 module 测试中，`test_system_gc_safe_controls_report_runtime_stats` 在单 VM state 中核对控制参数及选定计数，`test_system_native_types_register_complete_struct_fields` 核对字段元数据。Root 修改前运行整 module target 78 项，相关两项及 Vector3 构造均 PASS；另两项既有失败保留。另有 `tests/gc/gc_tests.c` 的 `test_system_gc_enable_callback_switches_generational_mode` 直接调用 Enable/Disable，断言模式及 stop flag，不经 dispatcher；本批尚未运行该直接 C 测试。上述测试未覆盖非法 numeric/kind、disable 后自动推进、allocator 软失败或并发快照。完整逐单元依据见 [GC 台账](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/coverage/zr_vm_lib_system_gc.tsv) 与 [GC 批次](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/batches/20261014-system-gc-contracts.md)。

## VM 快照与跨层调用契约

loadedModules 按 loadedModulesRegistry 的哈希桶和链遍历，不保证名称排序，也不扫描所有注册 descriptor。记录字段包括 name、sourceKind、sourcePath、registrationKind、hasTypeHints、moduleVersion、runtimeAbiVersion、requiredCapabilities、isDescriptorPlugin；对应文本为空时可按实现省略字段。返回数组和记录对象由 VM 管理，descriptor/plugin 文本由注册来源借用；需沿 root/搬迁协议核查分配期间 registry/pair、文本及尚未发布对象的有效性，不能由局部 C 指针推定稳定。

state 先统计 loadedModuleCount，再创建对象并逐字段读取 GC 和线程状态；对象及字段名分配可能推进 GC，这些值不是同一时刻的原子快照。garbageCollectionMode 只反映 gcMode；gc.disable() 只改 stopGC，不能用 mode 字段推断自动 GC 是否启用。stackDepth/frameDepth 是当前调用时的 VM 状态，不是全部线程或 C 栈的深度。

**TODO:** 同 global 可由 State_New/State_MutatorLaunch 建立多个 mutator，Module_AddToCache 会修改 loadedModulesRegistry；需沿 runtime/state.c:61-70,173-182、module/module.c:300-316 与 gc_domain_mutator.c 的受支持协议核实 loadedModules() 和 state() 的缓存遍历是否要求外层串行化。state() 还读取 gc.h:177,199,204 的普通 collector 字段；沿 gc.c:658-671 等写路径核查同步。跨字段非原子与读写受保护不是同一保证；当前只记录待核边界，不断言已证数据竞争。

**BUG:** 新快照对象的 Object_Init 初次建 nodeMap 失败后，void 字段 setter 虽会重试建表，仍可在合法的再次分配失败时跳过字段；已有容量 4、阈值 3 的表也可在第四个实际字段插入需要 HASH_BUCKET 扩容时失败。ignored-root ARRAY pin 扩容失败是另一条软失败路径。helper 不返回字段写入状态，State/LoadedModules 仍可能成功发布不完整对象；不把抛 VM 异常的分配路径混入这些结论。

**BUG:** LoadedModules 的字符串字段 helper 使用未初始化的 fieldValue；若待写值是未驻留短串，字符串驻留表跨阈值时的 HASH_BUCKET Memory_Allocate 失败可让创建返回 NULL，SetStringObject 随即不写槽。helper 仍把 fieldValue 交给对象字段 setter，后者可读取未初始化值。合法 descriptor 的 moduleVersion 或固定 sourceKind 文本可走到该路径；没有故障注入执行信用。

**BUG:** LoadedModules 忽略 Array_PushValue 的 false 返回。数组或 entry 的 ignored-root ARRAY 扩容失败可让追加正常失败，但循环仍继续，最终成功返回缺少条目的数组；它与记录内字段遗漏分别登记。

callModuleExport 借用两个 VM 字符串的原生文本，以 malloc 申请只保存 SZrTypeValue 浅拷贝的连续参数区；空数组保持 NULL。逐项读取失败、共享调用器正常返回后都 free 该区，不转移 VM 对象所有权。**TODO:** 共享调用器在本地恢复点前可能进行目标查找/惰性导入；需沿 ZrLib_CallModuleExport、Module_GetExport、CallValue 的实际 GC root 与异常跳转核实名称、浅拷贝对象及 malloc 区的有效期和回收边界。目前不据此断言已证泄漏或搬迁缺陷。

现有 nested-native 模块测试先注册 math，再直接调用导出，随后脚本转发 sqrt；断言只比较非空结果及类型一致，不验证数值 2。另一个 reference member-chain 用例实际编译执行 native_root_member_chain_pass.zr，调用 state() 并将 loadedModuleCount>0 转成返回1、断言1，r1该用例PASS；它仅支持正常单state计数投影。callbacks/probe项目脚本也会调用 loadedModules/state，但本批未运行 project suite；probe只断言阶段标记，Parser/LSP内同名源码仅供静态分析。上述范围不证明全部状态字段、惰性导入、GC压力、并发同步或分配故障。逐项定义、调用和证据见 [VM 台账](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/coverage/zr_vm_lib_system_vm.tsv) 与 [VM 批次](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/batches/20261014-system-vm-contracts.md)。

本批与 Vector4 共用 `system-vm-vector4-final-r1-results.json` 的实际验证：四个实现对象强制重建、构建成功；模块测试 78 项为 70 PASS / 8 FAIL，全部 78 行与最近 r3 基线精确一致，provider 9 项全部 PASS。相对更早 76 PASS / 2 FAIL 的记录，六项二进制元数据失败仍保留，本批未新增失败。当前输入与 r3 有三个并发 core/Parser 文件差异；保守扩展的 2950 个输入在构建及两个 suite 期间稳定，八个冻结源码未变化。Parser 的两条 C4701 和其他配置测试的 CMAKE_OBJECT_PATH_MAX 警告另行记录，所选 target 构建成功；不据此授予 State/LoadedModules 根对象协议、并发或分配失败测试信用，也不声称整个 suite 通过。

最终共享 global 约束注释使用四源 strict proof `system-vm-final-r2-comment-equivalence.json`，最终原生验证为 `system-vm-clang-final-r3e-results.json`：clang-cl/原生 ml64、Debug/static 配置经 cache guard 核对，构建成功并实际编译两个 VM 实现。模块测试 70/78 PASS、8 FAIL，与此前 r1 的全部 case 行一致；provider 9/9 PASS，reference member-chain 的 State/count 正常路径再次 PASS。vm_registry.c 的四条缺省 initializer 警告和其他模块/工具参数警告保留；不称全测试通过或无 warning。MSVC r2 的编译器缺失、r4 的 C1902 失败及未重建 object 另有记录。

最终验证期间 2950 个保守编译/配置输入及四个 VM 源码稳定，State 引用样例另核对单文件前后哈希；这些范围不涵盖全部运行时样例、生成输入或外部 SDK/toolchain。与 r1 之间有其他会话的 Core/Parser 改动且工具链不同，不能作隔离的行为归因。完整逐项比较见 `system-vm-clang-final-r3e-comparison.json`；没有执行分配故障、并发、GC 压力或惰性导入异常专项。

## Exception 构造与宿主分派

Error 及六个派生异常类共用 0..1 参数的 native 构造 callback。常规反射构造先保活新 receiver，
callback 将 `stacks` 初始化为空数组、把消息写入 `message`，并让 `exception` 指向 receiver；
callback 自身返回 null，反射调用者交付构造实例。省略消息写 VM null；显式消息经字符串转换协议处理，
可能执行用户代码并传播 VM 异常。抛出与规范化阶段维护诊断栈，不能把这里的空数组等同于已捕获 traceback。

**BUG:** 若进入反射构造前 ignored-root count 恰为 capacity-1，receiver pin 会占满表；
新建的 stacks 数组未先 pin。其字段 setter 对数组值做 ignored-root 扩容时，单次 `RawMallocWithType(ARRAY)`
软失败会被 void setter 吞掉；在其他分配、转换及 VM 状态均成功的限定路径上，反射构造仍会交付缺少
`stacks` 的实例。这是闭合静态调用链的结论，未做分配失败注入，不代表一般 OOM 路径都如此。

注册 handler 的 global 槽由 GC 标记与转发；可恢复 VM 函数/闭包可随 checkpoint 保存和恢复，原生指针/native closure 使 capture 失败。这不构成跨线程调用保证。
当前 CLI 的 `zr_cli_runtime_handle_failure` 与 REPL 的 `zr_cli_repl_session_handle_failure` 记录异常后重置状态；
测试宿主 `zr_tests_runtime_try_invoke_unhandled_handler` 才负责实际调用，并把 bool true 解释为消费异常。
**TODO:** 沿这两个生产失败出口核实 API 是否仅服务嵌入宿主，或 CLI/REPL 也应分派；同时确定重入及 handler 失败策略。
完整定义、绑定、间接入口、测试证据和逐单元结论见 [Exception 台账](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/coverage/zr_vm_lib_system_exception.tsv)
及 [审查批次](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/batches/20261014-system-exception-contracts.md)。

注册读取器还接受 VM closure 和原生函数指针，不校验 hint 的 `(Error) -> bool` 签名或原生地址。
宿主调用原生指针前必须保证非空入口满足 `FZrNativeFunction` ABI；原生指针和 native closure 所指向的代码及关联资源均须在 global 槽
仍可能引用它时保持有效；GC 不会延长原生代码映像的寿命。checkpoint 只接收满足可恢复条件的 VM 函数/闭包及对象图；NATIVE_POINTER 或 native closure 会使 capture 失败。


## Fs 公共对象、字段与资源边界

Fs 公共 helper 在当前 native 调用中借用 context、state、对象、字段槽和字符串视图。字段 getter 不延长 VM 对象的生命周期；调用者需要遵守实际 native lane/root 协议。MakeInfoObject 和 PopulateEntryObject 用临时根保护正在物化的对象，并在相关分配后重取对象；当前普通 OBJECT 的原址晋升路径已核查，不能把所有裸对象指针一概认定会因该路径搬迁。

字段 setter 是 void：返回到调用者不证明字段存在或已经更新。RefreshEntryObject 的 true 表示宿主查询和本地物化流程完成，不能据此保证 fileInfo 字段已替换；返回的查询快照与对象仍持有的字段可能不同。ReadOptionalBoolArgument 的错误标签是普通 false，并保留已写入的默认值；ReadString/ReadInt 的错误标签由共享 reader 的 VM 异常路径处理，两类失败不能互换。

**BUG:** WriteStringField 与异常 message helper 的局部 fieldValue 未初始化。若待写短串未驻留、驻留表跨扩容阈值，且受控 allocator 仅让这一次 HASH_BUCKET 申请返回 NULL、其余分配成功，字符串创建可软返回 NULL，void SetString 不写槽；随后对象 setter 的 pin lane 读取未初始化的值。该结论有完整静态调用链，没有分配故障注入信用，也不概括所有 OOM 路径。

**BUG:** FileStream 的 native owner 通过隐藏字段关联。限定隐藏 owner 键尚未驻留且其 HASH_BUCKET 扩容软失败，或相应 ignored-root ARRAY pin 扩容软失败，其余分配成功时，void setter 可跳过关联，而 NewStreamObject 仍交付非空 wrapper。File_Open 只在构造结果为 NULL 时关闭原句柄；finalizer 无隐藏 owner 时找不到 malloc 记录及句柄，因而无法释放。具体实施函数、间接入口与资源边界见 [Stream 逐单元台账](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/coverage/zr_vm_lib_system_fs_stream.tsv)。

StreamCloseInternal 由显式 close callback 调用；using 经注册的关闭入口进入。GC finalizer 独立尝试关闭并释放 native 记录，没有调用 StreamCloseInternal，也不向脚本报告 close 错误。显式 close 失败保留私有 closed=false；finalizer 忽略 close 结果后仍置 invalid/closed 并释放记录，字段不能证明宿主关闭成功，也不能承诺失败后可安全重试。

**TODO:** PopulateEntryObject 的 originalPath/fullPathOverride 可能借用 VM 字符串，并跨 MakeInfoObject 和递归 parent 构造分配后再次使用。需沿实际 caller 的参数根、native lane 与字符串存储稳定性核查这一窗口；目前不宣称已观察悬空。未检索到仓内调用的 GetBoolField 也只登记用途待核，不能直接认定死代码。

定义、实际 consumer、声明与实现范围、逐项测试边界和证据分别见 [Fs 公共支撑台账](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/coverage/zr_vm_lib_system_fs_common.tsv) 与 [批次记录](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/batches/20261014-system-fs-common-contracts.md)。此处没有赋予 allocator、GC、并发或异常重入的专项执行信用。

**BUG:** ReadTextFromHandle 在短串驻留表 HASH_BUCKET 扩容软失败时，void SetString 可不写结果，helper 仍报告成功；正常 dispatcher 预置的 null 在 Stream 后续快照查询成功时到达脚本。module/File readText 经临时句柄 helper，关闭成功且回调正常返回时亦可 true/null；File 刷新普通 false 被忽略，非局部异常则不形成 true 返回。限定未驻留短文本、一次 bucket 软失败及其余步骤正常返回，完整路径见 Stream 台账；与首 NUL 截断不同，均无故障注入信用。

**TODO:** stream 调用 RaiseErrnoIOException 前先求值路径 getter，需核查键物化/分配是否覆盖原 errno。**BUG:** ReadBytes/ReadText 已 malloc 的 bytes/text、File_Open 已开且尚未包装的句柄跨 VM 分配；该分配异常非局部退出会越过 C free/CloseHandle。Exception/TryRun 仅恢复 VM roots/scopes，不清理放弃的 C 栈资源。普通 false 的本地清理已核实；静态链闭合，无故障注入。


## FileStream 同步操作与快照

readText/readBytes 的 count 按字节计，0 不消费输入，所有负值目前都读取至 EOF；默认值 -1 并不是唯一接受的负数。文本发布/写入用 C-string，NUL 与 UTF-8 边界见前文。Stream callback 不保留 context/result，成功交付的 VM 数组/字符串由 VM 管理，临时 malloc 缓冲在普通返回路径释放；相关 VM 分配异常非局部退出会越过 free，按上述窄 BUG 记录；普通 false 仍正常释放。

NewStreamObject 在 initial StreamSyncFields 返回 false 时仍交付 wrapper，位置/长度查询失败的未知快照政策为 TODO。后续读写/seek/resize先执行宿主操作再同步；同步查询失败抛 IOException，不回滚已发生的副作用。setLength 忽略移动超尾游标的 seek 失败，因此返回成功不保证游标已经移到新长度。公开字段 setter 是 void，查询成功也不保证各字段均写入。

显式 close 的成功路径幂等；宿主错误后的句柄状态及重试安全性待平台核验。完整周期和增量 sweep 均能经普通 OBJECT 释放调用 finalizer；wait-to-release 列表尚无已证入队入口，不作为额外调用信用。缺陷、TODO 与现有测试实际到达范围见 [Stream 台账](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/coverage/zr_vm_lib_system_fs_stream.tsv) 和 [批次记录](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/batches/20261015-system-fs-stream-contracts.md)。

## File、Folder 一次性操作的调用边界

File.create 对已存在普通文件保留内容，缺失时才创建空文件。File copy/move 完成宿主操作后尝试刷新源receiver，普通refresh false被忽略；File.moveTo原receiver仍指旧路径。Folder copy/move不刷新原receiver。目标wrapper构造失败不回滚副本或移动。必需string/array缺参或错型分别沿ArityError/TypeError退出；可选bool错型是普通false，不能统一解释为IOException。

**BUG:** 一次性ReadBytes/ReadText已取得缓冲与临时handle后，相关VM分配异常非局部退出越过free及close；WriteBytes另限定合法非空RAW_CANONICAL数组物化需要新pairpool，分配异常退出时同样跳过这些清理。WriteText helper没有该VM分配路径。目录枚举成功后，在条目/数组VM物化中非局部退出还会跳过File_List_Free；平台普通失败的部分列表清理仍执行。Exception/TryRun只恢复VM roots/scopes，不回收这些C局部资源。

CurrentDirectory的限定短串HASH_BUCKET软失败可交付true/null；GetInfo、构造/刷新及目标对象字段写入受共享WriteStringField未初始化值BUG影响，结果未定义，不保证只是缺字段。上述均是静态调用链证据，本批没有allocator故障注入、GC压力、NUL或并发专项执行。参数保活、字符串存储有效期、File.open构造失败后close错误仍有具体TODO；Glob省略recursively的默认已由当前helper调用路径确认true。逐函数定义、全部调用坐标、证据与结论见[FsEntry台账](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/coverage/zr_vm_lib_system_fs_entry.tsv)和[批次记录](https://github.com/He-Jiahui/zr_vm/blob/main/docs/code-review/batches/20261015-system-fs-entry-contracts.md)。

写入、创建、复制、移动和递归删除都不承诺事务回滚。短写后报错可能已写出前缀；File.writeBytes虽先完整校验元素才写字节，却先以w打开，非法元素也可能已截断旧文件（appendBytes不追加非法内容，但可先创建缺失文件）。递归创建失败保留已建父目录，File创建后的close失败保留新文件；递归删除失败不恢复已删子项。overwrite复制/移动可先删除旧目标，随后失败留下部分目标；移动的复制后删源回退若删除失败，也可能两边仍有内容。这些是现有失败状态契约，本批没有故障注入或专项执行。

File.open的w/a/x在宿主open成功时已可截断或创建文件，后续append seek失败或wrapper构造失败也不恢复文件内容；普通wrapper失败只尝试关闭句柄。Folder.glob省略recursively时确定默认true，现有显式true测试不提供省略参数的专项运行覆盖。

复制的普通文件分支会忽略close返回值，成功不保证关闭成功；overwrite可先删除旧目标，失败时目标可能仍不存在或只留下已建父目录。BUG：File/Folder.moveTo覆盖同一现存路径会先删源，再以IOException失败；Folder.copyTo把目录复制到自身子目录会不断纳入新建目标，moveTo在rename失败后的复制回退也可触发。调用时避开这些路径组合；本轮仅记录静态已证缺陷，未改行为，也未运行这两类破坏性场景。

路径重叠的另一已证BUG：overwrite=true且目标是源的现存祖先目录，Copy/Move会先递归删除该祖先树，连源及兄弟项一起删除，再报IOException；目录Copy还可能留下重建的空目标。File/Folder包装名称不校验实际源类型，实际源为目录的File.copyTo也可能触发前述子目录递归；moveTo在rename失败后的复制回退适用。上述结论按NormalizePath实际输出的组件关系说明，不承诺文件系统别名或并发操作安全。
