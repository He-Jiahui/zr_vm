# 2026-06-25 AOT 11-S4E Generic Dictionary Type Layout Runtime Resolver

## 范围

- 计划来源：`docs/plans/aot/11-metadata.md` §4 token↔cTypeId↔ZrLayout 三向表，以及 `docs/plans/aot/08-generic-sharing.md` §2 泛型字典 TYPE_LAYOUT/SIZEOF slot。
- 本切片目标：让 08-S4 已有泛型字典 TYPE_LAYOUT/SIZEOF consumer 改读 11-S4D 的 public metadata runtime layout resolver。
- 非目标：TypeSpec/generic layout materialization、runtime layout construction、ownership offset 表、反射/GC consumer 强制迁移、完整 token/cTypeId/layout cache、shared generic runtime dispatch 改造。

## RED

新增 `tests/parser/test_aot_c_generic_reference_sharing.c` 覆盖：

- 字典 TYPE_LAYOUT/SIZEOF slot 的 `typeLayoutId` 为 42。
- metadata function 的 `prototypeFrameTypeLayouts[42]` 故意放入 stale layout，`byteSize = 96`。
- module metadata runtime 的 `codeRegistration->typeLayouts[42]` 放入 registry layout，`byteSize = 24`。
- 期望 `ZrLibrary_AotRuntime_GenericSlot_TypeLayout(state, &dictionary, metadataRuntime, 0u)` 返回 registry layout，`TryGetSizeOf()` 返回 24。
- registry 缺失时应返回 null/false，不允许 fallback 到 prototype layout cache。

RED 命令：

```text
wsl bash -lc 'cmake --build build/codex-wsl-gcc-debug --target zr_vm_aot_c_generic_reference_sharing_test -j2 && ./build/codex-wsl-gcc-debug/bin/zr_vm_aot_c_generic_reference_sharing_test'
```

RED 结果：编译先暴露旧 API 仍要求 `const SZrFunction*`；临时类型兼容后运行失败，registry layout 断言得到 null，证明字典 layout consumer 尚未通过 metadata runtime resolver。

## GREEN

完成项目：

- `zr_vm_library/include/zr_vm_library/aot_runtime.h`
  - `ZrLibrary_AotRuntime_GenericSlot_TypeLayout()` 与 `ZrLibrary_AotRuntime_GenericSlot_TryGetSizeOf()` 改为接收 `SZrMetadataRuntime*`。
- `zr_vm_library/src/zr_vm_library/aot_runtime/aot_runtime_generic_dictionary.c`
  - TYPE_LAYOUT/SIZEOF slot 仍保留 `staticTypeLayout` 快路径。
  - 非静态 layout 改为调用 `ZrCore_MetadataRuntime_ResolveTypeLayout(metadataRuntime, slot->typeLayoutId)`。
  - 删除 prototype frame layout cache fallback。
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_generic_sharing.c`
  - generated C 宏改为 `ZrAot_GenericSlot_TypeLayout(metadataRuntime, dict, slotIndex)`。
  - shared-reference generic 函数签名携带 `SZrMetadataRuntime *metadataRuntime`。

## 验证

WSL GCC Debug:

```text
zr_vm_aot_c_generic_reference_sharing_test 4/0
zr_vm_aot_c_generic_call_typed_test 6/0
zr_vm_aot_c_source_contracts_test 19/0
zr_vm_aot_c_frame_setup_contracts_test 1/0
zr_vm_metadata_runtime_type_layout_test 3/0
zr_vm_metadata_runtime_query_test 20/0
zr_vm_aot_c_shared_library_smoke_test 8/0
zr_vm_aot_c_value_type_shared_library_smoke_test 2/0
zr_vm_aot_c_descriptor_diagnostics_test 2/0
```

WSL Clang Debug:

```text
zr_vm_aot_c_generic_reference_sharing_test 4/0
zr_vm_aot_c_generic_call_typed_test 6/0
zr_vm_aot_c_source_contracts_test 19/0
zr_vm_aot_c_frame_setup_contracts_test 1/0
zr_vm_metadata_runtime_type_layout_test 3/0
zr_vm_metadata_runtime_query_test 20/0
zr_vm_aot_c_shared_library_smoke_test 8/0
zr_vm_aot_c_value_type_shared_library_smoke_test 2/0
zr_vm_aot_c_descriptor_diagnostics_test 2/0
```

Windows MSVC Debug:

```text
zr_vm_aot_c_generic_reference_sharing_test 4/0
zr_vm_aot_c_generic_call_typed_test 6/0, 3 ignored Unix-only branches
zr_vm_aot_c_source_contracts_test 19/0
zr_vm_aot_c_frame_setup_contracts_test 1/0
zr_vm_metadata_runtime_type_layout_test 3/0
zr_vm_metadata_runtime_query_test 20/0
zr_vm_aot_c_shared_library_smoke_test 8/0, 8 ignored Unix-only branches
zr_vm_aot_c_value_type_shared_library_smoke_test 2/0, 1 ignored Unix-only branch
zr_vm_aot_c_descriptor_diagnostics_test 2/0, 2 ignored Unix-only branches
```

Observed existing warnings:

- WSL Clang still reports existing const-discard warnings in `zr_vm_library/src/zr_vm_library/project/project.c`.
- WSL Clang generated-C smoke still reports the existing logical-not parentheses warning.
- MSVC still reports existing const qualifier and unreachable-code warnings in unrelated runtime/project paths.

## 决策

11-S4E 子切片通过。泛型字典 TYPE_LAYOUT/SIZEOF slot 现在复用 11-S4D 的 `SZrMetadataRuntime` layout resolver，满足 11 §4 “反射、泛型字典、GC descriptor 读取同一 layout 表”的泛型 consumer 接线要求。

完整 11-S4 仍未关闭；TypeSpec/generic layout materialization、ownership offsets、runtime layout construction、反射/GC consumer 迁移和完整三向缓存仍待后续。
