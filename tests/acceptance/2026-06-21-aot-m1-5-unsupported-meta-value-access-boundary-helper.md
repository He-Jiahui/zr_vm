# AOT M1.5 07-S5 Unsupported Meta Value Access Boundary Helper

Timestamp: 2026-06-21 15:25:37 +08:00

Status: sub-slice complete; 07-S5 partial; M1.5/07 partial; 08-12 not started.

Plan alignment: `docs/plans/aot/07-codegen-register-model-and-environment-isolation.md`
section 6 requires VM/native marshaling and dynamic/deopt edges to stay behind fixed
boundary templates instead of expanding interpreter state in generated C. This slice
centralizes the currently unsupported meta value access boundary in
`ZrLibrary_AotRuntime_UnsupportedMetaValueAccess()`.

## Scope

- AOT C meta value access lowering for `META_GET`, `META_SET`, cached super meta access,
  and static cached super meta access.
- Runtime boundary support under `zr_vm_library` for the current unsupported failure
  path.
- Source-contract and generated shared-library smoke coverage under `tests/parser`.

## Baseline

- `backend_aot_write_c_unsupported_meta_value_access()` expanded opcode locals,
  primary/secondary `SZrTypeValue *` stack reads, member/cache locals, and a
  meta-specific `ZrCore_Debug_RunError(state, ...)` template directly into generated C.
- The global shared-library smoke expected that old generated-C failure text and failed
  once the helper-only shape landed.
- The repository checkout is heavily dirty with unrelated AOT, parser, runtime, and docs
  work; this slice only accepts the focused WSL evidence below.

## RED

- `zr_vm_aot_c_global_contracts_test` first failed 1/6 after the contract required
  `ZrLibrary_AotRuntime_UnsupportedMetaValueAccess(struct SZrState *state,` and a
  helper guard in `backend_aot_c_lowering_values.c`.
- `zr_vm_aot_c_global_shared_library_smoke_test` then failed 1/7 because the meta value
  access smoke still looked for the old generated `"unsupported AOT meta value access"`
  inline failure text.

## GREEN

- `ZrLibrary_AotRuntime_UnsupportedMetaValueAccess()` now validates the frame slot base,
  primary slot, and secondary slot before reporting the unsupported meta value access
  failure from runtime code.
- `backend_aot_write_c_unsupported_meta_value_access()` now emits only
  `zr_aot_value_unsupported_meta_value_access` plus one helper guard carrying the
  primary slot, secondary slot, member/cache index, and opcode name.
- The generated-C smoke now requires the helper guard and rejects the old opcode local,
  primary/secondary `SZrTypeValue *` locals, member/cache local, and meta-specific
  generated `RunError` template.

## Verification

- `zr_vm_aot_c_global_contracts_test` passes 6/0.
- `zr_vm_aot_c_global_shared_library_smoke_test` passes 7/0.
- Broader WSL GCC focused group passes: source contracts 19/0, return contracts 1/0,
  frame setup contracts 1/0, typed scalar 1/0, value SemIR contracts 4/0, call contracts
  4/0, call shared-library smoke 3/0, global contracts 6/0, and global shared-library
  smoke 7/0.
- `ctest -R 'aot_c_(typed_scalar|call_shared_library|global_(contracts|shared_library_smoke))'`
  still matches only the registered `aot_c_typed_scalar` test in this build and passes
  1/1.
- Generated meta value access boundary C contains six
  `ZrLibrary_AotRuntime_UnsupportedMetaValueAccess(state, ...)` guards for the meta
  access opcode variants and no generated `const char *zr_aot_opcode_name`,
  `SZrTypeValue *zr_aot_primary`, `SZrTypeValue *zr_aot_secondary`,
  `const TZrUInt32 zr_aot_member_or_cache_index`, or
  `ZrCore_Debug_RunError(state, "unsupported AOT meta value access: %s", ...)` template.

## Acceptance Decision

Accepted for this 07-S5 sub-slice. The unsupported meta value access VM boundary is now
centralized in the shared AOT runtime instead of being expanded inside generated C.

Remaining work: this is not final meta value execution. Typed-to-typed native signature
routing, in/out writeback, deopt/dynamic bridges, dynamic value access helperization, and
other remaining boundary templates remain later 07-S5 work; stages 08-12 are not started.
