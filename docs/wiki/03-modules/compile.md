---
related_code:
  - zr_vm_parser/include/zr_vm_parser/compile_tool.h
  - zr_vm_parser/include/zr_vm_parser/comptime_contract.h
  - zr_vm_parser/include/zr_vm_parser/comptime_cache.h
  - zr_vm_parser/include/zr_vm_parser/compiler.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_descriptor.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_evaluator.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_binding.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_artifact.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_executor.c
  - zr_vm_parser/src/zr_vm_parser/comptime_contract.c
  - zr_vm_library/include/zr_vm_library/project.h
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_descriptor.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_evaluator.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_execution_scope.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_tool_project_provider.c
  - zr_vm_parser/src/zr_vm_parser/compiler/compile_time_executor.c
  - zr_vm_parser/src/zr_vm_parser/comptime_contract.c
plan_sources:
  - user: 2026-09-09 在 docs/wiki 构建完整 ZrVm 说明书
  - docs/parser-and-semantics/compile-time-typed-generation.md
  - docs/plans/syntax/2026-07-20-11-compile-time-attribute-decorator-typed-generation-design.md
tests:
  - tests/compileTime/test_comptime_contract.c
  - tests/compileTime/test_comptime_runtime_contract.c
  - tests/compileTime/test_compile_time_execution.c
  - tests/parser/test_compile_tool_project_import.c
  - tests/acceptance/2026-07-29-syntax-11-m1-build-facts-comptime-if.md
  - tests/acceptance/2026-08-02-syntax-11-m5-compile-tool-artifact-resolution.md
doc_type: module-detail
---

# `zr.compile`

**状态：`current`；CompileTool N3 provider，public contract
`zr.compile/v2`，hash `fnv1a64:ca60a1b2107c893b`。**

`zr.compile` 是编译器专用模块。它只在 compiler-owned compile-time execution scope
中可见，不会进入普通 runtime module，也不能由运行时 `import` 取得。模块把 build
feature、编译期诊断和 conditional call-elision 收敛成 descriptor 驱动的四个 callable；
parser 不再为每个功能增加一个关键字。

## 导入与源语法

```zr
let compile = import("zr.compile");

comptime if (compile.build.feature("simd")) {
    fn transform(values: Span<float>): void { simdTransform(values); }
} else {
    fn transform(values: Span<float>): void { scalarTransform(values); }
}

comptime {
    compile.assert(sizeOf<int>() == 4, "this module requires 32-bit int");
    compile.warning("slow fallback selected", target: typeid(transform));
}
```

`comptime if` 的所有分支都会先解析，只有 predicate 为真的分支进入 name binding、
类型推断和 lowering；因此未选分支仍必须是语法可恢复的 AST，但其中的 runtime 名称
不会造成未定义引用。`comptime {}` 只允许在 module scope，作用是求值和诊断，不直接
产生 runtime declaration。`comptime fn` 的结果必须在当前阶段可求值，不能自动生成
同名 runtime callable。

## 导出 callable

| 导出 | 源级签名 | effect | 最早阶段 | 语义 |
| --- | --- | --- | --- | --- |
| `compile.build.feature` | `feature(name: string): bool` | `PureValue` | `BuildFacts` | 查询 `.zrp` 当前 profile 中声明的 feature。未知 feature 是错误，不按 false 处理。 |
| `compile.assert` | `assert(condition: bool, message: string, target: SymbolId = null): void` | `Diagnostic` | `LateCheck` | 条件为 false 时发出 error；`target` 可把诊断绑定到 symbol。 |
| `compile.error` | `error(message: string, target: SymbolId = null): void` | `Diagnostic` | `LateCheck` | 无条件发出阻止编译完成的 error。 |
| `compile.warning` | `warning(message: string, target: SymbolId = null): void` | `Diagnostic` | `LateCheck` | 发出非致命 warning，不改变 Patch 的成功状态。 |

descriptor 中的 `SymbolId?` 是“可省略/可空”的接口记法；源码通过默认值
`target: SymbolId = null` 表达同一行为。每个 callable 还带 parameter count、return
type、effect 和 minimum phase，evaluator 按 role 查找，绝不按函数名字符串分派。

## Conditional attribute

```zr
#zr.compile.conditional("trace")#
fn trace(message: string): void {
    console.printLine(message);
}

fn update(): void {
    trace("tick");
}
```

`zr.compile.conditional` 是 `zr.compile` descriptor 声明的 metadata role。predicate 在
caller build profile 中求值；关闭时删除 bound call 和全部 argument lowering，但仍完成
name lookup、访问检查、重载选择和类型检查。该 role 只接受直接静态绑定、`void` 返回、
无 virtual/interface/override、无 ref/out 参数和无 callable/dynamic conversion 的函数。
函数 body 仍会编译；要删除整个 declaration 必须使用 `comptime if`。

## 阶段、effect 与隔离

编译阶段顺序为 `BuildFacts -> Signature -> Expansion -> Layout -> LateCheck`。effect
策略如下：

| context | 允许的 effect |
| --- | --- |
| `PURE_VALUE` | 仅 `PureValue` |
| `CHECK` | `PureValue`、`Diagnostic` |
| `DECLARATION_TRANSFORM` | 三种 effect 全部允许 |

每次 evaluator 调用都进入 `CompileToolExecutionScope`：provider binding、导入别名、
临时对象和 private helper 在 scope 结束时恢复；runtime 代码尝试读取 compile-only
binding 会得到 `compiletool.phase_mismatch`。编译期 predicate 不能读取时钟、随机数、
环境或文件系统；外部输入只能通过锁定的 build dependency artifact 进入。

## 预算与缓存

`SZrParserComptimeBudgetLimits` 对以下资源设置上限：fuel、call depth、heap bytes、
aggregate count、generated declaration count、diagnostic count。消费失败不会部分增加
usage，并报告 `comptime.budget_exceeded`。缓存 key 包含 provider/module、source digest、
调用位置、context、typed arguments、resolved version、package hash、lock graph hash、
artifact entry/hash 和 public contract；同长度源码改动也会失效。快照以固定宽度、摘要
排序格式导出，导入前验证 magic、大小、SHA-256 和顺序，失败时保留旧 cache。

## CompileTool 依赖与 `.zrm`

`.zrp` 的 `buildDependencies` 与 runtime `dependencies` 分离。resolver 只选择
`providerPhase=CompileTool` 的 lock entry，验证 package/version/public contract、完整
package SHA-256、`.zrm` entry hash 和 module key，然后把归档字节交给 compiler-owned
provider。它不会把 compile tool 注入 runtime dependency graph；当前 transitive provider
import 在未提升的边界上 fail-closed。

## C 接口

```c
const SZrParserCompileToolModuleDescriptor *ZrParser_CompileTool_FindModule(
    const TZrChar *moduleName);
TZrBool ZrParser_CompileTool_IsModuleName(const TZrChar *moduleName);
const SZrParserCompileToolCallableDescriptor *ZrParser_CompileTool_FindCallable(
    const SZrParserCompileToolModuleDescriptor *module,
    EZrParserCompileToolRole role);
TZrUInt64 ZrParser_CompileTool_ComputePublicContractHash(
    const SZrParserCompileToolModuleDescriptor *module);
TZrBool ZrParser_CompileToolContentHash_Bytes(
    const TZrByte *bytes, TZrSize byteCount,
    TZrChar *outHash, TZrSize outHashSize);
```

外部编译工具归档使用以下成对 API；`outArtifact` 必须是零初始化或已关闭的存储，
成功后由调用者显式 close：

```c
TZrBool ZrParser_CompileToolArtifact_OpenBuildDependency(
    const SZrLibrary_Project *project,
    const TZrChar *rawSpecifier,
    const SZrLibrary_ProjectManifestDependencyLockEntry *lockEntries,
    TZrSize lockEntryCount,
    const TZrChar *archivePath,
    SZrParserCompileToolResolvedArtifact *outArtifact,
    TZrChar *errorBuffer, TZrSize errorBufferSize);
TZrBool ZrParser_CompileToolArtifact_IsOpen(
    const SZrParserCompileToolResolvedArtifact *artifact);
void ZrParser_CompileToolArtifact_Close(
    SZrParserCompileToolResolvedArtifact *artifact);
```

compiler 集成入口是 `ZrParser_CompileTime_PrepareBuildFactsInCompilerState`、
`ZrParser_Compiler_EvaluateCompileTimeExpression`、
`ZrParser_CompileTimeDeclaration_Execute` 和
`ZrParser_Source_CompileWithComptimeCache`。这些函数借用 `SZrCompilerState` 的
semantic context；释放 compiler state 前必须先关闭所有 resolved artifact。

## 失败边界

- `feature()` 参数不是非空 string、feature 未在 manifest 声明：编译错误。
- 在 runtime function 中调用 `compile.*`：`compiletool.phase_mismatch`。
- diagnostic message 为空或 target 不是有效 `SymbolId`：typed evaluator 拒绝。
- lock/version/hash/entry 不匹配：报告 `compiletool.artifact.*`，不执行不可信 provider。
- provider public contract 或 phase 不匹配：binding 不建立，已有 runtime module 不受影响。
