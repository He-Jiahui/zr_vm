# AOT M1.5 / 07-S5 Dynamic Value-Access Deopt Bridge

时间：2026-06-24 03:24:02 +08:00

状态：子切片完成；07-S5、M1.5/07 仍为部分完成；08-12 未开始。

## 完成项目

- Dynamic member/index value-access 边界 writer 新增 `deoptId` 参数。
- `backend_aot_c_function_body.c` 对 `GET_MEMBER` / `SET_MEMBER` / `GET_BY_INDEX` / `SET_BY_INDEX` 及 member-slot fallback 从 ExecIR 取 SemIR deopt id，缺少可见 deopt row 时使用 `ZR_RUNTIME_SEMIR_DEOPT_ID_NONE`。
- generated C 在 `GetMember` / `SetMember` / `GetMemberSlot` / `SetMemberSlot` / `GetByIndex` / `SetByIndex` helper 前输出 `zr_aot_value_dynamic_deopt_bridge deopt=...` marker。
- 新增公开 runtime helper `ZrLibrary_AotRuntime_ValidateDynamicDeoptBridge(...)`，并让 `CallDynamicDeoptBridge(...)` 复用同一校验入口。
- `tests/parser/test_aot_c_dynamic_deopt_bridge_smoke.c` 扩展为同时覆盖 dynamic call、dynamic member get、dynamic index get 的 generated C 文本和 shared-library 链接。

## RED / GREEN

RED：

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_global_contracts_test >/tmp/zr_aot_value_deopt_red_build.log && ./build-wsl-gcc/bin/zr_vm_aot_c_global_contracts_test'
```

结果：`zr_vm_aot_c_global_contracts_test` 7 项中 1 项失败，dynamic member/index 合约按预期报缺少 `ZrLibrary_AotRuntime_ValidateDynamicDeoptBridge(...)` / value-access deopt bridge 生成形状。

GREEN focused：

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && ./build-wsl-gcc/bin/zr_vm_aot_c_global_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_dynamic_deopt_bridge_smoke_test'
```

结果：global contracts 7/0、dynamic deopt bridge smoke 2/0。

补充回归：

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_global_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_dynamic_deopt_bridge_smoke_test && ./build-wsl-gcc/bin/zr_vm_aot_c_global_shared_library_smoke_test && ./build-wsl-gcc/bin/zr_vm_semir_dynamic_member_deopt_test && ./build-wsl-gcc/bin/zr_vm_semir_dynamic_index_deopt_test'
```

结果：source contracts 19/0、global contracts 7/0、dynamic deopt bridge smoke 2/0、global shared-library smoke 9/0、SemIR dynamic member deopt 1/0、SemIR dynamic index deopt 1/0。

## 结构验证

```text
git diff --check -- zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_access_boundaries.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c zr_vm_library/include/zr_vm_library/aot_runtime.h zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_return.c tests/parser/test_aot_c_global_contracts.c tests/parser/test_aot_c_dynamic_deopt_bridge_smoke.c
```

结果：退出 0；无 whitespace error，仅提示既有 LF/CRLF 规范化警告。

规模：

- `backend_aot_c_value_access_boundaries.c`: 201 physical / 180 non-empty lines
- `test_aot_c_dynamic_deopt_bridge_smoke.c`: 337 physical / 288 non-empty lines
- `test_aot_c_global_contracts.c`: 716 physical / 666 non-empty lines

## 未完成

07-S5 full typed ABI、inline structs、in/out writeback、完整 07-S5 acceptance 和 08-12 仍未完成。
