---
date: 2026-06-06
area: aot-c-value-semir
status: accepted
---

# AOT Value SemIR Inline Field Layout Copy

## Scope

This slice tightens generated C lowering for value-SemIR nested inline struct field load/store.

Before this change, `zr_aot_value_exec_field_inline_struct_load` and
`zr_aot_value_exec_field_inline_struct_store` always emitted raw `memmove` between the nested field
span and the inline struct slot span. That is correct for POD field layouts, but it bypasses the
layout-aware copy path needed for nested fields that contain embedded `SZrTypeValue`, ownership, GC,
or other non-POD payload.

The generated C now resolves the field `SZrTypeLayout`, checks the emitted field byte size, uses
`ZrCore_TypeLayout_CanRawCopy(zr_aot_field_layout)` to keep POD transfers on `memmove`, and routes
non-POD transfers through `ZrCore_TypeLayout_CopyInline(state, ...)`.

## RED

Added `test_aot_c_value_semir_inline_struct_field_transfer_uses_layout_copy_for_non_pod` to
`tests/parser/test_aot_c_value_semir_contracts.c`.

Observed failure before production changes:

```text
Missing source contract text: const SZrTypeLayout *zr_aot_field_layout =
2 Tests 1 Failures 0 Ignored
```

## GREEN

Updated `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_semir_fields.c`:

- `LOAD_VALUE` nested inline field transfer emits a field-layout gate.
- `STORE_VALUE` nested inline field transfer emits the same field-layout gate.
- POD nested field layouts still use overlap-safe `memmove`.
- Non-POD nested field layouts call `ZrCore_TypeLayout_CopyInline(state, ...)`.

Updated `tests/parser/test_aot_c_source_contracts.c` so the aggregate AOT source contract also
locks `zr_aot_field_layout`, `ZrCore_TypeLayout_CanRawCopy(zr_aot_field_layout)`,
`zr_aot_value_exec_field_inline_struct_copy`, and `ZrCore_TypeLayout_CopyInline(state, ...)`.

## Validation

- GCC `build-wsl-gcc`:
  - `zr_vm_aot_c_value_semir_contracts_test`: 2 passed, 0 failed.
  - `zr_vm_aot_c_source_contracts_test`: 17 passed, 0 failed.
  - `zr_vm_aot_c_shared_library_smoke_test`: 8 passed, 0 failed.
- Clang `build-wsl-clang`:
  - `zr_vm_aot_c_value_semir_contracts_test`: 2 passed, 0 failed.
  - `zr_vm_aot_c_source_contracts_test`: 17 passed, 0 failed.
  - `zr_vm_aot_c_shared_library_smoke_test`: 8 passed, 0 failed.
- MSVC `build-msvc`:
  - `zr_vm_aot_c_value_semir_contracts_test`: 2 passed, 0 failed.
  - `zr_vm_aot_c_source_contracts_test`: 17 passed, 0 failed.
  - `zr_vm_aot_c_shared_library_smoke_test`: 8 ignored Unix-only runtime checks, target built.

Production scans:

- No `ZrLibrary_AotRuntime_` references in `backend_aot_c_value_semir_fields.c`.
- Field module contains `zr_aot_field_layout`, `ZrCore_TypeLayout_CanRawCopy(zr_aot_field_layout)`,
  `zr_aot_value_exec_field_inline_struct_copy`, and `ZrCore_TypeLayout_CopyInline(state, ...)`.
- `git diff --check` produced only existing LF-to-CRLF warnings.

## Remaining Risk

- LLVM still needs parity for value-SemIR field lowering.
- Source-level generated-C execution for non-POD nested inline field copy remains a future fixture.
- Whole-frame non-POD initialization/drop/finalization is still tracked separately.
