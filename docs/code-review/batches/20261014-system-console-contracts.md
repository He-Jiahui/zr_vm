---
related_code:
  - zr_vm_lib_system/src/zr_vm_lib_system/module.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_metadata.c
  - zr_vm_core/src/zr_vm_core/base/log.c
  - zr_vm_core/src/zr_vm_core/string/string.c
  - zr_vm_core/src/zr_vm_core/base/hash_set.c
  - zr_vm_core/src/zr_vm_core/base/memory.c
  - zr_vm_core/src/zr_vm_core/exception/exception.c
  - zr_vm_core/src/zr_vm_core/exception/exception_internal.h
  - zr_vm_cli/src/zr_vm_cli/runtime/runtime.c
  - zr_vm_cli/src/zr_vm_cli/repl/repl_session.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_import_metadata.c
  - zr_vm_parser/src/zr_vm_parser/syntax/lexer.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_ownership.c
  - zr_vm_language_server/src/zr_vm_language_server/metadata/lsp_native_declaration_projection.c
implementation_files:
  - zr_vm_lib_system/include/zr_vm_lib_system/console.h
  - zr_vm_lib_system/include/zr_vm_lib_system/console_registry.h
  - zr_vm_lib_system/src/zr_vm_lib_system/console/console.c
  - zr_vm_lib_system/src/zr_vm_lib_system/console/console_registry.c
plan_sources:
  - docs/code-review/comment-standard.md
  - user: 2026-10-13 System Console contract review assignment
tests:
  - tests/cmake/run_cli_suite.cmake
  - tests/cmake/run_projects_suite.cmake
  - tests/module/test_module_system.c
  - tests/library/test_official_provider_convergence.c
  - tests/debug/test_debug_step_edges.c
  - tests/fixtures/projects/native_numeric_pipeline/src/main.zr
  - tests/fixtures/projects/native_numeric_pipeline/src/callbacks.zr
  - tests/fixtures/projects/classes_properties/src/main.zr
doc_type: workflow-detail
---

# System Console 契约审查

## 范围与单元清点

本批只覆盖 Console 两个公开头文件及 console.c、console_registry.c，并新增逐单元台账 zr_vm_lib_system_console.tsv。台账共 35 项：

- console.h：include guard 和六个 callback 声明，共 7 项；六个公开声明均有 Summary、Parameters、Returns、Constraints and lifecycle、References 五节。
- console_registry.h：include guard 和静态 descriptor getter，共 2 项。
- console.c：9 个函数（两个 UTF-8 helper、共享 print helper、四个输出 callback、Read、ReadLine）及 11 个语义块，共 20 项。
- console_registry.c：条件数组计数 fallback、getter、functions 表、type hints 表、JSON hints 和模块 descriptor，共 6 项。

源码中没有改函数签名、控制流或数据；只增补文档注释。Console 之外的 dispatcher、Core logger/string/hash/exception、CLI、parser、LSP、共享测试和旧 System 总表不属于本批写入范围。

## 注册、参数与静态资源

System 聚合模块在 module.c:82 直接取得 Console descriptor；注册插件按 descriptor.functions/functionCount 注册各函数，native dispatch lane 读取函数 callback 指针并在 native_binding_dispatch_lanes.h:277 调用。kFunctions 对 print、printLine、printError、printErrorLine 固定 1..1，对 read、readLine 固定 0..0。dispatcher 在 native_binding_dispatch.c:364 先做 auto_check_arity，经 CheckArity 到 RaiseArityError 的 noreturn 路径，再进入 callback。因此正常注册调用的元数错误不是 callback 的 ZR_FALSE；输出 callback 的缺失 context argument 仅是绕过正常注册入口的防御情况。null context/result 防御分支也不能推导 state 任意为空都受支持，公开契约要求有效 state 和可写 result。

getter 返回函数表、提示表、JSON 及字符串均为静态描述符里的浅引用。注册器可能长期保存 callback 和嵌套指针；provider image 必须驻留至这些借用全部结束，浅拷贝 descriptor 不延长 image 生命周期。函数 descriptor 的尾部 dispatchFlags 未显式初始化，C 静态初始化置零并映射 GC_AWARE。阻塞 fgetc 和同步宿主日志 observer 期间可能延迟并发全局 GC pause；台账把 safepoint 生命周期与可安全拆分 VM 访问/阻塞阶段列为 TODO，没有在缺少并发复测时把它升级为 BUG，也没有建议直接把整 callback 标记为 BLOCKING_DETACHED。

console_registry.c 的本地 ZR_ARRAY_COUNT fallback 不是实际产品构建中的活动实现：真实 include 链从 console_registry.c:5，经 console_registry.h:8、console.h:8、System conf、zr_vm_library.h/library conf 到 zr_vm_common.h:11，先包含 zr_array_conf.h:13 的共享定义；因此本地 #ifndef 分支跳过。:48 和 :52 的数组计数展开使用共享宏，不列为本地 fallback caller。

## 输出与输入契约及缺陷边界

四个输出 callback 共用 system_console_print。它通过 Value_ConvertToString 转换第一个参数，可能查找并执行用户 TO_STRING 协议，故可重入并可能在写日志前抛 VM 异常；转换结果按显式 byte length 验证 UTF-8。日志交付顺序是默认 stdout/stderr FILE sink 先写，然后同步通知宿主 log observer；print/printLine 的 observer 元数据为 STDOUT channel、INFO level、RESULT kind；printError/printErrorLine 为 STDERR、ERROR、DIAGNOSTIC（console.c:78-88）。message 只在本次同步回调期间借用。默认 FILE 输出不会被 observer 替代。Log_Write/Log_Printf 不返回 FILE 或 observer 的写入状态，所以 callback 成功并把 result 置 null 只表示日志 API 调用完成，不表示输出已成功消费。读取输入前的 FlushDefaultSinks 只刷新默认 stdout/stderr，不会刷新 observer。

嵌入 NUL 是已证实的输出缺陷，且有合法脚本入口：lexer 接受字符串中的 \xXX 十六进制转义（lexer.c:262-275），save_char 将 byte 0 存入缓冲并仍增加显式 bufferLength（:51-78），read_string 把 buffer 与该长度传给 ZrParser_Lexer_CreateString（:302），实际 helper 在 parser/parser_ownership.c:463-505 接受显式 length，并于 :488 将 bytes/length 原样交给 ZrCore_String_Create；在有效 ownership/budget 下成功保存并返回字符串。因此 VM 字符串可含有效 UTF-8 NUL。Console 对显式长度完成 UTF-8 验证后把 native 指针交给日志；Log_Write 默认 sink 用 strlen，换行格式化路径用 %s，所以首次 NUL 后文本不可见，line callback 的换行接在可见前缀后。当前测试没有 NUL 输出断言。

Read 先刷新默认 sinks，再逐字节读取一个候选 UTF-8 序列。首字节 helper 只估计长度，DecodeCodePoint 才严格解码；EOF/流错误在首字节位置交付 null，在 continuation 位置会留下已消费前缀并抛运行时异常，输入不会回滚。ReadLine 逐字节增长 realloc 缓冲直到 LF/EOF；只有行首 EOF 且无内容返回 null，LF 空行创建空串，已有内容后 EOF 返回 partial line。实现没有检查 ferror，因此流错误与 EOF 共用路径。去尾逻辑在 LF 与 EOF 结束时都会删掉一个末尾 CR，EOF 前单独的 CR 也被删；ferror 和 EOF 终止 CR 的预期仍是具体 TODO。多个 VM state 可从不同宿主线程操作同一 stdin，Console 没有包住整码点或整行的互斥；调用方如要求读取原子性须自行串行化。

Read/ReadLine 的短串 soft-OOM 与 ReadLine 的硬异常泄漏是两条不同机制的风险。其一，Read/ReadLine 创建短串时若驻留表扩容的 raw bucket 分配失败，string_create_short 回 null 且不抛 VM 异常；SetStringObject 不写入空对象，而结果槽此前已初始化为 null，callback 仍返回 true。合法码点或短行于是可被成功报告为 null，表现得像 EOF。该 soft-OOM 与第二种硬对象分配失败不同。

泄漏只在读取至少一个非 LF 输入字节并由 realloc 成功取得非空 buffer 后，String_Create 中 RawObject/GcMalloc 首次分配与 full-GC 重试都失败并抛 MemoryError 时成立；即使尾 CR 剥除后有效长度变为零，buffer 仍可能非空。非局部异常跳过紧随其后的 free(buffer)。CLI runtime execute 与 REPL session 均使用 Exception_TryRun；exception restore 路径经 C longjmp 退出，因此存在合法的宿主恢复点路径。realloc 失败分支在 RunError 前先 free，invalid UTF-8 分支也先 free；空行/行首 EOF 没有非空缓冲。这些都不属于已标注的泄漏条件。本批没有内存故障注入，也没有改异常或字符串实现。

## 测试与证据边界

tests/cmake/run_cli_suite.cmake:527-559 的 inline code 用例实际调用 system.console.printLine，并只断言合并 CLI 输出包含参数 marker；:1133-1149 的 repl_native_import 输入调用 s.console.print("xxx")，分别断言 stdout 含 xxx、stderr 为空。这是有限的输出集成证据，不验证精确换行字节、stderr callback 隔离、custom observer、sink 错误、NUL 或无效 UTF-8。Project fixture classes_properties/src/main.zr:43-44 也调用 print，native_numeric_pipeline/src/main.zr:12 调用 printLine、callbacks.zr:29 调用 printErrorLine，关联注册用例见 run_projects_suite.cmake:488、:506-508；整体项目断言不等价于逐接口 I/O 行为断言。test_debug_step_edges.c:430 执行 printLine，但断言的是 step metadata。tests/library/test_official_provider_convergence.c:49 只核 provider 归属。

test_system_leaf_modules_expose_new_api_and_owned_types 在 test_module_system.c:3128-3282 导入 Console module 并检查导出名称、ModuleInfo type hints 及签名，不执行 callback。该共享 test function 的 canonical coverage key 已由 Process 叶表维护，本批只引用其证据，不重复新增测试单元。仓内测试和 fixture 搜索没有找到 system.console.read() 或 readLine() 的执行；也没有 ferror、CR+EOF 策略、码点字节交错、无效 stdin UTF-8、allocation failure、短串 bucket rehash OOM 或嵌入 NUL 输出行为测试。

## 修改前基线与验收边界

以下四个修改前 SHA 来自 Root 固定的 reports/first-party-review-01a13463/system-env-process-r1-source-pins.json；冻结后的工作树 SHA 仅包含本批注释改动：

| 文件 | 修改前 SHA256 | 冻结工作树 SHA256 |
| --- | --- | --- |
| zr_vm_lib_system/include/zr_vm_lib_system/console.h | a1ef30f7cacd8263783ec281934bfc68ac7a02ec60a07760f0698312f514ed0b | 31d169fc2a8c3b85af37afd21165162f63676b7f25ed3f79bc790cb27c60c11c |
| zr_vm_lib_system/include/zr_vm_lib_system/console_registry.h | 5ac104a40a958921b281f75a8682010c372b52613b0f6a9410513608e7bf9f76 | 0b9197ecd295b8b59c0aa148d2e437214b3b42a9e226f1850555753cb8e4ffd6 |
| zr_vm_lib_system/src/zr_vm_lib_system/console/console.c | 687d5d5d8f70a8747e039676228aedf46e2014a61f381528c3348e3f7d8269f6 | d94e36d6895207c95df0f2cae90cf5141e772daa26a42b56f2bc417ce65cd23e |
| zr_vm_lib_system/src/zr_vm_lib_system/console/console_registry.c | 565a50d530dccafcd714743cdb516d0153912e3a509a82bc469896f096213fa1 | fad6d14c51f78ac14de14c4ab58db4d5072f655432f2e11ca88234fe9ed66685 |

修改前同树 System static Debug 构建及 official-provider 9/9 结果由 Root 提供，记录于 reports/first-party-review-01a13463/leaf-libraries-final-r2-results.json；producer 本轮没有构建或运行测试。Root 完成的修改后验证记录于同目录：vector2-console-r1-comment-equivalence.json（SHA256 1fe1be435833684e9c4c3123331b9b616ca75c4de96d4a37588f089ce0373cc4）证明8个源码文件相对修改前基线 non-comment token kind/spelling/StartOfLine 严格等价；35个共享源码、7个测试/fixture和41个CLI源码 pins 前后稳定。实际 checkout 的 MSVC Debug/static、BUILD_CLI=ON 四目标构建退出码0，用时22.214秒；official-provider 9/9通过、用时0.805秒且逐case与System修改前相同。构建记录了7条既有CLI warning。Root另按 run_cli_suite.cmake:1134 中的输入字节在内存重放现有 REPL 用例，记录于 cli-enable-repl-native-import-results.json：进程退出码0、0.582秒，stdout包含xxx且stderr为空，exe/source pins稳定、临时目录前后均为0个文件。该执行只重放一个既有断言，不是完整CLI suite，也没有Console修改前CLI行为运行基线。本批没有read/ReadLine、NUL、OOM或GC行为测试信用，也没有新增测试/fixture。coverage check-batch 退出码0；它验证九列格式、重复键和锚点存在，逐条调用方向与语义由本审查按源码核对。

## 本批归属与状态

新叶台账 35 项只维护 Console 四源的声明、实现、语句块与注册配置；它不改共享 inventory、旧 System 整模块覆盖表、wiki、tests 或 provider fixture。Root 已从旧 System 总表迁出本叶覆盖的25个 Console 原始行，旧表现保留335个原始行；Root也已更新 docs/wiki/03-modules/system-api.md 的 EOF/CR、缺陷及测试边界说明。这些是Root拥有的旁路文档维护，不计作本producer写入。独立 reviewer 已核对四源、35项台账、注册/调用方向及全部缺陷静态链，并批准该冻结批次。Root 另按严格 token 行号映射更新原先干净的 UTF-8 台账中4处 Console 调用锚；其余文字与顺序保持不变。native 总台账的 SetStringObject caller 旧 :138 应映射为 :165，但该表有并发既有改动，本批只记录延期，不写入或提交。BUG/TODO 标签只覆盖本文所列具体路径；scanner计数、provider清单结果、导出/hint断言均不作为完整 I/O 契约完成的证据。