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
  - tests/core/test_string_native_layout.c
  - tests/cmake/string-native-layout-tests.cmake
doc_type: testing-guide
---

# 字符串存储分配失败

本组限定于已初始化的真实 Core：长串独立字节载荷、数字转换暂存区、短串驻留表 rehash，以及创建失败时的 API 字符串缓存提交。全局构造期的 StringTable 外壳及初始桶分配由 bootstrap 回归负责；本组不替代其证据。

## 实现与失败契约

`ZrCore_String_Create` 的长串分支先创建 GC 对象，再申请 STRING 类别的原生载荷。GC 对象已注册后，先设置 long 布局标记、长度和 NULL 载荷指针；随后载荷申请失败即返回 NULL。未交付对象仍归 GC 所有。现有 GC 析构仅在 long 标记且载荷非 NULL 时释放原生块，因此失败候选可安全回收，调用方不能自行释放它。对象构造返回 NULL 时也必须在写入字段前退出。

`ZrCore_String_FromNumber` 的 STRING 类别暂存区申请失败返回 NULL，不调用格式化宏。普通返回时仍按原大小和类别释放暂存区。GC 对象或 HASH_PAIR 分配仍可能经 Throw 离开；本次不修复、不宣称覆盖这些非局部退出中的原生暂存区清理。

短串 rehash 的普通失败路径已有回滚：撤销新候选在 `shortStringListHead` 的链接，保留旧桶和计数，返回 NULL。该路径保留为支持层基线。HASH_PAIR OOM 在候选入链后抛出异常属于另一条路径，必须独立记录 RED，不能以此基线的通过代替。

`ZrCore_String_CreateTryHitCache` 先创建候选，只有非 NULL 成功结果才移动旧槽并写入首槽。普通 NULL 返回或创建异常都不先改缓存。失败后原有有效对象和槽位顺序保留，不发布 NULL；解除故障后原地址可重试，随后命中成功对象。

## 代码职责

`string.c` 保留字符串存储、驻留表、缓存、比较及拼接。原生格式化的完整静态 helper 集合、`VFormat`、`Format` 和数字／指针文本转换移至 `string_format.c`，复用既有公开 String、Stack 和 Value 接口；没有新增私有跨文件 API。原生格式占位符、浮点文本策略、结果栈位置及尚未修复的格式化错误路径保持原行为。

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
