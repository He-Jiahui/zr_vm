# Debug 02：Frame、Value 与 Memory Introspection

## API对象

```text
FrameDescriptor(frameId, functionSymbol, moduleIdentity, generation, scopes[])
VariableDescriptor(symbolId, typeId, place/value location, mutability, availability)
ValueDescriptor(typeId, display, childrenHandle, flags, lifetimeGeneration)
```

variable location来自DebugMap/stack map/environment layout。debugger不能假设所有值都在boxed object或name->value字典中。

## 类型化展示

- struct按TypeLayout字段和accessibility展开；class/GC ref遵守null与GC handle。
- property默认不自动执行getter；明确请求时标记side effect/throw/allocation。
- ref显示referent type、readonly、region和validity，不暴露可写raw address。
- Unique/Shared/Weak显示owner state；moved/dropped Place不可伪装成null。
- PoolHandle显示pool/slot/generation validity；PoolRef只在guard/frame有效期内展开。
- union显示active variant和payload；generic显示精确TypeIds/dictionary context。
- ModuleNamespace显示Canonical ModuleId、package/version/provider与exports。

## 安全与生命周期

children/value handle绑定pause snapshot；resume、frame exit、module reload、pool recycle或GC generation变化后按contract失效。memory read/write是独立capability，默认只读，并校验target range、TypeLayout和readonly/ownership policy。

## 完成记录

[2026-06-20 introspection baseline](./02-introspection/2026-06-20-introspection-baseline.md) 记录已有快照/child shape能力；Canonical Type/Place及新owner/pool展示仍待接入。

## 值类别矩阵

| 类别 | 数据源 | 展示/展开规则 | 失效条件 |
|---|---|---|---|
| scalar/struct/tuple/union | TypeId + TypeLayout + value location | inline字段/active variant，不执行方法 | frame resume/optimized out |
| class/GC ref | GC handle + metadata | null/type/member，遵守visibility | collection generation/handle release |
| ref/ref readonly/ref struct/Span | Place/provenance/region + fat layout | referent与readonly/bounds，隐藏raw pointer | frame/region/guard结束 |
| Unique/Shared/Weak | owner facts + runtime carrier | available/moved/dropped/count/liveness | move/drop/upgrade generation |
| PoolHandle/PoolRef | pool/slot/generation/guard | handle validity与guarded value | recycle/guard/frame结束 |
| property | PropertyContract/accessor metadata | symbol与backing Place；默认不执行getter | module generation/trim |
| generic/reflection | exact TypeIds/tokens/dictionary | precise arguments/definition links | metadata runtime replacement |
| ModuleNamespace | ModuleIdentity/package/provider | exports与virtual source | module reload/unload |

实现阶段：先稳定Frame/Scope/Variable/ValueDescriptor schema，再接stack/register/environment location，随后接children paging、memory capability和optimized-out reason。任何write/evaluate操作独立于read API并经过readonly/owner/ref policy。

验证入口：`tests/debug/test_debug_introspection.c`、`test_debug_metadata.c`、`test_debug_variable_child_shape.c`、`test_debug_snapshot_contracts.c`。必须增加compacting GC、large paging、stale handle、trimmed field、invalid layout和resume race。

退出条件：debugger不假设boxed/name-map representation；每个value reference绑定pause generation；非法或过期访问稳定失败且不读取释放内存；四backend对同一可观察值给出等价TypeId/children。
