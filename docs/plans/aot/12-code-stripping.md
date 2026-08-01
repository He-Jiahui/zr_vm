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

## Syntax 上游追踪

| Syntax 节点 | 本计划消费的稳定输入 | 本计划退出责任 |
|---|---|---|
| [01/M4-M5](../syntax/2026-07-18-01-canonical-type-place-cfg-artifact-design.md) | canonical artifact rows 与 consumer identities | reachability graph 只接受 versioned token/TypeId/edge schema |
| [04/M1、M7](../syntax/2026-07-18-04-resource-ownership-drop-gc-bridge-design.md) | Resource Drop、ownership/domain roots 与 artifact projection | required Drop/root/safepoint closure，遗漏时 fail closed |
| [05/M5](../syntax/05-property-unified-ast/m5-property-consumers-reflection-migration.md) | canonical accessor identity 与 reflection visibility | getter/setter/ref-get required roots 与 metadata remap |
| [06B/M5](../syntax/2026-07-18-06-percent-migration-lsp-fixtures-design.md) | legacy cleanup gate | 删除旧 root/edge/string fallback，production allowlist 闭合 |
| [07B](../syntax/2026-07-19-07-comprehensive-syntax-reference-fixture-design.md) | current reference manifest | trim 前后四执行路径行为、异常和 profile 一致 |
| [08/M3-M5](../syntax/2026-07-19-08-reflection-library-type-system-design.md) | reflection graph、preserve policy 与 query visibility | constructor/invoker/member roots，trimmed token 稳定不可见 |
| [09/M4](../syntax/2026-07-19-09-generational-pool-handle-ref-struct-design.md) | pool provider/layout/reflection contracts | pool descriptor、thunk 与 required TypeLayout roots |
| [10R/M1-M2、10C/M4-M5](../syntax/2026-07-19-10-native-ffi-module-package-design.md) | ModuleIdentity、exports/provider roots 与 convergence | package/native export closure、provider drift 与 missing root 拒绝 |
| [11/M2-M5](../syntax/2026-07-20-11-compile-time-attribute-decorator-typed-generation-design.md) | phase-isolated comptime code、metadata roles 与 generated declarations | host-only roots 排除、generated runtime roots 和 conditional edge |
| [12/M3、M6](../syntax/2026-07-20-12-async-task-job-scheduler-design.md) | Job/Scheduler required roles 与 task/frame artifact | scheduler/task/frame/debug roots、provider and state closure |
| [13/M4](../syntax/2026-07-20-13-iterator-enumerator-yield-design.md) | iterator frame/resume artifact | resume/dispose thunk、frame TypeLayout 与 debug roots |
| [14/M1、M4](../syntax/2026-07-20-14-test-function-harness-design.md) | TestManifest 与 test-only debug/migration contract | production artifact 删除 test code/manifest；test artifact 保留精确 roots |

逐节点 root policy 和 evidence 状态见[完整追踪矩阵](./syntax-contract-traceability.md)。

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

[2026-07-30 frame TypeLayout closure verifier](./07-codegen/2026-07-30-frame-type-layout-closure-verifier.md) 完成
frame/type-layout reachability 输入在裁剪前的 canonical closure gate：不可达 owner 的 unresolved、invalid、
identity/kind/shape 漂移不再被静默删除；沿用既有 manifest，S1/S3/S6 与 AOT 12 仍为部分完成。

[2026-07-30 complete-frame parameter identity verifier](./07-codegen/2026-07-30-complete-frame-parameter-identity-verifier.md)
完成 retained frame manifest 输入的裁剪前 parameter marker gate：不可达完整表的缺失或等数错位 marker
均 fail closed；zero/sparse frame 优化保持合法，S1/S3/S6 与 AOT 12 仍为部分完成。

[2026-08-01 constructor bitmap layout verifier](./07-codegen/2026-08-01-constructor-bitmap-layout-verifier.md)
完成 constructor bitmap flag 所属 frame ABI 的裁剪前结构校验：不可达 owner 的非 constructor、非 slot 0
parameter、TypeLayout 漂移、tail 对齐/容量与 direct/indirect/borrowed storage overlap 均 fail closed；沿用既有
frame manifest，S1/S3/S6 与 AOT 12 仍为部分完成。

[2026-08-01 receiver role frame verifier](./07-codegen/2026-08-01-receiver-role-frame-verifier.md)
完成 canonical receiver role 的裁剪前 owner gate：不可达 owner 的未知/重复 role、缺失 identity、非 slot 0、
zero-parameter 或 materialized non-parameter receiver 均 fail closed；沿用既有 frame manifest，S1/S3/S6 与
AOT 12 仍为部分完成。

[2026-08-01 parameter binding identity verifier](./07-codegen/2026-08-01-parameter-binding-identity-verifier.md)
完成 parameter binding prefix 的裁剪前 owner gate：不可达 owner 的 partial/mixed identity、越界/重复 slot、
重复 SymbolId/PlaceId、缺失 parameter row 与越过 prefix 的 receiver 均 fail closed；沿用既有 frame manifest，
S1/S3/S6 与 AOT 12 仍为部分完成。

[2026-08-01 ExecIR parameter layout projection](./07-codegen/2026-08-01-execir-parameter-layout-projection.md)
完成 parameter metadata shape 的裁剪前 owner gate，并把已验证 parameter prefix 投影为内部 ExecIR slot 表；
不可达 owner 的 nonzero/null metadata table 或 metadata count overflow 均 fail closed，沿用既有 frame manifest，
不新增 reachability node，S1/S3/S6 与 AOT 12 仍为部分完成。

[2026-08-01 value-SemIR parameter layout consumption](./07-codegen/2026-08-01-value-semir-parameter-layout-consumption.md)
完成 retained callee ExecIR flat index 到 parameter sidecar 的 value-SemIR 消费；code stripping 后 shared-method
选择只接受 exact-count、无 receiver role 且已知引用 TypeRef 的保留表，malformed/unknown sidecar 不会启用共享
特化。不新增 reachability node 或 manifest schema，S1/S3/S6 与 AOT 12 仍为部分完成。

[2026-08-01 receiver-aware typed-call layout consumption](./07-codegen/2026-08-01-receiver-aware-typed-call-layout-consumption.md)
让裁剪后 retained callee sidecar 的 index-0 canonical receiver 进入 shared inline-struct `CALL_TYPED` 选择；
receiver 与显式参数共用原始 argument window，unknown/组合/错位 role 和 unknown TypeRef 保持 fail closed。
不新增 reachability node 或 manifest schema，S1/S3/S6 与 AOT 12 仍为部分完成。

[2026-08-01 parameter default-declaration projection](./07-codegen/2026-08-01-parameter-default-declaration-projection.md)
在全函数裁剪前校验 canonical default metadata bool，并把 retained callee 的可靠正声明投影到内部 parameter
sidecar；不可达 malformed owner 同样 fail closed。裁剪沿用既有 frame owner，不新增 reachability node、
manifest 或 artifact schema；S1/S3/S6 与 AOT 12 仍为部分完成。

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
