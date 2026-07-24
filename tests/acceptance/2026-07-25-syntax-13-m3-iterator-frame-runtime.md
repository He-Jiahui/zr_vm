# Syntax 13 M3 Iterator Frame Runtime Acceptance

## Scope

Syntax 13 M3 adds the synchronous core runtime primitive beneath future
iterator lowering. The changed layers are `zr_vm_core`, its direct Unity
targets, and core-runtime documentation. Parser M2 facts, bytecode, execution
dispatch, async scheduling, artifacts/AOT, debug/LSP, and dynamic fallbacks
remain outside this acceptance.

## Baseline

The first M3 runtime target failed to compile because
`zr_vm_core/iterator_runtime.h` did not exist. Subsequent lower-layer RED
tests exposed the missing `Fault` definition, missing-producer cleanup bypass,
absent current-value GC root, and absent typed pool API. Each failure was
closed by its narrow runtime implementation before final acceptance.

The fresh Clang and MSVC core builds print existing warnings outside M3,
including Clang GNU computed-goto warnings and MSVC warning-level/path-length
configuration warnings. Neither compiler emitted a diagnostic for an M3
source or test file, and both M3 executables exited successfully.

## Test Inventory

- `tests/iterator/test_iterator_runtime.c`: ordered values, unavailable
  current after completion, completion/fault/missing-producer cleanup,
  early/double close, same-frame reentrancy rejection, compact-GC value
  resolution, root replacement, pool reuse, and non-terminal release rejection.
- `tests/iterator/test_iterator_gc_drop.c`: compact collection while yielded,
  plus completion, fault, and early/double-close root release before direct
  `Unique` resource cleanup.

## Tooling Evidence

Each target was built and run from an independent Debug build directory:

| Toolchain | Build directory | Runtime target | GC/drop target | Process result |
| --- | --- | --- | --- | --- |
| GCC 11.4 | `.codex/build-s13m3-gcc` | 11 tests, 0 failures | 4 tests, 0 failures | exit 0 |
| Clang 14.0 | `.codex/build-s13m3-clang` | 11 tests, 0 failures | 4 tests, 0 failures | exit 0 |
| MSVC 19.44 | `.codex/build-s13m3-msvc` | 11 tests, 0 failures | 4 tests, 0 failures | exit 0 |

WSL commands used the `zr_vm_iterator_runtime_test` and
`zr_vm_iterator_gc_drop_test` targets after Debug CMake configuration with
`cc` and `clang`. MSVC imported `VsDevCmd` in the same PowerShell process,
then configured Ninja, built the same two targets, and ran both `.exe` files.

## Results

All fifteen focused assertions passed on all three toolchains. The final
pool boundary rejects a `READY` lease, while terminal storage is safely
reused. The GC/drop target confirms the frame root is released before a
direct resource owner is dropped for completion, fault, and early close.

## Acceptance Decision

Accepted on 2026-07-25 03:08 +08:00. The synchronous frame primitive is
complete for M3. Its deliberate exclusions are compiler lowering, async
iteration, execution-dispatch integration, artifacts/AOT, debug/LSP, and
legacy or dynamic fallback paths.

## Required Contract

- A caller-owned frame yields one current value per successful `MoveNext` and
  rejects reentrant advancement on that frame.
- Completion, fault, and close are terminal and invoke cleanup exactly once.
- A yielded GC object is read through a current `GcRootHandle` after compact
  collection; a terminal frame releases that root before cleanup.
- The optional typed frame pool reuses terminal storage without leaking a
  previous value, root, producer, or cleanup state into the next lease.
