# 2026-07-30 AOT 07 Frame TypeLayout Closure Verifier

## Scope

This sub-milestone closes every inline ExecIR frame row over the canonical prototype-frame TypeLayout before code
stripping can remove its owner:

- resolution must succeed for retained and unreachable owners alike;
- READY layouts must pass schema/hash validation and preserve `cTypeId == typeLayoutId` identity;
- only STRUCT and UNION layouts are legal inline payloads;
- canonical byte size and alignment must exactly match the frame row;
- payload identity remains separate from direct/indirect/borrowed physical storage validation.

The existing frame-layout and TypeLayout reachability manifests remain the reporting surface. This slice does not
derive complete frame ABI slots from `CallableContract` and does not claim A7.2 completion.

## RED Evidence

The frozen WSL GCC baseline at commit `14a0184` passed code stripping 31/31. Adding the canonical closure matrix against
unchanged production produced 31/32: the new malformed unreachable TypeLayout test was accepted at its first unresolved
identity check. The initial aggregate/schema/shape implementation restored 32/32.

Independent review then found that `cTypeId` is not covered by the layout hash. A READY cache row with canonical shape
but wrong identity reproduced the gap as 31/32 against that first implementation. The same review prompted isolated
negative cases: hash corruption flips one bit of a nonzero canonical hash, while the VALUE-kind case restores matching
size, alignment, and identity before recomputing its valid hash. Adding the explicit identity gate restored 32/32.

## Coverage Inventory

- Canonical union cache rows preserve the retained direct frame and the unreachable trim owner before corruption.
- Legal same-offset direct aliases remain accepted.
- High-alignment payloads remain accepted through lower-alignment indirect and borrowed physical bindings.
- An unresolved TypeLayout identity on unreachable owner 2 fails before filtering.
- A nonzero but mismatched layout hash fails independently.
- A valid aggregate layout with mismatched `cTypeId` fails independently.
- A valid VALUE layout with matching frame shape and identity fails only the aggregate-kind gate.
- Canonical payload-size and payload-alignment drift each fail independently.
- Every rejected writer attempt leaves the malformed output path absent.

## Validation

Effective source is commit `14a0184` plus the exact production/test overlays for this sub-milestone. Validation reused:

- WSL source: `/tmp/zr_vm-aot12-20260730-0346-fe684d12-c`
- Windows source: `C:\Users\HeJiahui\AppData\Local\Temp\zr_vm-aot12-20260730-0346-fe684d12`
- GCC 11.4.0, Clang 14.0.0, and MSVC 19.44

Results:

- WSL GCC: code stripping 32/0; adjacent generic reference sharing 9/0.
- WSL Clang: code stripping 32/0.
- Windows MSVC: code stripping 32/0.
- Main, WSL, and Windows copies of both changed code/test files have identical SHA-256 hashes.
- The malformed generated-C output is absent on WSL and Windows.
- MSVC retains only the existing MSB8029 warning caused by the isolated build residing below `%TEMP%`.
- Independent review's missing identity P2 and negative-isolation P3 were reproduced and resolved; final re-review
  returned no findings.

## Acceptance Decision

Accepted as AOT 07 A7.2B's canonical frame TypeLayout closure verifier and an AOT 12 pre-filter graph-input gate.
Complete CallableContract frame derivation, constructor/generic producer expansion, GC/debug maps, C/LLVM ABI parity,
policy modes, and the broader AOT 07-12 goal remain active.
