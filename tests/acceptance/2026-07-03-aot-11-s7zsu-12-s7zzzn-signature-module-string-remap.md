# AOT 11-S7ZSU / 12-S7ZZZN retained signature MODULE string remap

时间：2026-07-03 21:20:45 +08:00

## 范围

- 关闭 emitted `.zrp` metadata pruning 中 retained signature blob 的 `MODULE(nameStringOffset, versionStringOffset)` 遗漏。
- `backend_aot_c_zrp_metadata_string_pool.c` 现在把 retained signature `MODULE` 节点里的 module name/version offsets 作为 string-pool roots。
- `backend_aot_c_zrp_metadata_signature.c` 在 retained signature blob copy 后把 `MODULE` 的两个 string offsets 重写到 compacted string pool offsets。
- 本切片不改变 `ASSEMBLY_REF`/ModuleRef token remap 语义，不声明完整 11-S7、12-S7 或 07~12 总目标完成。

## RED

- 新增 `test_aot_c_zrp_metadata_methodspec_pruning_remaps_module_string_offsets`：
  - MethodSpec 的泛型实参为 `MODULE("__entry", "1.0.0")`。
  - module name/version 只被 retained signature blob 引用，前面有 unused string slice 迫使 offset 真实重映射。
- 旧实现下 WSL GCC focused run 失败：
  - `Expected 778 Was 764`
  - compacted `.zrp` 少保留了 `__entry` 与 `1.0.0` 字符串。

## GREEN

- retained signature scanner 对 `MODULE` 节点读取两个 string offsets，并把它们加入 compacted string-pool remap。
- retained signature rewrite 对同一 `MODULE` 节点的两个 payload 分别执行 string offset remap。

## 验证

- WSL GCC：
  - MethodSpec pruning 7/0
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
