# Ownership Nullable Callable Direct Guard Acceptance

## Status

- Date: 2026-08-29 (UTC+08:00)
- Focused status: `completed`
- Umbrella status: `validated_pending_full_acceptance`
- Design: `docs/superpowers/specs/2026-08-10-ownership-object-member-separation-design.md`
- Plan: `docs/superpowers/plans/2026-08-10-ownership-object-member-separation-implementation.md`

## Accepted contract

An absent ordinary nullable callable follows the same target-access rules as a
nullable object receiver:

- `callable?.(arguments)` returns null and evaluates no argument;
- `callable(arguments)` throws catchable `NullReferenceError` before evaluating
  an argument.

The regression obtains the callable through a real resource `const @call`
member and the public ownership transitions. It creates a Weak handle, drops
the last Shared owner, and calls `wake(weak)` to obtain an absent nullable
Shared callable. Both optional and direct calls use `bump()` as an observable
argument. The test requires the optional result to be null, the direct error to
match `NullReferenceError` rather than its `RuntimeError` base, and the side
effect count to remain zero.

This closes the prior gap between the nullable callable receiver-guard fact
test and the existing Weak callable runtime test. No production change was
required: the canonical null receiver guard already dominates argument
evaluation for both access modes.

## Toolchain evidence

The fixed source baseline was committed `39735d4`, with only
`tests/parser/test_ownership_intrinsic_member_separation.c` and
`tests/parser/test_ownership_optional_callable_cases.h` overlaid. Each static
Debug binary was executed directly after its build.

| Toolchain | Focused target | Direct result |
| --- | --- | ---: |
| GCC 11.4 | `zr_vm_ownership_intrinsic_member_separation_test` | 46/46, exit 0 |
| Clang 14 | `zr_vm_ownership_intrinsic_member_separation_test` | 46/46, exit 0 |
| MSVC 19.44 | `zr_vm_ownership_intrinsic_member_separation_test` | 46/46, exit 0 |

The new case is part of the normal Unity registration list and is not ignored
or selected through an environment filter.

## Boundary

This record accepts only direct and optional access on an absent ordinary
nullable callable. It does not promote the umbrella milestone: a stable
integrated full graph, tracked artifact replay, migration-inventory
regeneration, final legacy-path scan, and exact review remain required after
the external L8 overlay settles.

## Cleanup

After validation, the task removed and verified absent the WSL source snapshot
`ownership-nullable-callable-39735d4` and its GCC and Clang build roots. The
matching Windows source root, MSVC build root, and temporary source tar were
sent to the recycle bin and verified absent from their original paths. No
persistent test log was created, and unrelated shared caches and `.codex/logs`
were not modified.
