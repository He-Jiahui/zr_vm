# 2026-07-30 AOT 12-S1C / 12-S2G / 12-S6B Type-Layout Reachability Manifest

## Scope

This sub-milestone advances the type/layout node family required by `docs/plans/aot/12-code-stripping.md`:

- S1C gives each retained type layout a finite graph reason and stable provenance.
- S2G maps dynamic dependency layout/type/field annotations to explicit reflection roots and gives roots precedence
  over frame edges.
- S6B publishes a versioned manifest in ascending `typeLayoutId` order and rejects divergence from the independently
  computed retained-layout count.

The slice does not close the complete S1, S2, or S6 stages. Generic dictionaries, native imports, module initializers,
reflection metadata, debug sidecars, resource Drop nodes, CLI dump/diff, and full binary-size comparison remain open.

## RED And Regression Proof

The first MSVC RED added manifest assertions to three existing integration cases without production changes. The
focused executable ran 10 tests with 3 failures, each at the shared `assert_text_contains()` boundary because generated
C had no `reachability.typeLayoutManifest.version = 1` row.

The fail-closed negative was then verified by replacing only the emitter integration with the committed `be90390`
version in the isolated source tree. The 11-test executable reported 4 failures: the three missing-manifest failures
plus `test_aot_c_code_stripping_rejects_unresolved_retained_frame_type_layout` with
`Expected FALSE Was TRUE`. Restoring the new emitter made all 11 tests pass and removed the partial output file.

Independent review then identified that valid layout ID `0` was omitted by a scan beginning at `1`. Before changing
production, two ID-zero tests produced the expected MSVC RED (14 tests, 2 failures); the stable-flat-index test passed
and separately proved that table position `1` still reports predecessor flat index `2`. Scanning from `0` made the
suite pass 14/14.

## Test Inventory

- Frame-only layout 1 emits `edge.frame_layout predecessorFunction=1`.
- Layout ID 0 emits both a frame-edge row and, in a separate case, an annotation-root row.
- A retained function at compact table position 1 emits its stable flat index 2 as the frame predecessor.
- Trimmed layout 2 has no manifest row.
- Dynamic dependency layout 2 emits `root.reflection_annotation predecessorFunction=none`.
- Field-token roots for layouts 1 and 2 override layout 1's frame edge and remain sorted by layout ID.
- An unresolved layout 3 referenced by retained function 1 makes the public AOT writer fail and leaves no generated
  C artifact.
- Existing direct function reachability, export/manifest roots, metadata-size reporting, and MethodDef pruning remain
  green.

## Tooling And Commands

Effective source is commit `be90390` plus the exact owned code/test overlays for this sub-milestone. Validation used:

- WSL GCC 11.4.0, CMake 3.22.1, Ninja 1.10.1
- WSL Clang 14.0.0, CMake 3.22.1, Ninja 1.10.1
- Windows MSVC 19.44, Visual Studio 17 2022 generator

Source and build roots:

- WSL source: `/tmp/zr_vm-aot12-20260730-0346-fe684d12-c`
- GCC build: `/tmp/zr_vm-aot12-20260730-0346-fe684d12-c/build-gcc`
- Clang build: `/tmp/zr_vm-aot12-20260730-0346-fe684d12-c/build-clang`
- Windows source: `C:\Users\HeJiahui\AppData\Local\Temp\zr_vm-aot12-20260730-0346-fe684d12`
- Clean MSVC build: `build-msvc-type-layout-manifest`

Focused commands were equivalent to:

```text
cmake --build <build> --target zr_vm_aot_reachability_test zr_vm_aot_c_code_stripping_test -j4
ctest --test-dir <build> -R <focused-name> --output-on-failure
<build>/bin/zr_vm_aot_reachability_test
<build>/bin/zr_vm_aot_c_code_stripping_test

cmake -S <windows-source> -B <clean-msvc-build> -G "Visual Studio 17 2022" -A x64 ...
cmake --build <clean-msvc-build> --config Debug --target zr_vm_aot_reachability_test zr_vm_aot_c_code_stripping_test --parallel 8
ctest --test-dir <clean-msvc-build> -C Debug -R "aot_reachability|aot_c_code_stripping" --output-on-failure
```

## Results

- WSL GCC: focused CTest 2/2; direct reachability 17/0 and code stripping 14/0.
- WSL Clang: focused CTest 2/2; direct reachability 17/0 and code stripping 14/0.
- Clean Windows MSVC: focused CTest 2/2; direct reachability 17/0 and code stripping 14/0.
- SHA-256 hashes for the six involved code/test files matched the main worktree in both frozen validation trees.
- Generated-C inspection confirmed ascending node order, stable flat-function predecessors, root precedence, and
  omission of the trimmed layout, including valid ID-zero root and edge rows.

Two command-shell issues were excluded from product evidence: a PowerShell here-string added `\r` to the final GCC
direct-run path after CTest had passed, and WSL zsh interpreted the first combined Clang CTest regex as a shell glob.
The GCC direct binary was rerun without a here-string, and the Clang CTests were rerun as two literal-name commands;
both passed.

This acceptance directly fault-injects unresolved retained layouts and verifies partial-file cleanup. It does not
claim independent fault injection for allocation or failed-stream branches; those remain defensive implementation
guards rather than acceptance evidence.

## Acceptance Decision

Accepted as AOT 12-S1C / 12-S2G / 12-S6B. Every type layout retained by the focused AOT C stripping path now has a
deterministic root or frame-edge explanation, and unresolved retained frame layouts fail closed. Full AOT 12 and the
overall AOT 07-12 goal remain active.
