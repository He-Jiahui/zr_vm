---
related_code:
  - zr_vm_library/include/zr_vm_library/native_binding.h
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_support.c
implementation_files:
  - zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c
plan_sources:
  - .codex/plans/20261016-review-bugfix.md
  - docs/code-review/coverage/zr_vm_library_native.tsv
tests:
  - tests/library/test_native_string_publication_oom.c
  - tests/cmake/native-string-publication-tests.cmake
doc_type: failure-contract
---

# 原生字符串结果发布

`ZrLib_Value_SetString` 为原生回调的结果槽创建 VM 字符串。它先完成字符串构造，再把对象发布到目标槽；C 字符串为 NULL 时仍按空串处理。字符串构造失败时抛出 `ZR_THREAD_STATUS_MEMORY_ERROR`，原槽位保持不变，调用方可以通过受保护入口处理失败。

短串驻留表扩容和长串独立负载分配均可能返回 NULL。原入口把这个 NULL 传给会直接返回的 `ZrLib_Value_SetStringObject`，导致回调继续执行但结果没有写入。新检查把这类软分配失败接入异常通道，不改变已有字符串对象入口对 NULL 参数的处理约定。

回归使用实际 Core/Library 与按类别、大小、旧指针匹配的持续故障分配器。短串用例填充实际驻留表到扩容边界，长串用例拒绝独立负载分配；两者检查受保护调用返回 MemoryError、原整数槽位不变，并在撤销故障后用同一入口重试。正常用例覆盖 NULL 输入，普通文本成功由撤故障后的重试覆盖。销毁 VM 后分配跟踪必须归零。

2026-10-16 修复前实际 ClangCL 构建成功，三项 CTest 中两项分配失败用例失败，正常用例通过；输入哈希稳定。修复后 support-batch-5-green 实际重编译成功，三项全部通过，1929 个输入哈希无变化。短串首次异常规范化依赖 Meta 的空串创建失败检查；原夹具未预热完整 Error，保留该真实链路。
