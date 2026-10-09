---
related_code:
  - zr_vm_lib_system/include/zr_vm_lib_system/assembly.h
  - zr_vm_lib_system/include/zr_vm_lib_system/assembly_registry.h
  - zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c
  - zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly_registry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/module.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c
  - zr_vm_library/src/zr_vm_library/zrm.c
  - zr_vm_core/src/zr_vm_core/string.c
implementation_files:
  - zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c
  - zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly_registry.c
  - zr_vm_lib_system/src/zr_vm_lib_system/module.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_library/src/zr_vm_library/zrm.c
plan_sources:
  - docs/code-review/comment-standard.md
  - user: 全仓库调用优先的功能、使用限制、设计与生命周期注释审查
tests:
  - tests/system/test_system_assembly_module.c
doc_type: module-detail
---

# zr.system.assembly 资源调用契约

## 入口和调用意图

系统库注册时通过 `zr_vm_lib_system/src/zr_vm_lib_system/module.c:84` 取得静态 descriptor，并在 `zr_vm_lib_system/src/zr_vm_lib_system/module.c:100` 登记叶模块，随后发布聚合根。getter 借出静态表，不访问项目或读取资源。函数表提供三个单一 string 参数的导出：

| 导出 | 正常脚本结果 | 用途 |
| --- | --- | --- |
| `resourceExists(name)` | bool | 在读取前处理不存在或当前归档不可用的分支 |
| `readResourceText(name)` | VM string | 消费当前项目的文本资源 |
| `readResourceBytes(name)` | VM array，整数 `0..255` | 保留原始顺序、全部字节和嵌入零字节 |

调用者传 `.zrm` 的逻辑名，例如 `config/default.txt`。`zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c:56` 从当前 global 查项目，`zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c:58` 解析项目输出路径；脚本参数不选择另一宿主路径。名称校验在 `zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c:34`，缺参和类型错误由 `zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1207`、`zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1211` 进入 VM 异常。

## 注册、派发和生命周期

`zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly_registry.c:18`、`zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly_registry.c:21`、`zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly_registry.c:24` 只登记 callback。`zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:274` 进入 native GC 域，`zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch_lanes.h:277` 才同步执行回调。context、state、参数视图和 result 只借用此次调用；不得缓存 context/result 或无根 VM 对象跨调用。

archive entry 借用归档。普通返回前释放 ZRM bytes 并关闭 archive；结果 string/array 属于 VM，不引用这些临时缓冲区。宿主若要保留裸 VM 对象，须由上层维持根。资源查询不会为下一次读取保留归档；两次调用观察的文件状态可以不同。

## callback 状态和脚本值

`resourceExists` 的正常查询 callback 返回 `ZR_TRUE`，同时 result 可为 `false`。当前归档打不开与资源缺失都映射成脚本 false（`zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c:148`）；这个值不能区分两者。空 context/result 或逻辑名校验失败返回 callback false；非 string 或缺参走参数异常。

两个读取接口在归档/资源查找或 `ReadEntry` 明确返回失败时进入 `RunError`；helper 误报成功的解压失败不会进入这条错误路径。`zr_vm_core/src/zr_vm_core/debug.c:1080` 将其声明为不返回，并在 `zr_vm_core/src/zr_vm_core/debug.c:1126` 进入异常链；不能将这条路径描述成发出错误后正常返回 false。普通文本 malloc、数组创建或 PushValue 失败可以返回 false；只有成功时才能消费 result。

## 已确认的文本限制

BUG: 有效 UTF-8 资源也可以包含 U+0000，例如字节 `41 00 42`。当前包的资源由 `zr_vm_library/src/zr_vm_library/zrm.c:746` 从文件按字节读取，并在 `zr_vm_library/src/zr_vm_library/zrm.c:845` 按显式长度写入；读取在 `zr_vm_library/src/zr_vm_library/zrm.c:1328` 返回完整字节数。文本回调在 `zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c:190` 交给 NUL 文本 setter，最终 `zr_vm_core/src/zr_vm_core/string.c:973` 按 C 字符串长度创建，因此后缀 `00 42` 无法作为完整内容交付。二进制消费者应调用 Bytes；这仍留下文本 API 的长度/拒绝策略需要后续修复。

BUG: 对合法项目/state、已读资源和成功的临时文本 malloc，文本可选择未驻留的 <=127 字节内容并使 API cache miss；短串对象创建和登记先成功，而驻留表达到扩容阈值。宿主分配器可只令 HASH_BUCKET 增长申请失败并保留旧桶：zr_vm_core/src/zr_vm_core/global.c:370、zr_vm_core/src/zr_vm_core/global.c:374 安装该宿主及预算外层，zr_vm_core/src/zr_vm_core/execution/execution_memory.c:65、zr_vm_core/src/zr_vm_core/execution/execution_memory.c:70 和 zr_vm_core/include/zr_vm_core/memory.h:22 正常传递 NULL。zr_vm_core/src/zr_vm_core/hash_set.c:171、zr_vm_core/src/zr_vm_core/hash_set.c:181 拒绝扩容，zr_vm_core/include/zr_vm_core/hash_set.h:535、zr_vm_core/include/zr_vm_core/hash_set.h:537 在后续 pair 申请前返回 NULL；zr_vm_core/src/zr_vm_core/string.c:802、zr_vm_core/src/zr_vm_core/string.c:808 解除短串表链接并正常返回 NULL，zr_vm_core/src/zr_vm_core/string.c:973、zr_vm_core/src/zr_vm_core/string.c:974 继续传递。zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c:324 到 zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1313 不写结果，而 zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c:190、zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c:191 仍正常报告 success。这是当前合法调用的静态 BUG，不授予运行或故障注入信用。短串上限在 zr_vm_common/include/zr_vm_common/zr_string_conf.h:16；对象建立的成功前提不等于假定所有 GC 分配失败都会正常返回，GC 申请最终失败可在 zr_vm_core/src/zr_vm_core/memory.c:46 抛错。短串解除一条表链接也不证明内存泄漏，因为对象已在 zr_vm_core/src/zr_vm_core/gc/gc_object.c:1107 登记到 GC 对象链。

TODO: 沿 zr_vm_core/src/zr_vm_core/memory.c:46 的异常入口与 zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c:190、zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c:195 的文本交付和普通收尾核查非局部退出时 text/bytes/archive 的清理责任。Bytes 的数组创建/追加在 zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c:107、zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c:115，普通收尾在 zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c:225；分别追查文本与数组下层异常协议，当前不将清理疑问写成已证实泄漏。

## 已确认的读取失败误报

BUG: 已定位资源条目后，miniz 在 `zr_vm_library/third_party/zr_miniz/miniz/miniz_zip.c:1640` 清零输出长度；分配失败在 `zr_vm_library/third_party/zr_miniz/miniz/miniz_zip.c:1655`、解压失败在 `zr_vm_library/third_party/zr_miniz/miniz/miniz_zip.c:1661` 返回 NULL。`zr_vm_library/src/zr_vm_library/zrm.c:1322` 只拒绝 NULL 且长度非零，于是这类失败返回 true、NULL/0。文本消费者 `zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c:177` 和字节消费者 `zr_vm_lib_system/src/zr_vm_lib_system/assembly/assembly.c:219` 都只检查状态，随后可构造空字符串或空数组，误把失败当作空资源。合法归档上的分配失败已由完整静态路径说明；这不表示本批执行了故障注入。空资源与提取失败必须由下层失败协议区分，当前上层不能保证全部错误被报告。

## 现有测试覆盖

`tests/system/test_system_assembly_module.c:237` 和 `tests/system/test_system_assembly_module.c:247` 通过真实导出调用核对存在与缺失；`tests/system/test_system_assembly_module.c:257` 读取普通文本；`tests/system/test_system_assembly_module.c:267` 和 `tests/system/test_system_assembly_module.c:273` 读取并核对数组字节。这些用例的文本没有嵌入 NUL，也不建立提取失败、短串驻留扩容失败或非局部异常的覆盖。本次头文件注释验证只使用实际消费文件的编译比较，不表示这些运行用例已经执行。
