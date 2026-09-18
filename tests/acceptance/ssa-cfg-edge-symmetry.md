# SSA M1: structural CFG edge symmetry

## Scope

- `docs/plans/ssa/01-execir-ssa/03-effects-verifier.md`, structural verification
  slice; the core verifier now checks that each block-level successor has a
  reciprocal predecessor and vice versa.
- No graph construction, opcode execution, block-edge multiplicity, or
  terminator-to-block edge identity rules are introduced by this change.

## Baseline and RED

- Before the verifier change, the new `ssa_core_model` fixture first built
  successfully in WSL GCC 11.4, then failed as expected:
  `FAIL: forward CFG edge without reciprocal predecessor accepted`.
- The prior structural verifier checked edge IDs and ranges but did not
  compare the two adjacency lists. All block IDs and ranges are validated
  before the new reciprocal lookup, so invalid IDs cannot cause indexing past
  the block array.

## Test inventory

- Positive: two blocks connected in both lists pass structural verification.
- Negative: remove the target's predecessor entry, or remove the source's
  successor entry; both fail with `INVALID_BLOCK`, owning block and the
  respective source/target IDs. The fixture retains the original `BRANCH`
  instruction while testing structural graph adjacency independently.
- Existing `ssa_core_model` checks empty modules, deep clone ownership,
  invalid opcode and allocation overflow, and null operand pools.
- Neighbor consumers `ssa_effects_verifier` and `ssa_pass_manager_scalar`
  are also part of the focused regression selector.

## Tooling evidence (2026-09-17)

```text
ctest --test-dir build/ssa-gcc-debug -R 'ssa_(core_model|effects_verifier|pass_manager_scalar)$' --output-on-failure --no-tests=error
ctest --test-dir build/ssa-clang-debug -R 'ssa_(core_model|effects_verifier|pass_manager_scalar)$' --output-on-failure --no-tests=error
ctest --test-dir D:/zr-ssa-verify-871bc234 -R 'ssa_(core_model|effects_verifier|pass_manager_scalar)$' --output-on-failure --no-tests=error
```

WSL GCC 11.4, WSL Clang 14 and Windows MSVC 19.44 each report **3/3
passed** for these focused targets. MSVC uses its VS development environment;
the D: build was configured with Ninja, Debug, static library, tests enabled
and host JIT enabled. MSVC emitted the existing D9025 `/W3`→`/W4` warning.

GCC also compiled the standalone `ssa_core_model` fixture with
`-fsanitize=address,undefined -fno-omit-frame-pointer` against the core
ExecIR model, structural, SSA and effect verifiers, materializer and contract
sources. `/mnt/d/zr-ssa-verify-871bc234/ssa_cfg_asan` printed
`ssa core model PASS` with no sanitizer report (exit 0). There is no
resource/lease transfer in the new edge fixture; the module is freed normally.

The wider WSL GCC build initially stopped when `ar` could not copy
`build/ssa-gcc-debug/lib/libzr_vm_core.a` (`Input/output error`). `df -h`
showed E: at 100% usage with about 29 MB remaining; this is a build-volume
failure, not an ExecIR compiler diagnostic. A fresh WSL-native `/tmp` build
then compiled all four requested targets. Its four-test CTest run reported
3/4 passed: the existing, pre-edited `tests/parser/test_ssa_construction.c`
failed in `dominator_diamond_cfg` and
`dominator_rejects_invalid_successor`. Those tests directly exercise the
unchanged parser `exec_ir_cfg.c`; the diamond fixture calls an append API twice
for the same block while replacing its range, and the dominator implementation
silently skips an out-of-range successor. The fixture changes predated this
slice and are not staged here. The temporary WSL build directory subsequently
became unavailable; the individual GCC core/effects/pass executables were
also run from the named persistent E: build and each passed.

The original E: MSVC Debug build also failed during PDB output with
`LNK1201` because the same drive was full. The D: build above completed
the focused MSVC check without deleting or modifying existing build outputs.

## Acceptance decision

The symmetric adjacency check is accepted as a **focused structural slice**.
M1 stays open; the dirty-tree `ssa_construction` failures, executable backends,
duplicate-edge identity and full CFG/effect verification remain unresolved.
