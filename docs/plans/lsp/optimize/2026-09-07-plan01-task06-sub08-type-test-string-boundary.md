---
plan_id: optimize
task: plan01-task06-sub08
status: completed
related_code:
  - tests/parser/test_type_inference.c
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
doc_type: plan-record
---

# Plan 01 Task 6 Sub08: Type Test String Boundary

## 状态与产出记录

- 开始时间: 2026-09-07 23:20 +08:00
- 实际完成时间: 2026-09-07 23:27 +08:00
- 状态: 字符串边界修复完成；整套 sanitizer 门禁未完成
- 源码版本: `815900dc` 加已登记 Sub06/07 和共享 overlay
- 产出路径: 既有 parser 测试字符串边界、验证记录
- 剩余门槛: 导入模块缓存泄漏与父级完整门禁

## 失败与修复

Sub06 扩展 Clang ASan 检查在 106 个 PASS 后，由
`test_type_inference_source_import_array_preserves_iterable_protocol` 触发
global-buffer-overflow。堆栈为测试第 7152 行的 String_Create 到 string hash
再到 XXH_read64；测试给 `source_import_array_iterable_test.zr` 硬编码长度 40，
超过字符串存储边界。GCC/MSVC 普通运行不会可靠检测该越界。

测试改用 `ZrCore_String_CreateFromNative` 计算实际长度，字符串语义和生产
string/hash API 保持。日志证据前缀为
`.codex/lsp-optimize-validation/plan01-task06-sub08-`，原始 RED 见
`plan01-task06-sub06-clang-type-inference.log`。

## 验证

三工具链均重建 `zr_vm_type_inference_test` 后直接执行该程序。
GCC 与 MSVC 各 124/124、真实 exit 0。Clang ASan/UBSan 的 124 个功能
用例全部完成，原始 global-buffer-overflow 不再出现，但 LeakSanitizer
报告导入模块路径 5,187 bytes/44 allocations，真实 exit 1，不能计作完整
sanitizer 通过。泄漏堆栈来自 imported compile-time module/cache 生命周期，
与本项 source-name 字面量边界分别记录并继续处理。

构建目录：GCC `/home/hejiahui/.codex-builds/lsp-plan01-task06-sub03-gcc`；
Clang `/home/hejiahui/.codex-builds/lsp-plan01-task04-sub05-clang`；
MSVC `E:/Git/zr_vm/.codex/lsp-optimize-validation/msvc-current`。
命令为 `cmake --build <build> --target zr_vm_type_inference_test`，随后运行
`<build>/bin/zr_vm_type_inference_test`（Windows 加 `.exe`）。MSVC 通过
`Invoke-VsDevCommand.ps1` 导入编译器环境。

[测试模块边界](../../../testing-and-validation/index.md#parser-fixture-string-bounds)
记录 literal 输入与 runtime string 的所有权。
