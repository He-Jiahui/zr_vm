# AOT 11：Metadata、ModuleIdentity 与 Native ABI

## 统一 artifact

```text
ArtifactHeader(schemaVersion, target, featureFlags)
CanonicalTypeTable
TypeLayoutTable + hashes
Symbol/Callable/Property/Constructor contracts
Semantic/ExecIR sections
ModuleDependencyTable(ModuleIdentity, contractHash)
NativeImportTable(FfiSignature)
Reflection/PreserveRoots
DebugMap/SourceChecksums
```

每个section独立versioned，可选section由feature flag声明；reader对unknown required feature失败，不静默忽略。

## Module contract

import literal先按[syntax 10](../syntax/2026-07-19-10-native-ffi-module-package-design.md)解析。artifact只保存Canonical ModuleId、package identity/version、provider/artifact identity与public contract hash，不保存`#alias`、`@math.matrix`等本地spelling。

`.zrm`是assembly container，manifest入口为`META-INF/zrm.json`，模块payload为`.zro`。显式`.zrm` import解析default/declared entry；容器内部依赖仍使用ModuleIdentity。

## Native contract

`native extern`在binding期产生Canonical CallableContract + FfiSignature。artifact保存library locator、entry、ABI、parameter direction、marshaller、layout hashes与capability。VM/libffi与AOT import thunk共享表，不在调用时重解析字符串对象。

## Compatibility

- public callable/type/property/layout/module hash变化必须产生依赖失配诊断。
- debug-only metadata变化不应使release public contract失效。
- schema migration必须有reader fixture，writer永远只输出当前schema。
- source、binary和`.zrm` provider对同一公开模块产生相同public contract hash。

## Schema实施与迁移

1. **A11.1 section registry**：为每个section登记id/version/required flag/alignment/checksum/size limit；unknown required section失败，optional可跳过。
2. **A11.2 canonical contracts**：写入TypeNode/TypeUse、Layout、Callable、Property、Constructor、Drop、ModuleIdentity和FfiSignature，禁止pretty-name identity。
3. **A11.3 provider binding**：source、`.zro`、`.zrm`、package、native provider统一生成dependency row和public contract hash，循环/版本/exports在load前校验。
4. **A11.4 runtime views**：mmap/owned buffer view有明确lifetime、bounds、endianness与target；反射/debug consumer不保存悬空row pointer。
5. **A11.5 migration**：旧reader只读并产生versioned conversion report；current writer永远输出新schema；golden覆盖upgrade、downgrade拒绝和corruption。

现有证据从`tests/module/test_metadata_runtime_query.c`、`tests/module/test_metadata_runtime_binding_compatibility.c`、`tests/module/test_metadata_runtime_manifest_exports.c`与AOT 11 acceptance系列延续。新测试必须覆盖Canonical TypeNode、`#alias/@package`不进入identity、`.zrm` entry、FfiSignature ABI mismatch、DebugMap/source checksum和oversized/malformed section。

退出条件：所有consumer只读schema API；无私有字符串sidecar补语义；source/binary/assembly public hash一致；跨target/layout/schema不兼容在执行前失败；writer-reader-writer canonical bytes稳定（排除明确非确定字段）。

## 完成记录

- [Metadata runtime baseline](./11-metadata/2026-07-19-metadata-runtime-baseline.md)
- [MethodSpec reflection consumer](./11-metadata/2026-07-19-constructed-generic-method-object.md)
- [MethodSpec make consumer](./11-metadata/2026-07-19-make-generic-method-object.md)
- [Bounded argument object consumer](./11-metadata/2026-07-19-generic-method-argument-object-decoding.md)
- [Runtime-bound native callable](./11-metadata/2026-07-19-generic-method-native-entry.md)
- [Runtime-bound reflection service](./11-metadata/2026-07-19-runtime-bound-reflection-module.md)
- [Target-owned service identity](./11-metadata/2026-07-19-target-owned-reflection-module-cache.md)
- [Native import contract reachability](./12-stripping/2026-07-30-native-import-contract-reachability.md)

最新 native import 子切片使 AOT C 在裁剪前校验 canonical FfiSignature contract，并按稳定 owner 发布 retained
contract manifest 与计数；它未改变 `.zro/.zrp` schema。Canonical TypeNode、ModuleIdentity、FfiSignature
artifact schema/provider parity、DebugMap 与 schema migration 仍为 open。
