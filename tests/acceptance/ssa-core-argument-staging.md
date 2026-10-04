---
related_code:
  - zr_vm_core/include/zr_vm_core/execution_call_transfer.h
  - zr_vm_core/src/zr_vm_core/function_frame_place.c
  - zr_vm_core/src/zr_vm_core/function_argument_staging.h
  - zr_vm_core/src/zr_vm_core/function_argument_staging.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/function_argument_staging.c
plan_sources:
  - docs/plans/ssa/04-frame-native/02-call-return-tail.md
tests:
  - tests/core/test_argument_staging.c
  - tests/core/test_argument_staging_vm.c
  - tests/cmake/ssa-argument-staging-tests.cmake
doc_type: testing-guide
---

# Finite A argument staging candidate evidence

Status: D-only implementation candidate. No production lease, E edits, staging or commit. This is a finite public FromFrame correction with a real nonoverlapping VM consumer check; it does not close SSA 04.02 or establish whole pre-call atomicity.

## Genuine original RED

Evidence root: `D:/tmp/zr_vm/ssa-20261003-01a0fe2b/argument-staging-design/pre-red`.

* `msvc/unchanged-selfcopy-v1`: six actual TUs compiled exit0, link exit0, fixture exit1 without timeout. Actual FinalizeDirectFrameValueSlots, MakeFrameSlotPlace and InitAsInt setup checks passed; public FromFrame returned true but destroyed scalar42 into packed/dense NULL. `red-input-proof.json` SHA266281a26c666e7a8ce28094ae114834ff7dadd3aedd23523731d8f52edf99a4 verifies97 original/snapshot inputs,92 actual dependencies and128 uniquely mapped unchanged extracted Core source members; all current changed providers precede the readonly archive.
* `msvc/unchanged-latefailure-v1`: one fixture TU compiled exit0 with the terminal first RED's five actual provider objects read-only; link exit0; fixture exit1. A real finalized late checked slot is misaligned inside declared frame/backing and rejected. The helper returned false after changing an earlier11 to42. `late-input-proof.json` SHAd74bb4762e5da0c6d91e72605db63405f44b178411b730437707b8e23bb828bf records the provenance and explicitly labels link-input hashes collected after link.
* `msvc/unchanged-permutation-v1`: same public fixture as the initial candidate matrix, original provider objects; compile/link exit0 and fixture exit1 with actual swap/cycle/self-copy failures. No fake Core helpers or duplicated copy algorithm are used.

## Candidate lineage and local GREEN

Candidate v1 preserves the original legacy body and extracts its four shared selection/place helpers verbatim into the new private header. It passed the identical selfcopy fixture and whole-backing latefailure fixture. V2 adds diagnostics/control-storage protection, dense-only frame extents, no-selected compatibility and conservative descriptor flags. V3 adds the expanded always-active matrix and genuine argument-source VM metadata. V4 adds new-symbol Core export annotations and standard Unity fixture hooks; existing ABI values and production semantics are unchanged from v3.

`candidate-v3/msvc/matrix-candidate-v3` compiled four actual TUs, linked and ran exit0. Coverage includes exact self-copy, swap, three-cycle, repeated physical sources, identical/conflicting reused outputs, duplicate descriptor rejection, whole-frame late failure repeated, exact/short scratch, the >8-row CRT success branch, packed-NULL/dense and missing-layout dense fallbacks, primitive native constructors and bit preservation, diagnostic-in-frame rejection, workspace in a distinct current TLS profile, receiver/constructor/unknown flags, no selected rows with positive argument count, and exact enabled/disabled helper/profile counts.

`candidate-v3/msvc/vm-source-metadata-v3` compiled the real fixture and real runtime harness exit0, linked the actual current providers/read-only Core archive and ran exit0. It used real State_Create, Function_New and finalized descriptors. `PreCallKnownVmValueWithArgumentSource` carried caller source ordinal3; a packed-only137 reached both callee VALUE mirrors with DIRECT1/CHECKED0. No native function invocation, plugin loading, network or allocator callback test was performed.

V4 final matrix and VM receipts are included in the sealed D manifest. The final candidate review binds its exact ten file hashes and actual provider mapping. Earlier failed VM attempts are preserved: the first lacked Unity include paths and failed compilation; the second used a generic PreCall API that did not carry argument-source metadata and failed fixture setup. Neither is a successful gate. An intermediate v4 VM fixture used v2 leaf objects and is lineage evidence only; the exact v4 provider rerun is the final candidate gate.

## V4 additional local gates and V5 test epoch

The frozen V4 candidate and its original manifest are retained. Additional
evidence lives in `v4-extra-gates`, `formal-v4-prefix`, and `shared-v4` under the
same D evidence root. The supplemental matrix compiled two actual TUs, linked
and ran exit0. Its five compatibility controls are now part of the V5 formal
test source: ignored ordinal/count, individual ordinal overflow, partial dense
mirror suppression, distinct FLOAT, and actual owning legacy/profile fallback.
The V5 production helper, header and frame adapter bytes match sealed V4.

The CRT failure fixture is explicitly fault injected. A same-D copy of the actual
MSVC staging object changes only its undefined `__imp_malloc` name to a test-local
ordinary CRT pointer shim. All section bytes and relocation bytes remain equal,
the sole allocator relocation retains its position, and all other symbol records
are unchanged. The armed shim returns NULL once for the exact >8-row size;
public FromFrame returns false, the entire backing/state/control/profile fixture
remains byte equal, and no legacy counters appear. A separate diagnostic reports
NO_MEMORY; the unarmed shim delegates to ordinary malloc and succeeds. Compile,
link and fixture exit0. This object is not the canonical production product;
the uninjected V4 matrix remains the ordinary allocation success gate.

The actual new fragment has standalone static CTest 2/2 PASS with ten genuine
Ninja dependency records after setting the observed localized MSVC prefix.
The earlier database with zero tracked dependencies and malformed-path configure
failure remain historical evidence, not the final dependency gate.

The genuine shared project uses unchanged Core/Common/ThirdParty CMake,
215 Core and one common C sources, and actual Unity registration. Shared build,
map-only relink, DLL exports, consumer imports and registered CTest 2/2 exit0.
The actual Ninja database has 229 valid object records. The matrix's exact helper
counts and the VM's DIRECT1/CHECKED0 controls execute through the genuine Core
DLL, with one Core profile TU and no borrowed static Core TU in consumers.
The shared input proof is `shared-v4/input-proof.json`, SHA
`22424dd40e100b68b7e9b7755166493e5775c8e154abaa0835d970131698c5bc`;
the shared evidence manifest SHA is
`ca0393c2a09432e29e2e61ada79b8fe5137d7ec75bbb3de989bb17e5b5f9858d`.
It verifies all 1,264 original/snapshot/control pre-hashes after execution,
197 actual local header dependencies and all 229 valid Ninja records. Default
SDK headers are observed separately without a fabricated pre-hash stability
claim. The first export-parser proof falsely rejected dumpbin's `= @ILT(...)`
annotation; it is retained and superseded by exact export-name-column parsing.

V5's one changed formal test TU compiled exit0, linked against the unchanged
genuine shared DLL/import library and four actual harness objects, and ran exit0
with `argument staging matrix failures=0`. Its matrix input proof SHA is
`09e2c1b453cae9f6049827335ce25d3ee5d54f6d0174b86d7607ec0f96df4ee3`:
1,276 pre-hashes remain equal, including the genuine DLL and four reused harness
objects; all 56 actual local compiler dependencies have pre-hashes.

The exact new fragment's V5 standalone object-backed registration then configured,
linked both targets and passed CTest 2/2. Its input proof SHA is
`d33d5e54202bde1627fea95afbb492cb6abb2e95c8deec7af8b12790592ce5c3`;
manifest SHA is
`e0ea0ccccd8f0b80283d8c3ecbf96cbacc87eb874e7085af58b47756f399cead`.
All 165 source/header/object/DLL/control pins remain equal. The actual original
three CMake helpers and fragment declare the targets; their final source lists
explicitly reuse ten proven C11 objects. The changed formal test was compiled
once in the preceding matrix gate, and this registration project performs two
real links without another Core compilation. Its initial generation failed
because object-only targets lacked linker-language metadata; the retained
exit1 receipt is superseded by explicit matching `LINKER_LANGUAGE C` and actual
configure/link/CTest exit0. The default DLL is resolved from its original D bin
directory through a CTest PATH prepend, without copying products.

These are D candidate gates; they do not assert an E production seal. The
subsequent exact-V5 Linux gates are recorded below.

## V6 actual Linux candidate gates

`linux-v5/manifest.json` SHA
`e8b9f13eb8619afc7cf6346582db63d68b7d513f12248741ac99a17158e30ae0`
binds 174 evidence files and the unchanged V5 formal ten files. All 683 explicit
source/header/archive/control input pins remain equal. Each GCC/Clang matrix
compiled nine actual TUs exit0; each VM compiled one actual TU exit0 and reused
its own eight terminal providers/harness/Unity objects. ASan/UBSan instruments
these scoped fresh TUs. The formally delivered current GNU11.4 Core/vendor
archives are read-only plain support, not whole instrumented Core.

The four original GNU links exceeded their 90-second bounds and exited -15 after
owned process-group termination. Those receipts remain nonpass history, with no
original runtime acceptance. Native LLD then used the same own objects and
read-only archives, preserving the actual installed GCC/Clang sanitizer link
plans. All four links, actual WSL sanitizer runs and actual readelf commands
returned exit0 without timeout. Matrix logs report failures=0; the actual
nonoverlapping VM PreCall consumes packed-only137 through public FromFrame.
Actual ELF headers prove ELF64 little-endian x86_64; interpreter and ASan/UBSan
symbols are observed. Link maps bind 120 current archive members for each gate,
with actual function.c selected from current Core. The two retired frame-place
and call-transfer archive members are not selected; new staging binds exactly
once from the fresh object. Actual fresh dependency unions are 80 matrix and79
VM local inputs, all presealed.

The first Clang matrix proof incorrectly required an ELF64 literal in combined
readelf output; it is retained as a failed proof-construction history. Final
validation reads the actual binary header and actual interpreter/sanitizer
symbols. GCC VM's initial proof recorded only its 55 fresh-TU dependencies; its
retained complete-deps proof includes the reused eight objects'79-input union.
Neither correction recompiles or changes accepted binaries or original logs.

The Linux actual TU driver itself creates each own D run tmp directory and sets
Linux TMP, TEMP and TMPDIR to its `/mnt/d/.../tmp` before compiler and linker
children. This is an explicit Linux environment action, not an assumption that
a Windows outer environment propagates through WSL. Native LLD metadata probes
use installed compiler `-###` only; no object compilation occurs there. Native
linking and its temporaries use the own D output directory. Runtime checks use
explicit ASAN_OPTIONS and UBSAN_OPTIONS through `/usr/bin/env`.

## Pending production gates

E activation, actual current-E source/registration validation, coordinated
provider epoch and independent final acceptance remain pending. No second full
Linux Core build was run. Frozen V4/V5 histories, snapshots, receipts and final
binaries remain unchanged; this V6 epoch updates only the two formal docs.

## V7 independent Linux temporary-directory proof

Root independently repeated all four accepted ELF binaries using explicit Linux
TMPDIR, TMP and TEMP under its own D directory, alongside ASan/UBSan options.
All four runs returned exit0 without timeout; their logs equal the original
results byte for byte, and all 190 before/after pins remain equal. The root
receipt is `validation-control/argument-linux-root-v5/receipt.json`, SHA
`da855841788327b7503d2d0925cf2b99db5c3c3608e23087f7dedcea2c7d18fe`.

The original four runtime receipts establish real successful execution but do
not establish Linux temporary-directory propagation from the Windows wrapper.
The Linux compiler driver's current code explicitly sets its child environment,
and recorded run paths and GCC temporary artifacts support that provenance;
the exact executed driver source bytes were not presealed. This limitation is
retained rather than presenting a current driver hash as an execution pre-hash.
GCC VM's first 55-dependency proof has literal valid=true but incomplete union
coverage; its separate final complete-deps proof supersedes that coverage.
The original V4/V5/V6 and Linux seals remain immutable. E activation and
current-E registered/provider acceptance remain pending.
