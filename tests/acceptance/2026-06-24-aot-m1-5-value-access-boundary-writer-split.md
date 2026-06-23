# AOT M1.5 / 07-S5 Value-Access Boundary Writer Split

时间：2026-06-24 02:04:55 +08:00

状态：支撑子切片完成；07-S5、M1.5/07 仍为部分完成；08-12 未开始。

## 完成项目

- 新增 `backend_aot_c_value_access_boundaries.c`。
- 从 `backend_aot_c_lowering_values.c` 迁出 unsupported meta/dynamic value-access 边界 writer。
- 从 `backend_aot_c_lowering_values.c` 迁出 `GET_MEMBER` / `SET_MEMBER` / `GET_MEMBER_SLOT` / `SET_MEMBER_SLOT` / `GET_BY_INDEX` / `SET_BY_INDEX` 动态运行时边界 writer。
- `backend_aot_c_lowering_values.c` 继续保留 `TO_STRING`、常量/物化、所有权和其他值 lowering，不再持有上述 VM value-access 边界模板。

## RED / GREEN

RED：

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_aot_c_global_contracts_test >/tmp/zr_value_boundary_red_build.log && ./build-wsl-gcc/bin/zr_vm_aot_c_global_contracts_test'
```

结果：`zr_vm_aot_c_global_contracts_test` 7 项中 2 项失败；dynamic member/index 和 meta value-access 两个合约在读取缺失的 `backend_aot_c_value_access_boundaries.c` 时按预期报 `Expected Non-NULL`。

GREEN：

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && ./build-wsl-gcc/bin/zr_vm_aot_c_global_contracts_test && ./build-wsl-gcc/bin/zr_vm_aot_c_global_shared_library_smoke_test && ./build-wsl-gcc/bin/zr_vm_aot_c_source_contracts_test'
```

结果：global contracts 7/0、global shared-library smoke 9/0、source contracts 19/0。

## 结构验证

```text
wsl bash -lc 'cd /mnt/e/Git/zr_vm && cmake --build build-wsl-gcc --target zr_vm_parser_shared && find build-wsl-gcc -name "*backend_aot_c_value_access_boundaries.c.o*" -print | sort | head -20'
```

结果：`zr_vm_parser_shared` 构建通过，并找到 `build-wsl-gcc/zr_vm_parser/CMakeFiles/zr_vm_parser_shared.dir/__/zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_value_access_boundaries.c.o`。

规模：

- `backend_aot_c_lowering_values.c`: 839 physical / 755 non-empty lines
- `backend_aot_c_value_access_boundaries.c`: 165 / 147
- `test_aot_c_global_contracts.c`: 703 / 653

## 未完成

07-S5 full typed ABI、inline structs、in/out writeback、deopt/dynamic bridges、broader dynamic value access hardening、完整 07-S5 acceptance 和 08-12 仍未完成。
