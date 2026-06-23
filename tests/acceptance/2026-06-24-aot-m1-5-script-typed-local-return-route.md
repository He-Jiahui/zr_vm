# AOT M1.5 / 07-S5 Script Typed-Local Return Route

时间：2026-06-24 05:06:03 +08:00

状态：子切片完成；07-S5 typed return boundary route 部分完成；M1.5/07 仍为部分完成；08-12 未开始。

## Scope

- 在 script-level typed-local bool/u64/f64 return 已能推导 MethodInfo signature 的基础上，让生成 C 的 runtime return route 使用 typed native-to-VM return helper。
- 目标生成物：bool/u64/f64 typed-local script return 应分别调用 `ZrLibrary_AotRuntime_ReturnBool`、`ReturnU64`、`ReturnF64`，而不是 generic `ZrLibrary_AotRuntime_Return(state, &frame, slot, ...)`。
- 本切片不处理表达式直接返回未形成 scalar local 的情况，不处理 inline structs、full typed ABI、`in`/`out` writeback、deopt/dynamic bridge ABI 或性能计数器。

## Baseline

- 上一切片已让 bool/u64/f64 typed-local script return 生成非空 `SZrAotSignature.returnType`。
- 但 `backend_aot_try_write_c_typed_return(...)` 的 bool/u64/f64 route 只调用 `can_direct_return_*_local(...)`，这些 proof 仍要求 callable return metadata。
- 脚本入口缺少 callable return metadata，因此 generated C 仍落回 generic frame-slot return。

## Test Inventory

- Focused generated-product: `tests/parser/test_aot_c_method_info_signature.c`
  - 保留 bool/u64/f64 signature row 断言。
  - 新增 `ReturnBool` / `ReturnU64` / `ReturnF64` helper 断言。
  - 禁止 generated C 出现 `ZrLibrary_AotRuntime_Return(state, &frame, ...)` generic frame-slot return。
- Source contract: `tests/parser/test_aot_c_return_contracts.c`
  - 锁定 typed-return route 同时接受 direct callable proof 和 script inferred proof。
- Related regression: method-info signature、return contracts、frame setup contracts、source contracts、typed scalar generated/shared-library smoke。

## Tooling Evidence

RED:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_method_info_signature_test >/tmp/zr_aot_script_typed_return_route_red_build.log && ./build-wsl-gcc/bin/zr_vm_aot_c_method_info_signature_test'
```

结果：method-info signature test 1/1 失败，失败点为缺失 `ZrLibrary_AotRuntime_ReturnBool(state, zr_aot_b`。

GREEN focused:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_method_info_signature_test >/tmp/zr_aot_script_typed_return_route_green_build.log && ./build-wsl-gcc/bin/zr_vm_aot_c_method_info_signature_test'
```

结果：method-info signature 1/0。

Related regression:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_method_info_signature_test zr_vm_aot_c_return_contracts_test zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_typed_scalar_test >/tmp/zr_aot_script_typed_return_route_related_build.log && ./build-wsl-gcc/bin/zr_vm_aot_c_method_info_signature_test && ./build-wsl-gcc/bin/zr_vm_aot_c_return_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_frame_setup_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_typed_scalar_test'
```

结果：method-info signature 1/0、return contracts 1/0、frame setup contracts 1/0、source contracts 19/0、typed scalar 1/0。

Generated return-route grep:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && grep -nE -- "ZrLibrary_AotRuntime_ReturnBool\(state, zr_aot_b|ZrLibrary_AotRuntime_ReturnU64\(state, zr_aot_u|ZrLibrary_AotRuntime_ReturnF64\(state, zr_aot_f" build-wsl-gcc/tests_generated/aot_c_method_info_signature/bool/main.c build-wsl-gcc/tests_generated/aot_c_method_info_signature/u64/main.c build-wsl-gcc/tests_generated/aot_c_method_info_signature/f64/main.c && if grep -nE -- "ZrLibrary_AotRuntime_Return\(state, &frame," build-wsl-gcc/tests_generated/aot_c_method_info_signature/bool/main.c build-wsl-gcc/tests_generated/aot_c_method_info_signature/u64/main.c build-wsl-gcc/tests_generated/aot_c_method_info_signature/f64/main.c; then exit 1; fi'
```

结果：bool/u64/f64 分别命中 `ReturnBool(state, zr_aot_b2)`、`ReturnU64(state, zr_aot_u2)`、`ReturnF64(state, zr_aot_f2)`；generic frame-slot return grep 无命中。

CTest registration:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && ctest --test-dir build-wsl-gcc -R "^aot_c_method_info_signature$" --output-on-failure'
```

结果：CTest `aot_c_method_info_signature` 1/1 passed。

## Results

- `backend_aot_c_typed_return.c` now lets bool/u64/f64 script-level typed-local returns use the inferred scalar proof helpers when callable return metadata is absent.
- Existing typed callable direct-return helpers remain in the route; their callable metadata gate is still preserved in `backend_aot_c_scalar_locals.c`.
- Generated script-level bool/u64/f64 typed-local return now exits through typed native-to-VM return helpers rather than reading from frame slots through the generic return boundary.
- 规模：`backend_aot_c_typed_return.c` 56 physical lines；`test_aot_c_method_info_signature.c` 153 physical lines；`test_aot_c_return_contracts.c` 385 physical lines。

## Acceptance Decision

Accepted for this narrow 07-S5 script typed-local return-route slice.

Remaining open: expression-direct-return scalarization outside typed-local proof scope, full typed ABI, inline structs, `in`/`out` writeback, complete 07-S5 acceptance, performance/SZrValue runtime counters, and stages 08-12.
