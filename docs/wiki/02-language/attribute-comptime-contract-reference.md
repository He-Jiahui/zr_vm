---
related_code:
  - zr_vm_parser/include/zr_vm_parser/ast.h
  - zr_vm_parser/include/zr_vm_parser/compile_tool.h
  - zr_vm_parser/include/zr_vm_parser/attribute_contract.h
  - zr_vm_parser/include/zr_vm_parser/declaration_transform_contract.h
  - zr_vm_parser/src/zr_vm_parser/parser/parser_extern.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_attribute_binding.c
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/parser/parser_extern.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_attribute_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/comptime_runtime_contract.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_declaration_patch_attributes.c
plan_sources:
  - user: 2026-09-10 继续细化 Wiki 的语言规则、用例与 C 接口说明
  - docs/plans/syntax/2026-07-20-11-compile-time-attribute-decorator-typed-generation-design.md
  - docs/plans/syntax/12-async-task-job-scheduler/m6-4-lsp-artifact-projection-and-workspace-migration.md
tests:
  - tests/compileTime/test_compile_time_execution.c
  - tests/compileTime/test_declaration_transform_contract.c
  - tests/parser/test_syntax_reference_v1.c
  - tests/fixtures/projects/syntax_reference_v1/src/compile_time_and_attributes.zr
doc_type: language-reference
---

# Attribute 与 Comptime 契约参考

ZR 的 `#...#` decorator 和 `comptime` 语句能影响 declaration、编译期选择、artifact metadata
和测试/FFI/provider contract，但它们不是“在编译器里随意执行一段 ZR 代码”的逃生通道。
每一个 attribute 都要经过 schema/role/target/retention 验证；每一段 comptime 代码都有阶段、
输入、effect、预算与可重现性边界。本页给出 source grammar、执行模型和 C 工具接口。

已有概览页[Attribute 与编译期](attributes-comptime.md)说明常见 role；本页聚焦契约如何从 AST
变成可验证的编译期操作。

## 语法表面

```ebnf
decorator             = "#", expression, "#" ;
decorated-declaration = { decorator }, declaration ;

comptime-declaration  = "comptime", (
                          function-declaration
                        | if-statement
                        | block
                        | expression
                        ) ;
```

```zr
#zr.reflection.attributeUsage(
    targets: zr.reflection.AttributeTargets.type,
    retention: zr.reflection.AttributeRetention.artifact,
    repeatable: false,
    inherited: false
)#
pub readonly struct Label {
    pub let value: int;
}

comptime if (featureEnabled) {
    fn selected(): int { return 1; }
} else {
    fn selected(): int { return 0; }
}
```

parser 将 decorator 表示为 `ZR_AST_DECORATOR_EXPRESSION`，随后把它附到 declaration/member
的 decorator array；它不会仅看 `zr.reflection` 或 `zr.ffi` 字符串就执行任何 role。`comptime`
形成 `ZR_AST_COMPILE_TIME_DECLARATION`，保存 declaration type、inner AST、是否为 conditional
pruning，以及稍后填充的 selected-branch/build-facts 状态。

当前 parser 明确拒绝以下旧表面：

```zr
// 已移除：不是当前 comptime declaration surface。
comptime let oldValue = 1;
comptime var oldValue = 1;
comptime class OldDecorator {}
comptime struct OldDecorator {}
```

应使用普通 immutable data、module-scope `comptime { ... }`，或 role-marked ordinary
`comptime fn(...): DeclarationPatch` transform。Parser 接受的 expression form 仍要由所处
module/phase 的 evaluator 决定能否执行。

## attribute 的四个契约维度

一个可用 attribute 不只由名称决定。binding 至少要匹配以下维度：

| 维度 | 问题 | 例子 |
| --- | --- | --- |
| role/schema | 名称对应哪个已注册 contract，参数 shape 是否正确？ | `zr.ffi.entry` 需要 entry point 参数。 |
| target | 它能修饰 type、field、function、parameter 还是 module？ | `attributeUsage` 修饰 attribute type，而非任意表达式。 |
| retention/phase | 只在 compile、测试、artifact，还是运行时仍保留？ | reflection metadata 可保留至 artifact。 |
| effect/provider | 执行它是否需要 compile tool、FFI、testing provider 或 capability？ | declaration transform 必须在 compile phase。 |

这也是为什么 decorator 在语法上可以是 expression、但 unknown/位置错误的 decorator 仍会在
binding 阶段失败。attribute name 可解析不表示 host 已安装其 provider。

### 常见已覆盖的 role 类别

| 类别 | 源码例子 | 绑定/产物效果 |
| --- | --- | --- |
| reflection metadata | `#zr.reflection.attributeUsage(...)#` | 验证可用目标/retention，投影到 reflection/artifact metadata。 |
| declaration transform | `#zr.compile.declarationTransform#` | 识别 comptime transform 的严格签名和 patch contract。 |
| transform application | `#deriveSyntaxReference#` | 对目标 declaration 执行并提交已验证的 patch。 |
| conditional call | `#zr.compile.conditional("trace")#` | feature 不满足时按 contract 删除已绑定 call lowering。 |
| FFI ABI metadata | `#zr.ffi.entry(...)#`、`#zr.ffi.kind(...)#` | 输入 native import contract/layout validation。 |
| testing metadata | `#zr.testing.test#`、case/skip 类 role | 形成 test manifest/project test provider 数据。 |

上表是类别，而不是所有 descriptor 的枚举。自定义 role 必须由 provider 通过公共 contract
注册，且它的 public contract hash 要参与 artifact/lock 验证；不要用未声明 decorator 静默
存储私有字符串。

## `comptime if` 与分支选择

```zr
let compile = import("zr.compile");

comptime if (compile.build.feature("simd")) {
    fn dot(value: Vector4): f32 { return dotSimd(value); }
} else {
    fn dot(value: Vector4): f32 { return dotScalar(value); }
}
```

source parser 先建立完整 if AST，并将 outer declaration 标记为 conditional pruning。Compile
phase 用锁定的 build facts 选择分支，选择后的 declaration 才进入普通 binding/type/lowering。
这带来两个必须区分的结论：

1. 未选分支仍需要语法可恢复，不能包含无法分词、未闭合花括号或破坏 parser 恢复的文本。
2. 未选分支不应因为运行时名称未定义而产生“当前 build 的普通 unresolved symbol”错误；它不
   是当前被编译的 declaration graph。

feature、profile、package/lock、provider public contract、源/产物摘要等 inputs 必须来自可重现
的 build facts。不要让 `comptime if` 读取未锁定的墙上时间、随机数、环境变量或网络响应，以
免同一 source 在不同构建中产出同名却不兼容的 artifact。

## declaration transform：从 view 到 Patch

当前 fixture 的完整模式：

```zr
let declaration = import("zr.compile.declaration");

#zr.compile.declarationTransform#
pub comptime fn deriveLabel(target: declaration.Struct): declaration.Patch {
    let generated = init declaration.GeneratedField(
        name: "generated",
        type: typeid(bool),
        visibility: declaration.Visibility.public,
        mutability: declaration.Mutability.let
    );
    return init declaration.Patch(
        target: target.symbolId,
        additions: [generated]
    );
}

#deriveLabel#
pub struct GeneratedTarget {
    pub let value: int;
}
```

transform 的**来源函数**与**应用 decorator**是两件事：前者通过 role 标识为可执行 compile
tool callable，后者将该 callable 绑定到一个具体 target declaration。两者中任一方的
类型、phase、参数、return type 或 generic/async/effect 不符合 contract，都不应生成半成品
metadata。

一个可靠的 patch transaction 通常包含：

```text
resolve target and transform role
  -> construct read-only declaration view
  -> evaluate under compile tool scope and budget
  -> validate Patch schema, IDs, additions and attributes
  -> prepare detached symbol/layout/source-map updates
  -> commit all projections together or roll back all of them
  -> recompute artifact/public contract metadata
```

这保证了失败不会留下“字段数组增加了但 layout/symbol map 没更新”的中间状态。Patch 能否增加
哪些 declaration kind、可否覆盖已有 member、是否保留到 reflection，全部由当前
`declaration_transform_contract` 规则决定，不能由 user transform 随意写 raw compiler pointer。

## effect、预算与确定性

comptime evaluator 不是 runtime VM 的无界副本。不同 execution context 允许的 effect 不同，
典型可见边界如下：

| 上下文 | 应允许的工作 | 不应允许的工作 |
| --- | --- | --- |
| pure value/query | 常量、类型/声明查询、确定性计算 | 写宿主文件、网络、未锁定时钟。 |
| check/diagnostic | pure value + 定位的 diagnostic | 直接修改 declaration metadata。 |
| declaration transform | pure value、diagnostic、受控 Patch | 绕过 Patch validator 或保留 compiler-private object。 |
| runtime module | 正常运行时 effect | 直接调用 compile-only export。 |

实现会使用 execution scope/contract 限制 fuel、递归深度、heap/aggregate、generated
declaration 数量和 diagnostic 数量。预算耗尽、phase mismatch、递归 transform、hash/lock
不匹配都必须留下 diagnostic 并终止当前 transaction；“只提交已经算出的前半部分”会破坏
artifact 的一致性。

## C API：查询、验证与 artifact 依赖

| C 接口 | 用途 | 所有权/使用边界 |
| --- | --- | --- |
| `ZrParser_CompileTool_FindModule` | 查 compile provider descriptor | 返回 descriptor 通常为借用的静态/registry 数据。 |
| `ZrParser_CompileTool_FindCallable` / `FindType` | 查 role 对应 callable/type | 用结果前仍须按 phase/contract 验证。 |
| `ZrParser_CompileTool_FindMetadataRole` | 查 attribute metadata schema | 不能把未找到当成可忽略 decorator。 |
| `ZrParser_CompileTool_ComputePublicContractHash` | 计算 provider 公共契约摘要 | artifact/lock 对比使用，不能替代完整 validation。 |
| `ZrParser_AttributeContract_ResolveBuiltinDecorator` | 将 AST decorator 解析为 builtin schema | AST/source range 的 owner 必须仍有效。 |
| `ZrParser_AttributeContract_ValidateSchema` | 验证 role/schema 本身 | 用于 provider/工具启动或 admission。 |
| `ZrParser_AttributeContract_ValidateApplication` | 验证目标上的一次应用 | 需要 target/phase/argument 的完整上下文。 |
| `ZrParser_DeclarationPatch_Validate` | 校验 transform 返回的 Patch | 失败时禁止 commit。 |
| `ZrParser_DeclarationView_ValidatePhaseAccess` | 防止 view 跨 phase 访问 | view 是编译器借用对象，不可长期缓存。 |
| `ZrParser_CompileToolArtifact_OpenBuildDependency` / `Close` | 打开/关闭锁定的构建依赖 | 必须成对关闭，即使 evaluator 失败。 |

普通 native module callback 不应直接构造 declaration patch。它若需要参与编译期 provider，
应注册明确的 compile-tool/attribute contract；若只在运行时提供功能，使用 native registry
descriptor 和 `ZrLibCallContext`。两条路径的入口与生命周期不同。

## 错误诊断与迁移

| 症状 | 所属层 | 处理方式 |
| --- | --- | --- |
| `comptime let/var/class/struct` 被拒绝 | parser migration | 改为当前 block/function/transform surface。 |
| decorator 名称存在但 target 不允许 | attribute binding | 查 role schema 的 target/retention。 |
| transform 签名错误 | compile-tool binding | 修正参数 view、return Patch、async/generic/effect。 |
| patch 字段/TypeId/collision 不合法 | patch validation | 修正生成内容；不要重试提交。 |
| feature 未锁定或 provider hash 不同 | build admission | 更新 manifest/lock/artifact 的一致集合。 |
| runtime 调用 compile-only export | phase contract | 移到 comptime 或暴露独立 runtime API。 |
| cache 命中后行为不同 | determinism/input key | 检查 build facts、package/artifact/provider contract 摘要。 |

对编译期生成后的源码/metadata 进行调试时，应优先查看 compiler 产出的诊断和 artifact
projection，而不是假定原始 AST 已经被原地改写。更宽的产物格式和调试入口见[产物与二进制](../06-reference/artifacts.md)与[工具链索引](../04-tools/index.md)。
