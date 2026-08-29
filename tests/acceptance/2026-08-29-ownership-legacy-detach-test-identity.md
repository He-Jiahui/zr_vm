# Ownership Legacy Detach Test Identity Acceptance

## Status

- Date: 2026-08-29 (UTC+08:00)
- Focused status: `completed`
- Umbrella status: `validated_pending_full_acceptance`
- Design: `docs/superpowers/specs/2026-08-10-ownership-object-member-separation-design.md`
- Plan: `docs/superpowers/plans/2026-08-10-ownership-object-member-separation-implementation.md`

## Accepted contract

The runtime symbol `ZrLibrary_AotRuntime_OwnDetach` and numeric
`OWN_DETACH` opcode exist only for legacy artifact compatibility. Current
source uses `intoGc(owner)`, emits `OWN_INTO_GC_BOX`, and lowers through
`ZrLibrary_AotRuntime_OwnIntoGcBox`.

The active resource runner previously named its compatibility case
`test_aot_own_detach_consumes_resource_unique_into_gc_box`, which did not
distinguish a preserved artifact helper from the current language operation.
It is now named
`test_aot_legacy_detach_helper_preserves_gc_box_artifact_compatibility`.
The body remains a real compatibility execution: the helper consumes a Unique
resource, clears the source, returns a GC box without an ownership qualifier,
and leaves no ownership root behind.

No production symbol or behavior changed. Deleting or aliasing the helper into
the current source surface would break the deliberate numeric-artifact
boundary, so the modernization is limited to the Unity case identity.

## Toolchain evidence

The fixed source baseline was committed `82633fa`, with only
`tests/parser/test_resource_unique_drop.c` overlaid. Each static Debug binary
was executed directly after its focused build.

| Toolchain | Focused target | Direct result |
| --- | --- | ---: |
| GCC 11.4 | `zr_vm_resource_unique_drop_test` | 20/20, exit 0 |
| Clang 14 | `zr_vm_resource_unique_drop_test` | 20/20, exit 0 |
| MSVC 19.44 | `zr_vm_resource_unique_drop_test` | 20/20, exit 0 |

The renamed case remains in the normal Unity registration list and no case is
ignored or selected through an environment filter.

## Boundary

This record accepts only the legacy compatibility test identity. It does not
promote the umbrella milestone: the stable integrated full graph, tracked
artifact replay, migration-inventory regeneration, final legacy-path scan, and
exact review remain required after the external L8 overlay settles.

## Cleanup

After validation, the task removed and verified absent the WSL source snapshot
`ownership-legacy-detach-82633fa` and its GCC and Clang build roots. The
matching Windows source root, MSVC build root, and temporary source tar were
sent to the recycle bin and verified absent from their original paths. No
persistent test log was created, and unrelated shared caches and `.codex/logs`
were not modified.
