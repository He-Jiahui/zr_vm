---
related_code:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/canonical_type.h
  - zr_vm_parser/include/zr_vm_parser/generic_instantiation.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_types.c
  - zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_core.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_struct.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/canonical_type.c
  - zr_vm_parser/src/zr_vm_parser/canonical_type_definition.c
  - zr_vm_parser/src/zr_vm_parser/generic_instantiation.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_typed_export_generics.c
plan_sources:
  - user: 2026-09-10 继续细化 Wiki 的泛型、协议与 C API 说明
  - docs/plans/syntax/2026-07-19-08-reflection-library-type-system-design.md
tests:
  - tests/parser/test_generic_constraints.c
  - tests/parser/test_generic_instantiation.c
  - tests/parser/test_aot_c_generic_monomorphization.c
  - tests/module/test_reflection_dynamic_generic_instance.c
doc_type: language-reference
---

# 泛型参数、约束与实例化参考

本页把 ZR 的泛型从源码拼写一直追踪到 canonical `TypeId`、C 侧实例表和 AOT 选择。它补充
[泛型、接口与协议](generics-protocols.md)的概念说明，并以当前 parser 的实际入口
`parse_generic_declaration` / `parse_optional_where_clauses` 为准。一个重要结论是：当前源码
**不**把 `T: Protocol` 写在 `<...>` 内；协议和构造约束位于名称后的 `where T: ...` 子句。

## 阅读模型

```text
<T, const N: int> + where constraints
  -> SZrGenericDeclaration / SZrParameter
  -> typed generic export + inferred binding
  -> canonical generic parameter or closed generic instance TypeId
  -> generic-instantiation table (C instance id / sharing kind)
  -> interpreter metadata or AOT C/LLVM lowering
```

这里的每一层解决不同问题：parser 只保存形状；type inference 验证调用实参和约束；canonical
type interning 产生稳定的结构 identity；实例表则给 C 后端一个可重复使用的 emitted-instance
identity。不要把源码中显示的 `Box<int>` 字符串当成跨层唯一键。

## 语法规则

### 泛型声明

```ebnf
generic-declaration = "<", generic-parameter,
                      {",", generic-parameter}, [","], ">" ;
generic-parameter   = [variance], ["const"], identifier,
                      [":", TypeRef] ;
variance            = "in" | "out" ;
```

`generic-declaration` 至少有一个参数，允许结尾逗号。只有 `const` 形参需要紧随 `: TypeRef`；
当前 canonical 层把它投影为 `ZR_CANONICAL_GENERIC_ARGUMENT_CONST_INT`，公开示例和 semantic
display 均使用 `const N: int`。普通 type 参数没有 `<T: Constraint>` 这种 production。

```zr
struct Window<const Width: int> {
    var pixels: byte[Width];
}

interface Producer<out T> {
    fn current(): T;
}
```

`in` / `out` 由 lexer 作为关键字进入 generic parameter；parser 仅在 interface 的 generic
参数位置允许它。其他 declaration 中写方差会得到 `Variance is only allowed on interface generic
parameters` 诊断。方差的最终可赋值性/成员位置约束仍属于 semantic 阶段，不能因为 AST 有一个
`variance` 字段就假定所有 backend 都已实现同一种协变或逆变规则。

### `where` 约束

```ebnf
where-clauses = {"where", identifier, ":", constraint,
                  {",", constraint}} ;
constraint    = "class" | "struct" | "new", "(", ")"
              | "owner" | "unique" | "shared" | "weak" | TypeRef ;
```

一个 declaration 可以连续写多个 `where` 子句，每个子句绑定一个已经声明的泛型形参。子句中
用逗号分隔的是同一形参的约束，而不是第二个参数：

```zr
class NeedNew<T> where T: new() {
    var value: T;
}

let iteration = import("zr.iteration");

class NeedsIterable<T>
where T: iteration.Iterable<int> {
    var value: T;
}

class OwnedCache<T>
where T: owner, shared {
    var value: T;
}
```

最后一个例子展示 parser 记录的 ownership 约束形状，不应把它理解为自动转换为 shared owner；
调用实参本身必须具有与 contract 相符的所有权资格。`unique`、`shared` 和 `weak` 同时只允许
一个，parser 会对冲突组合报告 `Conflicting ownership constraints in where clause`。未找到的
参数名也会报告 `Unknown generic parameter in where clause`。

| 约束 | AST 字段 | type inference 要验证的事实 |
| --- | --- | --- |
| `class` | `genericRequiresClass` | 实参是 class 形状，而不是任意值类型或 interface。 |
| `struct` | `genericRequiresStruct` | 实参具有 struct/value 形状。 |
| `new()` | `genericRequiresNew` | 可见的零参数构造路径存在。 |
| `owner` | `genericRequiresOwner` | 实参是 owner-qualified 形状。 |
| `unique` / `shared` / `weak` | `genericRequiredOwnershipQualifier` | owner qualifier 与声明精确一致。 |
| TypeRef | `genericTypeConstraints` 数组 | 实参可满足对应 interface/protocol/名义类型契约。 |

最小的、由 `tests/parser/test_generic_constraints.c` 覆盖的构造约束形式为：

```zr
interface AbstractThing { }
class DefaultCtor { }
class NeedNew<T> where T: new() { var value: T; }

new NeedNew<DefaultCtor>();    // 可构造
new NeedNew<AbstractThing>();  // semantic constraint failure
```

不要用 `T: Send + Sync` 或 `<T: Send>` 替代上述语法。若某个 provider 要求多个 protocol，
其声明应把它们作为同一 `where T:` 的逗号分隔 TypeRef 约束，并以该 provider 当前 descriptor
和测试为准。

## 声明位置与 AST 投影

generic declaration 可附着在 function、function type、class、struct、union、interface，以及
它们的可泛型成员上。所有这些位置最终使用同一 `SZrGenericDeclaration`，其 `params` 数组的
元素是 `ZR_AST_PARAMETER`：

| 源码概念 | `SZrParameter` 中的保存位置 | 后续消费者 |
| --- | --- | --- |
| 名称 | `name`、`nameLocation` | scope/binding、诊断、metadata display |
| type/const kind | `genericKind` | canonical argument-kind 验证 |
| 方差 | `variance` | interface contract / typed export |
| 普通 TypeRef 约束 | `genericTypeConstraints` | call inference、native/typed metadata |
| class/struct/new/owner 约束 | 对应 `genericRequires*` 标记 | constraint resolver |
| 精确 owner 约束 | `genericRequiredOwnershipQualifier` | ownership contract 检查 |

AST 存在不表示类型可实例化。例如 open generic、interface、抽象 class、ref-like 类型和缺少
布局的动态参数，仍会在 constructor resolution、reflection 或 backend 注册点被拒绝。

## 绑定、推断与约束失败

generic 调用解析将显式实参和由实参/期望类型推得的 binding 放入同一个候选解析流程。解析器
不会因为推断信息不足而把 `T` 偷换为 `any`：候选必须在 arity、形参方向、TypeRef 约束、
class/struct/new、owner 和 ownership qualifier 上全部成立。否则调用候选产生 conflict 或
ambiguous diagnostic，而不是选择一个运行时随机分派。

```zr
fn same<T>(left: in T, right: in T): bool
where T: IEquatable<T> {
    return left.equals(right);
}

let equal = same(1, 1);
let explicitlyEqual = same<int>(1, 1);
```

这里 `<int>` 是闭合 type argument；`same` 的约束仍在函数头之后。对 generic function，
`SZrFunctionTypedGenericParameter` 会把名称、kind、variance、四类 boolean 约束、owner
qualifier 和 `constraintTypeNames` 投影到 typed metadata。工具、artifact writer 和 native
provider 不应只复制显示名称，否则相同名称但不同约束的 callable 会被误合并。

常见失败应按其阶段处理：

| 现象 | 负责层 | 合理修复 |
| --- | --- | --- |
| 方差出现在 class/function | parser | 把 `in`/`out` 限于 interface 参数，或删除方差。 |
| `where U: ...` 但 U 未声明 | parser | 改为已声明形参或补充 `<U>`。 |
| `new NeedNew<AbstractThing>()` | inference | 提供满足无参构造约束的闭合实参。 |
| owner qualifier 不符 | inference/ownership | 显式使用匹配 owner contract，不能靠 cast 掩盖。 |
| generic arity 或 kind 不符 | canonical definition | 保持 type/const 参数数量和顺序一致。 |
| AOT 找不到封闭实例 | backend/runtime | 保留实例 metadata，或允许当前配置走 interpreter deopt。 |

## Canonical `TypeId`：开放参数与闭合实例

canonical type 不是字符串池。`canonical_type.h` 区分 `GENERIC_PARAMETER` 与
`GENERIC_INSTANCE`，并把 generic argument 的 kind 与 payload 一起参与 structural hash：

```text
generic parameter: (ownerSymbolId, ordinal)
generic instance:  (definitionTypeId, [typeId | constIntValue | constParameter...])
nominal definition:(moduleIdentity, name, definitionToken)
```

因此下面三者在正确的 semantic context 中必定有不同的 identity：

```text
first module.Box<int>
first module.Box<float>
second module.Box<int>
```

同时，`ref T`、owner projection、nullable、数组 rank、function 参数的 passing form 和 effect
也会通过各自 canonical node 进入 contract。闭合 `Box<T>` 的图不能因为看到同一个 base name
而跨 module、跨 ownership 或跨 generic argument kind 共享。

| C API | 用途 | 关键调用者责任 |
| --- | --- | --- |
| `ZrParser_CanonicalType_InternGenericParameter` | 建立开放参数 identity | 使用声明 owner 的 symbol id 和稳定 ordinal。 |
| `ZrParser_CanonicalType_InternGenericInstanceEx` | 建立带 type/const 参数的闭合实例 | 传入完整 `SZrCanonicalGenericArgument` 序列。 |
| `ZrParser_CanonicalType_FromInferredWithGenericBindings` | 从推断类型替换开放参数 | binding 数组必须与调用上下文一致。 |
| `ZrParser_CanonicalType_RegisterGenericDefinitionEx` | 发布定义的参数 kind 列表 | type 与 const-int 顺序是 ABI/metadata contract 的一部分。 |
| `ZrParser_CanonicalType_FromFunctionSignatureWithGenericBindings` | 生成已 rebind 的 callable type | 不能丢掉 `in/ref/out` 和 effect 标记。 |

下面的片段说明 `InternGenericInstanceEx` 需要的参数形状；它不是独立可运行的宿主初始化代码，
因为 `SZrSemanticContext` 必须由 parser/compiler 生命周期拥有：

```c
SZrCanonicalGenericArgument arguments[2];
TZrTypeId closedType;

arguments[0].kind = ZR_CANONICAL_GENERIC_ARGUMENT_TYPE;
arguments[0].data.typeId = elementTypeId;
arguments[1].kind = ZR_CANONICAL_GENERIC_ARGUMENT_CONST_INT;
arguments[1].data.constIntValue = 16;

closedType = ZrParser_CanonicalType_InternGenericInstanceEx(
        semanticContext, definitionTypeId, arguments, 2u);
```

调用失败或返回 invalid id 时，工具必须把它当成 semantic-context/argument contract 失败，不能
构造一个手写字符串继续传给 artifact 或 native registry。

## C 后端实例表与共享策略

`SZrGenericInstantiationTable` 是 parser/compiler 使用的 emitted-instance 表，不是 ZR 程序
可直接改写的对象。每条 `SZrGenericInstantiationRecord` 记录：

| 字段 | 作用 |
| --- | --- |
| `baseToken` | 泛型定义或可调用的 metadata base identity。 |
| `arguments` | `SZrGenericInstantiationTypeArgument` 数组，含 inferred type 与 value/reference shape。 |
| `shareKind` | `MONOMORPHIZED_VALUE` 或 `SHARED_REFERENCE` 的后端选择结果。 |
| `cInstanceId` | C emission 可引用的稳定实例编号。 |

`ZrParser_GenericInstantiation_InferTypeShape` 和
`ZrParser_GenericInstantiation_ComputeShareKind` 根据已解析的实参决定表记录；调用者不得根据
类型名称自行猜测“引用一定共享”或“值一定单态化”。`GetOrAdd` / `GetOrAddResolved` 按
base token 和完整实参数组去重，返回的 record 仍由 table 持有；table reset/free 后不得缓存其
地址。AOT 生成代码需要共享泛型时会通过 generated generic dictionary 解析 slot，缺少实例时
应走当前 backend 所允许的 deopt/失败路径，而不是把错误实例号解释为零值。

## Protocol、反射和 native 边界

TypeRef 约束不仅影响源级 member lookup，也会影响 provider descriptor、reflection generic
metadata 和 AOT 可达性。动态反射实例使用 `SZrReflectionGenericTypeArgument`，其 kind 可为
primitive、type token、array、tuple、ownership、nullable 或 union；缓存键必须包括 generic
signature hash、metadata generation 和 type layout id。reload 后应调用
`ZrCore_Reflection_RevalidateDynamicGenericTypeInstance`，不能继续使用旧 generation 的 layout。

native module 作者需要同时保持四个层次一致：

1. 源/descriptor 声明的参数次序、type/const kind、variance 和 `where` 约束；
2. canonical TypeId 与 layout registry 中的 closed instance；
3. descriptor-derived call binding 的 contract hash 和 passing mode；
4. artifact/AOT registration 的 metadata token、generic signature 与 layout id。

仅在 C callback 里按字符串比较 `"Array<int>"`，会绕过上述验证并在 reload、跨 module 或
owner-qualified 参数下得到错误的实例。C callback 的值、root 和 write-back 生命周期见
[Native Call Context](../05-interop/native-call-context-reference.md)，动态反射调用见
[反射与稳定槽池 API](../03-modules/reflection-pooling-api.md)。

## 作者检查表

- 所有普通约束都写在 `where T: ...`，不使用 `<T: ...>` 或 `+` 组合语法。
- `new()` 约束只承诺零参数可构造性，不替代构造函数参数检查。
- 把 `in/ref/out`、owner qualifier 和 generic argument kind 视为 callable contract 的组成部分。
- 生成或缓存 closed type 时保存 module identity、definition token、argument 序列、signature hash
  和 metadata generation，而不只保存格式化名称。
- 在 C/AOT 路径中使用 canonical/generic-instantiation API 产生 identity；不要手写 C instance id。
- 为每一种约束失败、开放实例、动态实例 reload 和 AOT 缺实例路径提供测试。
