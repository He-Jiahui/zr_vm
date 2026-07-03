# AOT 11-S7ZSS / 12-S7ZZZL MethodSpec Imported MEMBER_REF Recursive Retained Token-Record Guard

时间：2026-07-03 20:26:48 +08:00

状态：完成。此切片只关闭 emitted `.zrp` metadata pruning 中 MethodSpec imported `MEMBER_REF`
recursive retained-token-record guard 缺口，不声明完整 11-S7、12-S7 或 07~12 总目标完成。

## 范围

- `METHOD_SPEC.methodToken = MEMBER_REF(...)` 继续允许合法的跨模块 imported method reference。
- imported `MEMBER_REF` token record 如果通过另一个 imported `MEMBER_REF` 间接指向已裁剪的本地
  MethodDef/FieldDef，MethodSpec、相关 `SIGNATURE` token record 和 signature blob 必须一起被裁剪。
- retained token-record pruning 与 MethodSpec row pruning 共享递归 retained-record 判定；直接自引用
  imported member record 仍可保留，递归深度以 token-record 数量封顶。

## RED

新增 `test_aot_c_zrp_metadata_methodspec_pruning_drops_nested_imported_member_ref_with_pruned_target` 后，
WSL GCC 聚焦测试失败：

```text
Expected 504 Was 639
```

这证明旧实现只检查第一层 imported `MEMBER_REF` record，未继续检查其 nested `MEMBER_REF` 引用链。

## GREEN

实现 recursive retained-aware imported `MEMBER_REF` token-record 判定后，聚焦测试通过：

```text
zr_vm_aot_c_zrp_metadata_methodspec_pruning_test: 5 Tests 0 Failures 0 Ignored
```

## 验证

- WSL GCC：MethodSpec pruning 5/0、metadata pruning 22/0、export token remap 10/0、source contracts 24/0。
- WSL clang：MethodSpec pruning 5/0、metadata pruning 22/0、export token remap 10/0、source contracts 24/0。
- Windows MSVC Debug：MethodSpec pruning 5/0、metadata pruning 22/0、export token remap 10/0、source contracts 24/0。

## 产出

- `tests/parser/test_aot_c_zrp_metadata_methodspec_pruning.c`
- `zr_vm_aot/zr_vm_parser/src/zr_vm_parser/backend_aot/backend_aot_c_zrp_metadata_remap.c`
