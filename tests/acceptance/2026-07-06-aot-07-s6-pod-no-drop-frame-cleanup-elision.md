# AOT 07-S6 POD No-Drop Frame Cleanup Elision

时间：2026-07-06 04:34:25 +08:00

## 状态

完成一个 07-S6 子切片：inline-struct frame slot 的静态 type layout 若已解析且
`dropKind == ZR_TYPE_LAYOUT_DROP_KIND_NONE`，生成 C 不再输出 frame cleanup scaffolding。

07~12 总目标未完成；GC 压力/root 正确性、exports/frame cleanup 更广覆盖、in/out writeback、性能计数和完整
07-S6/07~12 验收仍待后续。

## 完成项目

- `backend_aot_c_frame_cleanup_would_emit_for_function(state, functionIr)` 按函数原型 frame type layout 判断
  cleanup 是否需要输出。
- `backend_aot_write_c_frame_cleanup(file, state, functionIr)` 使用同一 resolved-layout 谓词，避免 prologue 与 exit
  cleanup 判定分叉。
- resolved `dropKind == NONE` 的 inline-struct frame slot 被跳过。
- layout 解析失败时仍保守保留 cleanup。
- POD value-type 生成物禁止 `zr_aot_frame_started`、`zr_aot_skip_drop_slot`、`zr_aot_value_frame_drop` 和
  `ZrCore_TypeLayout_DropInline`。
- ref/drop layout 生成物仍要求保留 cleanup path。

## RED

- WSL GCC `zr_vm_aot_c_source_contracts_test`：24 tests / 1 failure，source contract 要求新的 function-aware cleanup API
  后旧实现未满足。
- WSL GCC `zr_vm_aot_c_frame_setup_contracts_test`：1 test / 1 failure，function body 仍使用旧 cleanup predicate。
- WSL GCC `zr_vm_aot_c_value_type_shared_library_smoke_test`：5 tests / 1 failure，POD generated C 仍含 frame cleanup
  变量/helper/drop 调用。

## GREEN

- WSL GCC `zr_vm_aot_c_source_contracts_test`：24/0。
- WSL GCC `zr_vm_aot_c_frame_setup_contracts_test`：1/0。
- WSL GCC `zr_vm_aot_c_value_type_shared_library_smoke_test`：5/0。
- WSL Clang `zr_vm_aot_c_source_contracts_test`：24/0。
- WSL Clang `zr_vm_aot_c_frame_setup_contracts_test`：1/0。
- WSL Clang `zr_vm_aot_c_value_type_shared_library_smoke_test`：5/0。
- MSVC Debug `zr_vm_aot_c_source_contracts_test`：24/0。
- MSVC Debug `zr_vm_aot_c_frame_setup_contracts_test`：1/0。
- MSVC Debug `zr_vm_aot_c_value_type_shared_library_smoke_test`：5 tests / 0 failures / 1 expected Unix-only ignore。

## 生成物检查

- `build-wsl-gcc/tests_generated/aot_c_value_type_shared_library/gc_descriptor/src/pod_struct.c`
  未发现 `zr_aot_value_frame_drop`、`ZrCore_TypeLayout_DropInline`、`zr_aot_frame_started` 或
  `zr_aot_skip_drop_slot`；仅保留 `.dropKind = 0u`。
- `build-wsl-gcc/tests_generated/aot_c_value_type_shared_library/gc_descriptor/src/ref_struct.c`
  仍含 `.dropKind = 1u`、`zr_aot_frame_started`、`zr_aot_skip_drop_slot`、`zr_aot_value_frame_drop` 和
  `ZrCore_TypeLayout_DropInline`，证明 drop-needed layout 未被误删。
