# AOT M1.5 07-S5 Typed-Call Contract File Split

Timestamp: 2026-06-23 22:20:33 +08:00

## Scope

- Support sub-slice for 07-S5 only.
- Split the 1000-line typed-call contract aggregate before adding more ABI boundary assertions.
- No AOT production behavior changed.

## RED

- CMake was updated first to compile four split typed-call contract sources.
- `cmake --build build-wsl-gcc --target zr_vm_aot_c_typed_call_contracts_test` failed because `tests/parser/test_aot_c_typed_call_i64_contracts.c` did not exist yet.

## Implementation

- Kept `zr_vm_aot_c_typed_call_contracts_test` as the stable aggregate target.
- Reduced `tests/parser/test_aot_c_typed_call_contracts.c` to a thin Unity entry point.
- Moved per-type contract bodies into:
  - `tests/parser/test_aot_c_typed_call_i64_contracts.c`
  - `tests/parser/test_aot_c_typed_call_bool_contracts.c`
  - `tests/parser/test_aot_c_typed_call_u64_contracts.c`
  - `tests/parser/test_aot_c_typed_call_f64_contracts.c`
- Added `tests/parser/aot_c_typed_call_contract_cases.h` for case prototypes.
- Reused `tests/parser/aot_c_typed_call_contract_support.h` for repo text loading and needle assertions.

## Validation

- Focused build and test: `zr_vm_aot_c_typed_call_contracts_test` 4/0.
- Representative smoke: `zr_vm_aot_c_typed_direct_call_bool_shared_library_smoke_test` 28/0.
- Broader AOT group: source 19/0, call 4/0, typed call 4/0, constant 5/0, generic numeric 1/0, global 7/0, logical 4/0, power 2/0, frame setup 1/0, return 1/0, value SemIR 4/0, float 1/0, shared-library 8/0, call smoke 5/0, typed direct-call 5/0, i64 three-arg 8/0, bool 28/0, u64 25/0, f64 19/0, arithmetic 7/0, bitwise 6/0, global 9/0, logical 4/0, power 1/0, value-type 1/0.
- `git diff --check` on the touched test files passed with only existing LF/CRLF warnings.

## Size Notes

- `test_aot_c_typed_call_contracts.c`: 16 physical / 12 non-empty lines.
- `test_aot_c_typed_call_i64_contracts.c`: 260 / 255.
- `test_aot_c_typed_call_bool_contracts.c`: 308 / 302.
- `test_aot_c_typed_call_u64_contracts.c`: 224 / 218.
- `test_aot_c_typed_call_f64_contracts.c`: 207 / 201.
- `aot_c_typed_call_contract_cases.h`: 9 / 7.
- `aot_c_typed_call_contract_support.h`: 97 / 78.

## Still Open

- 07-S5 full typed ABI.
- Inline structs.
- `in`/`out` writeback.
- Deopt/dynamic bridge templates.
- General typed-return ABI.
- Full 07-S5 acceptance and stages 08-12.
