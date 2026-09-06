---
related_code:
  - zr_vm_library/src/zr_vm_library/file.c
  - tests/library/test_file_list.c
  - tests/cmake/zr_vm_file_list_tests.cmake
related_module_docs:
  - docs/library-and-builtins/zr-system-submodules.md
plan_sources:
  - docs/plans/lsp/optimize/03-canonical-semantic-query.md
  - docs/plans/lsp/astra.md
doc_type: milestone-record
---

# Plan 03 Task 7.69: Empty File Lists Do Not Call `qsort` With Null Storage

## Failure and Contract

The aligned Clang LSP replay reached the native module and project discovery
paths after Task 7.68. A separate UBSan run then exposed the lower library
contract: `ZrLibrary_File_ListDirectory` and `ZrLibrary_File_Glob` called
`qsort` with `entries == NULL` when a successful query returned zero items.
The C library declares the base pointer non-null even when the count is zero.

Both functions now sort only when `count > 1`. Empty successful lists continue
to use the existing zeroed representation (`entries == NULL`, `count == 0`,
`capacity == 0`), and one-item lists remain unchanged without a needless sort.
Non-empty multi-item results retain lexical ordering by normalized full path.

## RED/GREEN

`tests/library/test_file_list.c` creates and removes an isolated generated root
for every case. It covers empty direct and recursive directory listing, empty
direct and recursive glob, no-match glob, single-entry list/glob, and lexical
ordering for direct, recursive, and wildcard results.

Before the guard, the first Clang case reports UBSan at `file.c:1289:11` and
the empty glob reports UBSan at `file.c:1336:11`; strict CTest exits `8` even
though Unity's five assertions continue. After the guard, all five cases pass
with `UBSAN_OPTIONS=halt_on_error=1`.

## Verification

- GCC Debug CTest `file_list`: `5/5`, exit `0`.
- Clang ASan/UBSan CTest `file_list`: `5/5`, exit `0`; no UBSan report.
- MSVC static Debug CTest `file_list`: `5/5`, exit `0` through VsDevCmd.
- The rebuilt Clang complete LSP interface no longer reports the two
  `file.c` qsort UBSan locations. It still has the eight frozen functional
  failures and the previously recorded LSan report; those remain outside this
  lower-layer fix.
- `zr_vm_system_fs_test` was also replayed. GCC and MSVC retain one existing
  stream-modes failure; Clang stops earlier at a concurrent call-binding
  null-function UBSan in `native_binding_call_binding.c:525`. That code is not
  part of this submilestone.

Commands from the repository root (Linux commands run in WSL):

```text
cmake --build .codex/build-lsp-opt-gcc --target zr_vm_file_list_test --parallel 4
cmake --build .codex/lsp-optimize-validation/clang-asan-current --target zr_vm_file_list_test --parallel 4
cmake --build .codex/lsp-optimize-validation/task767-msvc-static --target zr_vm_file_list_test --parallel 4
ctest --test-dir <build> -R '^file_list$' -V
```

## 状态与产出记录

- 开始时间：2026-09-07 07:24:50 +08:00（底层回归文件建立时间）。
- 实际完成时间：2026-09-07 07:34:39 +08:00。
- 状态：空结果排序保护、底层回归和三工具链 CTest 完成；Plan 03 Task 3、
  Task 7、Task 8 及完整 sanitizer/interface 门禁继续进行中。
- 完成项目：`ListDirectory`/`Glob` 的 `count > 1` 排序门禁；五项文件库回归；
  `zr.system.fs` 模块契约更新；严格 UBSan 复现和 GREEN 证据。
- 源码版本：基于 `aeb65985` 后的共享工作树；本记录、列出的代码、测试和模块文档同提交。
  并发 call-binding 与其他 core/parser/AOT overlay 不属于本子项。
- 产出路径：本记录、`tests/library/test_file_list.c`、
  `tests/cmake/zr_vm_file_list_tests.cmake`、`docs/library-and-builtins/zr-system-submodules.md`；
  日志位于 `.codex/lsp-optimize-validation/task769-*.log`。
- 剩余门槛：LSP interface 八项功能失败和 Clang 泄漏；并发 call-binding UBSan、
  stream-modes 失败的独立责任层；Task 3 sourceless/provider generation、Task 7
  完整 consumer 矩阵、Task 8 的 16-target、stdio/CLI、WASM/editor/performance 验收。
