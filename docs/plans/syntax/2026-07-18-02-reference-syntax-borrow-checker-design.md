# 02 `fn/ref/in/out/scoped/readonly` 与 borrow checker

> 状态：细化草案，等待人工确认。
>
> 硬依赖：[Canonical TypeRef、Place IR、CFG facts 与 artifact schema](./2026-07-18-01-canonical-type-place-cfg-artifact-design.md)

## 1. 目标结果

统一函数、引用、只读能力、参数 passing contract 和借用检查，使以下语义全部由 Canonical Type、Place 和 CFG facts 承载：

- `fn` 声明与函数类型。
- `const fn` 只读 receiver 与普通 `fn` 可写 receiver。
- `ref T`、`ref readonly T` 和 `scoped ref T`。
- `in T`、`out T` 参数简写。
- 调用点 `ref`、`out`。
- ref local、ref return、reborrow。
- definite assignment、use-after-move、loan conflict、escape 和 suspension 检查。

目标不是给当前 `%in/%out/%ref` 换拼写，而是取消 passing mode、ownership borrowed/loaned 和引用类型三套语义并存。

## 2. 语法设计

### 2.1 关键字类别

建议：

- `fn`：保留关键字，所有函数声明和函数类型都需要。
- `ref`、`out`：保留关键字，同时出现在类型/参数和调用参数上下文。
- `in`、`scoped`、`readonly`：上下文关键字，只在能够组成类型或参数 contract 的位置生效。
- `const`：既用于编译期常量声明，也在 instance method 位置组成 `const fn`；由语法上下文消歧。

`const fn` 不表示 compile-time function。编译期执行仍使用 `comptime`。顶层 `const fn` 在第一版报错，因为顶层函数没有 receiver；这避免把 `const` 再扩展成 purity/effect 系统。

### 2.2 函数声明

```zr
pub fn add(left: int, right: int): int {
    return left + right;
}

class Buffer {
    const fn length(): usize {
        return this.count;
    }

    fn clear(): void {
        this.count = 0;
    }
}
```

规范规则：

- 顶层、局部、class、struct 和 interface 函数声明都以 `fn` 开始，前面可以有可见性、`static`、`async`、`const` 等受上下文限制的 modifier。native函数只允许作为`native extern("library")` block中的bodyless `fn` declaration。
- 普通 instance `fn` 的 receiver effect 为 writable。
- `const fn` 的 receiver effect 为 readonly。
- `static const fn` 非法；static 没有 receiver。
- interface 方法必须序列化 receiver effect。
- override/implementation 不能把 base/interface 的 readonly receiver 加强为 writable。
- writable base contract 可以由 readonly implementation 实现，但公开 dispatch contract 仍按 base 的 writable 要求调用；是否允许这种弱化需要在 overload/override 规则中保持唯一实现，本文采用“允许实现更少副作用”的默认值。

函数定义在参数列表之后使用 `:` 引出 return TypeRef。这里的 `:` 与 `name: Type` 同属声明到类型的绑定符号，但 grammar position 不同：parser 已处于 FunctionDefinition，消费完 `)` 后无需查询符号即可把 `:` 识别为返回类型边界。

```text
FunctionDefinition
  := Modifiers? "fn" Name TypeParameters?
     "(" Parameters? ")"
     (":" TypeRef)? FunctionBody

FunctionBody
  := Block
   | ";"  // interface/abstract/extern等bodyless declaration
```

block-bodied definition由`}`闭合，可以省略尾随分号；bodyless declaration必须以`;`结束。newline、`}`和EOF都不能代替该terminator。

### 2.3 匿名函数

匿名函数也是函数定义，因此显式返回类型同样使用 `:`：

```zr
let increment = fn(value: int): int => value + 1;

let transform = fn(value: int): string {
    return value.toString();
};

let factory = fn(): fn(int) -> string => createFormatter();
```

概念 grammar：

```text
AnonymousFunctionExpression
  := "fn" "(" Parameters? ")"
     (":" TypeRef)?
     (Block | "=>" Expression)
```

AnonymousFunctionExpression自身不消费外层statement terminator；当它是`let` initializer或expression statement时，外层声明/语句必须继续消费`;`，包括block body后的`};`。

- `=>` 只表示 expression body；block body 直接使用 `{ ... }`。
- 省略 `: TypeRef` 时允许从 body 推断匿名函数返回类型；公开命名声明是否允许推断仍由 API/artifact 规则决定，不由匿名函数语法放宽。
- `fn(value: T): R => expression` 是 value expression；`fn(T) -> R` 是 TypeRef。parser 通过所在的 expression/type grammar 和 `:`/`->` 分隔符直接区分，不依赖参数名大小写或绑定结果。
- 返回 callable 的匿名函数写作 `fn(): fn(A) -> R => expression`；外层 `:` 结束定义头，内层 `->` 只属于 return TypeRef。

### 2.4 函数类型

```zr
let compare: fn(readonly Item, readonly Item) -> int;
let factory: fn(Config) -> fn(string) -> Node;
```

普通自由函数类型不携带 receiver effect。未绑定 instance method 转换为 callable 时，receiver 作为第一个隐藏 contract 被保留，不能通过丢失 `const` 信息的普通函数指针绕过只读检查。

callable 类型中的 `->` 按右结合解析：

```text
fn(A) -> fn(B) -> R
== fn(A) -> (fn(B) -> R)
```

`fn(A): R` 在类型位置非法，并定向提示改为 `fn(A) -> R`；`fn name(A) -> R` 和 `fn(A) -> R => body` 在定义位置非法，并定向提示函数定义的返回类型使用 `:`。

### 2.5 类型语法

概念 grammar：

```text
Type
  := RefType
   | ReadonlyViewType
   | FunctionType
   | PrimaryType

FunctionType
  := "fn" "(" ParameterTypes? ")" "->" Type

RefType
  := "ref" Type
   | "ref" "readonly" Type
   | "scoped" "ref" Type
   | "scoped" "ref" "readonly" Type

ReadonlyViewType
  := "readonly" PrimaryType

ParameterType
  := "in" Type
   | "out" Type
   | Type
```

限制：

- `in/out` 只允许在 parameter type 入口，不允许局部 `var x: out T`。
- `scoped` 只允许修饰 ref、ref readonly 或被定义为 ref-like contract 的参数，不允许 `scoped int`。
- `ref ref T` 非法；需要 reborrow 时写表达式 `ref existingRef`，规范类型仍是单层 Ref。
- `readonly ref T` 不采用，统一写 `ref readonly T`；`readonly T` 表示对象/值能力视图，是另一语义。
- `readonly ref readonly T` 等重复组合产生定向诊断。
- nullable ref/owner 的组合由 TypeRef 规则处理，不能靠 token 顺序猜测。

### 2.6 参数声明

```zr
fn inspect(value: in Data): void;
fn mutate(value: scoped ref Data): void;
fn choose(left: ref Data, right: ref Data): ref Data;
fn observe(value: ref readonly Data): ref readonly Data;
fn create(result: out Data): bool;
```

参数 contract：

| 写法 | Canonical type | 入口状态 | 逃逸上界 | 临时值 | 调用标记 |
|---|---|---|---|---:|---|
| `value: T` | `T` | initialized | value 自身规则 | 是 | 无 |
| `value: in T` | `ref readonly T` | initialized | function | 是 | 无 |
| `value: ref T` | `ref T` | initialized | caller | 否 | `ref` |
| `value: ref readonly T` | `ref readonly T` | initialized | caller | 否 | `ref` |
| `value: scoped ref T` | `ref T` | initialized | function | 否 | `ref` |
| `value: out T` | `ref T` | uninitialized | function | 否 | `out` |

默认值规则：

- value 参数允许默认值。
- `in` 参数可以允许默认值，因为默认值形成不可逃逸临时 Place。
- `ref`、`ref readonly`、`scoped ref` 和 `out` 不允许默认值。
- variadic 参数第一版只允许 value contract。

### 2.7 调用参数

```zr
inspect(data);
mutate(ref data);
observe(ref data);
create(out data);
```

- `ref` 调用参数必须解析为可寻址 Place。
- `out` 调用参数必须解析为可写 Place；调用前旧值在语义上被结束，调用后仅在正常返回 edge 上 initialized。
- `in` 可接受 Place 或 rvalue；rvalue 被 materialize 为 function-scoped temporary Place。
- `ref readonly` 仍使用 `ref` 调用标记，因为该调用传递位置身份且引用可能返回/传播；readonly 由目标 contract 保证。
- 不允许省略 ref/out 标记后由 overload resolution 猜测。

### 2.8 Overload identity

为避免 ABI 相同但源 contract 不同导致歧义：

- value 与 by-ref 形态可以构成不同 overload，因为调用点形态不同。
- `in`、`ref readonly`、`ref`、`out` 不能只靠彼此差异形成 overload。
- `scoped` 不能单独形成 overload。
- receiver effect 参与 override/interface 合法性，但不能单独形成同名 overload。

artifact callable contract 仍完整保存这些差异，用于跨模块检查和诊断；“不能重载”不表示 contract 可以丢失。

## 3. readonly 能力

### 3.1 三种只读

必须区分：

- `let x: T`：绑定不可重新赋值。
- `ref readonly T`：不能通过该引用写入目标 Place。
- `readonly T`：不能通过当前对象/值 capability 调用 setter、写字段或调用 writable receiver。

`const` 不参与运行时只读 capability；它只表示编译期常量，或在 `const fn` 中标识 readonly receiver。

### 3.2 readonly class/interface handle

```zr
fn print(value: in Document): void {
    value.render();       // const fn，可调用
    value.clear();        // fn，可写 receiver，报错
}
```

从 readonly class/interface capability 复制出的 handle 仍是 `readonly T`。赋值、泛型推断、返回或 cast 都不能自动恢复 writable T。

readonly 是浅层 capability：其他 writable alias 仍可修改同一 GC 对象。第一版不把普通 GC class 的所有别名纳入 Rust 式独占借用，也不声称并发不可变。

返回`ref T`还要检查独立ref-export effect：普通readonly class/interface capability不能取得可写内部ref；但`Span<T>`/`PoolRef<T>`等显式writable-ref view即使自身fields为`let`，仍可导出其已经持有的writable referent capability。view readonly与referent readonly必须分开建模。

### 3.3 struct 与 owner receiver

- struct 的 writable `fn` 需要 writable Place 或 `ref T` receiver。
- `const fn` 可以接受 value、`in T`、`ref readonly T` 或 writable receiver。
- `Unique<T>` 可通过通用 owner-deref capability 产生 shared/mutable borrow。
- `Shared<T>` 只能自动产生 readonly borrow；不能凭引用计数 handle 获得独占 writable borrow。
- owner 自动解引用是 capability/protocol 行为，不得通过名字 `Unique`、`Shared` 特判。

## 4. 借用模型

### 4.1 Loan 类型

借用检查器只建立两类 loan：

```text
shared loan   -> ref readonly T
mutable loan  -> ref T
```

`Borrow<T>`、`Loan<T>`、borrowed/loaned ownership qualifier 不进入最终语言和 runtime value model。

### 4.2 基本冲突规则

对于 overlap 的 Place：

- 任意数量 shared loan 可以共存。
- mutable loan 存活时，不允许其他 shared/mutable loan。
- shared loan 存活时，不允许 Store、Initialize、Move 或 Drop。
- mutable loan 存活时，原 Place 不能通过非该 loan 路径读取、写入、move 或 drop。
- 对 disjoint Place 不产生冲突。
- unknown overlap 保守按 overlap 处理，除非 range/alias facts 能证明分离。

### 4.3 Non-lexical lifetime

loan 默认在最后一次可能使用后结束，而不是机械存活到词法 block 结尾：

```zr
let view: ref readonly int = ref value;
print(view);
value = 2; // view 后续不再使用，合法
```

算法以 CFG liveness 为基础：

1. 为每个 BorrowShared/BorrowMut 建立 LoanId。
2. 收集 ref value 的 use/propagation/escape。
3. 在 CFG 上反向计算 loan live-in/live-out。
4. 将 loan region 收缩到包含全部使用和必需 cleanup 的最小区域。
5. 对每条 Place access 与 live loan set 做 overlap/conflict 检查。

### 4.4 Reborrow

从现有 ref/owner 产生较短引用形成 reborrow：

```zr
fn use(value: scoped ref Data): void {
    inspect(value);       // mutable ref -> 临时 shared reborrow
    mutate(ref value);    // 较短 mutable reborrow
}
```

- shared reborrow 不消耗原引用，但在其存活期间冻结原 mutable ref 的写能力。
- mutable reborrow 在其存活期间暂停父 mutable ref 的全部访问。
- reborrow 的逃逸上界不得宽于父 ref/owner。

### 4.5 Method receiver 的两阶段 borrow

为降低常见方法调用负担，仅对 compiler-generated mutable receiver auto-borrow 采用受限两阶段规则：

```zr
buffer.push(buffer.length());
```

处理为：

1. reserve mutable receiver loan。
2. 计算不修改 receiver 的 value/in 参数。
3. 在调用前 activate mutable loan。
4. 调用结束或返回 ref contract 要求的 region 结束后释放。

显式 `ref` 表达式立即激活，不享受两阶段规则。reserved 阶段允许 shared read，不允许另一 mutable reserve、write、move 或 drop。

### 4.6 逃逸

逃逸格：

```text
temporary/block < function < caller < heap/static
```

约束：

- `in`、`out` 和 `scoped ref` 不得返回、闭包捕获、存入 heap/static 或跨 suspension。
- 非 scoped ref return 的结果 region 不得宽于所有可能来源的共同安全上界。
- 条件选择两个 ref 时，返回 region 取较短/更严格上界。
- ref 存入 ref struct 字段时，ref struct 的 escape 不得宽于字段来源。
- native API 若保存引用，必须显式声明 heap/static escape contract；普通 ref ABI 默认不允许。
- `PoolHandle<T>` 等 generational weak identity 不含 RefValue，不建立loan/region，可以长期保存。
- `PoolRef<T>` 等guarded ref struct从一次runtime validate/acquire call获得RefValue；派生ref不得宽于guard/view，active派生loan阻止whole view drop/move/recycle reclamation。
- runtime pool guard只协调跨alias的entity retirement/reuse，不替代局部static loan conflict检查，也不允许每次字段访问回退到generation check。

## 5. Move、out 与 borrow 的统一数据流

### 5.1 状态维度

每个 Place 分别跟踪 initialization、availability 和 loan set：

- `out` 修改 initialization。
- Unique move/drop 修改 availability。
- ref/in 修改 loan set。
- 三者在 CFG join 处独立合并。

这样 `out` 不需要伪装成 ownership state，borrow 也不需要把源变量运行时置 null。

### 5.2 out 规则

- 进入函数：目标 Place uninitialized，读取/借用其旧内容非法。
- 对字段逐项初始化：只有所有必需字段完成才认为 aggregate initialized。
- 正常 return：所有 out Place 必须 initialized。
- throw/异常 edge：不承诺调用者获得新值；调用者在异常路径按原语言异常模型处理。
- 调用返回 false 仍是正常 return，仍必须初始化 out；若 API 需要“失败无值”，使用 `Option<T>` return，不滥用 out。
- 将 out 参数传给另一个 out 参数可以转移 definite-assignment 责任。

### 5.3 Move 与 ref

- active loan 期间不能 move/drop source Place。
- move ref value 本身不移动其 referent；但移动包含 ref 的 ref struct 会转移该视图值，并保留 region。
- getter-only `ref T` property允许Store referent；`property = ref other`和ref setter不进入语言，重新绑定通过替换整个`var ref struct`或显式`rebind(ref)` method完成。
- Copy 类型按值复制不影响 source availability。
- Unique/其他 move-only 类型按值参数默认 move；只读访问应使用 `in` 或 receiver auto-borrow。

## 6. 闭包、async、generator 与 native

### 6.1 闭包

- 捕获 value：按 Copy/Move 规则决定。
- 捕获 ref：closure escape 必须不宽于 ref 来源。
- 逃逸 closure 不能捕获 scoped ref、stack-only ref struct 或 out parameter。
- writable capture 建立 mutable borrow；在 closure 生命周期内阻止外部冲突访问。

### 6.2 async/generator

- borrow/ref struct 活跃区间跨越 await/yield 时默认报错。
- 将普通 value/owner move 入 coroutine frame 可以合法，但 frame 必须拥有 drop glue。
- shared GC handle 可复制进入 frame；readonly capability 不因存入 frame 提升。
- 后续若支持 self-referential pinned frame，需要单独 unsafe/pin 设计，不能在第一版隐式放宽。

### 6.3 native

- `in/ref/out` native signature 必须序列化 calling/escape contract。
- native 不声明 capture 时，ref 默认 scoped 到调用。
- 允许保存引用的 native API 必须使用显式 handle/pin contract，而不是把 `scoped` 静默忽略。
- debug runtime 可以验证 pin/handle 使用，但语言正确性不能依赖运行时 borrow table。

## 7. Parser 与 AST 契约

### 7.1 Syntax AST

建议 AST 只保留语法差异：

```text
TypeSyntax
  RefTypeSyntax(access, scopedToken, target)
  ReadonlyTypeSyntax(target)
  FunctionTypeSyntax(parameters, arrowToken, returnType)
  ...

FunctionDefinitionSyntax
  name
  parameters
  returnColonToken?
  returnType?
  body

AnonymousFunctionExpressionSyntax
  parameters
  returnColonToken?
  returnType?
  bodyKind: block | expression
  expressionBodyArrowToken?

ParameterSyntax
  name
  typeSyntax
  sourcePassingForm

CallArgumentSyntax
  marker: none | ref | out
  expression

MethodSyntax
  receiverModifier: default | const
```

`sourcePassingForm` 用于 source map、formatter 和迁移 fix；语义层必须规范化为 CallableValueContract。旧 `EZrParameterPassingMode` 不再作为 compiler/runtime 决策依据。

`returnColonToken`、function-type `arrowToken` 和 expression-body `=>` 只服务 source fidelity、formatter、diagnostics 和 migration。binder 为三种 syntax node 构造同一 canonical callable signature/contract，不把 delimiter 变成 TypeId flag。

### 7.2 Error recovery

需要专门诊断：

- `ref` 缺少目标类型。
- `scoped` 未修饰 ref-like 类型。
- `out` 出现在非参数类型位置。
- `readonly ref` 顺序错误并建议 `ref readonly`。
- call-site `ref/out` 与目标 contract 不匹配。
- 旧 `%ref/%in/%out/%func` 的迁移建议。
- function definition 在 `)` 后误用 `->`，建议 `:`；anonymous definition 同样处理。
- FunctionTypeSyntax 在 `)` 后误用 `:` 或 `=>`，建议 `->`。
- `=>` 后缺 expression、block body 前多余 `=>`、return TypeRef 半输入时保留完整 delimiter/range。

恢复节点必须保留 range，使后续 LSP 不因半输入代码崩溃。

## 8. 诊断模型

borrow 诊断至少包含：

- primary conflict range。
- loan origin range。
- source Place declaration range。
- last use/escape range。
- related move/drop/write range。
- 稳定 diagnostic code 和 machine-applicable fix（存在时）。

示例：

```text
ZR2104: cannot write `buffer[0]` while readonly borrow `view` is active
  readonly borrow begins at line 12
  conflicting write occurs at line 15
  borrow is last used at line 18
```

对于 unknown alias 导致的保守拒绝，诊断必须说明“无法证明两个索引不重叠”，不能假装已经证明它们相同。

## 9. 里程碑

### M1 Syntax 与 canonical contract

- lexer/contextual keyword、parser、AST recovery 完整。
- 所有函数声明改由 `fn` 起始。
- 命名/匿名 FunctionDefinition 使用 `:`，FunctionTypeSyntax 使用 `->`，anonymous expression body 使用 `=>`。
- parameter/call argument 规范化为 Canonical Type + CallableValueContract。

晋级门：定义返回 callable、嵌套 callable、三种 delimiter 的所有合法/非法组合和 source range 测试通过；compiler 不再新增旧 passing mode 或 delimiter-based semantic 分支。

### M2 Place access 与 out definite assignment

- ref/out 调用只接受合法 Place。
- out entry/exit、字段初始化、CFG join、正常/异常 edge 完整。

晋级门：局部、字段、索引、deref、条件、循环、异常、跨调用 out 全覆盖。

### M3 Shared/mutable loan 与 NLL

- loan creation、liveness、overlap、reborrow、last-use 结束完整。

晋级门：Rust/C# 对应负例迁移、nested reborrow、动态索引 unknown alias、循环 loan 全覆盖。

### M4 Receiver/read-only 与 call boundary

- `const fn`、ordinary `fn`、readonly view、two-phase receiver borrow、owner auto-deref 完整。

晋级门：class/struct/interface/override/generic/dynamic/native 调用矩阵全部验证。

### M5 Escape、closure 与 suspension

- caller/function/heap escape、ref return、closure、async/generator 完整。

晋级门：所有非法逃逸有精确 origin/escape 诊断；VM/AOT 不含运行时 borrow fallback。

### M6 Artifact/LSP consumers

- callable contract roundtrip。
- hover/signature/semantic diagnostics 使用统一 facts。

晋级门：source、binary import、VM、AOT、LSP 对同一签名输出一致 canonical form。

## 10. 测试矩阵

### Syntax

- 命名/局部/member/interface/async函数定义以及native extern block中的函数声明使用 `:`，匿名函数显式返回类型也使用 `:`。
- callable type 使用 `->`，覆盖 nested callable、generic、nullable、array、tuple、union 与 ref 组合。
- 返回 callable：`fn make(...): fn(A) -> R`；多层 callable type 按右结合解析。
- definition 中误用 `->`、function type 中误用 `:`、return type 缺失、`=>` 误用于 type 的定向诊断与 token range。
- modifier 顺序、重复 modifier、缺失 type、半输入恢复。
- const fn 仅 member、static const fn 错误。
- ref readonly/scoped ref/scoped ref readonly。
- bodyless function缺少`;`、braced function省略/保留可选尾随`;`、block lambda initializer要求`};`，以及newline/`}`/EOF不触发隐式终止。

### Borrow/dataflow

- 多 shared loan、shared/write conflict、mutable/read conflict。
- field disjoint、tuple disjoint、array dynamic unknown。
- reborrow parent freeze/restore。
- NLL last-use、branch-only use、loop-carried loan。
- move/drop during borrow。
- out 未写、部分写、分支写、循环写、throw edge。

### Calls

- value/in/ref/ref readonly/scoped/out。
- default/variadic 限制。
- overload、interface、virtual、generic、dynamic、native。
- owner/struct/class receiver。
- two-phase method receiver。

### Escape

- ref return from local/parameter/field/index/ref struct。
- closure capture、global/module store、container store。
- await/yield 前后最后使用边界。
- native handle/pin contract。

### Stress

- 大函数上千 Place/Loan。
- 深层 projection 与大量 CFG join。
- 热循环 borrow analysis 时间和内存上限。
- LSP 增量编辑下 stale fact 不得复用到新 revision。

## 11. 参考依据

- C# span safety 和 safe-context：`lua/csharplang/proposals/csharp-7.2/span-safety.md`。
- C# ref field/scoped：`lua/csharplang/proposals/csharp-11.0/low-level-struct-improvements.md`。
- C# in/ref readonly 区分：`lua/csharplang/proposals/csharp-12.0/ref-readonly-parameters.md`。
- C# 将 method declaration 与 function-pointer type 使用不同表层结构：`lua/csharplang/proposals/csharp-9.0/function-pointers.md`、`lua/roslyn/src/Compilers/CSharp/Test/Syntax/Parsing/FunctionPointerTests.cs`。
- Roslyn ref escape/return/field 测试：`lua/roslyn/src/Compilers/CSharp/Test/Semantic/Semantics/RefEscapingTests.cs`、`RefLocalsAndReturnsTests.cs`、`RefFieldTests.cs`。
- Rust 对 declaration/function type 共用 `->` 的对照实现：`lua/rust/compiler/rustc_parse/src/parser/ty.rs`、`lua/rust/tests/pretty/fn-types.rs`。ZR 刻意分离 definition boundary，以改善返回 callable 时的可读性。
- CPython 分离 `function_def_raw` 与独立 `func_type` grammar：`lua/cpython/Grammar/python.gram`、`lua/cpython/Lib/test/test_type_comments.py`。ZR 进一步让两类 grammar 使用不同 delimiter，使 parser recovery 和 LSP 展示更直接。
- Rust MIR borrow checker：`lua/rust/compiler/rustc_borrowck/src/lib.rs`。
- Rust borrow/move/reborrow 负例：`lua/rust/tests/ui/borrowck`、`lua/rust/tests/ui/moves`。
- ZR 当前 passing mode 与 ownership facts：`zr_vm_parser/include/zr_vm_parser/ast.h`、`semantic_facts.h`、`type_inference_passing_modes.c`、`dataflow_ownership*.c`。

ZR 刻意保留 C# 风格的低标记调用体验，不暴露命名生命周期；同时采用 Rust 的 Place/loan 冲突和 move 静态检查。`const fn` 是 ZR 已确认的 receiver 拼写，不照搬 C# `readonly` member 或 Rust `&self/&mut self` 表层语法。函数定义使用 `:`、callable TypeRef 使用 `->`，是为了让 source signature 的声明边界和类型映射职责分离；二者绑定后仍生成同一个 canonical callable contract。
