# Span Inline Receiver Lowering

## Scope

This acceptance record covers the parser and interpreter path for structural
`Span<T>` receiver reads, indexed writes, and slices when the view is held in an
inline value. The fix preserves inline receiver layout and semantic facts while
lowering computed-member expressions, and keeps receiver storage separate from
the scalar result slot used by Span index reads.

The work is a narrow parser/lowering regression fix associated with
`docs/plans/ssa/05-data-layout/02-arrays-slices.md`. It does not close all of
05.02 or claim the broader arrays, slices, and storage milestone is complete.

## Baseline

The pre-fix shared-storage case `view[1] = 40` failed while the interpreter
loaded the structural Span fields. A GDB trace showed GET_MEMBER_SLOT reading
`source` from temporary receiver slot 8, which had no inline TypeLayout
(`typeLayoutId=UINT32_MAX`, value slot kind). The actual local Span was slot 6
with an inline TypeLayout (`typeLayoutId=0`, inline slot kind). The assignment
prefix had staged the inline value into an ordinary value slot.

The expression-result path had a related aliasing hazard: structural Span
index-get used one slot for both the inline receiver and the plain scalar index
result. GET_BY_INDEX requires a plain value destination, so that slot could
lose the receiver's inline layout before the field loads. Staging the root with
the SemIR-aware load also preserves view and loan facts; a raw stack copy did
not preserve the same fact identity.

## Test Inventory

- Focused `--inline-receiver` selector (five cases): inferred Span index write,
  explicit `container.Span<int>` index write, explicit typed direct index read,
  inferred Span slice, and explicit Span slice.
- The full `zr_vm_span_core_test` suite (23 cases), including the shared-storage
  assignment, inline slices, imported inline layout, constant and dynamic slice
  bounds, out-of-range access, read-only restrictions, structured view facts,
  owner/loan behavior, and pinned-close rejection.
- CTest registration `parser_span_inline_receiver` runs the focused selector.

`Span<InlineStruct>[index]` result-layout inference was not added or validated
by this slice.

## Tooling Evidence

The current source was built in the existing WSL Ninja cache. Ninja reported
14 completed build actions and linked `zr_vm_span_core_test`. The recorded
session did not retain Ninja, CTest, or compiler version strings.

```sh
ninja -C /mnt/d/tmp/zr_vm/close-proxy-core-red zr_vm_span_core_test -j8
LD_LIBRARY_PATH=/mnt/d/tmp/zr_vm/close-proxy-core-red/lib /mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_span_core_test --inline-receiver
LD_LIBRARY_PATH=/mnt/d/tmp/zr_vm/close-proxy-core-red/lib ctest --test-dir /mnt/d/tmp/zr_vm/close-proxy-core-red --output-on-failure -R '^parser_span_inline_receiver$'
LD_LIBRARY_PATH=/mnt/d/tmp/zr_vm/close-proxy-core-red/lib /mnt/d/tmp/zr_vm/close-proxy-core-red/bin/zr_vm_span_core_test
```

The focused test produced 5 tests and 0 failures. CTest reported 1/1 passed.
The full Span executable reported 23 tests and 0 failures. Negative tests print
their expected compiler diagnostics as part of the passing suite.

## Results

- The first RED trace localized the lost inline layout to assignment receiver
  staging; the follow-up lowering changes preserve the typed receiver and its
  SemIR facts.
- The index-get lowering now writes to a distinct result slot, leaving the
  inline receiver available for its structural field loads.
- A focused CTest entry covers the five new source-level cases.
- The final focused selector, registered CTest, and full Span suite all pass.

## Acceptance Decision

Accepted for this scoped Span inline receiver fix: selector 5/0, focused CTest
1/1, and full Span suite 23/0 on the current source. The parent milestone
05.02 remains open; generic inline-element result typing and the remaining
arrays/slices milestone coverage are outside this acceptance.
