# AOT 11-S7ZST / 12-S7ZZZM retained signature TYPE_REF string remap

时间：2026-07-03 20:50:52 +08:00

## 范围

- 关闭 emitted `.zrp` metadata pruning 中 retained signature blob 的 `TYPE_REF` name string offset 遗漏。
- `backend_aot_c_zrp_metadata_string_pool.c` 现在扫描 retained signature blobs，收集 `TYPE_REF` 节点里的 name string offset 作为 string-pool roots。
- `backend_aot_c_zrp_metadata_signature.c` 在 retained signature blob copy 后把 `TYPE_REF` name string offset 重写到 compacted string pool offset。
- 本切片不改变 `TYPE_DEF` signature node 的既有 token rewrite 语义，不声明完整 11-S7、12-S7 或 07~12 总目标完成。

## RED

- 新增 `test_aot_c_zrp_metadata_methodspec_pruning_remaps_typeref_string_offset`：
  - MethodSpec 的泛型实参为 `TYPE_REF(baseType=OBJECT, nameStringOffset=ExternalArg)`。
  - `ExternalArg` 只被 retained signature blob 引用，后面还有 unused string slice。
- 旧实现下 WSL GCC focused run 失败：
  - `Expected 776 Was 764`
  - compacted `.zrp` 少保留了 `ExternalArg` 字符串。

## GREEN

- string-pool remap 支持按需扩容，避免签名内新增 string roots 超出按行表估算的容量。
- retained signature scanner 覆盖 `METHOD_SIG`、`FIELD_SIG` 与 standalone type-node roots，并递归穿过 generic/tuple/array/union/nullable/ownership 节点。
- signature rewrite context 增加 string remap，`TYPE_REF` 节点保留 base type 后重写 name string offset。

## 验证

- WSL GCC：
  - MethodSpec pruning 6/0
  - pool pruning 8/0
  - metadata pruning 22/0
  - TypeDef pruning 2/0
  - TypeSpec pruning 2/0
  - source contracts 24/0
- WSL clang：同组全部通过。
- Windows MSVC Debug：同组全部通过。

## 仍未关闭

- 完整 metadata sweep/pruning
- full trim analyzer
- annotation/promotion policy
- cross-module provider/runtime binding remaining edges
- 更完整 ABI drift/deopt coverage
