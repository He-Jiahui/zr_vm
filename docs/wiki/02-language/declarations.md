---
related_code:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_declarations.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_struct.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_class.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_interface.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_extern.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_union.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_property.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_types.c
  - tests/fixtures/projects/syntax_reference_v1/src/model.zr
  - tests/fixtures/projects/syntax_reference_v1/src/object_model.zr
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/parser/parser_declarations.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_struct.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_class.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_interface.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_extern.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_union.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_property.c
  - zr_vm_parser/src/zr_vm_parser/parser/parser_types.c
tests:
  - tests/parser/test_parser.c
  - tests/parser/test_syntax_reference_v1.c
  - tests/fixtures/projects/syntax_reference_v1/src/model.zr
  - tests/fixtures/projects/syntax_reference_v1/src/object_model.zr
  - tests/fixtures/projects/syntax_reference_v1/src/compile_time_and_attributes.zr
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/zr_language_specification.md
doc_type: language-reference
---

# 声明与类型构造

声明决定名称、可见性、类型信息和后续语义阶段的 symbol shape。parser 会把声明保存在 AST 中，但不会在这里完成 overload、布局、泛型实例化或 ownership 检查。所有声明都遵循“先读修饰符，再读主体”的顺序；修饰符位置写错通常会触发结构化诊断，而不是被静默忽略。

## 顶层与 module 声明

```ebnf
source = [ "module", module-path, ";" ], { top-level-item } ;
module-path = identifier, { ".", identifier } ;
```

```zr
module app.accounts;

pub let version: string = "1";
pri fn helper(): void { return; }
```

`module` 名称由点分段组成，不能写字符串、括号或旧 `%module` 前缀。模块声明可省略；project resolver 在有 manifest/source URI 时推导 identity，但独立 parser 只保留“未声明”状态。顶层 `let`/`var`/`const`、函数、类型和 `native extern` 都可成为后续 module graph 的节点。

## 访问修饰符与通用修饰符

| 修饰符 | 适用位置 | 解析阶段的作用 |
| --- | --- | --- |
| `pub` | 顶层、类型成员、property/accessor | 导出到 module/public symbol surface。 |
| `pri` | 顶层、类型成员、property/accessor | 仅当前 owner/type 可见。 |
| `pro` | 顶层、类型成员、property/accessor | 由继承/受保护规则决定可见性。 |
| `static` | class/struct/interface 成员、property | 不绑定实例 receiver。 |
| `abstract`, `virtual`, `override`, `final`, `shadow` | class 成员或 class 声明允许的组合 | 记录 dispatch/继承约束；冲突由语义阶段拒绝。 |
| `readonly` | struct 声明、类型/引用上下文 | 建立只读 view 或隐式 const receiver。 |
| `ref` | `ref struct`、参数/返回、引用表达式 | 表示 Place/reference contract，不是复制值。 |

修饰符的顺序可参考 `pub const fn`、`pub readonly struct` 和 `abstract class`。上下文拼写 `readonly`、`resource` 等仍被 lexer 当作 identifier，由相应声明 parser 消费。

## 变量和解构绑定

```ebnf
variable = [access], ("var" | "let" | "const"), binding-pattern,
           [ ":", TypeRef ], [ "=", expr ], ";" ;
binding-pattern = identifier | "[", binding-list, "]" | "{", object-bindings, "}" ;
```

```zr
let immutable: int = 1;
var mutable: int;
const compileConstant: usize = 4;
var [first, second] = [10, 20];
var {name: displayName, id} = record;
```

`let` 和 `const` 在 AST 中标记为 immutable binding；`var` 允许后续 assignment。这里的 immutable 是绑定层属性，不会递归冻结对象字段。类型注解可省略，由 semantic/type inference 填充；若写出 `:`，后面必须是完整 TypeRef。初始化值可省略，但 `let`/`const` 是否允许未初始化由后续 definite-assignment contract 决定，不能以 parser 接受作为运行时保证。

数组模式按位置绑定；对象模式按键绑定，`{field}` 是 key/value 同名简写，`{source: local}` 把 `source` 映射为 `local`。模式只在声明、`for`/`foreach` 和 `using` 的守卫位置有特殊意义；普通表达式中的 `{}` 是 object literal。

## 函数声明

```ebnf
function-declaration = [decorator], [access], ["async"], "fn", identifier,
                       [generic-parameters], "(", parameters, ")",
                       [ ":", TypeRef ], [where-clauses], block ;
parameters = [ parameter, { ",", parameter }, [ ",", "...", parameter ] ] ;
parameter = ["const"], [identifier, ":"], ["in"|"out"|"ref"|"scoped"], TypeRef,
             [ "=", expr ] ;
```

```zr
fn add(left: int, right: int): int {
    return left + right;
}

fn read(value: in int): int { return value; }
fn update(value: ref int): void { value += 1; }
fn produce(value: out int): void { value = 7; }
fn logAll(prefix: string, ...values: string): void { return; }
```

函数声明中的返回类型分隔符是 `:`。`->` 只属于 function type；`=>` 只属于匿名 expression body。旧的 `func name(...) -> T {}`、无 `fn` 的 keywordless declaration 和箭头返回声明会得到 `legacy_syntax_removed`，不会回退成普通表达式。

函数 parser 先收集 decorators/access/async，再建立 generic、parameter、return TypeRef 和 body 节点。参数的 source passing form 会被记录在 `SZrParameter`，供 canonical type、borrow/ownership flow、native descriptor 和 AOT backend 共用；因此不要通过改变参数名来模拟 `in/out/ref`。

### 泛型与 where

```zr
fn identity<T>(value: T): T { return value; }
fn make<T>(value: T): T where T: new(), owner { return value; }
```

泛型参数写在名称后 `<...>`，至少一个参数；可选 `where` 子句可以要求 `class`、`struct`、`new()`、`owner`、`unique`/`shared`/`weak` 或另一个 TypeRef 约束。parser 将这些约束附加到 generic parameter；实例化、可构造性和所有权一致性在 semantic/canonicalization 阶段验证。

## 匿名函数与 function type

function type 使用 `fn(parameters) -> ReturnType`：

```zr
let transform: fn(int) -> int = fn(value: int): int => value + 1;
fn apply(factory: fn(int) -> int): int {
    return factory(7);
}
```

匿名函数可以使用 block body 或 `=> expression` body，也可以带 `async`：

```zr
let asyncTransform = async fn(value: int): Task<int> => value + 1;
```

声明 parser 与 function-type parser 是两条路径：前者需要函数体 `{}`，后者在类型上下文中需要 `->`。调用表达式会把 lambda、方法、native function 和变量中的 callable 统一视为可调用值，但 descriptor/closure capture 仍由 compiler 生成。

## struct、class 与 resource class

```zr
ref struct View { var data: Span<int>; }

struct Point {
    var x: int;
    var y: int;
    pub @constructor(x: int, y: int) {
        this.x = x;
        this.y = y;
    }
}

class Counter {
    pri var stored: int = 0;
    pub fn increment(delta: int): int {
        this.stored = this.stored + delta;
        return this.stored;
    }
}

resource class FileHandle {
    pub fn close(): void { return; }
}
```

`struct` 是值布局声明，可加 `ref`/`readonly` 形成 view 或只读 receiver；`class` 是对象/继承/dispatch 容器；上下文声明 `resource class` 在 AST 中标记 owned/resource modifier，供 Drop/using/ownership flow 使用。字段必须用 `var`、`let` 或 `const`（以及可选访问、static、decorator）；方法需要 `fn`，meta function 以 `@name(...)` 形式出现。

构造有三个互补语法：`init Type(args)` 生成 struct/value construction，`own Type(args)` 建立 resource owner，`new Type(args)` 表示普通 object construction。它们不是同一个 token 别名，canonicalization 会据此生成不同 ownership/allocator facts。

## interface

```zr
interface Printable<T>: Renderable {
    property text: string { get; }
    fn print(value: T): void;
}
```

接口可带泛型和逗号分隔的继承 TypeRef。成员可以是 bodyless method、field、property 或 `@meta` signature；bodyless method/property accessor 必须以 `;` 结束。接口只描述 contract，不产生可执行 method body；实现类的 override/dispatch 检查在 semantic phase 完成。

## enum 与 union

```zr
enum Color: int {
    Red = 1;
    Green;
    Blue = 4;
}

union Result<T> {
    Ok(value: T);
    Error { message: string; code: int; }
    @Empty;
}
```

enum member 是名称加可选 `= expr`，成员之间可用逗号或分号。可选 base TypeRef 常见为 `int`、`string`、`float` 或 `bool`。union variant 可以是 unit、tuple fields `(name: TypeRef, ...)` 或 struct fields `{ name: TypeRef; ... }`；variant 前的 `@` 标记 default-using variant，整个 union 最多一个。switch pattern、using guard 和 exhaustiveness facts 会读取这些 variant metadata。

## property 与 accessor

```zr
class Account {
    pri var balanceStorage: int = 0;

    pub property balance: int {
        pub get { return this.balanceStorage; }
        pri set { this.balanceStorage = value; }
        init { this.balanceStorage = 0; }
    }
}
```

`property name: Type { ... }` 是一个声明，accessor 可以是 `get`、`set` 或 contextual `init`。accessor body 有三种形态：`;`（bodyless）、`=> expr;`（expression）和 `{ ... }`（block），可在 accessor 前覆盖访问级别。property 不自动创建 backing field；示例中的 `balanceStorage` 必须显式声明。

语义阶段通常为 property 建立 property symbol 和 accessor symbols，再把 getter 的值返回、setter 的写入和 `ref` return 转换为 Place/Value contract。旧的分离 `get name(): T;`/`set name(v: T);` 只在迁移 parser 中识别，production parser 会建议合并为单个 `property` 声明。

## decorator、comptime 与 test

decorator 由 `#` 包围完整 expression：

```zr
#zr.reflection.attributeUsage(
    targets: zr.reflection.AttributeTargets.type,
    retention: zr.reflection.AttributeRetention.artifact
)#
pub readonly struct Label { pub let value: int; }

#zr.compile.declarationTransform#
pub comptime fn derive(target: declaration.Struct): declaration.Patch {
    return init declaration.Patch(target: target.symbolId);
}
```

decorator expression 在 declaration 前收集到 AST metadata；它不是注释，也不是运行时普通调用。`comptime` 是 contextual declaration marker，compile-time function 必须遵守 `zr.compile` provider 的阶段和预算；`test` 关键字保留给测试/member 名称兼容路径，当前推荐用 `#zr.testing.test#` metadata 加普通函数。

## bodyless 声明与分号

native/接口方法、委托和仅声明不实现的成员必须写分号：

```zr
interface Reader { fn read(out buffer: Span<byte>): int; }
native extern("libzr_demo") {
    fn nativeValue(): i32;
}
```

缺少 `;` 时 parser 会报告 declaration/member-specific diagnostic。bodyless 声明不能用 `=>` 或省略返回 TypeRef；具体 native ABI 约束见[模块、并发与 FFI](modules-concurrency.md)和 [Library C API](../05-interop/c-api-library.md)。
