# AOT M1.5 07-S5 typed direct-call bool smoke support split

Status: support sub-slice complete; 07-S5 partial; M1.5/07 partial; stages 08-12 not started.

## Scope

This support slice splits the repeated bool typed direct-call shared-library smoke harness out of the concrete bool test file.

## Baseline

- `tests/parser/test_aot_c_typed_direct_call_bool_shared_library_smoke.c` had grown to 914 physical lines while adding the bool `==` route.
- The file repeated the same project setup, binary write, AOT C generation, generated-C assertions, shared-library compile command, and runtime execution for each bool thunk case.
- More bool expression routes remain likely, so adding more cases without a split would push the file over the large-file threshold.

## Implementation

- New `tests/parser/aot_c_typed_direct_call_bool_smoke_support.h` owns `SZrAotTypedDirectCallBoolSmokeCase`, generated path formatting, project/binary/AOT setup, generated-C assertions, Unix shared-library compilation, and runtime execution.
- The concrete bool smoke file now only defines the six bool cases and calls `run_aot_c_typed_direct_call_bool_smoke()`.
- This support split adds no new typed thunk behavior and preserves the same six smoke cases.

## Tooling Evidence

- WSL GCC Debug build directory: `build/codex-aot-07-wsl-gcc-debug`.
- Command:
  - `cmake --build build/codex-aot-07-wsl-gcc-debug --target zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test -j 2`
  - `./build/codex-aot-07-wsl-gcc-debug/bin/zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test`
- Result:
  - bool typed direct-call smoke 6/0
- File sizes after split:
  - `tests/parser/test_aot_c_typed_direct_call_bool_shared_library_smoke.c`: 170 physical / 157 non-empty lines
  - `tests/parser/aot_c_typed_direct_call_bool_smoke_support.h`: 211 physical / 191 non-empty lines

## Acceptance Decision

Accepted as a 07-S5 support split. This does not close 07-S5 and does not add behavior; it keeps the bool smoke ready for further narrow bool expression routes such as inequality.
