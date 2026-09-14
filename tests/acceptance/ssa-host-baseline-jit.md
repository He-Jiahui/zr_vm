---
related_code:
  - zr_vm_jit/CMakeLists.txt
  - zr_vm_jit/include/zr_vm_jit/backend.h
  - zr_vm_jit/src/orc_backend.cpp
  - zr_vm_jit/src/jit_state_maps.cpp
  - zr_vm_core/include/zr_vm_core/host_baseline_jit.h
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

The existing `ssa_host_baseline_jit` C test covers target/ABI/import/publication
validation and the prepare → publish → acquire → evict → release → collect
lease sequence.  With `ZR_VM_ENABLE_HOST_JIT=ON`,
`ssa_host_jit_optional` additionally covers:

- C ABI descriptor shape and explicit no-ORC availability;
- all four state-map registrations and malformed count/hash rejection;
- compile-request operation and frame-layout checks;
- allow-listed imports and fallback selection;
- active-lease shutdown refusal followed by safe collection;
- no synthesized entry address on an unavailable backend.

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
