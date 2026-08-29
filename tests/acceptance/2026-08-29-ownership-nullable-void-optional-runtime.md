# Ownership Nullable Void Optional Runtime Acceptance

## Status

- Date: 2026-08-29 (UTC+08:00)
- Focused status: `completed`
- Umbrella status: `validated_pending_full_acceptance`
- Design: `docs/superpowers/specs/2026-08-10-ownership-object-member-separation-design.md`
- Plan: `docs/superpowers/plans/2026-08-10-ownership-object-member-separation-implementation.md`

## Accepted contract

An absent nullable Shared receiver makes `receiver?.method(arguments)` skip the
complete call suffix. For a `void` method, the expression completes as a void
no-op: the method body is not entered and its arguments are not evaluated.

The focused regression obtains the nullable Shared value through the public
ownership path rather than constructing a synthetic null:

1. create a Unique resource and convert it with `share(owner)`;
2. create a Weak handle with `degrade(shared)`;
3. release the last Shared owner with `drop(shared)`;
4. obtain the absent nullable Shared value with `wake(weak)`;
5. execute `nullable?.consume(bump())`.

`consume` throws if entered and `bump` increments a visible counter. The test
requires the receiver to remain null, the counter to remain zero, and the
function to return normally. This closes the prior evidence gap between the
semantic `ZR_RECEIVER_GUARD_RESULT_VOID_NOOP` fact test and the existing Weak
runtime suffix-skipping tests.

No production change was required. The existing fact-driven receiver-guard
lowering already branches before the suffix, materializes the absent merge
value, and preserves the void-noop contract.

## Toolchain evidence

The fixed source baseline was committed `fa2dc6c`, with only
`tests/parser/test_ownership_intrinsic_member_separation.c` overlaid. This
excluded the concurrently moving L8 parser/LSP and unrelated worktree changes.
Each binary was executed directly after a static Debug build.

| Toolchain | Focused target | Direct result |
| --- | --- | ---: |
| GCC 11.4 | `zr_vm_ownership_intrinsic_member_separation_test` | 45/45, exit 0 |
| Clang 14 | `zr_vm_ownership_intrinsic_member_separation_test` | 45/45, exit 0 |
| MSVC 19.44 | `zr_vm_ownership_intrinsic_member_separation_test` | 45/45, exit 0 |

The new case is registered in the normal Unity runner and is neither filtered
nor ignored. Build success was not used as a substitute for process execution.

## Boundary

This record accepts only the nullable-void optional-call runtime boundary. It
does not promote the umbrella milestone: the stable integrated full graph,
tracked artifact replay, migration-inventory regeneration, final legacy-path
scan, and exact review remain required after the external L8 overlay settles.

## Cleanup

After validation, the task removed and verified absent the WSL source snapshot
`ownership-nullable-fa2dc6c`, its GCC and Clang build roots, and the earlier
discarded shared-tree GCC cache `ownership-nullable-void-gcc`. The matching
Windows source root, MSVC build root, and temporary source tar were sent to the
recycle bin and verified absent from their original paths. No persistent test
log was created, and unrelated shared `.codex/logs` and build caches were not
modified.
