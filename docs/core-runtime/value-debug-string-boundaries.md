---
related_code:
  - zr_vm_core/include/zr_vm_core/value.h
  - zr_vm_core/src/zr_vm_core/value/value.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/value/value.c
  - tests/core/test_value_debug_string_bounds.c
  - tests/cmake/value-debug-string-boundary-tests.cmake
plan_sources:
  - .codex/plans/20261016-review-bugfix.md
tests:
  - tests/core/test_value_debug_string_bounds.c
  - tests/cmake/value-debug-string-boundary-tests.cmake
doc_type: regression-contract
---

# 值调试字符串的容量边界

本组通过实际 Core 公共 `ZrCore_Value_ToDebugString` 检查固定诊断缓冲的截断行为。
当前已确认生产实现把 `snprintf` 返回的所需长度当作实际写入长度累加，长内容后续写入会越过栈缓冲。
新夹具尚未执行；不授予 RED、GREEN 或内存安全验证信用。

十个独立进程覆盖长类型名、长成员名、长字符串字段、长数组元素、格式总长度恰为容量减一与容量的两个边界，
以及短对象、短数组、十项和十一项数组预览控制。类型、字段和值均由实际 Core API 创建；数组使用实际节点存储。
输出预期保留原格式的有界前缀；容量包含末尾 NUL。测试先检查托管字符串长度，再在其实际载荷范围内寻找 NUL。
原实现可能破坏其栈，每项由 CTest 独立子进程执行，并在 Windows 关闭故障弹窗。

API 不接收外部缓冲或容量，不能据此推导零容量、单字节容量或空外部缓冲契约。
夹具关闭自动 GC，使用带大小和分类核对的真实分配器检查正常退出时的 Core 释放平衡。
本组不覆盖元方法分配导致的对象搬迁、UTF-8 裁剪、Testing TLS 重入或 Throws 的当前异常保存。
