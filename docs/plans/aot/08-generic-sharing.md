# AOT 08：泛型实例化与共享

## Identity

Generic definition、closed instantiation与method instantiation都使用Canonical token/TypeId vector：

```text
GenericInstanceKey = DefinitionToken + CanonicalTypeId[] + target-relevant layout classes
```

禁止用pretty-printed type name、source alias或runtime pointer作持久identity。

## 策略

- exact specialization用于layout/ABI/operation不同的value type实例。
- representation-compatible reference实例可以共享code body，通过dictionary提供type handle、layout、method slot、GC/drop与constraint witness。
- generic method与generic type上下文分开保存并组合解析。
- constrained call在binding期形成witness contract；AOT不按名字再次搜索member。
- open generic只进入metadata/reflection，不进入machine code。
- shared body的exception、GC stack map与debug generic context必须仍可还原精确实例。

## Artifact

`.zro` 保存definition token、argument TypeIds、sharing key、dictionary schema、required specializations与layout hashes。loader必须检测版本或hash不兼容，不能默默使用错误共享body。

## 里程碑

1. canonical generic identity/roundtrip。
2. reference sharing + dictionary。
3. value specialization与ABI。
4. constrained/interface/virtual call。
5. generic reflection/method invoke。
6. cross-module dedup与stripping roots。

## 阶段验收

| 阶段 | 关键输入/输出 | 必测失败 |
|---|---|---|
| G1 identity | definition token + Canonical TypeId vector -> instance key | alias/name相同但TypeId不同、open generic |
| G2 sharing | representation class + dictionary schema -> shared body | layout/ABI不兼容却误共享 |
| G3 specialization | closed value layout -> exact body/thunk | missing instance与deopt策略 |
| G4 constraints | witness contract -> constrained call | missing/ambiguous witness、accessibility |
| G5 reflection/invoke | MethodSpec/TypeSpec + runtime generation -> object/invoker | stale runtime、arity/order/recursive shape mismatch |
| G6 cross-module/trim | ModuleIdentity + roots -> dedup/closure | provider drift、token remap、trimmed required instance |

证据入口包括`tests/module/test_reflection_dynamic_generic_instance.c`、`tests/module/test_reflection_dynamic_generic_method_context.h`以及`tests/acceptance/2026-06-24-aot-08-s1-generic-instantiation-table.md`至S7系列。每个sub-milestone必须明确是既有metadata consumer、interpreter deopt还是full-AOT closure，不能互相代替。

最终退出：reference sharing与value specialization均通过四backend；dictionary/stack map/debug context可还原精确实例；跨模块identity不依赖pretty name；reflection preserve与stripping graph闭合。

## Syntax 上游追踪

| Syntax 节点 | 本计划消费的稳定输入 | 本计划退出责任 |
|---|---|---|
| [01/M1、M4](../syntax/2026-07-18-01-canonical-type-place-cfg-artifact-design.md) | Canonical generic TypeId、definition token 与 artifact schema | instance/dictionary key roundtrip，不使用 pretty name/pointer |
| [02/M6](../syntax/2026-07-18-02-reference-syntax-borrow-checker-design.md) | callable contract 的跨模块 canonical projection | generic call ABI 与 dictionary callable identity |
| [03/M1](../syntax/2026-07-18-03-struct-ref-struct-span-layout-design.md) | closed TypeLayout 与 value representation | value specialization 与 reference sharing 分类 |
| [04/M1-M3](../syntax/2026-07-18-04-resource-ownership-drop-gc-bridge-design.md) | generic owner operations 与 witness contract | Drop/retain/witness dictionary entries 和 required roots |
| [05/M5](../syntax/2026-07-18-05-property-unified-ast-design.md) | generic property/accessor identity | accessor dictionary entry、MethodSpec 与 trim root |
| [08/M1-M4](../syntax/2026-07-19-08-reflection-library-type-system-design.md) | TypeSpec/MethodSpec、constraint witness 与 runtime generation | generic construct/invoke object、cache 与 preserve closure |
| [09/M4](../syntax/2026-07-19-09-generational-pool-handle-ref-struct-design.md) | generic pool layout/provider contract | closed pool layout specialization 与 provider-owned TypeId |
| [10F/M3、10C/M4](../syntax/2026-07-19-10-native-ffi-module-package-design.md) | generic native signature 与 provider convergence | FFI specialization、dictionary/native roots 与 canonical identity |
| [11/M4](../syntax/2026-07-20-11-compile-time-attribute-decorator-typed-generation-design.md) | generated generic declarations 与 canonical constraints | generated/source instance 使用同一 key/dictionary pipeline |
| [12/M1-M6](../syntax/2026-07-20-12-async-task-job-scheduler-design.md) | Task/Job generic carrier、frame 与 provider identity | exact Task<T>/Job<T> dictionary、frame map 与 trim roots |
| [13/M1-M4](../syntax/2026-07-20-13-iterator-enumerator-yield-design.md) | Iterator/AsyncIterator generic carrier 与 witness | iterator body sharing/specialization、resume roots 与 debug context |

逐节点 readiness 与 AOT 状态见[完整追踪矩阵](./syntax-contract-traceability.md)。

## 完成记录

- [Typed layout baseline](./02-type-layout/2026-06-24-typed-layout-baseline.md)
- [Generic sharing runtime baseline](./08-generics/2026-07-19-generic-sharing-runtime-baseline.md)
- [Constructed generic method object](./08-generics/2026-07-19-constructed-generic-method-object.md)
- [MakeGenericMethod object](./08-generics/2026-07-19-make-generic-method-object.md)
- [Argument object decoding](./08-generics/2026-07-19-generic-method-argument-object-decoding.md)
- [Native entry](./08-generics/2026-07-19-generic-method-native-entry.md)
- [Runtime-bound reflection module](./08-generics/2026-07-19-runtime-bound-reflection-module.md)
- [Target-owned reflection module cache](./08-generics/2026-07-19-target-owned-reflection-module-cache.md)
- [Canonical generic dictionary reachability](./12-stripping/2026-07-30-canonical-generic-dictionary-reachability.md)

这些记录证明本地attached runtime的部分generic reflection链路；跨模块binding、Invoke、Canonical TypeRef闭环和full-AOT closure仍为open。
