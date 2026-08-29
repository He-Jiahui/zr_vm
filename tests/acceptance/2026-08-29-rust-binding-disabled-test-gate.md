# Rust Binding Disabled Test Gate Acceptance

## Status

- Date: 2026-08-29 (UTC+08:00)
- Focused status: `completed`
- Umbrella status: `validated_pending_full_acceptance`
- Design: `docs/superpowers/specs/2026-08-10-ownership-object-member-separation-design.md`
- Plan: `docs/superpowers/plans/2026-08-10-ownership-object-member-separation-implementation.md`

## Failure

The fixed `dbc182c` GCC shared Debug full-graph preflight configured with
`BUILD_RUST_BINDING=OFF` but still created
`zr_vm_rust_binding_api_test`. The `ALL` build then failed while linking the
test because `zr_vm_rust_binding_shared` did not exist.

The same invalid target could be created when `BUILD_RUST_BINDING=ON` but the
Rust binding module was skipped because its required network provider target
was unavailable.

## Accepted contract

The Rust binding API test target is created only when the implementation
target selected by `BUILD_SHARED_LIB` exists:

- shared builds require `zr_vm_rust_binding_shared`;
- static builds require `zr_vm_rust_binding_static`;
- disabling or dependency-skipping the Rust binding module omits the test
  target from `ALL` instead of passing a missing target name to the linker.

This gate does not change Rust binding behavior and does not edit the binding
module. When the selected implementation target exists, the API test keeps its
existing sources, include paths, and parser/core/library/binding link set.

## Validation

The fixed source baseline was `dbc182c`, with only `tests/CMakeLists.txt`
overlaid. GCC 11.4 shared Debug configuration with `BUILD_RUST_BINDING=OFF`
reported the binding module skipped, omitted
`zr_vm_rust_binding_api_test` from the Ninja target graph, and completed the
entire `ALL` build. This closes the original missing-library linker failure.

Configure-only target-presence checks covered every gate branch:

| Library mode | Rust binding | Network provider | Expected API test target | Result |
| --- | --- | --- | --- | --- |
| shared | enabled | enabled | present | present, exit 0 |
| shared | disabled | enabled | absent | absent, exit 0 |
| static | enabled | enabled | present | present, exit 0 |
| static | disabled | enabled | absent | absent, exit 0 |
| shared | enabled | disabled | absent | absent, exit 0 |

The registered GCC graph then ran 135 CTest entries: 128 passed and 7 failed.
Those failures are retained as full-acceptance inputs, not attributed to this
gate:

- `language_pipeline`: signed-branch AOT shared-library smoke failure;
- `language_server`: two semantic analyzer fact/type failures;
- `language_server_stdio_smoke`: missing short-circuit diagnostic;
- `aot_c_value_construction_guardrail`: generated function runtime failure;
- `aot_c_typed_scalar`: stale forbidden generated-frame marker assertion;
- `aot_c_method_info_signature`: return-type signature assertion;
- `debug_expression_diagnostics`: compiled function-call fact assertion.

The LSP/debug/semantic failures intersect active external L8 work or existing
dirty paths. The four AOT failures are independent pre-existing fixed-baseline
failures and remain separate support-first work; none is hidden or weakened by
this CMake change.

## Boundary

This record accepts only the CMake feature gate found during ownership
full-graph preflight. It does not promote the ownership umbrella milestone;
the stable integrated three-toolchain graph, tracked artifact replay,
migration-inventory regeneration, final legacy-path scan, and exact review
remain required.

## Cleanup

After validation, the WSL fixed source snapshot, GCC full build root, and five
configure-matrix build roots were removed and verified absent. The matching
Windows transfer tar was sent to the recycle bin and verified absent from its
original path. No persistent task log was created, and unrelated shared caches
and repository logs were not modified.
