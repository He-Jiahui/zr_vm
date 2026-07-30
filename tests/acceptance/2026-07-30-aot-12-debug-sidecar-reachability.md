# 2026-07-30 AOT 12 Debug Sidecar Reachability

## Scope

This sub-milestone makes the existing canonical function execution-location rows visible to AOT 12 reachability:

- ExecIR validates every owner before function filtering;
- retained rows are linked to their owning function and published in deterministic flat-owner/source-row order;
- before/after/removed counters measure debug row trimming independently from function and metadata counts;
- malformed rows on an unreachable owner fail before output and leave no partial artifact;
- quickening-style distinct source ranges may legally share one instruction offset.

This is owner-linked debug sidecar reachability evidence. It does not claim safepoint variable locations or the AOT 11
versioned DebugMap artifact section.

## RED Evidence

The frozen WSL GCC baseline at commit `ad6aa29` passed code stripping 30/30. Against unchanged production, the expanded
31-test suite had three expected failures: the primary trim fixture could not find debug counts/manifest, a malformed
unreachable sidecar was accepted, and the all-empty fixture could not find zero-row reporting. The other 28 tests
passed.

The first implementation passed 31/31, but independent review identified an over-strict count bound. Quickening can
retain two different source ranges after remapping both to one live instruction. Changing the positive fixture to
`rowCount=2`, `instructionCount=1`, offsets `[0, 0]` reproduced the bug as 30/31. Removing only the count upper bound
while retaining per-row offset and ordering checks restored 31/31.

## Coverage Inventory

- Four original rows become three retained rows after owner-function trimming.
- Counts report `debugLocationsBefore=4`, `After=3`, and `Removed=1`.
- Manifest rows order owner 0 before owner 1, then owner 1 source locations 0 and 1.
- Owner 1's two distinct source ranges share one valid offset, proving quickening coalescing and
  `rowCount > instructionCount` remain legal.
- An all-empty property-accessor fixture reports zero rows and emits no debug sidecar node.
- A nonempty row count with a null table on unreachable owner 2 fails before filtering.
- Negative, out-of-range, and decreasing instruction offsets each fail.
- An inverted explicit line range and an inverted same-line column range each fail.
- Every malformed writer attempt leaves its output path absent.

## Validation

Effective source is commit `ad6aa29` plus the exact production/test overlays for this sub-milestone. Validation reused:

- WSL source: `/tmp/zr_vm-aot12-20260730-0346-fe684d12-c`
- Windows source: `C:\Users\HeJiahui\AppData\Local\Temp\zr_vm-aot12-20260730-0346-fe684d12`
- GCC 11.4.0, Clang 14.0.0, and MSVC 19.44

Results:

- WSL GCC: code stripping 31/0; adjacent generic reference sharing 9/0.
- WSL Clang: code stripping 31/0.
- Windows MSVC: code stripping 31/0.
- Generated C contains exact 4-to-3 debug-row deltas and stable multi-owner/coalesced-row manifest entries; malformed
  output is absent.
- MSVC retains only the existing MSB8029 warning caused by the isolated build residing below `%TEMP%`.
- Independent review's quickening-coalescing P1 was reproduced and fixed; the missing linked status-record P3 was
  addressed, and final re-review returned no findings.

## Acceptance Decision

Accepted as the AOT 12 canonical debug sidecar reachability sub-milestone. The AOT C path rejects malformed canonical
location rows before trimming and makes every retained source row auditable through its owner. Safepoint variable maps,
spill/provenance rewrite, source checksums, a versioned DebugMap artifact section, policy modes, and broader AOT 12
convergence remain active.
