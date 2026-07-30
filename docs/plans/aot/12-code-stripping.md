# AOT 12：可达性与 Code/Metadata Stripping

## Reachability graph

节点至少包括function、type/layout、field/property accessor、constructor、generic instance/dictionary、native import、module initializer、reflection metadata与debug sidecar。边来自：

- direct/virtual/interface/callable call；
- TypeLayout的GC/drop/copy glue；
- module dependency和initializer；
- reflection/preserve roots；
- native callback/export；
- generic dictionary与constraint witness；
- serialization或注册schema的显式root。

禁止通过字符串扫描猜反射或native可达性。

## 模式

- **debug**：保留完整debug map和可配置reflection metadata。
- **release safe**：裁剪证明不可达code，保留声明的reflection roots。
- **release aggressive**：要求完整closed-world/package graph；dynamic load/reflection必须有显式descriptor。

裁剪不能改变`typeof/typeid`身份、可保留member的query结果、Drop/GC maps、module initialization顺序或native export。被裁剪member的反射查询必须稳定不可见，而不是悬空token。

## 验收

构建前后比较reachable manifest、binary size和行为；负例覆盖missing preserve、dynamic module、generic reflection、property accessor、callback和resource Drop。linker dead-strip只能作为后端优化，不能替代语言级reachability graph。

## 完成记录

[2026-07-03 metadata stripping baseline](./12-stripping/2026-07-03-metadata-stripping-baseline.md) 记录已有metadata stripping能力；新ModuleIdentity/reflection roots/native callback边需纳入统一graph。

[2026-07-30 function reachability manifest](./12-stripping/2026-07-30-function-reachability-manifest.md) 完成
function-node reason schema、root predecessor chain 与 deterministic manifest 子切片；S1/S2/S6 仍为部分完成。

[2026-07-30 type-layout reachability manifest](./12-stripping/2026-07-30-type-layout-reachability-manifest.md) 完成
type/layout-node frame edge、reflection annotation root、unresolved-layout fail-closed gate 与 deterministic manifest 子切片；
S1/S2/S6 仍为部分完成。

[2026-07-30 executable property accessor required root](./12-stripping/2026-07-30-property-accessor-required-root.md) 完成
非 abstract getter/setter/initializer 必需函数根、`root.property_accessor` 报告与 missing-required-root fail-closed 子切片；
S1/S2/S3/S6 仍为部分完成。

[2026-07-30 Resource Drop required root](./12-stripping/2026-07-30-resource-drop-required-root.md) 完成
resource prototype 的非 abstract destructor 必需函数根、`root.resource_drop` 报告与 unresolved Drop fail-closed 子切片；
S1/S2/S3/S6 仍为部分完成。

[2026-07-30 generic MethodSpec required root](./12-stripping/2026-07-30-generic-methodspec-required-root.md) 完成
current-module `MemberDef` MethodSpec preserve binding 的必需函数根、`root.generic_methodspec` 报告与
missing/ambiguous binding fail-closed 子切片；S1/S2/S3/S6 仍为部分完成。

[2026-07-30 reflection constructor required root](./12-stripping/2026-07-30-reflection-constructor-required-root.md) 完成
concrete class/struct public constructor 的保守必需函数根、`root.reflection_constructor` 报告与 unresolved
constructor fail-closed 子切片；S1/S2/S3/S6 仍为部分完成。

[2026-07-30 package method export required root](./12-stripping/2026-07-30-package-method-export-required-root.md) 完成
current-module `MemberDef` package method export 的必需函数根、`root.package_export` 报告与 invalid/ambiguous
binding fail-closed 子切片；S1/S2/S3/S6 仍为部分完成。

[2026-07-30 native callback materialization edge](./12-stripping/2026-07-30-native-callback-materialization-edge.md) 完成
structured native escape binding 到三类 callable materialization 的 `edge.native_callback`、普通 direct edge
分离与 malformed metadata fail-closed 子切片；S1/S3/S6 仍为部分完成，S2 descriptor root 仍开放。

[2026-07-30 canonical generic dictionary reachability](./12-stripping/2026-07-30-canonical-generic-dictionary-reachability.md) 完成
typed `TypeId` dictionary identity、owner `edge.generic_instance` manifest、2→1 trim stats 与 malformed schema
fail-closed 子切片；S1/S3/S6 及 AOT 08 G6 仍为部分完成。

[2026-07-30 native import contract reachability](./12-stripping/2026-07-30-native-import-contract-reachability.md) 完成
canonical contract 全树预检、retained owner `edge.native_import` manifest、4→3 trim stats 与 unreachable malformed
contract fail-closed 子切片；S1/S3/S6 及 AOT 11 A11.2 仍为部分完成。

[2026-07-30 debug sidecar reachability](./12-stripping/2026-07-30-debug-sidecar-reachability.md) 完成
canonical execution-location 全树预检、retained owner `edge.debug_sidecar` manifest、4→3 trim stats 与
unreachable malformed row fail-closed 子切片；S1/S3/S6 仍为部分完成，safepoint variable map 与 AOT 11
versioned DebugMap section 仍开放。

## 阶段与可证明性

| 阶段 | 图输入 | 输出/验证 |
|---|---|---|
| S1 graph schema | token、call/layout/module/native/reflection edges | deterministic reachable manifest |
| S2 root policy | entry/export/preserve/debug/native callback/dynamic descriptor | root reason chain与unknown dynamic warning/error |
| S3 code trim | function/thunk/generic instance/drop glue | link manifest、missing-edge negative |
| S4 metadata trim | type/member/property/constructor/string/signature pools | token/RID/pool remap和query visibility |
| S5 artifact publication | compacted `.zro/.zrm` + contract hashes | source/binary/full-AOT loader parity |
| S6 reporting | before/after bytes、root reasons、untrimmed causes | stable CLI/dump/diff output |

证据入口包括`tests/cli/test_cli_aot_compacted_metadata_sidecar.c`以及AOT 12 acceptance系列。目标扩展必须覆盖property accessor、resource Drop、generic dictionary/MethodSpec、reflection createInstance/invoke、native callback、ModuleIdentity/package export与DebugMap sidecar。

退出条件：每个保留节点有root/edge理由，每个删除token在reflection/debug中稳定不可见；裁剪前后目标程序行为一致；corrupt remap和遗漏required root fail closed；size收益与功能policy分别报告。
