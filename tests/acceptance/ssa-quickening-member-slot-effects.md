---
related_code:
  - zr_vm_common/include/zr_vm_common/zr_instruction_conf.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_expression_contiguous_view.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_quickening.c
plan_sources:
  - docs/plans/ssa/05-data-layout/02-arrays-slices.md
  - docs/plans/ssa/03-interpreter-binding/04-generated-fusion.md
tests:
  - tests/parser/test_span_core.c
  - tests/parser/test_compiler_w2_performance_quickening.c
doc_type: acceptance-record
status: accepted-with-adjacent-baseline-failure
---

# Quickening Member Slot Effects

## Scope

This acceptance covers accurate read/write slot classification for
`GET_MEMBER` and `GET_MEMBER_SLOT`, plus a no-growth Span regression that
depends on those effects. The code change is limited to the shared quickening
classifiers. It does not change operand encoding or implement the separate
typed `Array<int>.add` specialization gap noted below.

## RED Evidence

The regression `test_span_length_arithmetic_preserves_following_index_receiver`
uses a four-element Array and evaluates
`view.length * 10000000 + view[0] * 100000`. Before the classifier fix, the
`zr_vm_span_core_test` executable exited 1 with 23 passes and one failure. The
new case failed at `span_compound_view_read.zr:8` with
`Contiguous view index out of range`.

A GDB trace showed that the three lowered inline member loads initially shared
the receiver copy, but the quickened function retained the final `length`
receiver at an empty temporary after the temporary load had been removed. The
resulting length was zero, so the valid index zero failed its bounds check.
This failure occurred without any Array growth.

## Implementation

`compiler_quickening_instruction_may_read_slot` now treats only
`operand1[0]` as the receiver read for `GET_MEMBER` and `GET_MEMBER_SLOT`.
`operand1[1]` is member/cache metadata. The write classifier uses only
`operandExtra`, the result frame slot, for these instructions. Keeping these
cases out of generic operand groups prevents metadata bytes in the overlapping
operand union from terminating temporary-copy forwarding as false frame-slot
reads or writes.

## Verification

Builds ran only in `/mnt/d/tmp/zr_vm/close-proxy-core-red`:

```sh
wsl.exe --exec cmake --build /mnt/d/tmp/zr_vm/close-proxy-core-red --target zr_vm_span_core_test zr_vm_compiler_w2_performance_quickening_test -j 4
wsl.exe --exec env LD_LIBRARY_PATH=/mnt/d/tmp/zr_vm/close-proxy-core-red/lib /mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_span_core_test
wsl.exe --exec env LD_LIBRARY_PATH=/mnt/d/tmp/zr_vm/close-proxy-core-red/lib /mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_compiler_w2_performance_quickening_test
wsl.exe --exec ctest --test-dir /mnt/d/tmp/zr_vm/close-proxy-core-red -N -R 'span_core|w2_performance_quickening|quickening' --no-tests=error
```

The build exited 0. The direct Span suite exited 0 with 24 tests and no
failures, including the new regression. There is no CTest selector registered
for either executable; the filtered `ctest -N` probe listed zero tests.

The adjacent quickening suite exited 1 with 19 passes and one failure:
`test_w2_super_array_add_variable_value_elides_dead_receiver_setup` expected
`SUPER_ARRAY_ADD_INT`, but the compiled function retained
`KNOWN_NATIVE_MEMBER_CALL` at instruction 44. GDB showed the same opcode shape
with the original classifiers. An A/B build with only the classifier hunks
temporarily restored reported 18/20: the same Array add case failed, and
`test_w2_get_member_slot_direct_result_store_elides_temp_copy` also failed.
With the fix, that member-slot test passes, leaving only the baseline Array add
case. No code for that separate specialization was changed.

## Acceptance Decision

Accepted for this scoped classifier correction: the new no-growth regression
passes in the full Span suite, and A/B evidence shows that the remaining
quickening performance-suite failure is unchanged by the fix. The original
classifier also failed the adjacent member-slot forwarding test; the correction
removes that failure. The Array add specialization gap remains a separate
follow-up and is not represented as a passing gate.
