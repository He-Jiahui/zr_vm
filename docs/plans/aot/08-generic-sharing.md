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

## 完成记录

- [Typed layout baseline](./02-type-layout/2026-06-24-typed-layout-baseline.md)
- [Generic sharing runtime baseline](./08-generics/2026-07-19-generic-sharing-runtime-baseline.md)
- [Constructed generic method object](./08-generics/2026-07-19-constructed-generic-method-object.md)
- [MakeGenericMethod object](./08-generics/2026-07-19-make-generic-method-object.md)
- [Argument object decoding](./08-generics/2026-07-19-generic-method-argument-object-decoding.md)
- [Native entry](./08-generics/2026-07-19-generic-method-native-entry.md)
- [Runtime-bound reflection module](./08-generics/2026-07-19-runtime-bound-reflection-module.md)
- [Target-owned reflection module cache](./08-generics/2026-07-19-target-owned-reflection-module-cache.md)

这些记录证明本地attached runtime的部分generic reflection链路；跨模块binding、Invoke、Canonical TypeRef闭环和full-AOT closure仍为open。
