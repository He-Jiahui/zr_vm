# AOT Scalar Branch SemIR Dataflow Acceptance

## Status

- Date: 2026-08-29 (UTC+08:00)
- Focused status: `completed`
- Umbrella status: `validated_pending_full_acceptance`
- Design: `docs/superpowers/specs/2026-08-10-ownership-object-member-separation-design.md`
- Plan: `docs/superpowers/plans/2026-08-10-ownership-object-member-separation-implementation.md`

## Failure

The fixed `9ecb19a` GCC full-graph replay failed
`aot_c_typed_scalar` because descriptor-free typed scalar source still contained
`/* zr_aot_generated_frame_setup */`. The test source defines signed and
unsigned constants, crosses two conditional regions, and then reuses those
values in bitwise and shift expressions.

Support-first probes found two transfer errors:

- `JUMP` has `operandExtra == 0`, but the scalar reaching-kind transfer treated
  every otherwise-unhandled `operandExtra` as a destination write. The first
  branch therefore erased the reaching `int64` definition for slot zero.
- scalar SemIR recorded the primitive result of generic bitwise/shift bytecode,
  then the same fallback path immediately cleared that destination. A following
  stack copy could not prove local provenance and forced dense value-frame use.

The generated instruction that exposed the second edge was unsigned
`BITWISE_AND` at exec index 35. Its operands were both proven `uint64`, while
`instruction_writes_primitive`, result-slot elision, and the following copy
source query all returned false. The later signed `BITWISE_AND` at index 47
exposed the slot-zero branch-state loss.

## Accepted contract

- Instructions classified by AOT step flags as control flow or return preserve
  reaching scalar writes after all real result-write handlers have run.
- Primitive `BITWISE_NOT`, binary bitwise, and shift opcodes with canonical
  scalar SemIR destinations update the reaching kind used by stack-copy and
  result-liveness queries.
- The SemIR fast path is limited to the bitwise/shift family whose scalar
  generated-C emitters synchronize their C local destination.
- Dynamic and unknown instructions still invalidate an apparent destination;
  frame-required values are not promoted from type annotations alone.
- Descriptor-free typed scalar products omit generated frame setup and retain
  interpreter/generated-product result parity.

## Validation

All Unix commands used a fixed `9ecb19a` source snapshot with only
`backend_aot_c_scalar_locals.c` overlaid. Tests were executed through CTest after
their exact targets were built.

| Toolchain | Focused targets | Result |
| --- | --- | --- |
| GCC 11.4 | value construction profile, SemIR typed opcode guardrails, AOT C value construction, typed scalar, method-info signature | 5/5 passed, exit 0 |
| Clang 14.0 | same five targets | 5/5 passed, exit 0 |
| MSVC 19.44 | same five targets | 5/5 passed, exit 0 |

MSVC compiled and linked the complete five-target dependency graph, including
the changed AOT parser object, before CTest ran. The Unix typed-scalar case
performs the generated-C compile/load/execute path,
rejects the frame marker, and compares its integer result with the interpreter.
The neighboring value-construction and method-signature cases guard against
silently removing frames where object/value storage or metadata still requires
them.

A clean GCC full-graph replay at committed `da114f9` then exposed one stale
generated-product assertion in the logical shared-library runner. The improved
fixed-point proof correctly selected the typed
`zr_aot_jump_if_bool_false_scalar_local` path for a proven bool condition, while
the test still required the weaker generic-truthiness marker. The assertion now
requires the typed marker and rejects the generic marker. The rebuilt runner
compiled and loaded every generated shared library and passed 6/6 with exit 0.
The aggregate `language_pipeline` advanced beyond this target before stopping
at an independently owned compile-time source-provenance baseline failure.

## Boundary

This leaf repairs a lower AOT support failure found during ownership full-graph
acceptance. It does not change ownership syntax, object member dispatch, weak
navigation, VM bytecode, public ABI, or artifact schema. The ownership umbrella
still requires replay on the final integrated baseline after independently
owned L8 parser/LSP work is stable.

The touched scalar-local implementation remains a large single-purpose module.
The coherent future extraction boundary is the complete scalar kind
transfer/fixed-point/query service; extracting only these opcode predicates
would create an artificial split.

## Cleanup

After validation, the fixed WSL source snapshot, GCC and Clang build roots, and
four exact temporary probe logs were removed and verified absent. The Windows
MSVC source/build roots and matching transfer tar were sent to the recycle bin;
all three original paths were verified absent. No repository build product or
persistent task log was created, and unrelated shared caches/logs were not
modified.
