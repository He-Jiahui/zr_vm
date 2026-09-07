---
related_code:
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_call_semantic_facts.c
related_module_docs:
  - docs/parser-and-semantics/semantic-call-fact-ownership.md
tests:
  - tests/parser/test_semantic_query_calls.c
  - tests/parser/test_semantic_query_call_type_lifetime_cases.h
  - tests/parser/test_canonical_type_graph.c
  - tests/language_server/test_semantic_analyzer.c
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/astra.md
doc_type: milestone-record
---

# Plan 03 Task 7.71: Call Type Lifetime

## Failure and Correction

The Clang semantic-analyzer runner reported a heap-use-after-free in
`type_inference_resolved_call_type_id`. The producer borrowed the canonical
declared function, instantiated the resolved signature (which could reallocate
the canonical type array), then read the old pointer's kind to select the
declaration-contract rebinding path.

The producer now copies the validity decision before interning. Receiver and
callable effects were already copied at that point. The later branch consumes
the copied decision and the rebinder looks up records by their canonical ids.
No request-time query fallback or public API is added.

## Reproduction and Validation

The new parser regression forces canonical-array relocation while instantiating
a call. Before the fix, the cleared retired record sends publication down the
fallback path and loses the declaration's readonly return contract:
`32 Tests 1 Failures`, expected readonly `1`, actual writable `0`.

The test checks the exact callable identity, substituted return pointee, effect
flags, and declared return access after relocation. It restores the allocator
and frees the retired storage before making final assertions.

GCC Debug, Clang ASan/UBSan Debug, and MSVC static Debug all pass call-query
`32/32` and canonical-type-graph `19/19`, with real exit 0. Clang used
`ASAN_OPTIONS=detect_leaks=1:halt_on_error=1` and
`UBSAN_OPTIONS=halt_on_error=1`.

All three broad semantic-analyzer runners reach the end and report the same
nine existing functional failures. The Clang run no longer reports the call
producer use-after-free and reaches LeakSanitizer, which reports 448 bytes in
eight allocations. The three analyzer processes therefore still exit 1. This
slice closes the call-type lifetime defect, not full analyzer acceptance.

The remaining analyzer cases are unannotated function return detail, using
cleanup/template segments, local reference lookup, local rich hover,
compile-time test/lambda symbols, generic function detail, ownership-qualified
signatures, fail-closed signature display, and generic type detail.

## 状态与产出记录

- 开始时间：2026-09-07 08:33:00 +08:00。
- 实际完成时间：2026-09-07 14:22:00 +08:00。
- 状态：Task 7.71 修复、确定性 RED/GREEN 回归及三工具链窄验证完成。
- 完成项目：保存 canonical declared-function validity；新增强制类型数组搬迁回归；
  记录调用事实发布的 ownership 与 exactness 合同。
- 验证命令：各缓存执行 `cmake --build <build> --target zr_vm_semantic_query_calls_test
  zr_vm_canonical_type_graph_test zr_vm_language_server_semantic_analyzer_test -j 2`，
  随后直接运行对应 executable。原始输出保存在 `.codex/task771-*`。
- 源码版本：基于 `7b451d31` 开始，验证完成时 HEAD 为 `bfdfc333` 的共享工作树；
  其它 core/parser/AOT/call-binding、stdio、
  benchmark overlay 不属于本子项。
- 产出路径：本记录、`semantic-call-fact-ownership.md`、call-fact producer、
  `test_semantic_query_calls.c` 与独立 lifetime regression header。
- 剩余门槛：local write/reference projection、其它既有
  analyzer/interface producer failures、Clang LSan，以及 Plan 03 Task 3/7/8 完整验收。
