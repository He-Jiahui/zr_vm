---
related_code:
  - zr_vm_lib_ffi/CMakeLists.txt
  - zr_vm_lib_ffi/cmake/preprocess_libffi_asm.cmake
  - zr_vm_lib_ffi/cmake/fficonfig_x86_win64.h.in
  - zr_vm_lib_ffi/third_party/zr_libffi/libffi/include/ffi.h.in
  - zr_vm_lib_ffi/third_party/zr_libffi/libffi/include/ffi_cfi.h
  - zr_vm_lib_ffi/third_party/zr_libffi/libffi/src/x86/win64_intel.S
  - zr_vm_lib_ffi/third_party/zr_libffi/libffi/src/x86/asmnames.h
  - zr_vm_lib_ffi/third_party/zr_libffi/libffi/src/x86/ffitarget.h
  - tests/cmake/cli-fixture-targets.cmake
  - tests/cmake/libffi-asm-dependencies-validation.cmake
implementation_files:
  - zr_vm_lib_ffi/CMakeLists.txt
  - zr_vm_lib_ffi/cmake/preprocess_libffi_asm.cmake
  - zr_vm_lib_ffi/cmake/fficonfig_x86_win64.h.in
plan_sources:
  - "user: 2026-10-16 bounded bundled Win64 libffi assembly dependency follow-up"
  - docs/plans/syntax/2026-10-08-macro-tokenstream-structured-generation-design.md
tests:
  - tests/cmake/libffi-asm-dependencies-validation.cmake
  - tests/cmake/cli-fixture-targets.cmake
doc_type: module-detail
---

# Bundled Libffi Build

## Purpose And Selection

`zr_vm_lib_ffi/CMakeLists.txt` first searches for a system `ffi.h` and libffi
library. When that pair is unavailable, the bundled fallback is
enabled only for Windows, MSVC, and a 64-bit pointer size. It builds the
`zr_vm_libffi_vendor` static target from the existing vendor C and Win64
assembly sources. This document concerns that build path, not the runtime FFI
API or other libffi platforms.

The project's CMake integration owns generated configuration and dependency
edges. The follow-up does not modify the vendored libffi sources, ABI constants,
compiler flags, or timeout policy.

## Generation Pipeline

Let `<vendor>` be
`zr_vm_lib_ffi/third_party/zr_libffi/libffi`, and `<gen>` be
`<build>/zr_vm_lib_ffi/generated/libffi/x86_win64`. Generated files stay in the
actual external build tree, not the checkout.

1. CMake configuration selects `X86_WIN64` and generates `<gen>/ffi.h` from
   `<vendor>/include/ffi.h.in`. It also generates `<gen>/fficonfig.h` from the
   project-owned `fficonfig_x86_win64.h.in`. The C and assembly implementations
   must consume this same Win64 configuration.
2. The custom command calls `preprocess_libffi_asm.cmake` with `C_COMPILER`,
   `ASM_SOURCE`, `ASM_OUTPUT`, `GEN_DIR`, and `VENDOR_ROOT`. The script invokes
   the selected MSVC C compiler with `/EP /TC`, Win64/static-build definitions,
   and generated/vendor include directories. It writes
   `<gen>/win64_intel.asm`; a nonzero preprocessing result is a fatal build
   error.
3. CMake marks that output as generated `ASM_MASM` source. MASM produces its
   native object, which is archived with the vendor C objects into
   `zr_vm_libffi_vendor`. The `zr.ffi` targets link this backend through
   `zr_vm_attach_libffi`.

The preprocessing step consumes `fficonfig.h`, `ffi.h`, `ffi_cfi.h`, and
`asmnames.h` directly. Generated `ffi.h` also includes the x86 `ffitarget.h`.
After `/EP`, those C preprocessor includes have been expanded into the MASM
input. Assembly-object dependency records alone cannot establish the upstream
preprocessing closure.

## Nine-Input Graph Contract

Ninja's input closure for `<gen>/win64_intel.asm` must contain all nine paths
below. Templates and their generated headers are separate graph inputs: a
template participates in configuration, while its generated header is the
file actually consumed by preprocessing.

| Input | Role |
| --- | --- |
| `<vendor>/src/x86/win64_intel.S` | Assembly source before C preprocessing |
| `<vendor>/include/ffi.h.in` | Template for the public configuration header |
| `zr_vm_lib_ffi/cmake/fficonfig_x86_win64.h.in` | Project Win64 configuration template |
| `zr_vm_lib_ffi/cmake/preprocess_libffi_asm.cmake` | Preprocessing command implementation |
| `<vendor>/include/ffi_cfi.h` | Assembly CFI macro definitions |
| `<vendor>/src/x86/asmnames.h` | Assembly symbol-name macros |
| `<vendor>/src/x86/ffitarget.h` | Target-specific ABI and trampoline definitions |
| `<gen>/ffi.h` | Generated header read by the preprocessor |
| `<gen>/fficonfig.h` | Generated feature and ABI configuration |

The registered baseline custom command declares the first six inputs but omits
`ffitarget.h` and both generated headers. A consumed header can therefore
change without making the generated MASM source out of date. The repair adds
those three missing `DEPENDS` entries. This is an
incremental dependency defect; it does not by itself demonstrate current native
ABI corruption.

## Regression Contract

The new focused regression is
`tests/cmake/libffi-asm-dependencies-validation.cmake`. Its inputs are:

- `SOURCE_DIR`: the actual checkout root containing the vendor inputs.
- `BUILD_DIR`: the existing configured build containing generated headers and
  the generated MASM target.
- `NINJA_EXE`: the native Ninja executable for that build.

The script's contract is to query Ninja with `-t inputs` for the generated
MASM source and require all nine inputs by normalized, exact line membership.
Missing required paths or a failed native query must fail the regression;
substring matches are insufficient evidence of a dependency edge.

The CTest name is `ffi_win64_asm_dependencies`, registered in
`tests/cmake/cli-fixture-targets.cmake` only when the bundled vendor target
exists and the generator is Ninja. System-libffi and non-Ninja configurations
must not receive an inapplicable bundled-graph test. The existing explicit
foundation52, lower323, original127, and selected full55 inventories are not
expanded by this focused registration.

The test inspects the generated build graph. It does not touch headers to
simulate edits, rebuild the vendor library, change CTest timeouts, or exercise
native FFI calls. Graph membership is not native ABI validation, proof that all
C objects have valid dependency records, or platform/milestone acceptance.

## Current Status

The regression was run on the unmodified Ninja graph and exited with failure,
reporting exactly the three missing target/generated-header inputs. The source
repair then added only those three dependency entries; the regression remained
unchanged. After normal same-tree reconfiguration, the identical regression
passed with all nine required inputs. Its normal standalone CTest registration,
`ffi_win64_asm_dependencies`, then passed 1/1 with native exit 0 and all 13
recorded inputs unchanged. The receipt is
`E:/cargo-targets/zr_vm/reports/m1-msvc-libffi-asm-ctest-r1-receipt.json`
(SHA-256 `6E7868344D725D6F01D9E56B8971F965F29D80DFE259AA0EADCF513CE8B58896`).

Canonical preprocessing with the current inputs also exited 0. Its generated
MASM remained byte-identical at 6,641 bytes and 1,124 lines, SHA-256
`A11717D8894E205EC02E139792073C606ED821D4C651210069478406E65F0895`; the
receipt is
`E:/cargo-targets/zr_vm/reports/m1-msvc-libffi-build-followup-r1-receipt.json`
(SHA-256 `145D625DDBC5535E0C5ED09BE3EE4FBF3B65EEABA96BA1235FCD5034BAFA89A0`).
This is graph-regression and canonical-preprocessing evidence, not proof of
previous ABI corruption, complete C-object cache validity, or platform acceptance.

Windows r9 subsequently completes its normal full build with native exit 0,
956 planned actions and no `FAILED:` rows; foundation CTest passes 52/52.
The normal two-test graph run also passes both `ffi_win64_asm_dependencies` and
`msvc_compile_object_paths`, with all 20 recorded inputs unchanged. Receipt
`E:/cargo-targets/zr_vm/reports/m1-msvc-build-graph-ctest-r2-receipt.json`
has SHA-256 `D563475EF1AD7F794D8D9CE41620E467E48B30B616561DDB59ECA5B1383AAB79`.
Its FFI result confirms the nine-input graph closure, not native ABI correctness.

The separate typed-cache audit passes only its selected 119 C and one MASM
objects (receipt `m1-msvc-cache-post-r9.json`, SHA-256
`D4F8443A9E0C672F59D1C81F9D8BD41E0A11568F7F5B34921FADAAC8A75398E5`).
It did not cover the subsequently identified zero-dependency fixture objects
with mixed PDB layouts. R9 remains raw FAIL
after the yield native access violation and 12 source changes; the later matrix
gates were not executed. See the [current validation gate](../testing-and-validation/macro-foundation-validation.md)
for the debugger boundary and actual linked stale-COMDAT ABI mismatch.

The seven-object follow-up r1 compiled successfully but retained seven `VALID 0`
header records, so it remains an actual failure with no native tests. Local
ASCII showIncludes-prefix normalization plus a deferred CMake prefix hook then
restores all seven positive `VALID` closures in r2. Its saved existing-C inventory
contains 2,474 positive closures and only the exact 84-byte headerless tinydir
placeholder at zero. Original native 7+3+4+127 cases all pass, but receipt
`E:/cargo-targets/zr_vm/reports/m1-msvc-zero-deps-r2-receipt.json`
(SHA-256 `558DFD252894A57C84E4ED689BFDE7B72AEB7C75EA3D9BA1F4E2E31FB8C53674`)
remains FAIL for six foreign source changes and changed HEAD. These are dated
cache/native results, not a platform PASS; neither repair changes vendor code.

The r10-v2 attempt has naturally ended: foundation52 and lower323/original127
pass, but full CTest is52 PASS/3 FAIL of55 (AOT source contracts, LSP ownership,
CLI incremental full-AOT output). Its strict source gate also rejects one foreign
CMake input change. Auxiliary3+4/source-final were not reached. The subsequent
focused AOT contract repair passes26/26 without a vendor or production AOT edit;
see the [validation ledger](../testing-and-validation/macro-foundation-validation.md).
Original full gates remain required. Linux remains unpassed; M1 is unaccepted
and uncommitted.
