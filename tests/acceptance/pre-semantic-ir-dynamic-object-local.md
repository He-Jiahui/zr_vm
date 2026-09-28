# Pre-SemanticIR Dynamic OBJECT Local Reads

## Scope

Keep top-level reads of unannotated locals with a valid source `SymbolId` but
no canonical `TypeId` on the legacy `GET_STACK` path while no SemanticIR source
CFG is active. Do not fabricate a semantic type or Place for the value. An
active source CFG still cannot accept this incomplete load; the compiler keeps
the startup barrier and analysis-only graph boundary. Explicitly typed locals
continue through normal SemanticIR `LOAD` lowering.

## Baseline

The initial WSL GCC 11.4.0 direct run of
`/mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_pre_semantic_ir_test`
reported `117 Tests 1 Failures 0 Ignored`. The only failure was
`test_unannotated_dynamic_object_local_read_keeps_legacy_cfg`, at
`dynamic_marker_source_cfg.zr:2:13-2:19`, with
`Failed to lower load through pre-execution Semantic IR`. The explicitly typed
`int` control passed. The remaining 116 tests passed.

## Test Inventory

- `test_unannotated_dynamic_object_local_read_keeps_legacy_cfg` checks the
  source binding identity and invalid canonical type, legacy `GET_STACK`, a
  blocked startup barrier, inactive source CFG, no SemanticIR load at the
  source read, no SemanticIR return, and structural validation of the
  analysis-only graph.
- `test_explicit_scalar_local_keeps_source_cfg_load` is the positive control
  for a canonical scalar local and checks SemanticIR `LOAD` and `RETURN`.
- The full type-inference runner exercises the previously added dynamic
  OBJECT return inference case alongside generic member substitution and
  cleanup registration.
- `ssa_source_cleanup_cfg` is the registered CTest for source cleanup
  regression coverage.

## Tooling Evidence

- WSL Ubuntu 22.04, GCC 11.4.0, CMake 3.22.1, Ninja 1.10.1.
- Build cache: `D:\tmp\zr_vm\close-proxy-core-red`.
- The current-source pre-semantic target build completed successfully after
  CMake regenerated its source list. It linked six dependencies and the test
  runner. The D cache was then configured with
  `CMAKE_SUPPRESS_REGENERATION=ON` to avoid rescanning the shared source tree
  on incremental builds.
- RED command: `wsl.exe --exec
  /mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_pre_semantic_ir_test`.
- Final pre-semantic target build command:
  `wsl.exe --exec cmake --build
  /mnt/d/tmp/zr_vm/close-proxy-core-red --target zr_vm_pre_semantic_ir_test
  -- -j4` (exit 0; two incremental steps after the local test-helper warning
  was fixed).
- Type-inference adjacent target build command:
  `wsl.exe --exec cmake --build
  /mnt/d/tmp/zr_vm/close-proxy-core-red --target zr_vm_type_inference_test
  zr_vm_ssa_source_cleanup_cfg_test -- -j4` (exit 0; seven steps).
- The first candidate cache, `D:\tmp\zr_vm\ssa-optional-member-gcc`, began
  an 829-edge full rebuild after glob regeneration. It was safely stopped at
  240/829 with no compile error, and its D-drive objects were retained. No
  artifacts were moved across drives.

## Results

- RED: the dynamic OBJECT local test failed on the reported load-lowering
  error; the explicit `int` control and 116 other tests passed.
- GREEN: the final direct pre-semantic runner reported
  `117 Tests 0 Failures 0 Ignored`. The new fixture helper's discarded-const
  warning was removed by matching its path parameter to the native-string API
  type; the final pre-semantic build emitted no warning.
- `ctest -R pre_semantic_ir` cannot select this target because the CMake test
  file does not register a pre-semantic CTest. The direct Unity runner above is
  the focused gate. The same CTest command returned `No tests were found`.
- The adjacent type-inference runner reported `125 Tests 4 Failures 0 Ignored`.
  The failures were:
  - `test_using_statement_compilation_records_cleanup_plan` failed at
    `tests/parser/test_type_inference.c:3072`, `TEST_ASSERT_FALSE(cs->hasError)`
    after compiling `using resource`; the compiler reported
    `Failed to register using cleanup proxy`. Its `resource` local is
    string-inferred.
  - `test_using_statement_cleanup_plan_records_ownership_generic_kind` failed
    at `tests/parser/test_type_inference.c:3163`, the same `cs->hasError`
    assertion after compiling `using resource`, with the same cleanup-proxy
    diagnostic. Its local is explicitly typed `Unique<Resource>`.
  - `test_type_inference_source_generic_class_member_substitutes_closed_field_type`
    failed at `tests/parser/test_type_inference.c:4939`, where direct
    `ZrParser_ExpressionType_Infer` was expected to return true for
    `new Box<int>().value`.
  - `test_type_inference_source_generic_inheritance_substitutes_closed_base_member_type`
    failed at `tests/parser/test_type_inference.c:6174`, where direct
    `ZrParser_ExpressionType_Infer` was expected to return true for
    `new Derived<int>().value`.

  The two cleanup fixtures have inferred or explicit canonical local types,
  while the changed fallback predicate requires a binding with invalid
  `TypeId`; their failures occur on cleanup-proxy registration, so these
  fixtures do not exercise this fallback. Their underlying cause is not
  established here and needs separate diagnosis. The two generic-member
  failures call type inference directly and are also listed in
  `dynamic-object-return-inference.md`. The target build also reported
  missing-`passingMode` initializer warnings in `test_type_inference.c`.
- `ctest -R '^ssa_source_cleanup_cfg$' --output-on-failure --no-tests=error`
  passed 1/1.

## Acceptance Decision

Accepted for the scoped top-level dynamic OBJECT local read fallback. The
focused runner passes and the source cleanup CTest passes. The adjacent type
suite remains partially failing on the four unrelated assertions listed
above; no broad type-inference or cleanup-proxy repair is included here.
