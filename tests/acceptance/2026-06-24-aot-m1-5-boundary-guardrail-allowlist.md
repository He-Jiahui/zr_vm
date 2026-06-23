# AOT M1.5 / 07-S5 Boundary Guardrail Allowlist

时间：2026-06-24 03:34:00 +08:00

状态：支撑子切片完成；07-S5 acceptance/guardrail、M1.5/07 仍为部分完成；08-12 未开始。

## 完成项目

- `tests/parser/test_aot_c_guardrail_contracts.c` 的 runtime-call classifier 显式分类当前 07-S5 VM/native boundary helper。
- 允许的边界 helper 覆盖 typed return、inline-struct return、`Sync*Local`、`CallStackValue`、`CallStaticDirect`、`CallInlineStruct`、`CallDynamicDeoptBridge`、`ValidateDynamicDeoptBridge` 和 dynamic member/index `Get*` / `Set*` helper。
- 继续拒绝未分类 VM fallback：`ZrCore_Stack_GetValue`、`ZR_VALUE_FAST_SET`、`ZrLibrary_AotRuntime_Add`。
- 本切片只收紧 acceptance 护栏，不改变 generated C 行为。

## RED / GREEN

RED：

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_guardrail_contracts_test >/tmp/zr_aot_guardrail_075_red_build.log && ./build-wsl-gcc/bin/zr_vm_aot_c_guardrail_contracts_test'
```

结果：`zr_vm_aot_c_guardrail_contracts_test` 4 项中 1 项失败，新增 07-S5 boundary helper 按预期未被 allowlist 分类。

GREEN focused：

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_guardrail_contracts_test >/tmp/zr_aot_guardrail_075_build.log && ./build-wsl-gcc/bin/zr_vm_aot_c_guardrail_contracts_test'
```

结果：guardrail contracts 4/0。

补充回归：

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_guardrail_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_global_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_return_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_call_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_typed_call_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_dynamic_deopt_bridge_smoke_test'
```

结果：source contracts 19/0、guardrail contracts 4/0、global contracts 7/0、return contracts 1/0、call contracts 4/0、typed-call contracts 4/0、dynamic deopt bridge smoke 2/0。

## 结构验证

```text
git diff --check -- tests/parser/test_aot_c_guardrail_contracts.c
```

结果：退出 0；无 whitespace error，仅提示既有 LF/CRLF 规范化警告。

规模：

- `test_aot_c_guardrail_contracts.c`: 159 physical / 139 non-empty lines

## 未完成

07-S5 full typed ABI、inline structs、in/out writeback、完整 07-S5 acceptance 和 08-12 仍未完成。
