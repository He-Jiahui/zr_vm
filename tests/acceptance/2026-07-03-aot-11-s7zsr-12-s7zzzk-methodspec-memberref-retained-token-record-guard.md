# AOT 11-S7ZSR / 12-S7ZZZK MethodSpec Imported MEMBER_REF Retained Token-Record Guard

时间：2026-07-03 19:54:49 +08:00

状态：完成。此切片只关闭 emitted `.zrp` metadata pruning 中 MethodSpec imported `MEMBER_REF`
retained-token-record guard 缺口，不声明完整 11-S7、12-S7 或 07~12 总目标完成。

## 范围

- `METHOD_SPEC.methodToken = MEMBER_REF(...)` 继续允许合法的跨模块 imported method reference。
- 当对应 imported `MEMBER_REF` token record 存在但其成员字段指向已被裁剪的本地 MethodDef/FieldDef 时，
  MethodSpec、相关 `SIGNATURE` token record 和 signature blob 必须一起被裁剪。
- MethodSpec count/copy、signature blob remap 与 MethodSpec signature rewrite 使用同一套 retained-record 判定，
  避免 compacted `.zrp` 中残留悬空 MethodSpec/signature payload。

## RED

新增 `test_aot_c_zrp_metadata_methodspec_pruning_drops_imported_member_ref_with_pruned_target` 后，
WSL GCC 聚焦测试失败：

```text
Expected 600 Was 639
```

这证明旧实现只检查 imported `MEMBER_REF` token record 是否存在，未继续确认该 record 的成员引用是否仍可保留。

## GREEN

实现 retained-aware imported `MEMBER_REF` token-record 判定后，聚焦测试通过：

```text
zr_vm_aot_c_zrp_metadata_methodspec_pruning_test: 3 Tests 0 Failures 0 Ignored
```

## 验证

- WSL GCC：MethodSpec pruning 3/0、metadata pruning 22/0、export token remap 10/0、source contracts 24/0。
- WSL clang：MethodSpec pruning 3/0、metadata pruning 22/0、export token remap 10/0、source contracts 24/0。
- Windows MSVC Debug：MethodSpec pruning 3/0、metadata pruning 22/0、export token remap 10/0、source contracts 24/0。

## 产出

- `tests/parser/test_aot_c_zrp_metadata_methodspec_pruning.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_remap.{h,c}`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_prune.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_signature.{h,c}`
