# M2 Task 2 Super-Array Canonical Storage

## Scope

Pure signed-integer super arrays now use an exclusive raw buffer as their
canonical storage. The node map is materialized only at generic pair/value
boundaries. Storage mode and generation make the raw-to-node transition
explicit; append, cached indexed access, iteration, container operations,
clone, and raw-buffer cleanup respect the mode.

## TDD Evidence

- RED: the focused source did not have a storage-mode/materialization contract
  before the canonical raw-array fields and API existed.
- GREEN: `tests/core/test_super_array_raw_int_canonical_storage.c` covers raw
  append without node pairs, generic materialization with a non-power-of-two
  length, generic type drift to a string value, GC evacuation/root resolution,
  reflection type inspection followed by generic access, and int-to-object
  transition. Generic spread, debug formatting, cross-domain transfer, typed
  conversion, and native-binding array length/push use the same canonical
  boundary.

## Verification

- Strict GCC syntax checks passed for `object_super_array.c`, `object.c`,
  `gc_cycle.c`, `gc_object.c`, and the focused Unity source.
- Clang syntax checks passed for the changed array/container/native dispatch
  sources; existing unrelated warnings in `object.c` were excluded only for
  that source-level check.
- The focused test source passes strict C11 syntax checks under WSL GCC 11.4
  and Clang 14.0.
- `git diff --check` is clean for the touched array/core files.
- The focused CMake target is registered as
  `super_array_raw_int_canonical_storage`.
- MSVC Debug focused binary passes 7/7, including GC evacuation clone/root
  resolution, reflection boundary access, int-to-object materialization, and
  mixed raw/node four-lane append coverage.
- `ctest --test-dir build/codex-msvc-benchmark-p0-debug -C Debug -R
  '^super_array_raw_int_canonical_storage$' --output-on-failure` passes 1/1.
- MSVC Debug `zr_vm_container_runtime_test` passes 49/49 after updating the
  stale node-map capacity/pair-pool expectations. The run also exposed and
  fixed a real lower-layer bug: raw typed append now updates the receiver's
  cached `capacity` field when the raw sidecar grows.
- The complete MSVC Debug `containers` CTest suite passes 1/1 after building
  all registered container/GC runners; its child binaries report 49/49,
  3/3, 12/12, 4/4, 14/14, 3/3, 2/2, and the remaining pool/layout cases with
  zero failures.
- The independently registered MSVC Debug `zr_vm_gc_test.exe` runs 67/67
  with zero failures. This build tree does not expose a `gc_tests` CTest
  registration, so the result is from direct executable invocation.
- WSL GCC Debug focused executable `zr_vm_super_array_raw_int_canonical_storage_test`
  passes 7/7 after building the current source with the no-regenerate Ninja
  file; the matching focused CTest entry passes 1/1.
- WSL GCC Valgrind Memcheck rerun passes all 7 focused tests with zero invalid
  accesses, zero definite/indirect leaks, and `0 bytes in 0 blocks` at exit.
  The run used a temporary core shared library linked with the current
  post-fix `hash_set.c` object against the existing focused build objects,
  because the mounted-source incremental Ninja build remains I/O-bound.
- WSL Clang Debug focused executable
  `zr_vm_super_array_raw_int_canonical_storage_test` also passes 7/7, with
  the matching focused CTest entry passing 1/1.
- A fresh ext4 Clang 14 ASan Debug build passes the focused canonical-storage
  binary 7/7 with `detect_leaks=1`, `halt_on_error=1`, and
  `abort_on_error=1`. The adjacent full GC binary passes 67/67 under the same
  ASan/LeakSanitizer policy. Validation exposed and fixed the shutdown case in
  which an already-`RELEASED` function allocation was unlinked without reaching
  type-specific cleanup and `RawFree`.
- The current GCC Release `zr_vm_native_benchmark_runner` builds from the
  current source and returns the `array_index_dense` core checksum
  `723012102`, matching the registry contract. A diagnostic run with explicit
  `--scale 1` returns the expected smoke/profile checksum `175707665`; the
  explicit scale intentionally overrides the tier-derived scale.
- The ext4 GCC 11.4 Release suite records a full `array_index_dense` core
  checksum of `723012102` for both C and ZR interp. Its keyed environment
  fingerprint is
  `1b5197ed0de0b933c1ad8d790d23ac05331eba7da730acf9f1245d6befd8c1de`,
  source identity is unchanged during the run, and CPU 2 is affinity-isolated.
  Raw reports are retained as
  `tests_generated/performance_array_index_dense_core_process_final`.
- The calibrated process-scope timing series extended to 20 samples but stayed
  unstable. C reports median `4.095 ms`, CV `7.07%`; ZR interp reports median
  `205.810 ms`, CV `17.18%`. Both are `gate_eligible=false`, so the run proves
  checksum parity but not the planned 20% speedup. The persistent steady
  command matrix currently supports only numeric/dispatch cases, so no
  different-scope result is substituted for this gate.

## Acceptance Boundary

The focused canonical-storage, container suite, and standalone GC binaries are
verified on MSVC. The focused canonical-storage executable and CTest entry are
also verified on current WSL GCC and Clang builds; focused Valgrind and Clang
ASan/LeakSanitizer are clean.
The ext4 Release tree supplies a complete C/ZR-interp core checksum run, but its
wall-time samples are unstable and the registry has no ZR-binary implementation
for this case. The 20% same-scope wall-time gate remains open.
