---
related_code:
  - tests/cmake/ssa-canonical-type-lookup.cmake
  - tests/cmake/ssa-tests.cmake
  - tests/cmake/ssa-sccp-conversion-tests.cmake
  - zr_vm_parser/include/zr_vm_parser/canonical_type.h
  - zr_vm_parser/src/zr_vm_parser/canonical_type_lookup.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_index.c
  - tests/parser/test_ssa_canonical_type_lookup.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/canonical_type_lookup.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_index.c
plan_sources:
  - .codex/plans/20261009-ssa-canonical-lookup-extraction-v1.md
tests:
  - tests/parser/test_ssa_canonical_type_lookup.c
  - docs/testing-and-validation/canonical-type-lookup.md
doc_type: acceptance-record
status: finite-validated-rootproof-commit-pending
---

# Canonical lookup extraction finite observations

## Authorized behavior-preserving scope

Root authorized the actual existing Find body/documentation move from mutable
index TU to new lookup TU. The public declaration stays unchanged. Current plan
SHA is `720F810400C70300FF58D8A7444B5151A39FEEA759C5F815CFD1839441AD3DDA`.
This is a source extraction prerequisite, not an interner rewrite or new
canonical validation/lifetime API. Root owns registrations, current provenance,
native/finite compiler gates and the normal exact-scope Git transaction.

Find returns a borrowed node address from a correctly typed initialized sorted
live canonical array, NULL for NULL context/invalid ID/miss/empty table, and
does not allocate, mutate or retain a registry. Reallocation, Reset/Free and
owner expiry invalidate borrows. Constructed fixtures obey those preconditions;
they do not establish same-compilation SCRIPT Binder provenance.

## Actual source/test freeze and independent review

Current retained index199 SHA:
`9DA659E53F971E605D878FD4274F7CF3640590FA16CBAACAAC4671B314B31AE1`.
Moved lookup43 SHA:
`D2FE39133D76A10A933A10DA759A776983934CE28D57AEFA76331FCDF87F4934`.
Test98 SHA:
`9489F4C852676D4F321FCC01181800A8EA9598A8C53852414D71D1BE26FB52E1`.
The only post-r1 source correction removes the final empty EOF line. The
remaining retained prefix and moved Find body/comment are unchanged.
Independent read-only review finds the body/comment unchanged from the earlier
actual index definition, no remaining Find definition in retained index, and
eight live declared-array cases within existing lookup preconditions. New CMake
links actual private lookup OBJECT plus test/Unity, with no index/VM library.
The finite receipts below supply actual compiled provider/settings/runtime
observations. Root independent current proof and commit remain pending.

## Current finite native r2

Receipt `E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/canonical-lookup-r2-receipt.json`
has SHA `1A8AB284752700881F89E95D50D96C4BFB1D410EBE1C0E8B4A57A0BF3B95DEC2`,
validated true/error null. Configure41104/build37912/llvm-nm18500/CTest20812
all terminate naturally0. Raw CTest `canonical-lookup-r2-lookup-ctest.log` SHA is
`CE2EBC7F54F713189FE709F21953E7F0339ECEA9B9CDDE4ECD06D7FFF859332B`.
Actual default8 is8PASS/0FAIL/0ignored. All four physical phase-log hashes match
receipt with no warning/UBSan diagnostic. Native input pins remain5635.

Runtime r2 only relinks to produce its new map, with no selected TU recompile.
Its selected3 current unchanged objects are lookup, test and Unity; configured44
TUs is a separate count. Actual Find is supplied solely by the lookup private
object with no index/VM allocator imports. Enabled assertions/UBSan and current
settings/input/object pins belong to this replay, not a claim of full rebuild.
No Core source changed or Core support rerun is credited.

## Current finite MSVC r2

Receipt `canonical-lookup-msvc-r2-receipt.json` SHA is
`2C3B75F8CEB5D28CC0DE984F0C712151371247AD13046B8F2751EA90E7D58040`,
validated true/error null. Actual header15780/lookup35132/retained-index8076/
test6904 all four terminate naturally0/zero warnings. Each raw-log/object hash
independently matches receipt. This compile-only replay refreshes current pins
including the index EOF correction; it does not establish full Parser runtime.

## Historical finite native r1

Receipt `E:/cargo-targets/zr_vm/reports/ssa-20261005-01a0fe2b/canonical-lookup-r1-receipt.json`
has SHA `39CB1E4C95BAF34CB807FE85C9216F51E1299C7CEA994AD5AD3E98A7FA8D6424`,
validated true/error null. Actual four phases configure276/build22484/
llvm-nm19584/CTest14836 terminate naturally0. Raw CTest log
`canonical-lookup-r1-lookup-ctest.log` SHA is
`63E6576DD2D6E0412B3ADD063B9001D31754811B8785476EDA37E0892C32D6B1`.
Actual default8 is8PASS/0FAIL/0ignored; raw names match the eight registered cases.
All four physical phase-log hashes match receipt with no warning/UBSan diagnostic.

Actual build has four steps: three compiles (lookup, main, Unity), then link.
Selected graph3 is distinct from the44 configured project TUs. The actual Find
provider is `canonical_type_lookup.c.obj`; the finite link/symbol evidence has no
index/VM allocator imports. The native gate records5635 current input pins.
Assertions and UBSan remain part of Root's actual configured gate. No Core source
changed: previously committed Core support is historical prerequisite evidence,
not a newly executed support run in these four phases.

## Historical finite MSVC r1

Same report directory receipt `canonical-lookup-msvc-r1-receipt.json` SHA is
`6E71DAF43248CB99F1D429B223A25B409FDBC2E2673B3973E6D32DE92A94F262`,
validated true/error null,4298 current input pins. Actual header15300/lookup6124/
retained-index25440/test20612 each terminate naturally0/zero warnings. All four
physical raw-log/object hashes independently match receipt. These four units
compile only; retained-index compilation does not prove its runtime allocator
closure or full ordinary Parser linkage. No lookup fault denominator exists.

## Preserved commit-r1 refusal and current provenance

Historical r1 retained index200 SHA was
`BBEAA9628255AF9DA01B0A492E2613B6175A75EDCC5AE6746F9680D0D68A561F`.

Historical commit receipt `canonical-lookup-commit-r1.json` SHA is
`DDCB68CE101479E2A4E7DCC1BE9D6595DD55F3086256BF0C6C9712242E396B8A`,
validated false. The pre-stage working diff check exits2 because index line200
is a new blank line at EOF. Its cached-before output is empty; no stage or
commit follows. Root observed no index lock. The transaction remains a failure;
r2 does not flip that receipt. Only one final empty line was removed, followed
by the fresh runtime/MSVC replays above. No foreign source/state was reset.

The earlier r1 gate did not itself enforce the later exact tool-path check;
Root's independent proof supplemented that evidence. Current r2 gate now checks
five configured paths: compiler clang-cl.exe, linker lld-link.exe and archive
llvm-lib.exe under `E:/Visual Studio/VC/Tools/Llvm/x64/bin/`, plus
`E:/cargo-targets/zr_vm/tmp/ssa-20261004-01a0fe2b/source-llvm-toolchain-v35.cmake`
and its sibling `source-llvm-rules-v35.cmake`. It also checks each of the three
actual compile command executable prefixes against the configured clang-cl path.
These are observed checks in the new gate, not retroactive claims for r1.

Actual CMake/CTest programs are under
`E:/Visual Studio/Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/`;
llvm-nm is `E:/Visual Studio/VC/Tools/Llvm/x64/bin/llvm-nm.exe`. Finite MSVC uses
`E:/Visual Studio/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe`.
Root independent current proof and the new finite commit remain pending. No
proof/commit hash is invented. Normal explicit source-list edits still require
exact owned-patch review preserving all foreign working changes.

These finite receipts establish the lookup contract only; they do not credit
Source wrapper/Binder, interner, registry retention or any SSA leaf completion.

## Acceptance limits and next consumer

Accepted finite lookup evidence must independently bind real moved source,
actual object/provider, genuine const-preserving node reads and lifetime-safe
constructed tests. It cannot credit canonical graph mutation, allocator closure
of the retained index TU, ordinary full Parser runtime or registry retention.
MSVC compilation of that retained TU remains compile-only.

Whole-module finalized source admission still requires separate diagnostics,
frame validation, genuine caller Binder provenance and live context contracts.
Source mode4/default/AOT behavior is unchanged by this extraction. Source wrapper
GREEN and current finalizer completion are not claimed. Full normal/Linux/MSVC,
backend parity, default adoption and all47 remain OPEN.

- [Borrowed lookup contract](../parser-and-semantics/canonical-type-lookup.md)
- [Eight-case validation design](../testing-and-validation/canonical-type-lookup.md)
