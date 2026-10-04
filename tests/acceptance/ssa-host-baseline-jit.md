---
related_code:
  - zr_vm_jit/CMakeLists.txt
  - zr_vm_jit/include/zr_vm_jit/backend.h
  - zr_vm_jit/src/orc_backend.cpp
  - zr_vm_jit/src/jit_state_maps.cpp
  - zr_vm_core/include/zr_vm_core/host_baseline_jit.h
  - zr_vm_core/src/zr_vm_core/execution/host_baseline_jit.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/execution/host_baseline_jit.c
  - zr_vm_jit/src/orc_backend.cpp
  - zr_vm_jit/src/jit_state_maps.cpp
tests:
  - tests/core/test_ssa_host_baseline_jit.c
  - tests/core/test_ssa_host_jit_optional.c
plan_sources:
  - docs/plans/ssa/10-jit-platforms/02-host-baseline-jit.md
  - docs/plans/ssa/10-jit-platforms/01-backend-service.md
  - "user: 2026-09-14 implement SSA plan directory"
doc_type: testing-guide
status: implemented-subset
---

# SSA 10.02 Host baseline JIT

## Scope

The core C contract and the optional C++ facade are implemented.  The facade
accepts only host x86-64/AArch64 target witnesses, validates the runtime/native
symbol allow-list, and delegates publication/lease ownership to the checked
core manager.  Android, iOS, and WASM targets are rejected before an
executable path is considered.

The current checkout has no verified LLVM ORC/JITLink provider.  This is an
intentional unavailable result, not a claim of generated machine code:
registration exposes capability and fallback state, while compile and entry
lookup return `BACKEND_UNAVAILABLE` or `FALLBACK_EXECBC` and never produce an
address.

## Focused tests

The `ssa_host_baseline_jit` C fixture constructs host target and publication
witnesses. It checks valid options, an unknown option flag, zero cache size,
WASM target rejection, the other supported architecture's mismatch, import
allow-list lookup and duplicate symbol IDs, complete versus incomplete map
registration, and single-operation versus combined-operation queries.
The manager sequence checks prepare, publish, duplicate identity rejection,
lease overflow, manager count corruption, retirement, deinit refusal while a
leased retired record remains, release, collection, and stale-handle rejection.
The facts and entry metadata do not represent generated or executed code.

With `ZR_VM_ENABLE_HOST_JIT=ON`, `ssa_host_jit_optional` additionally checks:

- registration, capability/fallback state, and the descriptor's explicit
  unavailable target-query result without a machine-code provider;
- a valid four-map declaration and the negative case `debugEntryCount == 0`;
- a valid compile request and fallback selection with an empty output handle;
- valid import lookup, wrong signature, absent symbol, and null manifest;
- prepared/published record ownership, zero entry address, eviction,
  active-lease shutdown refusal, release, collection, and completed shutdown;
- Android registration rejection after shutdown.

The fixture's temporary `publication.target.platform = WASM` is not consumed
by the subsequent `Register` call, which receives unchanged HOST options
while already registered. That call checks duplicate registration refusal.
Malformed map hashes, unsupported compile-operation masks, and frame-layout
mismatch are not constructed as negative cases in this fixture. An ABI
version mismatch is not constructed in the core fixture either.

## Reproducible evidence

Direct contract-only checks (WSL GCC/Clang):

```text
gcc -std=c11 -Wall -Wextra -Wpedantic -Werror \
  -I zr_vm_jit/include -I zr_vm_core/include -I zr_vm_common/include \
  -c tests/core/test_ssa_host_jit_optional.c
g++ -std=c++17 -Wall -Wextra -Wno-pedantic -Wno-write-strings -Werror \
  -I zr_vm_jit/include -I zr_vm_core/include -I zr_vm_common/include \
  -c zr_vm_jit/src/orc_backend.cpp
g++ -std=c++17 -Wall -Wextra -Wno-pedantic -Wno-write-strings -Werror \
  -I zr_vm_jit/include -I zr_vm_core/include -I zr_vm_common/include \
  -c zr_vm_jit/src/jit_state_maps.cpp
```

The resulting objects, `host_baseline_jit.c`, and the C fixture link and exit
zero with both GCC and Clang.  The CMake checks used for this slice were:

```text
cmake --build /tmp/zr_vm_jit_off --target zr_vm_ssa_host_baseline_jit_test -j 4
ctest --test-dir /tmp/zr_vm_jit_off -R '^ssa_host_baseline_jit$' --output-on-failure --no-tests=error

cmake --build /tmp/zr_vm_jit_on --target zr_vm_ssa_host_jit_optional_test -j 4
ctest --test-dir /tmp/zr_vm_jit_on -R '^ssa_host_jit_optional$' --output-on-failure --no-tests=error
```

Both CTest selections passed.  The `*_on` configure used Ninja, WSL GCC
11.4.0, C++17, `BUILD_SHARED_LIB=ON`, and all unrelated optional modules
disabled.  The default configure did not load a C++ compiler or the JIT
subdirectory.

## Explicit open gates

Real ORC/JITLink lowering, W^X page transitions, generated stack maps and
platform unwind/debug registration, two-architecture machine-code execution,
and compile-latency/warm-throughput measurements remain unavailable.  The
adapter reports those states rather than promoting a contract-only test to a
runtime JIT acceptance.
