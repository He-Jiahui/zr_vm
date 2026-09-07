---
plan_id: optimize
task: plan01-task02-sub28
status: completed
related_code:
  - zr_vm_language_server/stdio/stdio_json.c
  - zr_vm_language_server/stdio/stdio_diagnostic_json.c
  - zr_vm_language_server/stdio/stdio_diagnostics.c
  - tests/language_server/test_stdio_diagnostic_json.c
  - tests/language_server/test_stdio_diagnostic_publication.c
  - tests/cmake/zr_vm_lsp_stdio_handler_tests.cmake
plan_sources:
  - docs/plans/lsp/astra.md
  - docs/plans/lsp/optimize/01-protocol-lifecycle-and-transport.md
tests:
  - language_server_stdio_diagnostic_json
  - language_server_stdio_diagnostic_publication
doc_type: plan-record
---

# Plan 01 Task 2 Sub28: Diagnostic JSON Ownership

## 状态与产出记录

- 开始时间: 2026-09-07 20:12 +08:00
- 实际完成时间: 2026-09-07 20:27 +08:00
- 状态: 已完成
- 源码版本: `48cb1530` 加共享工作树 overlay
- 完成项目: 诊断序列化与 report 故障传播、三工具链验证、逐分配点注入和内存检查
- 计划产出: 诊断内容与 report 故障传播、逐分配点回归、所有权与验证文档

## 范围

Sub27 已保证通知外层的失败状态不会提交诊断缓存，但嵌套 serializer 仍可能返回
不完整的成功对象。此项先验证 position/range/location，再验证包含 code、message、
code description、related information 和 fix data 的单项/数组诊断，最后检查 document
和 workspace 的 full/unchanged report。每个已执行的 JSON 分配点分别注入单次和持续
失败，要求返回空指针、handler 分类为 INTERNAL_ERROR，并释放完整的部分树。

## 失败证据与实现

GCC RED 九项测试全部失败：position、range、location、diagnostic、diagnostic array、
document full/unchanged report 在第 2 个分配点失败后仍返回非空结果；workspace
full/unchanged report 在第 4 个分配点出现相同错误。成功控制先验证完整字段，排除了
夹具无诊断或从未到达相关分支的情况。

`stdio_json.c` 的 position/range/location 现在检查数值字段和子对象挂接。range 失败
会删除已经构造的 start/end，location 失败会删除 range 与 URI。非空 runtime URI 的
字符串转换失败返回失败，不会转为成功 JSON null。

诊断树按创建顺序将 edit、fixes、code description 和 related information 归入唯一
父节点，所有转换和挂接结果都向上传播。可选字段本来缺失时仍可省略；已请求构造的
可选 data/fix/related 内容失败时，整个诊断失败。诊断数组的任一元素失败会删除前面
已经加入的完整诊断，不再产生截断成功数组。

document 和 workspace 的 full/unchanged report 检查 kind、resultId、URI、version
与 items。workspace 根对象在开始收集 report 前就拥有 items，后续任一 report 失败
统一清理根对象与 runtime diagnostics/URI list。两个 handler 通过既有 status/result
契约返回 INTERNAL_ERROR；取消仍由共同 helper 保持优先级。

## 验证

直接 JSON 回归 9/9、诊断发布回归 6/6。每个分配点都执行单次与持续故障注入，
断言结果不存在、handler 为 INTERNAL_ERROR、零个活动 JSON allocation。正常路径
断言精确 range、URI、message、code、descriptor、related location、fix edit，以及
full/unchanged 身份和内容。分配点矩阵如下：

| 路径 | 分配点 |
| --- | ---: |
| position | 5 |
| range | 13 |
| location | 18 |
| 单项诊断 | 56 |
| 含 data 的双项诊断数组 | 227 |
| document full / unchanged | 119 / 7 |
| workspace full / unchanged | 127 / 15 |
| 非空真实诊断通知 | 130 |

JSON 测试合计 587 点，真实诊断通知另加 130 点，共 717 点和 1,434 次新增故障注入。
原有清空/空文档通知的 16/18 点也保持通过。真实通知失败时 stdout 为零字节、缓存不
提交；成功通知继续用生产 frame reader 回读。

GCC、MSVC Debug、Clang ASan/UBSan 的相关 CTest 均为 13/13，耗时分别为
32.36/36.57/58.44 秒。组内覆盖六个直接 stdio 目标、position encoding、protocol
conformance、envelope mutations、resolve、navigation、document sync 与 snapshot
workspace diagnostics。

GCC Valgrind 两个直接诊断测试均为 `0 bytes in 0 blocks`、`0 errors`，分别完成
387,576 和 215,460 次成对分配/释放。

构建目录沿用 GCC `/mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc`、Clang
`/home/hejiahui/.codex-builds/lsp-plan01-task04-sub05-clang` 和 MSVC
`E:/Git/zr_vm/.codex/lsp-optimize-validation/msvc-current`。构建与 CTest 日志前缀为
`.codex/lsp-optimize-validation/plan01-task02-sub28-{gcc,clang,msvc}-`，另有
`plan01-task02-sub28-gcc-red-ctest.log` 和两个 `gcc-diagnostic-*-valgrind.log`。

GCC 验证命令如下；Clang 替换构建目录，MSVC 用 VsDevCommand wrapper 并加 `-C Debug`：

```sh
cmake --build /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc --parallel 8 --target zr_vm_language_server_stdio zr_vm_language_server_stdio_diagnostic_json_test zr_vm_language_server_stdio_diagnostic_publication_test zr_vm_language_server_stdio_initialize_test zr_vm_language_server_stdio_handler_cancellation_test zr_vm_language_server_stdio_transport_output_test
ctest --test-dir /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc --output-on-failure -R '^language_server_stdio_(initialize|request_progress|diagnostic_publication|diagnostic_json|handler_cancellation|transport_output|protocol_conformance|protocol_envelope_mutations|position_encoding_smoke|navigation_capabilities_smoke|snapshot_workspace_diagnostics_smoke|resolve_capabilities_smoke|document_sync_conformance)$'
valgrind --leak-check=full --errors-for-leak-kinds=definite,indirect --error-exitcode=99 /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio_diagnostic_json_test
valgrind --leak-check=full --errors-for-leak-kinds=definite,indirect --error-exitcode=99 /mnt/e/Git/zr_vm/.codex/build-lsp-opt-gcc/bin/zr_vm_language_server_stdio_diagnostic_publication_test
```

## 剩余门槛

其他 handler 的嵌套 serializer、运行时分配分类和 Plan 01 父级门禁继续待办。
