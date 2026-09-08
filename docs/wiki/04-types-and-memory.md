---
related_code:
  - zr_vm_common/include/zr_vm_common/zr_type_conf.h
  - zr_vm_core/include/zr_vm_core/value.h
  - zr_vm_core/include/zr_vm_core/type.h
  - zr_vm_core/include/zr_vm_core/type_layout.h
  - zr_vm_core/include/zr_vm_core/object.h
  - zr_vm_core/include/zr_vm_core/gc.h
  - zr_vm_core/include/zr_vm_core/gc_domain.h
  - zr_vm_core/include/zr_vm_core/ownership.h
  - zr_vm_core/include/zr_vm_core/bridge.h
  - zr_vm_parser/include/zr_vm_parser/canonical_type.h
  - zr_vm_parser/include/zr_vm_parser/place.h
  - zr_vm_parser/src/zr_vm_parser/canonical_type.c
  - zr_vm_core/src/zr_vm_core/type_layout.c
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/ownership.c
implementation_files:
  - zr_vm_core/src/zr_vm_core/value.c
  - zr_vm_core/src/zr_vm_core/object/object.c
  - zr_vm_core/src/zr_vm_core/type_layout.c
  - zr_vm_core/src/zr_vm_core/gc/gc.c
  - zr_vm_core/src/zr_vm_core/ownership.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type.c
tests:
  - tests/parser/test_canonical_type_graph.c
  - tests/parser/test_ref_struct_restrictions.c
  - tests/parser/test_resource_unique_drop.c
  - tests/parser/test_resource_shared_weak.c
  - tests/parser/test_reference_loan_nll.c
  - tests/core/test_gc_concurrent_major.c
  - tests/core/test_type_layout_inline_copy.c
  - tests/acceptance/2026-08-03-syntax-08-m1-reflection-provider-contract.md
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/plans/syntax/2026-07-18-01-canonical-type-place-cfg-artifact-design.md
  - docs/plans/syntax/2026-07-18-02-reference-syntax-borrow-checker-design.md
  - docs/plans/syntax/2026-07-18-04-resource-ownership-drop-gc-bridge-design.md
doc_type: module-detail
---

# 类型、布局与内存模型

**状态：`current`；跨 domain、AOT aggregate 和部分 pooling 能力标为 `experimental`。**

ZR 的类型系统同时描述“值是什么”和“值如何存活/传递”。编译器先把源码 TypeRef 归一化为 Canonical TypeId，再结合 Place、借用和 CFG facts 生成 TypeLayout/调用 ABI。运行时 `SZrTypeValue` 负责通用值承载；inline struct/AOT frame 则使用 layout metadata 直接访问字节区域。

## 类型类别

| 源码类型 | 主要存储 | 生命周期 | 典型用途 |
|---|---|---|---|
| primitive (`bool`, `int`, `i32`, `float` 等) | 栈槽/寄存器/inline bytes | 值语义 | 算术和小型参数 |
| `struct` | inline layout | 按值 copy/drop | 小型记录、数学值 |
| `readonly struct` | inline layout | 字段不可替换 | 不变配置/尺寸 |
| `ref struct` | stack/ref-like | lexical scope | Span、临时视图、guard |
| `class` | GC heap object | tracing GC | 对象图、共享状态 |
| `resource class` | ownership world | Drop/Close contract | 文件、socket、native handle |
| `interface` | contract/type identity | 由实现类型决定 | 多态约束 |
| `enum`/`union` | tag + payload/value | 值语义 | 状态和错误结果 |
| `fn(...) -> R` | closure/native/AOT witness | managed callable | 回调和高阶函数 |

`let` 只限制绑定更新，不会使引用对象深度 immutable；`readonly`、`ref readonly` 和 property accessor 才表达读权限。

## Canonical Type graph

`zr_vm_parser/canonical_type.h` 提供 interning API：

```text
primitive -> nominal/type definition -> generic instance
array/tuple/union/nullable/ref/owner/readonly -> function signature
```

同一语义类型在一个 semantic context 中得到稳定 `TZrTypeId`。函数类型的结构哈希包含参数数量、varargs、返回类型、参数 TypeRef 和 passing mode；不能用显示字符串替代。跨 module 的 artifact 额外携带 metadata token、signature hash、module signature hash、layout version/hash。

## 传参与 Place

Place 是可寻址存储位置的语义身份，可能是 local、参数、字段、数组元素、property ref 或 closure place。常见 contract：

```zr
fn byValue(value: Data): void { }
fn readOnly(value: in Data): void { }
fn mutate(value: ref Data): void { }
fn observe(value: ref readonly Data): void { }
fn initialize(value: out Data): void { value = init Data(); }
```

- value：复制/移动值，不能保留调用者 Place 的别名。
- `in`：只读借用；临时 rvalue 可以物化为调用作用域的 Place。
- `ref`：可读写借用；实参必须是可寻址 Place。
- `ref readonly`：只读借用；不能通过该引用写入。
- `scoped ref`：引用不能逃出声明的 region。
- `out`：调用前允许未初始化，callee 必须在所有正常返回边上写入。

借用冲突、move 后使用、跨 closure/await/yield 逃逸和 definite assignment 在 compiler/dataflow 阶段拒绝；运行时只保留无法静态证明的 bounds/null/type checks。

## TypeLayout 与 inline 值

TypeLayout 为每个可布局类型记录：size、alignment、字段 byte offset/size、nested layout id、primitive/POD 标志、GC scan kind、drop/ownership flags 和 layout hash。函数 frame layout 进一步记录每个 stack slot 的 byte offset、slot kind、parameter role、alias/borrowed alias 和 constructor initialization bitmap。

inline struct receiver 调用必须携带 receiver frame base/source provenance；getter/setter 写回时按 layout 复制字段，而不是把 struct 当作普通 heap object。AOT C/LLVM 使用同一 layout id，遇到未知或不兼容 layout 进入显式 runtime/deopt 错误。

## GC world

普通 `class`、字符串、数组、闭包、模块和运行时 descriptor 进入 GC heap。`SZrGlobalState` 持有 collector，`SZrState` 是 mutator/thread state，并记录当前 `SZrGcDomain`。collector 支持增量 step、full collection、write barrier、remembered object、native call pin 和 AOT root frame。

宿主/模块写入 managed field 后必须调用 write barrier（高层对象 helper 会替调用者执行）；native callback 暂存 `SZrTypeValue` 或对象指针时使用 `ZrCore_Gc_NativeCallPinObject` 或 `ZrCore_Gc_NativeCallPinValue`，也可使用 `SZrGcRootHandle`。AOT 生成 frame 在执行期间 push/pop `SZrAotGcRootFrame`，root map 描述每个可追踪槽。

GC domain 提供 mutator enter/leave/poll、native enter/leave、stop-the-world 和 root handle API。跨 domain 传递不能直接共享对象地址；应使用规定的 bridge/transport，并检查 domain identity 与 generation。

## Ownership world

```zr
resource class FileHandle { pub fn write(data: in Bytes): void; }

let unique: Unique<FileHandle> = own FileHandle();
let shared: Shared<FileHandle> = share(unique);
let weak: Weak<FileHandle> = degrade(shared);
let live = wake(weak);       // inferred type: Shared<FileHandle> or null
if (live != null) {
    use(live);
}
drop(shared);
```

| 操作 | 规则 |
|---|---|
| `own T(...)` | 创建唯一 owner；move 后原 Place 不可读/借用/drop。 |
| `share(unique)` | 把 owner 转为同 domain 的共享 owner；具体原子性由 provider contract 决定。 |
| `degrade(shared)` | 创建不延长寿命的 weak handle；只接受 `Shared<T>`。 |
| `wake(weak)` | 有 shared owner 时返回非空 shared，否则返回 null；只接受 `Weak<T>`。 |
| `intoGc(owner)` | 通过显式 bridge 转入 GC world；需要类型/释放 contract。 |
| `drop(owner)` | 立即消费 owner；对象成员、索引和 closure place 也按 Place 规则检查。 |

`resource class` 的 Drop 不等于 GC finalization，也不等于 `using` 的 Close/Dispose。异常、return、break、continue 都必须走 cleanup CFG；如果 cleanup 自身失败，运行时保留原 pending control 和新异常状态供上层报告。

当前源码没有把 `T?` 作为普通 TypeRef 后缀语法；上例的“或 null”是 `wake` 的推断结果
和运行时 guard 语义。需要显式标注可空性的工具/API 应使用 canonical nullable TypeId。

## ref struct 与连续视图

`ref struct`、`Span<T>`、`PoolRef<T>` 和 lock guard 是 ref-like：不能存入 GC heap、普通数组、closure 或 suspension frame，也不能跨 `await`/`yield`。连续视图通常携带 address、byte size、alignment、layout id、owner/pin provenance 和 bounds；native FFI 只有在存在明确 pin/copy/marshaller 时才能接收它。

## nullability 与对象访问

nullable/weak 目标的普通 `.` 必须运行时验证非 null，失败抛 `NullReferenceError`；`?.` 在目标缺失时返回 null（void accessor 则 no-op），并跳过整个后续 suffix 与实参求值。这个规则也适用于 property getter/setter 和 callable optional-call。

## C 侧值与内存注意事项

`SZrTypeValue` 是 runtime-owned tagged value，不能按普通 C struct 长期 memcpy 后脱离 state 使用。创建字符串/对象/数组应走 core/library helper；写入对象字段、数组元素和 closure slot 时遵守 barrier。任何返回的 `SZrObject*`、`SZrString*`、`SZrTypeValue*` 的有效期都受 GC、call frame 和 root/pin 规则约束，详见 [C API 总览](05-interop/c-api.md)。
