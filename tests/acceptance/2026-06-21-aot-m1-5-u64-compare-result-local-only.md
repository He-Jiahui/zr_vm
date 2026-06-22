# AOT M1.5 07-S2 U64 Compare Bool Result Local-Only

Date: 2026-06-21 05:18:36 +08:00
Status: partial 07-S2 slice complete

## Scope

This slice removes frame destination/result materialization from the focused unsigned compare bool results when every reachable later consumer can use the maintained bool scalar local, or the bool value is killed by reset-stack-null.

For the focused typed scalar source:
- unsigned compare expression for `unsignedSum > unsignedRight`
- loop guard compare `unsignedInverted <= unsignedRight`
- later `JUMP_IF_BOOL_FALSE` consumer of that guard

the generated C now emits only:
- `zr_aot_b14 = (TZrBool)(zr_aot_u8 > zr_aot_u7);`
- `zr_aot_b23 = (TZrBool)(zr_aot_u21 <= zr_aot_u7);`

Those compare blocks no longer declare `SZrTypeValue *zr_aot_destination`, create `zr_aot_u_result`, or write the bool payload back through `frame.slotBase[14/23].value`.

The bool result skip proof extends the existing reachable-consumer scan to bool locals. It recognizes local `JUMP_IF_BOOL_FALSE`, bool stack-copy, and reset-stack-null kills, while preserving conservative rejection for unknown frame-dependent reads. The same scan remains used by u64 result local-only slices.

## RED/GREEN

- RED: `zr_vm_aot_c_typed_scalar_test` failed when generated product still contained the old `dstSlot=14` u64 compare destination block.
- GREEN: typed scalar passed after unsigned compare result local-only emission used bool result-skip proof.
- GREEN: source contracts passed.
- GREEN: registered CTest passed.
- CHECK: `git diff --check` exited 0 and reported only existing LF/CRLF conversion warnings.

## Remaining

This is not full 07-S2. More scalar result materialization, broader primitive constant frame writes, direct return/result frame fallbacks, generic float copy/type checks, prologue/frame setup, reset-stack-null frame writes, and boundary local restoration remain.

Note: `backend_aot_c_scalar_locals.c` is above the modularization threshold; this slice kept the proof in-place because it extends the same scalar-local result liveness boundary. The smallest follow-up split is extracting result-skip/liveness proof helpers into a dedicated scalar liveness module.
