---
related_code:
  - zr_vm_core/src/zr_vm_core/value/meta.c
  - zr_vm_core/src/zr_vm_core/value/value.c
  - zr_vm_core/src/zr_vm_core/string/string_format.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/value/meta.c
plan_sources:
  - user: 2026-10-16 active BUG repair with support-layer RED before production
tests:
  - tests/core/test_meta_string_oom.c
  - tests/cmake/meta-string-oom-tests.cmake
  - tests/library/test_native_string_publication_oom.c
doc_type: failure-contract
---

# 基础值字符串元方法的内存失败

`ZrCore_Value_ConvertToString` 先调用值类型的 TO_STRING 元方法，再考虑直接转换。
内建 null、bool 和 number 元方法须在取得有效字符串后才发布返回值。字符串创建
普通返回 NULL 时，这三个入口现在抛出 `ZR_THREAD_STATUS_MEMORY_ERROR`，不把 NULL
传给会读取 `object->type` 的 `ZrCore_Value_InitAsRawObject`，也不发布成功返回槽。
正常文本与调用签名保持原有语义。

number 原先在 `ZrCore_String_FromNumber` 返回 NULL 时创建空串。这会把数字格式缓冲
申请失败变成成功的空文本；该回退已删除，失败统一沿 MemoryError 通道返回受保护
调用方。发生异常的内部调用栈由既有异常/调用恢复契约处理，本次未新增栈回滚保证。

## 首次异常规范化的可达路径

实际未预热 `native_string_publication_short_rehash` 拒绝驻留表旧 buckets 的扩容。
SetString 取得 NULL 后抛 MemoryError，首次 NormalizeStatus 读取栈顶 null 作为消息。
Error 原型名字创建失败可安全回退基础 Error；创建 stacks 键失败也有 NULL 检查。
随后消息转换调用 `meta_to_string_null`，创建四字节文本 null 同样失败，旧代码立即
把 NULL 传给 Value 初始化并崩溃。这条链不需要假设 GC 或悬空对象。

现有 binary 的 `ZR_VM_TRACE_CORE_BOOTSTRAP=1` 诊断确认最后边界依次为原文本扩容失败、
MemoryError 名字失败、Error/frames 创建成功、stacks 键失败、null 文本失败，进程自然
以 `0xC0000005` 退出。二进制 SHA256 为
`df36bfb62fd2d71779d1ac71c6d27f1a9bb96a65e2e8aa20cee5bdc61ed39594`；
trace 与收据保留在外部报告任务 `string-publication-first-oom-debug`。LLDB 启动阻塞
没有提供调用栈，不把该次调试器尝试作为栈证据。

## 回归与范围

独立真实 Core 目标 `zr_vm_meta_string_oom_test` 注册五个进程，CTest 前缀为
`core_meta_string_oom_`。null/true/false 用例证明目标尚未驻留，再填充实际表到扩容
阈值，按旧指针、HASH_BUCKET 类别与增长字节数持续拒绝。number 拒绝 STRING 类别
`ZR_NUMBER_TO_STRING_LENGTH_MAX` 的原生格式缓冲。夹具用字符串载荷预热 Error 字段，
以隔离转换本身，原 publication 用例仍保留首次未预热的规范化覆盖。

检查包括故障确实命中、受保护 MemoryError、不正常成功、调用方结果保持有效、
恢复点及显式根数量平衡、撤故障后完整文本重试，以及销毁后分配尺寸/类别与零残留。
Root 的实际 `meta-thread-file-red-r2` configure/build 均 0，1918 个输入
`changed_inputs=[]`；null/true/false 崩溃、number 吞 OOM 用例失败、正常控制通过。
本次实现随后写入，support-batch-5-green 实际重编译后五项全部通过；1929 个输入哈希无变化，首次未预热的 publication short_rehash 也通过。

此修改仅为上述三个已有函数增加局部 guard。meta.c 已超过模块拆分建议行数，但本次
不增加职责，按明确授权保留三处局部修复，避免把无关重构混入已冻结回归输入。
其他字符串运算元方法的分配边界及 Exception 连续根保护属于独立工作，本组不作
已修复或已验证声明。
