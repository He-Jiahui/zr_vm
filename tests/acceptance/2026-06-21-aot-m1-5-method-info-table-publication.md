# AOT M1.5 07-S5 MethodInfo Table Publication

Date: 2026-06-22 04:43:15 +08:00

## Scope

This slice publishes generated MethodInfo descriptors through the public compiled-module descriptor.

Covered:

- `ZR_VM_AOT_ABI_VERSION` is now `3u` because the compiled-module descriptor layout changed.
- `ZrAotCompiledModule` exposes `methodInfos` and `methodInfoCount`.
- The AOT C emitter writes `zr_aot_method_infos[]` and points each entry at a generated `zr_aot_method_info_N`.
- The generated `zr_aot_compiled_module` initializer publishes the MethodInfo table and function count.
- The generated shared-library smoke loads the module descriptor and verifies `module->methodInfos[0]->signature` is non-null.

Out of scope:

- Replacing helper-based static direct calls with typed-to-typed direct C calls.
- Replacing inline-struct typed call/return helper boundaries.
- `in` / `out` writeback templates.
- Deopt/dynamic bridge boxing templates.
- Marking 07-S5, 07, or M1.5 complete.

## RED

`zr_vm_aot_c_shared_library_smoke_test` first failed to compile because `ZrAotCompiledModule` did not yet expose `methodInfos` or `methodInfoCount`.

## GREEN

Focused WSL GCC validation:

- `zr_vm_aot_c_frame_setup_contracts_test`: 1/0.
- `zr_vm_aot_c_source_contracts_test`: 19/0.
- `zr_vm_aot_c_shared_library_smoke_test`: 8/0.

Broader WSL GCC focused group:

- call contracts: 4/0.
- call shared-library smoke: 5/0.
- shared-library smoke: 8/0.
- value-type shared-library smoke: 1/0.
- power contracts/smoke: 2/0 + 1/0.
- source contracts: 19/0.
- generic numeric contracts/smoke: 1/0 + 1/0.
- global smoke: 9/0.
- logical contracts/smoke: 4/0 + 4/0.
- typed scalar: 1/0.
- return contracts: 1/0.
- frame setup contracts: 1/0.

## Notes

This publishes the metadata table needed by later typed-to-typed native signature routing. It intentionally leaves actual direct typed C call/return lowering for a follow-up 07-S5 slice.
