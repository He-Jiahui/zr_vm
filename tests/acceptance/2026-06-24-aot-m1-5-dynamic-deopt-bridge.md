# AOT M1.5 / 07-S5 Dynamic Deopt Bridge

时间：2026-06-24 02:53:01 +08:00

状态：子切片完成；07-S5、M1.5/07 仍为部分完成；08-12 未开始。

## 完成项目

- `backend_aot_write_c_dynamic_function_call()` 新增 `deoptId` 参数。
- `backend_aot_c_function_body.c` 对 SemIR dynamic `FUNCTION_CALL` / no-arg call 从 ExecIR 取 `deoptId`，super dynamic 路径使用 `ZR_RUNTIME_SEMIR_DEOPT_ID_NONE`。
- generated C dynamic call 边界新增 `zr_aot_dynamic_deopt_bridge deopt=...` marker，并调用 `ZrLibrary_AotRuntime_CallDynamicDeoptBridge(...)`。
- runtime helper 在可见 SemIR deopt table 时校验 deopt id，NONE 或缺少 metadata 时保持兼容并委托 `CallStackValue`。
- 新增独立 `tests/parser/test_aot_c_dynamic_deopt_bridge_smoke.c`，手工构造 SemIR dynamic call fixture，验证生成 C 文本与 shared-library 链接。
- 原 `test_aot_c_call_shared_library_smoke.c` 保持普通 generic direct-call `CallStackValue` 覆盖，不误判为 deopt bridge。

## RED / GREEN

RED：

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_call_contracts_test >/tmp/zr_aot_dynamic_deopt_red_build.log && ./build-wsl-gcc/bin/zr_vm_aot_c_call_contracts_test'
```

结果：`zr_vm_aot_c_call_contracts_test` 4 项中 1 项失败，动态 call 合约按预期报缺少 `TZrUInt32 deoptId` / dynamic deopt bridge 生成形状。

GREEN：

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake -S . -B build-wsl-gcc >/tmp/zr_aot_dynamic_deopt_reconfigure.log && cmake --build build-wsl-gcc --target zr_vm_aot_c_call_contracts_test zr_vm_aot_c_call_shared_library_smoke_test zr_vm_aot_c_dynamic_deopt_bridge_smoke_test >/tmp/zr_aot_dynamic_deopt_split_build2.log && ./build-wsl-gcc/bin/zr_vm_aot_c_call_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_call_shared_library_smoke_test && ./build-wsl-gcc/bin/zr_vm_aot_c_dynamic_deopt_bridge_smoke_test'
```

结果：call contracts 4/0、call shared-library smoke 5/0、dynamic deopt bridge smoke 1/0。

补充回归：

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_source_contracts_test zr_vm_aot_c_global_contracts_test zr_vm_semir_dynamic_call_deopt_test >/tmp/zr_aot_dynamic_deopt_related_build2.log && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_global_contracts_test && ./build-wsl-gcc/bin/zr_vm_semir_dynamic_call_deopt_test'
```

结果：source contracts 19/0、global contracts 7/0、SemIR dynamic call deopt 1/0。

## 结构验证

```text
git diff --check -- tests/CMakeLists.txt tests/parser/test_aot_c_call_contracts.c tests/parser/test_aot_c_call_shared_library_smoke.c tests/parser/test_aot_c_dynamic_deopt_bridge_smoke.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_emitter.h zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_call_boundaries.c zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_function_body.c zr_vm_library/include/zr_vm_library/aot_runtime.h zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_return.c
```

结果：无 whitespace error。

规模：

- `backend_aot_c_call_boundaries.c`: 233 physical / 223 non-empty lines
- `test_aot_c_call_shared_library_smoke.c`: 898 physical lines
- `test_aot_c_dynamic_deopt_bridge_smoke.c`: 189 physical lines

## 未完成

07-S5 full typed ABI、inline structs、in/out writeback、broader dynamic value access hardening、完整 07-S5 acceptance 和 08-12 仍未完成。
