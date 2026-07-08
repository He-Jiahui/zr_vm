# AOT 07-S6 GC Root Frame Static Map Push

时间：2026-07-06 04:21:03 +08:00

## 状态

完成一个 07-S6 子切片：对已证明需要 GC root frame 的生成函数，`ZrCore_Gc_AotRootFramePush(...)`
不再通过 `zr_aot_context.methodInfo->gcRootMap` 做运行期查找，而是直接绑定当前函数的静态
`&zr_aot_gc_root_map_%u`。

07~12 总目标未完成；exports/frame cleanup、in/out writeback、性能计数和完整 07-S6/07~12 验收仍待后续。

## 完成项目

- `backend_aot_write_c_frame_setup()` 在 `includeGcRootFrame` 为真时发出
  `/* zr_aot_gc_root_frame_static_map_push */`。
- 生成的 root frame push 现在直接使用 `&zr_aot_gc_root_map_%u`。
- frame setup source contract 要求静态 push marker，并禁止 `zr_aot_context.methodInfo->gcRootMap`。
- value-type smoke 对 ref struct 生成物要求 root slots/root map、静态 push marker、直接 static map 实参与 pop。
- POD struct 生成物继续要求无 root map、无 root slots、无 root-frame push/pop、无静态 push marker。

## RED

- WSL GCC `zr_vm_aot_c_frame_setup_contracts_test`：1 test / 1 failure，缺少
  `zr_aot_gc_root_frame_static_map_push` 且仍包含 methodInfo root-map lookup。
- WSL GCC `zr_vm_aot_c_value_type_shared_library_smoke_test`：5 tests / 1 failure，ref struct 生成物尚未包含静态
  root map push marker。

## GREEN

- WSL GCC `zr_vm_aot_c_frame_setup_contracts_test`：1/0。
- WSL GCC `zr_vm_aot_c_value_type_shared_library_smoke_test`：5/0。
- WSL Clang `zr_vm_aot_c_frame_setup_contracts_test`：1/0。
- WSL Clang `zr_vm_aot_c_value_type_shared_library_smoke_test`：5/0。
- MSVC Debug `zr_vm_aot_c_frame_setup_contracts_test`：1/0。
- MSVC Debug `zr_vm_aot_c_value_type_shared_library_smoke_test`：5 tests / 0 failures / 1 expected Unix-only ignore。

## 生成物检查

- `build-wsl-gcc/tests_generated/aot_c_value_type_shared_library/gc_descriptor/src/ref_struct.c`
  含 `zr_aot_gc_root_frame_static_map_push`，并在两处 root-frame push 中直接传入
  `&zr_aot_gc_root_map_0` / `&zr_aot_gc_root_map_1`。
- `build-wsl-gcc/tests_generated/aot_c_value_type_shared_library/gc_descriptor/src/pod_struct.c`
  未发现 root-frame marker、push/pop、root map 或 root slots。
