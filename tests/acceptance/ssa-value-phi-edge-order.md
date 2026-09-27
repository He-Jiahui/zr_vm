# SSA 01.02: value phi predecessor edge order

## Scope

- Core value SSA verification now requires each phi incoming to occupy the
  same slot as its corresponding predecessor edge, even when both named
  predecessors occur elsewhere in the row. Missing slots fail before any
  out-of-bounds predecessor lookup.
- The parser's existing diamond place-promotion fixture exercises the actual
  promoted phi. This change does not add an explicit serialized edge-ID field,
  nor does it complete every CFG producer and exception case in 01.02.

## Baseline and RED

- Before implementation, WSL GCC `ssa_place_promotion` failed with
  `FAIL: value phi accepted incoming values in the wrong edge order` after
  swapping the two valid `(predecessor, value)` records.
- A second RED run with a shortened phi range failed with
  `FAIL: value phi accepted fewer incoming slots than predecessor edges`.

## Test Inventory

- Positive: the original promoted diamond phi remains valid, and a repeated
  promotion remains idempotent.
- Negative: swapped incoming records and one missing edge slot each report
  `PHI_PREDECESSOR_MISMATCH` at the join block, with the expected/actual edge
  identity or count; the fixture restores its phi and checks structural and
  value SSA verification afterward.
- Regression: builder dominance, effect verifier, and parallel-edge Oracle
  tests guard the adjacent CFG and edge-occurrence consumers.

## Tooling Evidence (2026-09-27)

WSL GCC 11.4 rebuilt `zr_vm_ssa_place_promotion_test`,
`zr_vm_ssa_builder_dominance_test`, `zr_vm_ssa_effects_verifier_test`, and
`zr_vm_ssa_oracle_parallel_edges_test`. The focused selector passed **4/4**
after rebuilding on WSL GCC 11.4, WSL Clang 14, and Windows MSVC 19.44
Debug/static:

```text
ctest --test-dir /home/hejiahui/zrvm-ssa-nested-gcc.4pVemu -R '^ssa_(place_promotion|oracle_parallel_edges|effects_verifier|builder_dominance)$' --output-on-failure --no-tests=error
ctest --test-dir /home/hejiahui/zrvm-ssa-nested-clang.kBIWlA -R '^ssa_(place_promotion|oracle_parallel_edges|effects_verifier|builder_dominance)$' --output-on-failure --no-tests=error
ctest --test-dir build/codex-ssa-conversion-msvc-static -R '^ssa_(place_promotion|oracle_parallel_edges|effects_verifier|builder_dominance)$' --output-on-failure --no-tests=error
```

GCC AddressSanitizer and UndefinedBehaviorSanitizer with leak checking built
the same place-promotion target and passed **1/1**:

```text
cmake -S /mnt/e/Git/zr_vm -B /home/hejiahui/zrvm-ssa-phi-asan -G Ninja -DCMAKE_BUILD_TYPE=Debug -DCMAKE_C_COMPILER=gcc -DCMAKE_C_FLAGS='-fsanitize=address,undefined -fno-omit-frame-pointer' -DCMAKE_EXE_LINKER_FLAGS='-fsanitize=address,undefined' -DBUILD_TESTS=ON -DBUILD_CLI=OFF -DBUILD_LANGUAGE_SERVER=OFF
cmake --build /home/hejiahui/zrvm-ssa-phi-asan --target zr_vm_ssa_place_promotion_test -j 4
ASAN_OPTIONS=detect_leaks=1:abort_on_error=1 UBSAN_OPTIONS=halt_on_error=1 ctest --test-dir /home/hejiahui/zrvm-ssa-phi-asan -R '^ssa_place_promotion$' --output-on-failure --no-tests=error
```

## Acceptance Decision

The value phi slot check is accepted as a focused 01.02 stage. The full plan
and serialized edge identity remain open. The focused test emits existing
missing-braces initializer warnings when built under GCC; no warning was
introduced by the changed verifier. These four focused checks do not assert
the full SSA milestone gate.
