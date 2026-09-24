# ExecIR deopt reconstruction validation

## Scope

2026-09-24: plan 01.03 structural verification and 01.04 state maps. The
core function verifier validates every deopt reconstruction range and each
value referenced by that range before parser liveness can read it. No runtime
frame-switch implementation or full SSA milestone completion is claimed.

## Baseline and test inventory

The new `ssa_deopt_validation` executable initially reported **7 tests,
6 failures** against the unchanged verifier. The positive range case passed;
the verifier accepted spare-capacity access, a wrapped range, an empty range
past the pool end, invalid reconstruction IDs, and invalid unreferenced states.
The builder also accepted invalid metadata and replaced the published map.

`tests/parser/test_ssa_deopt_validation.c` covers those cases with production
verification and construction APIs. It checks diagnostic code, function token,
source ID, referring instruction ID, and preservation of the published map's
pointer, storage and resume identity on failure. Physically allocated padding
makes the original logical-range failure deterministic without requiring a crash.
The wrapped-range case also exercises a null diagnostic destination.

## Tooling evidence

Run from the WSL repository root, using GCC 11.4.0, Clang 14.0.0 and CMake
3.22.1. For each `build/ssa-gcc-debug` and `build/ssa-clang-debug` directory:

```bash
cmake --build <build-dir> --target zr_vm_ssa_deopt_validation_test zr_vm_ssa_state_maps_test zr_vm_ssa_core_model_test zr_vm_ssa_effects_verifier_test zr_vm_ssa_value_validation_test -j 4
ctest --test-dir <build-dir> -R '^ssa_(deopt_validation|state_maps|core_model|effects_verifier|value_validation)$' --output-on-failure --no-tests=error
```

Both toolchains rebuilt successfully and passed **5/5 CTest suites**.

Windows MSVC 19.44.35228.0 was initialized using
`C:/Users/HeJiahui/.codex/skills/using-vsdevcmd/scripts/Import-VsDevCmdEnvironment.ps1`.
The same five targets were rebuilt in `build/ssa-msvc-debug` with `--config
Debug`; CTest used the same expression plus `-C Debug` and passed **5/5**.
The build emitted existing `/W3` to `/W4` override warnings and CMake path-length
warnings for unrelated numeric-loop targets; no selected target failed.

The existing GCC sanitizer configuration has
`-fsanitize=address,undefined -fno-omit-frame-pointer`. Both selected suites
passed **2/2**, without sanitizer reports:

```bash
cmake --build build/ssa-gcc-asan-phase80 --target zr_vm_ssa_deopt_validation_test zr_vm_ssa_state_maps_test -j 4
ASAN_OPTIONS=detect_leaks=1:halt_on_error=1 ctest --test-dir build/ssa-gcc-asan-phase80 -R '^ssa_(deopt_validation|state_maps)$' --output-on-failure --no-tests=error
```

## Acceptance decision

Accepted for deopt reconstruction range/ID preflight and builder rollback.
No obsolete compatibility paths were introduced or found in the changed logic.
The full 01.04 gate
still requires CFG liveness, richer logical frames and executable recovery;
this change establishes safe structural input for that work. Reconstruction
value dominance is not proved by these range/ID checks.
