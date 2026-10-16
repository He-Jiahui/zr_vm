---
related_code:
  - zr_vm_core/include/zr_vm_core/string.h
  - zr_vm_core/src/zr_vm_core/string/string.c
  - zr_vm_core/src/zr_vm_core/string/string_format.c
  - zr_vm_core/src/zr_vm_core/gc/gc_object.c
  - tests/core/test_string_storage_oom.c
  - tests/cmake/string-storage-oom-tests.cmake
implementation_files:
  - zr_vm_core/include/zr_vm_core/string.h
  - zr_vm_core/src/zr_vm_core/string/string.c
  - zr_vm_core/src/zr_vm_core/string/string_format.c
plan_sources:
  - "user: 2026-10-16 支撑层现役 BUG 修复与真实回归"
tests:
  - tests/core/test_string_storage_oom.c
  - tests/cmake/string-storage-oom-tests.cmake
  - tests/core/test_string_format_oom.c
  - tests/cmake/string-format-oom-tests.cmake
  - tests/core/test_string_native_layout.c
  - tests/cmake/string-native-layout-tests.cmake
doc_type: testing-guide
---

# 字符串存储分配失败

本组限定于已初始化的真实 Core：长串独立字节载荷、数字转换暂存区、短串驻留表 rehash，以及创建失败时的 API 字符串缓存提交。全局构造期的 StringTable 外壳及初始桶分配由 bootstrap 回归负责；本组不替代其证据。

## 实现与失败契约

`ZrCore_String_Create` 的长串分支先创建 GC 对象，再申请 STRING 类别的原生载荷。GC 对象已注册后，先设置 long 布局标记、长度和 NULL 载荷指针；随后载荷申请失败即返回 NULL。未交付对象仍归 GC 所有。现有 GC 析构仅在 long 标记且载荷非 NULL 时释放原生块，因此失败候选可安全回收，调用方不能自行释放它。对象构造返回 NULL 时也必须在写入字段前退出。

`ZrCore_String_FromNumber` 的 STRING 类别暂存区申请失败返回 NULL，不调用格式化宏。字符串创建使用局部 TryRun 边界；无论普通返回还是 GC 对象／HASH_PAIR 分配抛出，都先按原大小和类别释放暂存区，再返回结果或传播原状态。上下文属于恢复点调用方，不依赖被 longjmp 放弃的回调局部变量。

短串候选先进入 `shortStringListHead`，供 HASH_PAIR 分配重试的实际 GC 扫描。哈希插入普通失败或 Throw 时，局部恢复边界都只摘除该候选；异常规范化可能加入有效字符串，因此不能直接覆盖链头。失败候选仍归 GC 所有，不手动释放。普通 rehash 失败返回 NULL；捕获的异常在摘链后按原状态传播。成功才登记 minor GC 的年轻短串桶。

`Format`／`VFormat` 在写入栈槽前检查创建结果，拼接后检查结果是否为非 NULL STRING，资源失败抛出 MemoryError。受保护执行器保存栈顶的字节偏移，失败时按当前栈基址恢复，允许期间扩栈。成功保留一个栈顶结果，其原生文本视图由该栈根保护。执行器复制并结束 `va_list`；`Format` 自己的 `va_start` 参数也在向外传播前结束。嵌套 TryRun 复用异常层已有的规范化抑制规则。

`ZrCore_String_CreateTryHitCache` 先创建候选，只有非 NULL 成功结果才移动旧槽并写入首槽。普通 NULL 返回或创建异常都不先改缓存。失败后原有有效对象和槽位顺序保留，不发布 NULL；解除故障后原地址可重试，随后命中成功对象。

## 代码职责

`string.c` 保留字符串存储、驻留表、缓存、比较及拼接。原生格式化的完整静态 helper 集合、`VFormat`、`Format` 和数字／指针文本转换移至 `string_format.c`，复用既有公开 String、Stack 和 Value 接口；没有新增私有跨文件 API。原生格式占位符、浮点文本策略及成功结果栈位置保持原行为；未定义占位符和孤立 `%` 的契约不在本组修复范围。

该提取将约 1100 行的原文件降至约 934 行，格式化单元约 226 行；采用职责边界而非行号切片。模块的现有 `CONFIGURE_DEPENDS` 递归源码收集会纳入新文件，不需添加顶层 CMake 列表。

## 回归与证据范围

四项 CTest 使用同一个测试程序的独立 argv 进程，各设 30 秒超时，并关闭 Windows 故障／断言对话框。真实上游宿主 allocator 记录所有块的地址、大小和类别；注入期间允许释放，对匹配请求持续拒绝直至 TryRun 返回，不替换 Core 实现。

| 用例 | 实际故障 seam | 关键断言 |
| --- | --- | --- |
| `long_payload` | STRING，新块，精确 `length + 1` 字节 | NULL 或受保护 MemoryError；重试完整内容；失败对象最终安全析构 |
| `number_buffer` | STRING，新块，精确 `ZR_NUMBER_TO_STRING_LENGTH_MAX` 字节 | 不写 NULL 格式缓冲区；解除故障可转换同一数字 |
| `short_rehash` | HASH_BUCKET，真实旧桶地址及实际倍增字节数 | 旧桶、容量、计数和候选链头不变；重试成功 |
| `cache_rehash` | 相同 rehash seam，经真实缓存入口 | 预置不同缓存对象；失败后完整缓存身份与顺序不变、无 NULL；重试后同地址命中 |

每项销毁真实 GlobalState，断言存活块为零且释放元数据匹配。`support-batch-2-red` 已实际记录：长串 NULL 写入触发 memory.h 断言、数字暂存区 NULL 触发 UCRT 断言、缓存写入 NULL；短串 rehash 支持基线通过。修复后 root 复用实际 checkout 重新编译，integrated-support-batch-3-red 中四项全部通过且输入哈希稳定。增强后的缓存条目顺序断言同属已确认缓存提交缺陷，不扩大到 HASH_PAIR 故障契约。

## 格式化与驻留提交的非局部失败回归

`test_string_format_oom.c` 的五个独立进程直接链接真实 Core；四项持续精确故障覆盖长片段 STRING 创建软失败、拼接 ARRAY 软失败、FromNumber 的 HASH_PAIR Throw、短串候选的 HASH_PAIR Throw，另有正常格式控制。格式失败断言调用前栈深恢复、撤障重试全文正确；数字转换断言 44 字节暂存区不泄漏；驻留失败按内容检查候选不残留于根链和桶。保活基线对象使用真实 lease；stopGcFlag 仅抑制自动步骤，不禁止 GcMalloc 的显式完整 GC 重试。最终销毁检查零存活块。

root 的 `integrated-string-support-red-r2-receipt.json` 记录 configure/build 均为 0、8 项中 3 PASS／5 FAIL、1912 输入不变；本组四项分别出现两个 SegFault、数字暂存区泄漏及候选链残留，正常格式通过。修复后 root 的 support-batch-5-green 实际重编译成功，本组五项及存储四项全部通过，1929 个输入哈希无变化。独立 publication short_rehash 的 Meta soft-NULL 消费仍由 Meta 模块修复，本组不授予该路径信用；也不覆盖并发 major GC、畸形格式串或溢出。

格式化职责已整体提取，当前 string.c 约 965 行，新增驻留事务辅助仍属于原有驻留职责；本次保持其在同文件，避免把依赖私有候选生命周期的窄回滚拆成额外跨文件 API。string_format.c 约 293 行。
