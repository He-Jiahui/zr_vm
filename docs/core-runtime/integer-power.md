---
related_code:
  - zr_vm_core/include/zr_vm_core/math.h
  - zr_vm_core/src/zr_vm_core/execution/execution_dispatch.c
  - zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime.c
implementation_files:
  - zr_vm_core/include/zr_vm_core/math.h
plan_sources:
  - user: 2026-10-16 fix integer power zero-base and overflow boundary bugs
tests:
  - tests/core/test_native_math_failures.c
  - tests/cmake/native-math-direct-validation/CMakeLists.txt
  - tests/acceptance/native-math-failures.md
doc_type: module-detail
---

# Integer Power Helpers

## Purpose

`ZrCore_Math_UIntPower` and `ZrCore_Math_IntPower` provide the shared integer-power implementation used by typed interpreter and AOT paths. Keeping these operations in the core header gives both execution paths the same overflow and boundary behavior.

## Behavior Model

Both helpers use exponentiation by squaring. An exponent of zero returns one. Unsigned zero to a positive exponent returns zero without division. Before each result multiplication, the helper checks `result > UINT64_MAX / base`; before each needed square, it checks `base > UINT64_MAX / base`.

The signed helper returns zero for a negative exponent and handles bases zero, one, and minus one directly. For other negative bases it computes the magnitude with unsigned subtraction, which also represents the magnitude of `INT64_MIN`. A negative odd result whose magnitude is exactly `INT64_MAX + 1` returns `INT64_MIN` directly; larger magnitudes and positive results above `INT64_MAX` return the overflow sentinel zero.

## Constraints and Tradeoffs

The public signatures return only an integer, so zero represents both a legitimate zero result and overflow. Callers cannot distinguish these outcomes through this API. Typed interpreter and AOT paths apply language-level domain checks before calling the helpers; this helper contract does not replace those checks.

## Test Coverage

The standalone test executable separates zero-base behavior, unit and exact results, unsigned accumulated overflow, signed parity, and exact/overflow `INT64_MIN` cases into independent CTest cases. The independent CMake project lets these header-level checks run without changing the shared test manifest or requiring VM initialization.

## Plan Sources

This update fixes the zero-base division, incorrect overflow guard, and signed minimum-magnitude defects identified in the integer-power coverage audit.

## Open Issues

An API that needs to report overflow separately from a valid zero result would require an explicit status/out-parameter contract and coordinated caller migration. This fix preserves the existing public signatures.
