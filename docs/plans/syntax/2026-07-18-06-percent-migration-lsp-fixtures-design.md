# 06 `%xxx` 迁移、LSP、文档与全项目 fixture

> 状态：细化草案，等待人工确认。
>
> 硬依赖：本目录 01-05 子设计全部通过各自 promotion gate。

## 1. 目标结果

一次性把仓库和用户源码从旧 `%xxx`/兼容语法迁移到新规范，并确保 compiler、LSP、规范、示例、fixture、golden、artifact 和扩展表现一致。

目标状态：

- 正式 compiler 只执行新语义，不保留长期双轨 lowering。
- 旧语法由专用 migration frontend 识别并生成结构化 edits/人工迁移报告。
- LSP 的 hover、signature、diagnostic、semantic token 和 code action 全部消费统一 semantic facts。
- `docs/zr_language_specification.md` 只描述当前正式语法。
- 全仓可执行 `.zr`、项目 fixture、benchmark 和 golden 已迁移。
- 旧 `.zro` schema 明确拒绝并要求重编译。
- `%xxx` 仅存在于迁移测试、负例和带历史标记的计划文档。

## 2. 迁移原则

### 2.1 语义切换与源码改写分离

- 新 compiler 只接受新 AST/Canonical Type/SemIR。
- 旧 parser 可以被封装进 `zr migrate syntax`，但不进入正式 compile path。
- migration frontend 将旧源码解析为旧 AST，绑定到尽可能完整的 semantic facts，再输出 text edits。
- 不能证明等价的改写不自动应用，必须输出 primary/related ranges、原因和推荐目标形式。

### 2.2 不做字符串替换

禁止全仓简单替换 `%ref -> ref`，原因包括：

- `%ref name: T` 要重排为 `name: ref T`。
- call site 需要按目标 contract 添加 `ref/out`。
- `%type` 需要区分 `typeof(expr)` 与 `typeid(T)`。
- `%using` 当前承担资源、union pattern 和 plugin guard 多种职责。
- `%borrow/%loan` 需要产生 ref binding/reborrow，而不是删除 `%`。
- 注释、字符串、历史文档和 migration negative fixture 不应被误改。

### 2.3 单一语义截止点

推荐使用一个 repository cutover commit/里程碑：

1. 新基础层和新语法在独立 gate 中完成。
2. migration tool/LSP fix 准备完成。
3. 一次性迁移源码、测试、文档和 golden。
4. 正式 parser 把旧语法改为 migration diagnostic。
5. 删除旧 lowering/runtime opcode 路径。

不允许新旧语法长期同时生成不同 IR。

## 3. 迁移分类

### 3.1 可纯语法改写

在 AST 结构足够明确时自动应用：

| 旧写法 | 新写法 |
|---|---|
| `%module app.main` | `module app.main;` |
| `let math = %import("core.math")` | `let math = import("core.math");` |
| `%import core.math` | 生成module-scope import binding候选；alias冲突时requiresReview |
| `%async name(...): T` | `async fn name(...): Task<T>`；无法确定completion type时requiresReview |
| `%await expression` | `await expression` |
| `%extern("library") { ... }` | `native extern("library") { ... }` |
| `%test("display") { body }` | `#zr.testing.test# fn generatedName(): void { body }`；display只用于生成稳定identifier |
| `%compileTime fn/let/{...}` | `comptime fn`、`const`或`comptime {...}`；不引入`comptime let` |
| `%func(A)->R` | `fn(A) -> R` |
| `%func(A)=>R` | `fn(A) -> R` |
| `func name(...): R` | `fn name(...): R` |
| `fn name(...) -> R` | `fn name(...): R` |
| `(value: T): R -> { ... }` | `fn(value: T): R { ... }` |
| `(value: T): R => expression` | `fn(value: T): R => expression` |
| `%owned class T` | `resource class T` |
| `%release(value)` | `drop(value)` |
| `%upgrade(weak)` | `weak.upgrade()` |
| `%weak(shared)` | `shared.weak()` |
| `%shared(unique)` | `unique.share()` |
| `%detach(unique)` | `unique.intoGc()` |

兼容 `func name(...)` 和无关键字函数声明也迁移为 `fn name(...)`，保留/补全定义返回类型前的 `:`。migration frontend 必须先区分 FunctionDefinition、AnonymousFunctionExpression 和 FunctionTypeSyntax：定义中的 `->` 改为 `:`，类型中的 `->` 保留，旧 type arrow `=>` 改为 `->`，expression body 才保留 `=>`。

`%test`迁移不仅删除`%`：新TestEntry必须附着在稳定identifier且显式返回`void`的普通函数上。旧display string只参与生成identifier，不增加`name` metadata。旧body若用`return 0/1`表达harness结果，只有能证明该convention时才改写为`zr.testing` assertion，否则标记requiresReview。

旧`%compileTime class/struct`、`@decorate`class convention和runtime module-init decorator不能按token改为`comptime class/struct`。migration必须绑定其target/patch能力：只产生metadata的迁为带`AttributeUsage` role的普通`readonly struct`，只做validation/generation的迁为带`DeclarationTransform` role且显式返回typed `Patch`的普通`comptime fn`；修改已有body/live prototype或依赖runtime object identity的用法标记blocked。

所有machine edit同时补全目标语法要求的statement terminator。newline不能作为旧/新语法共同终止点；migration frontend根据AST declaration/statement range插入 `;`，不得按文本行末批量追加。已有braced declaration尾随分号可以保留输入range，但formatter规范化时删除。

import迁移还必须检查binding facts：module-scope `var alias = %import("path");` 只有在alias从未重新赋值时才能改为`let alias = import("path");`；local/conditional import不得自动hoist，统一requiresReview。旧standalone `%import core.math` 可以用最后一个path segment生成alias候选，但alias已占用、非法identifier或member访问依赖旧隐式注入时必须requiresReview，不能静默改变名字解析。

旧 `%detach(shared)` 即使运行时 strong count 恰好为 1 也不自动改写：新设计不允许 Shared `.intoGc()`，该情况标为 blocked/requiresReview，调用者需要重构为在 share 前转换，或显式选择其他 bridge。

旧 `%unique(expr)` 只有在 `expr` 是新鲜 resource 构造且不存在其他 alias 时才能改写为 `own T(...)`；从普通 GC value 强化为 Unique 不再支持。无法证明唯一性的用法标为 blocked。

### 3.2 需要类型/调用解析的改写

struct 构造迁移也属于本类，不能对 `$` 做纯 token 替换：

| 旧写法 | 新写法/结果 | applicability |
|---|---|---|
| `$Point(1, 2)` | `init Point(1, 2)` | target 静态绑定为 value-constructible TypeRef 时 machineApplicable |
| `$module.Point(1, 2)` | `init module.Point(1, 2)` | qualified TypeRef 可稳定绑定时 machineApplicable |
| `$Alias(1, 2)` | `init Alias(1, 2)` | alias canonical TypeId 具有 valueConstructible capability 时 machineApplicable |
| `$proto(1, 2)` | `reflection.requireConstructible(proto).createInstance(...constructionArgs)` + requiresReview | `proto` 可绑定为 `zr.reflection.Type`；tool 生成 capability check和`object[]`参数数组候选，但要求人工确认装箱和constructor选择 |
| `$(expr)(1, 2)` | requiresReview 或 blocked | expression 结果可绑定为 `zr.reflection.Type` 时生成同类候选；否则 blocked |

动态 Type object case 必须迁往 `zr.reflection.Type` + `requireConstructible` + `ConstructibleType.createInstance(...constructionArgs: object): object`。migration frontend 输出 target range、inferred TypeId/type-object facts、capability check、call sites、参数装箱 edit 和 reason，并把 edit 标为 `requiresReview`；调用端的 `constructionArgs` 为 `object[]`，使用 `...constructionArgs` 展开。即使 expected result type 已知，也不能把运行时 `proto` 伪装成 TypeRef 或插入 `init`。

普通 call 与构造必须按 bound kind 区分：

```zr
// 旧
let value = $StructData(1);
let called = StructData(1);

// 新
let value = init StructData(1); // @constructor
let called = StructData(1);     // ordinary call / @call
```

如果 `StructData(...)` 在旧程序中实际依赖 `@call`，migration 不得改写；如果它不可调用但 target 是可构造 TypeRef，可提供 `init` code action，但只有 semantic query 证明不存在 `@call`/shadowing 歧义时才 machineApplicable。

```zr
// 旧
func swap<T>(%ref left: T, %ref right: T): void;
swap(a, b);

// 新
fn swap<T>(left: ref T, right: ref T): void;
swap(ref a, ref b);
```

需要 semantic query：

- `%in/%ref/%out` parameter 重排。
- 所有直接、member、generic、overload、interface、imported call site 添加 marker。
- function type 参数 contract 重写。
- method group/delegate/FFI signature 重写。
- ref/out 参数透传。

如果 target dynamic/unresolved，migration tool 不猜测，输出人工项。

### 3.3 ownership 类型改写

| 旧写法 | 新写法 |
|---|---|
| `%unique T` | `Unique<T>` |
| `%shared T` | `Shared<T>` |
| `%weak T` | `Weak<T>` |
| `%unique new T(...)` | `own T(...)` |
| `%shared new T(...)` | `own T(...).share()` |
| `Borrow<T>` / `%borrowed T` | `ref readonly T` 或 `in T`，按使用位置 |
| `Loan<T>` / `%loaned T` | `ref T` 或 `scoped ref T`，按逃逸 |

Unique/Shared/Weak 泛型形式如果已经符合新规范则保留；只需重新绑定到 canonical owner TypeRef。

### 3.4 borrow/loan 表达式

```zr
// 旧
var view = %borrow(owner);
var loan = %loan(owner);

// 新候选
let view: ref readonly T = ref owner;
let loan: ref T = ref owner;
```

自动迁移需要证明：

- source 可形成合法 Place/owner deref。
- inferred target type 确定。
- ref 不逃逸到新规则禁止的位置。
- loan 的旧 return-loan/作用域可由 NLL/reborrow 等价表达。

无法证明时输出人工迁移，不生成运行时 Borrow/Loan compatibility wrapper。

### 3.5 `%type`

- operand解析为TypeRef且结果只用于identity equality/map/dispatch：`typeid(T)`。
- operand解析为TypeRef且结果调用member/meta/layout/create API：`reflection.resolve(typeid(T))`，必要时再做ConstructibleType capability check。
- operand是运行时expression：`typeof(expr)`。
- 旧`%type`结果参与类型位置、反射值、泛型参数时需要完整type-value facts；`typeid`不能伪装成descriptor，`typeof`不能替代静态TypeRef identity。
- unresolved/dynamic ambiguity 不自动改写。

### 3.6 `%using`

按旧 AST/semantic role 拆分：

- Close/Dispose resource scope：改为普通 `using`。
- owner 构造/owner lifetime：删除 using，依赖 Unique 自动 Drop。
- union pattern guard：改为 `if let` 或 `switch/match`。
- plugin/import guard：改为 `loadPlugin(...)` 等动态加载 API + union/result pattern。
- field-scoped using：迁移为 owner field 或显式 Close field，不保留 field flag。

由于 control flow 会变化，union/plugin/field using 默认标为“需要人工确认”；tool 可以生成候选 edit 和 diff，但不自动提交。

### 3.7 property 迁移

旧 getter/setter 需要按 declaring type、name、static、type 和 visibility 组合为统一 property：

- 能唯一配对且 body 不冲突时自动生成 PropertyDecl。
- 只有 getter 或 setter 时生成单 accessor property。
- class/interface property 使用同一目标格式。
- 类型、可见性或 static 不一致时输出冲突报告。
- 已有field必须保留并规范为`pri/pro/pub let|var`；生成的accessor显式读取/写入该field，不改为contextual `field`。
- 旧auto/bodyless concrete accessor若无可绑定field，生成`requiresReview`并建议先声明`pri let/var _name`；不能静默合成storage。
- ref-return property迁为getter-only；旧ref setter/ref rebind标记blocked并建议替换whole ref-like view或显式method。

## 4. Migration frontend

### 4.1 命令接口

建议：

```text
zr migrate syntax <path>
  --check
  --write
  --format json|text
  --include-generated
  --language-from legacy
  --language-to current
```

- `--check` 不写文件，返回是否存在迁移项。
- `--write` 只应用 machine-applicable edits。
- generated/bin/golden 默认不直接改，由对应生成流程重建。
- JSON 输出供 LSP/CI/IDE 消费。

### 4.2 Edit 等级

```text
machineApplicable
maybeIncorrect
requiresReview
blocked
```

- machineApplicable：语法和语义均证明等价。
- maybeIncorrect：能生成候选，但存在 dynamic/overload/format 风险。
- requiresReview：control flow、lifetime 或 API 会改变。
- blocked：旧代码依赖新模型明确禁止的行为。

`--write` 默认只应用 machineApplicable。

### 4.3 幂等与冲突

- 对已迁移文件再次执行必须无 edit。
- edits 按 source range 从后向前应用，并检查 document version/hash。
- overlapping edits 先在 AST/semantic plan 合并；无法合并则标 blocked。
- 文件修改后重新 parse/bind，只有新语法无新增错误才算该文件迁移成功。
- 跨文件调用点迁移必须以 project graph 为单位，不按单文件猜测 imported signature。

### 4.4 报告

每项包含：

```text
diagnosticCode
file/range
oldConstructKind
targetConstructKind
oldTargetBindingKind
resolvedTargetTypeId?
applicability
edits[]
relatedDeclarations[]
reason
```

报告可以由 LSP 直接转换为 code action/WorkspaceEdit。

## 5. 正式 parser 的旧语法行为

cutover 后：

- 识别 `%` + 已知 legacy directive，产生稳定 migration diagnostic 和 fix hint。
- 不把它当普通 token error，也不进入旧 semantic lowering。
- diagnostic 指向完整 directive/parameter/type range。
- 对可以单点修复的形式提供 machine-applicable edit。
- 对需要 project binding 的形式提示运行 `zr migrate syntax`。
- 未知 `%identifier` 继续按语言普通 `%`/错误规则处理，不能把任意拼写误当旧关键字。
- 识别旧 `$Type(...)`/`$(expr)(...)` construct AST 并产生独立 migration diagnostic；不把 `$` 当作新核心语法继续 lowering。
- 静态 TypeRef target 提供 `$Type(...) -> init Type(...)` edit；dynamic prototype target 明确标记 requiresReview；unresolved/invalid target 只有在无法满足 reflection contract 时 blocked，二者都不能提供不安全 quick fix。

旧 parser 只存在于 migration tool target，并设删除期限；不能被 CLI compile/repl/import fallback 调用。

正式parser还必须拒绝：

- 任何依赖newline、`}`或EOF结束的simple declaration/statement，并提供插入 `;` 的定向修复。
- 裸 `import core.math;`、`import "core.math";` 或 standalone `import("core.math");`。
- `import(path)`、local/conditional import binding和mutable `var module = import(...)`。

唯一current static import入口是module-scope `let alias = import("literal.path");`。它形成专用ImportExpression/ModuleNamespaceBinding，不按普通CallExpression绑定。

## 6. LSP 设计

### 6.1 单一 semantic source

LSP 通过 semantic query 获取：

- canonical TypeId/format。
- SymbolId/PropertyId/PlaceId。
- CallableValueContract/receiver effect。
- move/loan/escape/definite-assignment facts。
- migration diagnostic/fix applicability。

删除 LSP 内 `%unique` 字符串表、ownership generic unwrap 和 getter/setter 独立推断等重复语义。

### 6.2 Hover 与 signature

统一显示：

```text
fn swap<T>(left: ref T, right: ref T): void
const fn length(): usize
value type: fn(ref T, ref T) -> void
pub property current: ref readonly Item { pub get; }  // interface/abstract contract
let texture: Unique<Texture>  [moved at ...]
let entity: PoolHandle<Entity>  [weak generational identity]
let view: PoolRef<Entity>  [scoped direct ref + guard]
```

- declaration/signature hover 使用 definition surface：return TypeRef 前显示 `:`；纯 callable value/type hover 使用 `fn(...) -> R`。
- hover 展示 canonical surface，不展示内部 flat enum/typeName。
- signature help 标识 value/in/ref/ref readonly/out/scoped。
- owner method显示“consumes receiver”“returns shared owner”等 effect。
- readonly/ref-like/resource capability 放入 rich hover 的 Type/Effects sections。
- `init TypeRef(...)` signature help 展示所选 `@constructor`；hover 标记 `value construction`、result TypeId 和“inline/no GC allocation”契约。
- 普通 `TypeName(...)` 仍按 call/`@call` 展示，不能把 constructor overload 混入 call signature list。
- import alias hover展示原spelling、ModuleDomain、canonical ModuleId、provider/source-binary-descriptor resolution和readonly ModuleNamespace object type；definition跳到module declaration或descriptor virtual document，member definition继续跳到exported symbol。
- `typeof(expr)` hover显示静态可证明的最精确TypeOf子类或erased `Type`；`typeid(T)`明确标记lightweight identity/no member metadata root。
- PoolHandle只补全validate/tryBorrow/tryRead/recycle等handle API；PoolRef才补全referent property，并显示禁止heap store/suspension。

### 6.3 Semantic tokens

在现有 token type 基础上增加/复用 modifier：

```text
readonly
modification
moved
borrowed
resource
refLike
deprecated
```

- `fn/ref/in/out/scoped/readonly/resource/property/init` 在相应上下文着色为 keyword。
- `field` 始终按普通identifier着色；只有隐式setter参数`value`使用contextual parameter分类。
- moved symbol 可以由扩展弱化显示，但标准 diagnostics 仍是权威。
- legacy `%xxx` 标记 deprecated，并关联 code action。
- legacy `$` construct marker 标记 deprecated；普通 `$`/其他 token 用途若仍存在，不能仅凭字符着色为 deprecated。

客户端不支持自定义 modifier 时仍必须保留标准 token type，不影响可读性。

### 6.4 Diagnostics/code action

- legacy spelling replacement。
- parameter 重排与 call-site marker project edit。
- `func`/keywordless function -> `fn`，并把 definition return delimiter 规范为 `:`。
- definition 中的 `->` -> `:`；function type 中的旧 `=>` -> `->`；anonymous expression body 的 `=>` 保留。
- `%owned class` -> `resource class`。
- ownership builtin -> method/drop/own。
- `$StaticType(...) -> init Type(...)`，以及裸 struct call 在确定不可调用时的 `init` 建议。
- `$proto(...)`/`$(expr)(...)` 在 target 可绑定为 `zr.reflection.Type` 时提供 `requireConstructible(target).createInstance(...constructionArgs)` 的 requiresReview action；target 无法满足 reflection type contract 时报告 blocked，绝不建议 `init`。
- getter/setter -> unified property。
- use-after-move/borrow escape 显示 related ranges。
- missing simple-statement terminator在newline、`}`和EOF前统一提供插入`;`的Quick Fix；不得提供“保留换行即可”的修复。
- `%import`迁为module-scope `let alias = import("literal");`；dynamic/local/mutable/alias-conflict case只提供requiresReview或blocked action。
- `import("C:/...")`、POSIX/UNC裸绝对路径不自动猜成module name。目标`.zr/.zrp/.zrm`的identity可静态读取时提供requiresReview的规范`file:` URI改写；发布代码仍优先引导迁到`.zrp` dependency/alias/nativeProviders。
- source/package/file target/custom descriptor声明`zr.*`或使用`native:zr.task`时只报告reserved-root迁移诊断，不建议改成alias绕过；native descriptor name与manifest key不一致时同时定位`.zrp`和descriptor source。
- `%test`迁为带`#zr.testing.test#`的named普通`fn(...): void`；旧display string只用于生成稳定identifier，旧integer-return harness convention不能静默保留。
- `%compileTime` value/check迁为`comptime`；旧compile decorator迁为带`#zr.compile.declarationTransform#`的普通`comptime fn(...): DeclarationPatch`，runtime decorator改为retained metadata或显式runtime call，不生成token/AST rewrite。
- `{{...}}`和generator `out expr`迁为显式返回`Iterator<T>`的local/named普通`fn`+`yield`；涉及element type、capture或escape无法证明时提供requiresReview action。

只有 migration frontend 标记 machineApplicable 的 fix 才能作为默认 Quick Fix；requiresReview 作为明确标注的 refactor action。

### 6.5 Completion/inlay

- completion 插入新语法模板，不再建议 `%xxx`。
- function/member/interface/anonymous definition和native extern block中的函数声明completion插入 `: ReturnType`；TypeRef completion插入 `fn(...) -> ReturnType`。
- expression-start completion 可建议 `init`；`init` 后只建议 valueConstructible TypeRef/alias，不建议 runtime value/prototype variable。
- parameter completion 使用 `name: in/ref/out T`。
- module scope completion插入`let alias = import("module.path");`，并根据已解析ModuleId建议无冲突alias；function/local scope不建议static import。
- import completion按domain区分ModuleSpecifier：`zr.`只列OfficialNative inventory，`native:`只列compile descriptor catalog，`#`/`@`分别列alias/dependency；`file:`提供规范URI locator completion但不将路径混入workspace module completion。`.zrp`的`nativeProviders.library`/dependency path字段继续提供filesystem completion。
- formatter保留所有required semicolon，删除braced declaration的冗余尾随semicolon，并可安全输出single-line minified form。
- override/interface completion生成正确 `const fn` 和 property accessors。
- completion复用普通声明模板：attribute schema生成带`AttributeUsage`的`readonly struct`，declaration transform生成带role的`comptime fn(...): Patch`，async生成`async fn(...): Task<T>`，iterator生成`fn(...): Iterator<T>`，test生成`#zr.testing.test# fn(...): void`。不建议`attribute`、`decorator`、`iterator`、`test`函数关键字、runtime decorator class、TaskRunner或匿名test block。
- hover/definition消费condition decision、generated origin、async completion/Task、iterator element/Iterator和TestEntry facts；LSP不得执行decorator或扫描文本发现test。
- `zr.*`/custom native hover显示ModuleDomain、Canonical ModuleId、provider phase和descriptor ABI/contract hash；N0-N3只对OfficialNative显示。physical library/source path单独显示为provider locator，不混入TypeId或rename。
- inlay hints 默认不显示每个隐式 shared borrow，避免噪声；可选显示 move/owner-consume/implicit temporary。

### 6.6 增量一致性

- semantic facts 绑定 document revision/hash。
- migration edit 后失效旧 TypeId/PlaceId position index，重新 bind affected project slice。
- cross-file call-site edit 使用 workspace edit 和 version guard。
- stale fact 不能用于 rename/code action。

## 7. 规范与文档

正式切换时更新：

- `docs/zr_language_specification.md`：唯一当前规范。
- 关键字表和 grammar。
- 类型、ref、readonly、borrow、ownership、property、Span 章节。
- function definition、anonymous function 与 callable TypeRef grammar；明确 `:`/`->`/`=>` 的唯一职责。
- statement terminator grammar；明确newline永不终止、braced declaration可省略一个尾随分号、compound control-flow不消费declaration分号、simple declaration/statement必须显式`;`。
- module import grammar；静态导入固定为 `let alias = import("module.path");`，并与runtime loader分离。
- ModuleDomain grammar；`zr.*`/`native:`/workspace/`@package`形成不同identity，`file:`只定位目标identity，alias展开后保留domain。
- struct construction 章节：`init TypeRef(...)`、call/`@call` 分离、dynamic prototype reflection boundary。
- reflection章节：`zr.reflection` Type/TypeOf层级、member/meta query、`typeof/typeid/resolve`和ConstructibleType。
- property章节：显式`pri/pro/pub let|var` field、无auto/backing、getter-only ref property。
- pooling章节：`zr.pooling` PoolHandle/PoolRef、generation/retirement/deferred reuse和GcScanKind。
- migration guide：完整 old -> new 映射和非机械案例。
- performance guide：GC/Unique/Shared/Span/`zr.pooling`选择，以及GcFree NoScan与GcMapped/barrier成本分账。
- native/FFI guide：ref/owner/handle/pin contract。
- LSP/extension 文档和 snippets。

历史 `.codex/plans`：

- 不做无差别全文改写。
- 在已被替代计划顶部加 superseded 指向本目录。
- 保留历史决策上下文，但不得被当前规范索引为有效语法。

文档代码块纳入 compile/doc-test：

- 标为 current 的 zr 示例必须 parse/typecheck。
- migration 文档 old 示例标为 `zr-legacy` 或不进入 current compile。
- 负例声明 expected diagnostic code。

## 8. 全项目 fixture

### 8.1 源码

扫描：

- `tests/**/*.zr`。
- `tests/fixtures/projects/**`。
- benchmarks 的 ZR case。
- `zr_vm_lib_*` 和 `zr_vm_library` 脚本资源。
- examples、docs code snippets。
- extension snippets/tests。

排除必须显式列出：migration input、legacy parser tests、历史 plan、预期 diagnostic fixture。

### 8.2 Golden 与 binary

- `.zrs` 由新 AST writer 重建。
- `.zri` 由新 canonical type/Place/CFG/contract dump 重建。
- `.zro` 因 schema version 改变全部重编译。
- binary fixture 不使用文本 patch。
- golden diff 需要审查：语法变化、token/range、TypeRef signature、layout/contract hash。

### 8.3 Reference fixture manifest

每个相关 feature group 至少区分：

- current pass。
- current compile fail + diagnostic code。
- migration legacy input + expected edits/report。
- VM/AOT equivalent output。
- artifact roundtrip。
- LSP expected hover/token/fix。

### 8.4 Benchmarks

- 迁移前记录语义等价 benchmark baseline。
- 迁移后验证结果一致。
- 单独记录 borrow checker compile time、artifact size、Unique/Shared/Span runtime 性能。
- 语法迁移不允许以删除检查换取虚假性能提升。

## 9. Artifact cutover

- 提升 source/binary schema version。
- loader 对旧 `.zro` 返回明确 `artifact version requires recompilation`。
- 不在 runtime 中把旧 ownership/ref signature 动态映射为新 contract。
- build cache key 包含 language version、schema version、type/layout/contract hash。
- `.zri/.zrs` header 标记语法/语义版本，工具不得混读。
- AOT manifest、metadata sidecar、debug source map 同步升级。

## 10. 仓库收口检查

cutover gate 使用结构化 allowlist，而不是简单要求零 `%`：

允许：

- `%` 算术取模。
- 字符串/注释中的普通文本。
- migration/legacy negative fixture。
- superseded 历史计划。

禁止：

- current `.zr` 中已知 `%xxx` directive。
- current spec/snippet 中旧语法。
- LSP completion/token metadata 推荐旧语法。
- compiler/runtime 新语义路径引用 borrowed/loaned ownership kind。
- VM/AOT 继续发出新设计已删除的 borrow/loan/return-loan 指令。

检查工具应解析 token/文件类别，避免 `Select-String "%"` 产生大量误报。

## 11. 里程碑

### M1 Migration inventory

- 构建所有 `%xxx`、`func`、keywordless function、旧 property、old artifact 使用清单。
- 构建所有 `$Type(...)`、`$(expr)(...)`、裸 type-name call、`new Struct(...)` 和 native prototype factory 使用清单。
- 为每类标 machine/manual/blocked。

晋级门：仓库所有 current source 和 fixture 都被分类；无“未知以后再看”。

### M2 Migration frontend + LSP fixes

- legacy parser adapter、semantic edit planner、JSON/text report、idempotence。
- LSP diagnostics/code action/workspace edit。

晋级门：每类 migration 都有 pass/ambiguous/blocked 测试；machine edit 后新 parser/typecheck 通过。

### M3 Repository dry run

- `--check` 运行全仓，不写文件。
- 审查 manual/blocked 清单。
- 重建 golden/binary 的命令和预期 diff 准备完成。

晋级门：blocked 项为零；manual 项全部有已确认改写。

### M4 Atomic cutover

- 应用源码 edits。
- 更新 spec/docs/snippets。
- 重建 `.zrs/.zri/.zro`。
- 正式 parser 切换 migration diagnostics。
- 删除旧 lowering/runtime emission。

晋级门：parser -> semantic -> VM/AOT -> artifact -> CLI -> LSP 全矩阵通过。

### M5 Cleanup

- 删除 migration frontend 以外的 legacy parser helper。
- 删除旧 AST fields/enums/opcodes/formatters。
- 历史 plans 标 superseded。
- migration tool 保留一个明确支持周期后再评估移除。

晋级门：shared production path 无旧 spelling/owner-kind branch；allowlist 只剩有意保留项。

## 12. 测试矩阵

### Migration tool

- 每个 `%xxx` 单项、嵌套和组合。
- missing semicolon at newline/`}`/EOF、multiline expression、braced declaration optional single trailing semicolon、拒绝`}`与`else/catch/finally`之间的`;`和formatter normalization。
- `%import` expression到module-scope `let alias = import("literal");`；standalone alias推导冲突、dynamic/local/conditional import拒绝。
- named/local/member/interface/async/anonymous definition与native extern block declaration的return delimiter迁移；callable TypeRef arrow不误改。
- 返回 callable、嵌套 callable、definition/type 半输入和 `:`/`->`/`=>` 错位诊断。
- comments/strings/modulo 不误改。
- `$StaticType(...)` machine edit；qualified/generic/alias TypeRef；runtime `$proto(...)` requiresReview；`$(expr)(...)` 按 reflection contract 分类为 requiresReview/blocked。
- constructor/`@call` 同时存在、type/value shadowing、import alias 和 unresolved target 不误改。
- overload/import/generic/dynamic call-site markers。
- `%type` type/expression/ambiguous。
- borrow/loan escaping/blocked。
- using resource/pattern/plugin/field。
- idempotence、overlapping edits、stale hash、partial failure。

### LSP

- hover/signature/completion/semantic token/inlay。
- declaration hover 使用 `:`、value type hover 使用 `->`、anonymous expression body 使用 `=>`。
- `init` constructor signature 与普通 `@call` signature 严格分离。
- migration diagnostic/fix/code action。
- cross-file workspace edit。
- moved/borrowed/readonly/resource/refLike display。
- incremental edit/revision invalidation。
- `zr.`/`native:`/`file:`/`#alias`/`@package` completion分域，hover分开显示ModuleIdentity与provider locator。

### Docs/fixtures/artifacts

- current code block compile。
- expected negative diagnostics。
- all current `.zr` migration scan clean。
- `.zrs/.zri/.zro` golden/roundtrip/version rejection。
- source and binary import contract parity。
- import binding的ModuleId、ModuleNamespace object identity、type namespace和artifact dependency roundtrip一致。
- RegisteredNative与Workspace同segments可同时导入；同domain duplicate provider失败；alias保留target domain；`file:` locator path不进入TypeId或可发布golden。

### Full project

- parser/type/semantic/compiler/core/AOT/library/CLI/LSP suites。
- `tests/fixtures/reference/core_semantics` 全矩阵。
- `lsp_language_feature_matrix` 和 `lsp_ownership` 项目。
- benchmarks result parity/performance guardrails。
- inline struct construction fixtures 在 VM/AOT 下无新增 prototype dispatch、boxing 或临时复制。

### Stress

- 大项目跨文件 call-site migration。
- 数万 edits 的排序、冲突和内存。
- 多次 dry-run/write/check idempotence。
- LSP workspace edit 中途文件变化。
- 大量旧 artifact cache 的清晰拒绝和重建。
- 100,000条simple statement的single-line/pretty双输入产生相同AST/semantic hash；newline/comment/string边界不触发ASI或错误minify。

## 13. 参考依据

- Rust edition/diagnostic migration 模式：`lua/rust/compiler/rustc_lint/src/macro_expr_fragment_specifier_2024_migration.rs`、`lua/rust/tests/ui/closures/2229_closure_analysis/migrations` 中的 `.rs/.fixed/.stderr` 组合。
- Roslyn code fix 服务与测试：`lua/roslyn/src/Features/Core/Portable/CodeFixes/Service/CodeFixService.cs`、`lua/roslyn/src/EditorFeatures/Test/CodeFixes/CodeFixServiceTests.cs`。
- C# breaking-change/feature proposals：`lua/csharplang/proposals`，用于区分语言切换与历史兼容成本。
- callable syntax migration 对照：`lua/csharplang/proposals/csharp-9.0/function-pointers.md`、`lua/roslyn/src/Compilers/CSharp/Test/Syntax/Parsing/FunctionPointerTests.cs`、`lua/rust/compiler/rustc_parse/src/parser/ty.rs`、`lua/cpython/Grammar/python.gram`。
- 显式semicolon和missing-terminator recovery：`lua/jdk/src/jdk.compiler/share/classes/com/sun/tools/javac/parser/JavacParser.java`、`lua/rust/compiler/rustc_parse/src/parser/diagnostics.rs`、`lua/rust/tests/ui/parser/missing-semicolon.rs`。QuickJS的ASI对照位于`lua/QuickJS-master/quickjs.c::js_parse_expect_semi`和`lua/QuickJS-master/tests/test_language.js::test_parse_semicolon`；ZR明确不采用。
- module object/namespace：`lua/cpython/Python/import.c::PyImport_ImportModuleLevelObject`、`lua/cpython/Lib/test/test_import`、`lua/QuickJS-master/quickjs.c`的`JS_CLASS_MODULE_NS`，以及仓库既有`.codex/plans/%import Reserved Syntax Migration Plan.md`专用ImportExpression基础。
- ZR current semantic/LSP facts：`zr_vm_parser/include/zr_vm_parser/semantic_facts.h`、`zr_vm_language_server/src/zr_vm_language_server/interface/lsp_completion_semantic_facts.c`、`semantic/lsp_semantic_tokens.c`。
- ZR artifact/golden：`zr_vm_parser/include/zr_vm_parser/writer.h`、`tests/golden/ast`、`tests/fixtures/projects/lsp_language_feature_matrix`。

ZR 的刻意差异是：不引入多年 edition 双轨；保留一个结构化 migration frontend 和诊断周期，但正式 compiler 在 cutover 后只有一套 Canonical Type/SemIR 语义。
