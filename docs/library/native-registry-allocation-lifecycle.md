---
related_code:
  - zr_vm_library/src/zr_vm_library/common_state.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.h
  - zr_vm_core/include/zr_vm_core/array.h
  - tests/library/test_native_registry_oom.c
  - tests/cmake/native-registry-oom-tests.cmake
  - tests/library/test_native_plugin_shadow_io.c
  - tests/cmake/native-plugin-shadow-io-tests.cmake
implementation_files:
  - zr_vm_library/src/zr_vm_library/common_state.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_registry_plugin.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_plugin_shadow_io.h
plan_sources:
  - user: 2026-10-15 修复现役 BUG 注释并验证分配失败清理
tests:
  - tests/library/test_native_registry_oom.c
  - tests/cmake/native-registry-oom-tests.cmake
  - tests/language_server/descriptor_plugin_fixture_int.c
  - tests/library/test_native_plugin_shadow_io.c
  - tests/cmake/native-plugin-shadow-io-tests.cmake
doc_type: module-detail
---

# NativeRegistry 分配失败生命周期

## Attach 的发布边界

Attach 在原始 GLOBAL 分配器取得 registry 后，先 Construct 三个数组，再通过
TryInit 建立 moduleRecords、bindingEntries、pluginHandles。任何一次失败都释放三个
数组中已取得的存储及 registry，然后返回 false。该阶段尚未发布 nativeRegistryState、
cleanup 或解析器等钩子，宿主已有钩子保持原值；允许宿主在解除分配故障后重试。

原始数组分配不触发 GC。TryInit/Push 通过 false 表达分配失败，不走 void 数组 API 的
MEMORY_ERROR 抛出路径，因此调用方能够回收局部所有权。钩子发布后若基础模块注册失败，
现有 NativeRegistry_Free 负责恢复宿主钩子和清理 registry。

## 注册记录与 callable

新增模块记录先取得 moduleName/sourcePath 两个 GLOBAL 字符串，然后 TryPush。
追加失败时旧数组内容与长度保持不变，局部两个字符串均释放，注册返回 false。
更新已有记录的路径在字符串全部取得后才替换旧内容。

callable 创建的 closure 在 bindingEntries 追加成功后才标记为永久对象。
TryPush 失败返回 false，不发布结果值或永久 GC 根；未发布的 closure 由正常 GC 回收。
TryPush 与身份赋值使用原始分配路径，不触发 GC，因此推迟永久化不会使局部 closure
在提交前被回收。这一修复范围是 bindingEntries 增长失败，不代表所有后续模块导出失败
均已实现事务回滚。

## CommonGlobalState 构造契约

GlobalState_New 返回 NULL 时立即返回，禁止继续 ReadAll。Project 建立后若 Attach
返回 false，调用现有 CommonGlobalState_Free 释放 project、registry（如果仍存在）及
global，最终返回 NULL。调用方只有取得非 NULL 结果才拥有完整 common state。
该 NULL 分支不增加产品故障注入接口，通过代码审查与现有构造成功 smoke 验证。

## 插件加载事务

加载器在 ABI、模块名、活跃 owner 和兼容性检查后，先构造新句柄的三个定位字符串，
再 TryPush 到 pluginHandles，然后登记 moduleRecords。元数据或句柄数组分配失败时，
关闭新库、释放已取得的字符串、删除新影子副本，旧模块记录及句柄保持有效。
moduleRecords 登记失败时 Pop 刚追加的新句柄并完整回收；登记函数本身在任何失败前
均保留旧记录。新模块记录成功后才删除同名旧句柄，之后没有分配或失败出口。

新句柄通过数组索引保留。删除较低位置的旧记录后修正保留索引，不能按系统句柄值
判断代际：未使用影子副本时，重复装载可能返回相同的句柄值，但仍有各自装载引用。
错误信息在关闭新库前生成，禁止在库卸载后访问 descriptor 中的字符串。

字符串依次取得 sourcePath、moduleName、loadedPath，任一步失败即停止后续分配。
这保证 loadedPath 有值时 sourcePath 已存在，通用清理可以安全区分源插件与影子副本，
不会在 sourcePath 分配失败且直接装载源库时删除源文件。若 loadedPath 自身失败，
加载器仍使用栈上的 loadPath 删除影子副本。

## 故障注入与验收边界

CTest 对每个故障点启动独立进程，设置 30 秒超时；Windows 关闭故障与断言对话框。
分配器在完整 VM 构造后才开启故障，持续计数 ARRAY/GLOBAL 存活块，以构造后基线
比较局部回滚。GLOBAL 包含 registry 与注册记录字符串，避免只检查数组而漏检字符串。
Attach 三处初始化失败、module record 增长、binding entry 增长均已在未修复生产输入下
取得真实失败；夹具 include 修正产生的编译失败不算行为 RED。

plugin 另有四个独立用例：pluginHandles 增长失败、第三次 GLOBAL 字符串分配失败、
moduleRecords 增长失败、已有模块重载时字符串失败。它们使用现有真实 descriptor DLL，
比较失败前后的模块指针（不解引用失败后可能悬垂的指针）、两数组长度和 GLOBAL 块数。
记录增长用例也是首批注册修复后的回归守卫。root 串行实际构建中，首批五项已 GREEN，
plugin_record_growth 也通过；句柄增长、字符串分配、失败重载三项均取得真实 RED。
加载事务修复后，在 `integrated-support-oom-string-red-r2` 的真实构建中，这九项
`native_registry_oom_*` 测试全部通过，收据记录 `changed_inputs=[]`；pool 的 14 项
与 CLI hello_world smoke 同轮通过。该批次其他模块仍有预期 RED，不能将此结论扩展为
整个 CTest 批次通过。

失败 reload 用例实测的是旧 descriptor 仍可查询且指针不变、模块和句柄数量不变、
GLOBAL 分配块数回到基线；未实际执行旧代际回调。因此旧回调可继续执行属于依据
句柄保留顺序的代码审查结论，不能宣称已被该用例运行验证。构造失败时宿主钩子
保持原值同样由发布顺序审查支撑，当前 Attach 用例直接断言 registry/cleanup 未发布。

## 影子副本 I/O 与失败清理

目录、临时缓存路径、二进制复制和影子文件命名已提取到私有
`native_binding_plugin_shadow_io.c/.h`。加载编排继续留在 registry_plugin 中，
避免超过千行的文件继续积累 I/O 职责；模块已有源码 GLOB 收集新增实现，无公开 API。
提取先保留原行为，在真实 Library 构建中取得两项行为 RED 后才修复。

`native_registry_copy_plugin_streams` 接管两个有效且不同的流。复制循环检测短写、
源和目标错误标志，随后分别检查源关闭与目标关闭结果，任一步失败返回 false。
两次关闭是独立语句，源关闭失败也不能跳过目标关闭。路径 wrapper 在失败后仅删除
目标不完整副本，保留诊断所指向的源插件。成功复制但 open_library 失败时，加载器
在记录源路径诊断后，只在 loadedFromShadow 为真时删除本次 loadPath。

独立测试 `native_plugin_shadow_io_open_failure` 重复加载真实无效映像三次，枚举
专属缓存目录并检查源文件内容、模块和句柄数量。未修复输入得到 before=0、after=3。
`native_plugin_shadow_io_target_close_failure` 使用 Windows CreatePipe 写端的真实
全缓冲 FILE 流；关闭读端后先证明小块 fwrite 成功、fclose 返回 EOF，再调用生产
owning-stream helper。未修复 helper 仍报告成功，因此测试真实失败。这些用例未替换
生产实现或伪造 fclose 返回值；Linux 分支使用 pipe 并忽略 SIGPIPE 以检查 stdio 返回。

证据在 `integrated-support-batch-2-red-receipt.json` 与同目录 `-2.log`，
收据记录 changed_inputs=[]。修复后 root 在实际 checkout 统一重编，integrated-support-batch-3-red 中两项均通过，输入哈希稳定；源 fclose 失败、
短写及读错误分支本轮为代码审查覆盖，不能称为各自已运行故障注入。
测试 TMP/TEMP/TMPDIR 指向 `${CMAKE_CURRENT_BINARY_DIR}/native-plugin-shadow-io-tmp/`
下的各 case 专属目录。RED 留下的旧影子副本由 root 按指纹清理；GREEN 的前后数量比较
允许已登记的基线残留存在，不意味着历史残留已清空。

## 构建证据与产物

实际构建由 root 在共享 top-clangcl 目录串行驱动。证据及源码身份见
`E:/cargo-targets/zr_vm/reports/review-bugfix-20261016/integrated-baseline-red-r3-receipt.json`
和 `integrated-stack-registry-lsp-plugin-red-receipt.json`。最终九项 GREEN 及输入身份见
`E:/cargo-targets/zr_vm/reports/review-bugfix-20261016/integrated-support-oom-string-red-r2-receipt.json`，
逐项结果位于同目录 `integrated-support-oom-string-red-r2-2.log`。
plugin 临时环境 TMP/TEMP/TMPDIR 指向当前测试构建目录的 `native-registry-oom-tmp`，
配置使用 `${CMAKE_CURRENT_BINARY_DIR}`，随已批准的实际构建产物根目录迁移。
其影子动态库由加载生命周期回收；失败测试残留由任务登记后定向清理。
