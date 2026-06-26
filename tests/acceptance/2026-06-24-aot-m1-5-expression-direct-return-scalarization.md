# AOT M1.5 / 07-S5 Expression Direct Return Scalarization

时间：2026-06-24 05:30:05 +08:00

状态：子切片完成；07-S5 expression direct-return scalarization 部分完成；M1.5/07 仍为部分完成；08-12 未开始。

## Scope

- 在 script-level bool/u64/f64 typed-local return 已具备 MethodInfo signature 与 typed return helper route 的基础上，覆盖直接表达式返回：
  - `return left > right;`
  - `return left + right;` for `uint`
  - `return left + right;` for `float`
- 目标生成物必须同时满足：
  - `SZrAotSignature.returnType` 非空，`hasReturnValue=1`，signature type row 使用 bool/u64/f64 对应 `baseType/staticCType`。
  - 返回边界调用 `ZrLibrary_AotRuntime_ReturnBool` / `ReturnU64` / `ReturnF64`。
  - 表达式结果本身保持在 `zr_aot_bN` / `zr_aot_uN` / `zr_aot_fN`，不生成 generic `ZrLibrary_AotRuntime_Return(state, &frame, ...)`，也不在表达式结果块生成 `SZrTypeValue *zr_aot_destination`。
- 本切片不处理 full typed ABI、inline structs、`in`/`out` writeback、deopt/dynamic bridge ABI、完整 07-S5 acceptance 或性能计数器。

## Baseline

- 上一切片已让 typed-local bool/u64/f64 script returns 走 typed native-to-VM return helper。
- 但直接表达式返回没有显式 typed-local binding，例如 `return left > right;` 的结果槽未被 scalar-local 声明记录覆盖。
- 生成物表现为 MethodInfo signature 缺少 return type，表达式结果写回 `SZrTypeValue` frame slot，并在 return 边界走 generic frame-slot return。

## Test Inventory

- Focused generated-product: `tests/parser/test_aot_c_method_info_signature.c`
  - 保留 typed-local bool/u64/f64 return cases。
  - 增加 bool/u64/f64 direct expression return cases。
  - 对所有 cases 断言 signature row、typed return helper，并禁止 generic frame-slot return。
- Source contract: `tests/parser/test_aot_c_source_contracts.c`
  - 锁定 `backend_aot_c_scalar_locals_kind_from_result_opcode(...)` 与 `record_result_destinations(...)` 形态。
  - 更新 bool logical scalar-local contract，从 declared typed-local gate 改为 expression result destination recording。
- Related regression: method-info signature、return contracts、frame setup contracts、source contracts、typed scalar generated/shared-library smoke。

## Tooling Evidence

RED:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_method_info_signature_test >/tmp/zr_aot_expr_return_red_build.log && ./build-wsl-gcc/bin/zr_vm_aot_c_method_info_signature_test'
```

结果：method-info signature test 1/1 失败，direct bool expression case 缺失 `.returnType = &zr_aot_signature_0_types[0],`。

GREEN focused:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_method_info_signature_test >/tmp/zr_aot_expr_return_green_build.log && ./build-wsl-gcc/bin/zr_vm_aot_c_method_info_signature_test'
```

结果：method-info signature 1/0。

Related build and regression:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_method_info_signature_test zr_vm_aot_c_return_contracts_test zr_vm_aot_c_frame_setup_contracts_test zr_vm_aot_c_source_contracts_test zr_vm_aot_c_typed_scalar_test >/tmp/zr_aot_expr_return_related_build.log'
```

结果：相关目标构建通过。一个早先 build+run 合并命令在 124s 处超时；拆分构建后重新运行二进制验证。

```text
./build-wsl-gcc/bin/zr_vm_aot_c_method_info_signature_test
./build-wsl-gcc/bin/zr_vm_aot_c_return_contracts_test
./build-wsl-gcc/bin/zr_vm_aot_c_frame_setup_contracts_test
./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test
./build-wsl-gcc/bin/zr_vm_aot_c_typed_scalar_test
```

结果：method-info signature 1/0、return contracts 1/0、frame setup contracts 1/0、source contracts 19/0、typed scalar 1/0。

Generated expression-return grep:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && grep -nE "returnType = &zr_aot_signature_0_types\[0\]|hasReturnValue = \(TZrUInt8\)1u|baseType = \(TZrUInt16\)(1|9|11)u|staticCType = \(TZrUInt16\)(1|9|11)u|ReturnBool\(state, zr_aot_b|ReturnU64\(state, zr_aot_u|ReturnF64\(state, zr_aot_f|ZrLibrary_AotRuntime_Return\(state, &frame,|SZrTypeValue \*zr_aot_destination" build-wsl-gcc/tests_generated/aot_c_method_info_signature/bool_expr/main.c build-wsl-gcc/tests_generated/aot_c_method_info_signature/u64_expr/main.c build-wsl-gcc/tests_generated/aot_c_method_info_signature/f64_expr/main.c'
```

结果：bool/u64/f64 expression cases 均命中 signature return type、`hasReturnValue=1`、对应 `baseType/staticCType` 和 `ReturnBool` / `ReturnU64` / `ReturnF64`；未命中 generic frame-slot return 或 `SZrTypeValue *zr_aot_destination`。

CTest registration:

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && ctest --test-dir build-wsl-gcc -R "^aot_c_method_info_signature$" --output-on-failure'
```

结果：CTest `aot_c_method_info_signature` 1/1 passed。

Diff hygiene:

```text
git diff --check -- tests/parser/test_aot_c_method_info_signature.c tests/parser/test_aot_c_source_contracts.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_scalar_locals.c
```

结果：通过，仅报告 touched C files 的 LF/CRLF 工作区换行提示。

## Results

- `backend_aot_c_scalar_locals.c` now records scalar-local destination coverage for typed expression result opcodes through `backend_aot_c_scalar_locals_kind_from_result_opcode(...)` and `backend_aot_c_scalar_locals_record_result_destinations(...)`.
- The return-boundary liveness proof now uses the shared kind-based `backend_aot_c_scalar_locals_can_return_kind_local(..., expectedKind, ZR_FALSE)` path, so bool/u64/f64 expression results can terminate value-slot liveness at `FUNCTION_RETURN` just like i64.
- The constant skip proof uses the same kind-based return predicate, keeping future scalar constant direct-return handling aligned with the expression path.
- Generated bool/u64/f64 direct expression returns now keep the expression result in scalar locals and return through typed native-to-VM helper boundaries.
- 规模：`backend_aot_c_scalar_locals.c` 2999 physical lines；`test_aot_c_method_info_signature.c` 174 physical lines；`test_aot_c_source_contracts.c` 2116 physical lines。

## Acceptance Decision

Accepted for this narrow 07-S5 expression direct-return scalarization slice.

Remaining open: full typed ABI, inline structs, `in`/`out` writeback, complete 07-S5 acceptance, performance/SZrValue counters, and stages 08-12.
