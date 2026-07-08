# AOT 07-S6 GC-Root-Only Frame Setup Retention

时间：2026-07-06 04:45:41 +08:00

## 状态

完成一个 07-S6 子切片：当生成函数需要 GC root frame 时，即使 frame descriptor 和 byte-frame 都可省略，
frame setup 也不能提前返回。

07~12 总目标未完成；LOCAL_ADDRESS root map 发射、GC 压力/root 正确性、exports/frame cleanup 更广覆盖、
in/out writeback、性能计数和完整 07-S6/07~12 验收仍待后续。

## 完成项目

- `backend_aot_write_c_frame_setup()` 的 `includeStackFrameSetup` 判定加入 `includeGcRootFrame`。
- setup 保留条件现在是 `includeFrameDescriptor || frameByteSize > 0u || includeGcRootFrame`。
- frame setup source contract 锁定该门禁，防止未来 register/local-address GC roots 被 zero-byte/descriptor-free
  入口提前剪掉。
- 既有静态 `&zr_aot_gc_root_map_%u` root-frame push 形态保持不变。

## RED

- WSL GCC `zr_vm_aot_c_frame_setup_contracts_test`：1 test / 1 failure，新增 `includeGcRootFrame` setup-retention
  断言后旧实现仍只按 descriptor/byte-frame 决定是否输出 setup。

## GREEN

- WSL GCC `zr_vm_aot_c_frame_setup_contracts_test`：1/0。
- WSL GCC `zr_vm_aot_c_value_type_shared_library_smoke_test`：5/0。
- WSL Clang `zr_vm_aot_c_frame_setup_contracts_test`：1/0。
- WSL Clang `zr_vm_aot_c_value_type_shared_library_smoke_test`：5/0。
- MSVC Debug `zr_vm_aot_c_frame_setup_contracts_test`：1/0。
- MSVC Debug `zr_vm_aot_c_value_type_shared_library_smoke_test`：5 tests / 0 failures / 1 expected Unix-only ignore。
