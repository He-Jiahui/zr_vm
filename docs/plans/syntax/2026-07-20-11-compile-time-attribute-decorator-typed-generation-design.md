# 11 编译期执行、静态元数据与类型化声明生成实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task.

**Goal:** 复用 `fn`、`struct`、`comptime`、`if`、`#...#` 和 `init TypeRef(...)`，提供确定、可缓存的条件编译、编译期检查、静态attribute与类型化声明生成，不增加独立macro语言。

**Architecture:** attribute是带registered metadata role的readonly struct；declaration transform是带registered role的普通`comptime fn`。`zr.compile`/`zr.compile.declaration`由N3 CompileTool native host descriptor提供，compiler只接受immutable declaration view和结构化Patch数据；没有runtime module或第二套ZR标准库实现。

**Tech Stack:** ZR parser、Canonical TypeRef/SymbolId、Semantic IR、TypeLayout、`.zrp/.zri/.zro`、compiler sandbox、incremental cache、LSP、Unity/CMake tests。

---

> 状态：按Occam原则修订；类型化声明生成边界已确认。第一版 M4 已完成 typed
> `CompileDiagnostic[]`、canonical `interfaceAdds: TypeId[]`、schema-checked
> `attributeAdds: AttributeData[]` 与 `GeneratedField` provenance/retention；
> generated source map 已投影到 `.zri`，runtime decorator path 已删除。当前
> GeneratedField/interfaceAdds/
> attributeAdds 已共享同一提交边界，并覆盖分配失败后的完整可见状态回滚，
> M5 已接入 v2 `buildDependencies` 的独立 manifest/规范 writer/CompileTool
> phase lock graph 基础。compiler-owned artifact resolver 现只接受 CompileTool
> lock/ZRM，从同一份 compiler-owned bytes 校验 package/version/public contract、
> 实际条目与整包 SHA-256，并把规范 CompileTool lock-section SHA-256 交给
> comptime cache v4；cache entry 比较完整 32-byte digest，不再压缩为 64-bit FNV。
> runtime dependency graph 不接收该 provider。外部 provider 的 import/执行激活、
> 实际传递 provider 图验证、persistent incremental cache、formatter 与其余
> consumers 仍未完成，不能据此
> 提升整门 Gate。
>
> 上游契约：[01 Canonical TypeRef、Place、CFG与artifact](./2026-07-18-01-canonical-type-place-cfg-artifact-design.md)、[03 `init TypeRef`与layout](./2026-07-18-03-struct-ref-struct-span-layout-design.md)、[05 property](./2026-07-18-05-property-unified-ast-design.md)、[06A migration inventory/frontend](./2026-07-18-06-percent-migration-lsp-fixtures-design.md)、[08 reflection metadata](./2026-07-19-08-reflection-library-type-system-design.md)、[10R module/package foundation](./2026-07-19-10-native-ffi-module-package-design.md)。本计划不依赖 06B 最终仓库切换。

## 1. Occam硬约束

第一版只保留不能由现有实体表达的能力：

1. 不新增 `attribute`、`decorator`、`macro`、`when` 等关键字。
2. 所有用户函数仍写成 `fn name(...): ReturnType`；compile transform也只是显式返回`Patch`的`comptime fn`。
3. 条件声明和条件语句统一写`comptime if`；不再提供重复的`#zr.compile.when(...)#`。
4. attribute schema是普通`readonly struct`，通过既有`#...#` metadata标记usage。
5. declaration transform通过既有`#...#`标记普通`comptime fn`，不增加特殊function declaration。
6. 生成type/field/method/property和受限新body时，只构造typed data；没有token、AST pointer、raw identifier lookup、源码输出或fluent builder函数。
7. 任何新增公开类型、constructor、函数或metadata role必须分别进入第10节reference ledger，同时给出必要性、至少一个仓库内implementation路径和一个独立behavior/compiler test路径；关键机制尽量用第二种语言交叉验证。没有来源的实体从第一版删除。

此前确认的“compiler提供typed builder”在此收敛为一组compiler-owned readonly view与`Patch`/`GeneratedDeclaration`数据schema：用户用已确认的`init TypeRef(...)`构造它们。它仍然只能生成类型、字段、方法、属性和受限新body，但不再额外提供需要记忆和维护reference的fluent builder函数。

### 1.1 N3 CompileTool native owner

`zr.compile`与`zr.compile.declaration`属于第10章OfficialNative domain的N3 CompileTool native module。BuildFacts/diagnostic functions、Conditional/DeclarationTransform roles、declaration view和Patch schema都由compiler host descriptor生成Canonical TypeId/CallableContract；普通runtime native registry不物化这些module。`AttributeUsage`schema/role的唯一owner仍是OfficialNative N2 Runtime模块`zr.reflection`，compiler只消费其registered metadata contract。

CompileToolNamespace仍通过普通`import(...)`形成；其OfficialNative ModuleIdentity不携带phase，descriptor/resolution record与binding的providerPhase固定为CompileTool。Runtime/Test expression使用这些binding时报phase mismatch。禁止新增`zr.macro`、`zr.codegen`、`zr.attribute`或runtime版本的`zr.compile`。

第一版明确删除此前草案中的：`comptime let`、`#when#`、`attribute ...` declaration、`decorator fn`、`compile.note`、runtime decorator、任意AST/body rewrite和多轮expansion。

## 2. 唯一编译期表层

### 2.1 `comptime fn`、`comptime { }`、`comptime if`

```zr
let compile = import("zr.compile");

pub comptime fn tableSize(count: usize): usize {
    return count.nextPowerOfTwo();
}

const vertexStride: usize = sizeOf<Vertex>();

comptime if (compile.build.feature("simd")) {
    fn transform(values: Span<float>): void {
        simdTransform(values);
    }
} else {
    fn transform(values: Span<float>): void {
        scalarTransform(values);
    }
}

comptime {
    compile.assert(vertexStride == 32, "Vertex layout changed");
    if (alignOf<Vertex>() < 16) {
        compile.warning("Vertex is not SIMD aligned", target: typeid(Vertex));
    }
}
```

- `comptime fn`实参必须在调用phase可求值，结果成为typed constant/compile object；它不能转换成runtime callable，也不生成隐藏runtime版本。
- module constant继续使用既有`const`。compiler-only临时值使用comptime function/block内的普通`let`，不增加`comptime let`。
- module-scope `comptime { }`只执行检查和diagnostic，不能生成declaration。
- `comptime if`可出现在declaration list或statement list。所有分支先parse，只有active branch进入name/type/borrow/lowering。
- predicate只能读取declared build feature和canonical target facts；源码不能直接读取environment、clock、random或filesystem。
- feature必须在`.zrp`声明；未知名称是error，不按false处理。

建议manifest继续使用一个build区段：

```json
{
  "build": {
    "features": ["trace", "simd"],
    "profiles": {
      "debug": { "enableFeatures": ["trace"] },
      "release": { "enableFeatures": [] }
    }
  }
}
```

### 2.2 C# Conditional风格调用消除

```zr
#zr.compile.conditional("trace")#
fn trace(message: string, value: int): void {
    console.writeLine(message, value);
}

fn update(): void {
    trace("value", expensiveValue());
}
```

禁用`trace`时，bound call和全部argument lowering一起删除。调用仍完成name lookup、overload、access和type check。

限制保持最小且可证明：

- function必须直接静态绑定并返回`void`；
- 禁止virtual/interface/override、callable conversion、dynamic invoke、ref/out参数和ref return；
- condition在caller build求值，跨模块artifact保存canonical predicate；
- function body始终编译；要删除declaration本身使用`comptime if`。

## 3. attribute不需要新关键字

```zr
let reflection = import("zr.reflection");

#zr.reflection.attributeUsage(
    targets: reflection.AttributeTargets.field
        | reflection.AttributeTargets.property
        | reflection.AttributeTargets.parameter,
    retention: reflection.AttributeRetention.runtime,
    repeatable: false,
    inherited: false
)#
pub readonly struct Range {
    pub let min: int;
    pub let max: int;
}

class Player {
    #Range(min: 0, max: 100)#
    pri var _health: int = 100;
}
```

规则：

- 带`AttributeUsage` role的readonly struct才是attribute schema；普通struct不能用于`#Type(...)#`。
- public `let` fields按声明顺序形成argument schema；named arguments使用field name。第一版不增加attribute constructor/property setter规则。
- field type只允许compile-constant-safe形态：bool、number、string、enum、TypeId、固定tuple/array及其明确nullable组合。
- application规范化为`AttributeData { typeId, fieldValues, sourceRange }`。
- retention为`source | artifact | runtime`；runtime隐含artifact。runtime reflection只看到retained data，不实例化attribute object。
- targets/repeatability/inheritance由registered role解释，不按`Range`或`attributeUsage`源码字符串特判。

## 4. declaration transform仍是普通函数

```zr
let declaration = import("zr.compile.declaration");

#zr.compile.declarationTransform#
pub comptime fn deriveMarker(
    target: declaration.Struct
): declaration.Patch {
    let marker = init declaration.GeneratedField(
        name: "_generatedEquality",
        type: typeid(bool),
        visibility: declaration.Visibility.private,
        mutability: declaration.Mutability.let,
        initializer: init declaration.ConstantValue(boolValue: true)
    );
    return init declaration.Patch(
        target: target.symbolId,
        additions: [marker]
    );
}
```

示例只使用`init`构造完整typed data；不引入`addMethod/body/finish/mapFields`等无reference helper。正式fixture可以把同一schema扩展到GeneratedMethod/Property，但每个公开variant/operation都必须先通过第10节reference gate。

### 4.1 immutable view

```text
DeclarationView {
  symbolId;
  kind;
  name;
  ownerSymbolId?;
  visibility;
  sourceRange;
  attributes[];
}

TypeView : DeclarationView {
  typeId;
  capabilities;
  fields[];
  methods[];
  properties[];
  interfaces[];
}
```

view不暴露已有method body、writable AST、token/source buffer、compiler pointer、runtime object或native pointer。

### 4.2 Patch data

```text
Patch {
  target: SymbolId;
  additions: GeneratedDeclaration[];
  interfaceAdds: TypeId[];
  attributeAdds: AttributeData[];
  diagnostics: CompileDiagnostic[];
}

GeneratedDeclaration = GeneratedField

GeneratedField {
  name: string;
  type: TypeId;
  visibility: Visibility;
  mutability: Mutability;
  initializer?: ConstantValue;
}
```

该variant对应已有declaration种类，不创造第二套语义。generated declaration随后走普通binder、access、CFG、borrow、layout、Drop和artifact流程。

第一版只发布通过第10节reference gate并有生产实现/独立测试的`GeneratedField`。`GeneratedType`、`GeneratedMethod`和`GeneratedProperty`不是第一版公开API；以后只有在各自通过同一reference gate后，才允许扩展`GeneratedDeclaration` union。未发布variant的缺席不是第一版M4阻断项。

允许新增declaration、interface implementation和metadata。禁止remove/rename/replace existing symbol、修改已有field/type/order/visibility/body、注入native/intermediate、生成comptime transform或再次触发transform。

新body只保存typed operation union：literal、local、Place read/write、resolved SymbolRef call、structured branch/loop、return、throw、try/finally。它不保存raw identifier/token/source；所有operation仍由普通semantic checker验证。

## 5. 单轮phase与cycle

```text
BuildFacts < Signature < Expansion < Layout < LateCheck
```

固定顺序：

1. parse所有module；
2. resolve ModuleIdentity/build facts，执行`comptime if`；
3. bind手写signature、AttributeUsage和DeclarationTransform roles；
4. 按ModuleIdentity/source order执行一次transform；
5. 合并Patch、分配generated SymbolId并rebind；
6. 计算layout/CFG/borrow/effect；
7. 执行late `comptime { }` checks；
8. conditional call elision后进入lowering。

每个compile intrinsic/view field声明minimum phase。transform不能读取可能被自己改变的final layout；late check可以读取layout但不能生成Patch。依赖回边统一报`comptime.phase_cycle`并列出chain。

generated declaration的普通attribute可以retained，但DeclarationTransform role不会再次执行。第一版只有一个round。

## 6. sandbox、预算与诊断

`ComptimeEffect = PureValue | Diagnostic | DeclarationBuild`：

- build predicate/attribute field value只允许PureValue；
- `comptime { }`允许PureValue+Diagnostic；
- declaration transform允许三者，但DeclarationBuild只能写当前Patch。

禁止IO、network、process、environment、clock、random、native FFI、runtime GC identity和未声明host state。每次evaluation限制fuel、call depth、heap bytes、aggregate count、generated declaration count和diagnostic count；超限报告使用量与上限。

`zr.compile`第一版native descriptor只保留以下函数；代码是生成的interface projection，不是ZR实现源码：

```zr
fn feature(name: string): bool; // zr.compile.build.feature
fn assert(condition: bool, message: string, target: SymbolId? = null): void; // zr.compile.assert
fn error(message: string, target: SymbolId? = null): void; // zr.compile.error
fn warning(message: string, target: SymbolId? = null): void; // zr.compile.warning
```

`error`阻止build；不再增加FatalError/Info/Note同义层级。warning升降级由diagnostic id和project policy处理。

## 7. Artifact、module与LSP

- compile-only import仍写`let tool = import("@derive");`；resolver绑定`CompileToolNamespace`，只在comptime context可用，不进入runtime graph。
- `zr.compile`与`zr.compile.declaration`host descriptor是TypeId/CallableContract/role的唯一真源；generated interface/LSP virtual document只读投影。
- exported transform executable进入compile-tool section；runtime loader不映射。
- `.zrs`保存source syntax；`.zri`保存condition decision、expansion provenance、generated source map与cache key；`.zro`保存最终public generated declarations、AttributeData和conditional predicate。
- runtime reflection只看到最终member和runtime-retained AttributeData，不看到handler/Patch/compile heap。
- LSP显示inactive branch、attribute schema、transform origin和read-only generated virtual document；LSP不执行transform。

cache key至少包含handler artifact hash、compiler schema、arguments、target observable declaration facts和build facts。clean/incremental输出必须byte-identical。

## 8. 迁移

| 旧形态 | 目标 | 处置 |
|---|---|---|
| `%compileTime fn` | `comptime fn` | machine edit |
| `%compileTime { }` | `comptime { }` | machine edit |
| `%compileTime var` | `const`或comptime block local | 按是否进入runtime constant决定 |
| compile-time decorator class/`@decorate` | `#declarationTransform# comptime fn ...: Patch` | requiresReview |
| runtime decorator | retained attribute data或普通显式runtime call | 禁止隐藏module-init mutation |
| object/string patch | typed Patch data | requiresReview；已有body mutation blocked |

正式parser/writer不保留旧双命名空间和runtime decorator fallback。

## 9. 分层里程碑与验收

### M1 build facts/comptime if

登记`zr.compile`CompileTool descriptor，实施declared features、closed predicate、declaration/statement pruning、unknown feature和public contract hash。

### M2 typed comptime/check

拆分typed evaluator、effects、budget、deterministic cache和四个diagnostic APIs。

### M3 AttributeUsage与Conditional

实现readonly struct schema、AttributeData retention/targets/repeatability，以及direct void conditional call elision。

### M4 DeclarationTransform与Patch

登记`zr.compile.declaration`descriptor，实施immutable views、typed Patch schema、single round、第一版`GeneratedField` rebind/source map；任何后续generated declaration variant或operation API必须先通过第10节reference gate。

### M5 consumers/migration

接通artifact、reflection、LSP、formatter、buildDependencies和reference fixture；删除runtime decorator path。

晋级至少证明：

- parser没有新增attribute/decorator/macro/when keyword；
- `zr.compile`/`zr.compile.declaration`只有N3 CompileTool native owner；Runtime/Test provider不能注册同名module或复制TypeDef。
- host descriptor、artifact、LSP virtual document和reflection-visible最终member共享同一TypeId/role schema。
- 所有transform都是普通`comptime fn(...): Patch`；
- shared path没有concrete attribute/transform type-name dispatch；
- conditional disabled call没有argument SemIR；
- generated declarations全部经过普通semantic/layout；
- sandbox limit、phase cycle、name/layout collision、10,000 additions和incremental invalidation有测试；
- runtime不执行transform。

## 10. Public API reference ledger

任何未列在本表的公开type/constructor/function/metadata role都不属于第一版。表内路径必须在当前仓库真实存在；实施时由测试再次验证，不允许用类名、目录名或`lua/...`代替文件路径。

| ZR surface | 必要性 | reference implementation | reference tests |
|---|---|---|---|
| `#zr.reflection.attributeUsage#` | 不增加keyword地声明targets/retention | `lua/runtime/src/libraries/System.Private.CoreLib/src/System/AttributeUsageAttribute.cs`；`lua/csharplang/spec/attributes.md` | `lua/roslyn/src/Compilers/CSharp/Test/Emit3/Attributes/AttributeTests.cs` |
| `#zr.compile.declarationTransform#` | 把普通comptime fn注册为typed generator | `lua/roslyn/src/Compilers/Core/Portable/SourceGeneration/IIncrementalGenerator.cs`；`lua/jdk/src/jdk.compiler/share/classes/com/sun/tools/javac/processing/JavacProcessingEnvironment.java` | `lua/roslyn/src/Compilers/CSharp/Test/Semantic/SourceGeneration/GeneratorDriverTests.cs`；`lua/jdk/test/langtools/tools/javac/processing/GenerateAndErrorTest.java` |
| typed `Patch` / `GeneratedDeclaration` data | 生成声明但不开放token/AST mutation | `lua/roslyn/src/Compilers/CSharp/Portable/Syntax/SyntaxFactory.cs`；`lua/jdk/src/jdk.compiler/share/classes/com/sun/tools/javac/processing/JavacFiler.java` | `lua/roslyn/src/Compilers/CSharp/Test/Semantic/SourceGeneration/GeneratorDriverTests.cs`；`lua/jdk/test/langtools/tools/javac/processing/filer/TestFilerConstraints.java` |
| `CompileDiagnostic(isError: bool, message: string, target: SymbolId)` typed constructor | declaration transform通过Patch产生结构化warning/error，不开放字符串日志通道或可写source位置 | `lua/jdk/src/jdk.compiler/share/classes/com/sun/tools/javac/processing/JavacMessager.java`；`lua/roslyn/src/Compilers/Core/Portable/SourceGeneration/GeneratorContexts.cs`；`lua/roslyn/src/Compilers/Core/Portable/Diagnostic/Diagnostic.cs` | `lua/jdk/test/langtools/tools/javac/processing/messager/MessagerBasics.java`；`lua/roslyn/src/Compilers/CSharp/Test/Semantic/SourceGeneration/GeneratorDriverTests.cs` |
| `#zr.compile.conditional#` | 无runtime branch地删除call和arguments | `lua/runtime/src/libraries/System.Private.CoreLib/src/System/Diagnostics/ConditionalAttribute.cs` | `lua/roslyn/src/Compilers/CSharp/Test/Emit3/Attributes/AttributeTests_Conditional.cs`；`lua/roslyn/src/Compilers/CSharp/Test/Emit/CodeGen/CodeGenLocalFunctionTests.cs` |
| `compile.build.feature(name): bool` | 单一build predicate来源 | `lua/rust/compiler/rustc_attr_parsing/src/attributes/cfg.rs` | `lua/rust/tests/ui/cfg/cfg_stmt_expr.rs`；`lua/rust/tests/ui/cfg/cfg-false-feature.rs` |
| `compile.assert(...): void` | 编译期不变量失败 | `lua/rust/compiler/rustc_builtin_macros/src/compile_error.rs`；`lua/jdk/src/jdk.compiler/share/classes/com/sun/tools/javac/processing/JavacMessager.java` | `lua/rust/tests/ui/macros/compile_error_macro.rs`；`lua/jdk/test/langtools/tools/javac/processing/messager/MessagerBasics.java` |
| `compile.error/warning(...): void` | 产生结构化diagnostic | `lua/jdk/src/jdk.compiler/share/classes/com/sun/tools/javac/processing/JavacMessager.java`；`lua/roslyn/src/Compilers/Core/Portable/Diagnostic/DiagnosticDescriptor.cs` | `lua/jdk/test/langtools/tools/javac/processing/messager/MessagerDiags.java`；`lua/roslyn/src/Compilers/CSharp/Test/Emit3/Diagnostics/DiagnosticAnalyzerTests.cs` |
| `sizeOf<T>/alignOf<T>` | layout compile check | `lua/rust/library/core/src/mem/mod.rs` | `lua/rust/tests/ui/consts/const-size_of-align_of.rs`；`lua/rust/tests/ui/consts/const-size_of_val-align_of_val.rs` |

刻意差异：ZR不复制Rust token macro、Roslyn arbitrary source text或JDK多round filer；只保留typed view、diagnostic和append-only declaration generation的共同核心。`CompileDiagnostic`第一版只有`isError/message/target`，target必须是当前Patch的canonical SymbolId；不公开任意diagnostic id/category/location、source text或host logger。
