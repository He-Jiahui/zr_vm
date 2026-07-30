# 2026-07-30 AOT 07 ExecIR Frame ABI Verifier

## Scope

This sub-milestone establishes a fail-closed boundary for the existing function frame sidecar before AOT C consumes
it:

- ExecIR validates every function, including owners that code stripping will later remove;
- slot kind, TypeLayout identity, flags, parameter markers, alignment, storage bounds, and stack-slot uniqueness are
  checked before the sidecar is copied;
- indirect and borrowed aliases use their runtime binding size/alignment while legal overlapping payload aliases stay
  valid;
- retained frame rows publish deterministic owner/slot ABI facts and before/after/removed counts;
- malformed unreachable layouts fail before output and leave no generated file.

This is the A7.2 verifier/reporting prerequisite. It does not derive receiver, `in/ref/out`, return, spill, or
address-taken slots from `CallableContract`, and it does not claim C/LLVM frame-layout parity.

## RED Evidence

The frozen WSL GCC baseline passed 28/28 before the test edit. Against unchanged production, the expanded 29-test
suite had exactly two failures: the existing trim fixture could not find frame-layout counts/manifest, and the new
negative fixture observed that a malformed unreachable frame layout was accepted. The other 27 checks passed.
Independent review then identified an over-strict payload-alignment check for indirect/borrowed aliases. The added
30th test reproduced it: legal direct overlap passed, but the high-alignment payload backed by a lower-alignment
indirect binding was rejected. Moving the check to the final physical `storageAlign` restored that legal ABI shape.

## Coverage Inventory

- The primary fixture's root/value row plus two child rows become two retained rows after function trimming.
- Counts report `frameLayoutSlotsBefore=3`, `After=2`, and `Removed=1`.
- Retained rows are ordered owner 0 `value` before owner 1 `inline_struct`, with each owner as predecessor.
- Manifest order is ascending flat owner followed by source slot-layout index.
- A second fixture reports 4-to-3 trimming and preserves two owner-1 layouts that legally overlap at byte offset 0;
  the alias row retains flags `0x0001` after its owning direct row.
- High-alignment inline payload metadata remains legal when physical storage is an indirect binding (`0x0003`) or a
  borrowed binding (`0x0013`) with lower binding alignment.
- An all-empty property-accessor fixture reports zero frame rows and emits no manifest node.
- A non-power-of-two alignment on unreachable owner 2 fails before filtering.
- After restoring alignment, a byte span `[8,16)` outside an eight-byte frame also fails before filtering.
- Both malformed output paths are absent after the writer returns false.
- The adjacent generic-sharing suite retains its nonempty/null frame-table rejection.

## Validation

Effective source is commit `9ff0701` plus the exact five production/test overlays for this sub-milestone. Validation
reused:

- WSL source: `/tmp/zr_vm-aot12-20260730-0346-fe684d12-c`
- Windows source: `C:\Users\HeJiahui\AppData\Local\Temp\zr_vm-aot12-20260730-0346-fe684d12`
- GCC 11.4.0, Clang 14.0.0, and MSVC 19.44

Results:

- WSL GCC: code stripping 30/0; adjacent generic reference sharing 9/0.
- WSL Clang: code stripping 30/0.
- Windows MSVC: code stripping 30/0.
- Generated C contains exact 3-to-2 and 4-to-3 deltas, stable multi-owner/slot and alias rows, and a zero-row
  manifest; both malformed files are absent.
- The frozen GCC `source_contracts` target is 21/24 and `frame_setup_contracts` is 0/1. All four failures are existing
  source-text contract drift at their text-scanning assertions and do not execute the new ExecIR verifier. No source
  contract, frame setup, parser, core runtime, or CMake file is changed by this sub-milestone.
- MSVC retains only the existing MSB8029 warning caused by the isolated build residing below `%TEMP%`.
- Independent review's payload-alignment P2 and missing alias/order/empty-manifest coverage P3 were both resolved;
  final re-review returned no findings.

## Acceptance Decision

Accepted as the AOT 07 ExecIR frame ABI verifier/reporting prerequisite. The AOT C path now rejects malformed frame
sidecars before reachability filtering and makes retained frame rows auditable. Complete CallableContract frame
derivation, register/spill maps, C/LLVM ABI parity, A7.2, and the broader AOT 07-12 goal remain active.
