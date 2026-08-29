# Ownership Member Kinds And Throw Profile Acceptance

## Status

- Date: 2026-08-29 (UTC+08:00)
- Focused status: `completed`
- Umbrella status: `validated_pending_full_acceptance`
- Design: `docs/superpowers/specs/2026-08-10-ownership-object-member-separation-design.md`
- Plan: `docs/superpowers/plans/2026-08-10-ownership-object-member-separation-implementation.md`

## Accepted contract

The reserved intrinsic spellings remain ordinary names after an object member
operator. A runtime class declares fields named `share` and `degrade`, plus
properties named `wake`, `intoGc`, and `drop`. Direct `.` reads return the
assigned or computed member values. Recursive instruction inspection requires
zero `OWN_SHARE`, `OWN_DEGRADE`, `OWN_WAKE`, `OWN_INTO_GC_BOX`, and `OWN_DROP`
instructions, so these accesses cannot silently lower as ownership control.

The CFG throw profile follows the receiver guard rather than the member name:

- direct Weak member access makes a matching `NullReferenceError` catch
  reachable;
- direct member access on the nullable Shared value returned by `wake(weak)`
  makes the catch reachable;
- optional Weak member access does not add a `NullReferenceError` edge;
- the explicit `wake(weak)` intrinsic itself does not add that edge.

No production change was required. The existing parser, receiver-guard fact,
compiler lowering, and CFG throw-profile implementation already satisfy these
boundaries. The focused changes only close missing declaration-kind and CFG
evidence.

## Toolchain evidence

The fixed source baseline was committed `5614e51`, with only
`tests/parser/test_ownership_intrinsic_member_separation.c` and
`tests/parser/test_cfg_throw_effects.c` overlaid. Each static Debug binary was
executed directly after its focused build.

| Toolchain | Ownership member separation | CFG throw effects |
| --- | ---: | ---: |
| GCC 11.4 | 47/47, exit 0 | 6/6, exit 0 |
| Clang 14 | 47/47, exit 0 | 6/6, exit 0 |
| MSVC 19.44 | 47/47, exit 0 | 6/6, exit 0 |

All new cases are registered in their normal Unity runners and none are
ignored or selected through an environment filter.

## Boundary

This record accepts ordinary field/property spellings and the focused
receiver-guard exception profile. It does not promote the umbrella milestone:
a stable integrated full graph, tracked artifact replay, migration-inventory
regeneration, final legacy-path scan, and exact review remain required after
the external L8 overlay settles.

## Cleanup

After validation, the task removed and verified absent the WSL source snapshot
`ownership-members-cfg-5614e51` and its GCC and Clang build roots. The matching
Windows source root, MSVC build root, and temporary source tar were sent to the
recycle bin and verified absent from their original paths. No persistent test
log was created, and unrelated shared caches and `.codex/logs` were not
modified.
