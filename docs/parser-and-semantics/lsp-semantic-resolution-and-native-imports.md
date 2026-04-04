---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_module_metadata.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_module_metadata.h
  - zr_vm_language_server/src/zr_vm_language_server/lsp_project.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_project_features.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic_analyzer.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic_analyzer_symbols.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_signature_help.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_super_navigation.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_decorator_navigation.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_import_target_navigation.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_project_navigation.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_token_metadata.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_semantic_tokens.c
  - zr_vm_language_server/src/zr_vm_language_server/reference_tracker.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_interface_support.c
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_parser/src/zr_vm_parser/parser_statements.c
  - zr_vm_parser/src/zr_vm_parser/compiler_typed_metadata.c
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_project_features.c
  - tests/language_server/stdio_smoke.js
  - tests/parser/test_parser_extern.c
implementation_files:
  - zr_vm_language_server/src/zr_vm_language_server/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_module_metadata.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_project.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_project_features.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic_analyzer.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic_analyzer_symbols.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_signature_help.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_super_navigation.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_decorator_navigation.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_import_target_navigation.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_project_navigation.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_token_metadata.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_semantic_tokens.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_interface_support.c
  - zr_vm_language_server/src/zr_vm_language_server/reference_tracker.c
  - zr_vm_language_server/stdio/stdio_requests.c
  - zr_vm_parser/src/zr_vm_parser/compiler_typed_metadata.c
  - zr_vm_parser/src/zr_vm_parser/parser_statements.c
plan_sources:
  - user: 2026-04-04 实现“ZR LSP 语义内核与元信息推断增强计划”
tests:
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_project_features.c
  - tests/language_server/stdio_smoke.js
  - tests/parser/test_parser_extern.c
doc_type: module-detail
---

# LSP Semantic Resolution And Native Imports

## 范围

这份文档说明 language server 里最近补齐的四条关键语义链路：

1. `this` / `super` / compile-time / `%test` / lambda 等局部符号，必须按真实作用域命中。
2. hover / definition / references 不能再被宽范围声明误导，必须优先命中最具体的引用范围。
3. `%import("module")` 的字符串字面量必须成为一等导航目标，hover / definition 直接落在导入目标模块上。
4. `%import("zr.math")` 这类导入必须在语义分析阶段就把 native/binary/source module metadata 预热进 parser/type inference，后续 LSP 才能正确解析 `$math.Vector3(...).y` 这类值类型构造链。

## 隐式接收者符号

`semantic_analyzer_symbols.c` 里的 `collect_function_like_scope(...)` 会为实例方法、构造函数、struct 方法建立隐式运行时符号：

- `this`
- `super`

修复前，这两个隐式符号复用了所属方法 AST 节点。结果是 hover 命中 `this` 时，LSP 会把它当成“方法符号”，显示方法签名而不是接收者类型。

当前行为改成：

- `this` / `super` 仍然是普通变量符号，参与 completion / hover / references。
- 但它们不再绑定到方法 AST 节点，因此 hover 会回落到变量类型展示。
- `this` 的 hover 现在展示当前 owner type，例如 `Derived`。
- `super` 的类型来自继承链上的 base type，后续 definition/reference 继续通过真实符号关系处理。

这条修复直接覆盖了类内 receiver 场景，不再需要 hover markdown 反推“当前是不是 method”。

## `super(...)` 构造函数导航

`lsp_signature_help.c` 和新抽出的 `lsp_super_navigation.c` 现在把 class meta function 里的 `super(...)` 识别成独立调用上下文，而不是要求它先降级成普通 `FunctionCall` AST。这个补口专门解决派生类构造函数里的场景：

```zr
class BossHero: BaseHero {
    pub @constructor(seed: int) super(seed) {
    }
}
```

当前行为是：

- signature help 会先识别当前位置是否落在 `super(...)` 参数区间。
- 若命中，LSP 直接解析当前 owner class 的 direct base type。
- 基类构造函数元方法优先走 compiler/type inference 里的结构化 member info。
- 如果当前 analyzer 尚未持有对应 prototype member，则回退到源码 AST 里的 `@constructor` 声明，按基类泛型实参做参数类型专门化。
- 最终 signature label 与 parameter list 统一走结构化 builder，所以 `super(seed)` 会展示 base constructor 的参数名和类型，而不是派生类构造函数自己的参数列表。
- `lsp_super_navigation.c` 会把两类入口统一归并到同一个“基类构造函数目标”上：
  - 派生类构造函数里的 `super(...)`
  - 基类上的 `@constructor` 声明
- `lsp_interface.c` 的 definition / references / document highlight 都改成先走这套统一目标解析；当光标落在 `super` 关键字、参数区或基类 `@constructor` 声明上时，导航和高亮会落到同一组结果。
- goto definition 会直接跳到基类 `@constructor` 声明，而不是停在派生类构造函数作用域里的隐式 `super` 变量上。
- find references 会把基类 `@constructor` 声明和所有匹配的 `super(...)` 调用归并成同一引用集。
- document highlight 会在当前文档里同时标出基类 `@constructor` 声明和对应的 `super(...)` 调用位置。

这让 `super(...)` 不再只是 signature help 的补丁式特判，而是开始具备统一的构造函数导航语义，也避免了“必须先有完整 prototype 编译结果才能提示”的额外耦合。

## Decorator 导航与 parser 修复

`lsp_decorator_navigation.c` 现在把 `#singleton#`、`#trace#` 这类 decorator token 当成第一类语义入口处理：

- goto definition 会默认跳到被修饰的 class / method / field / property / function 声明。
- hover 会展示 decorator 名称、类别以及目标声明，例如 `Target: class SingletonClass`。
- 若 decorator 未来具备额外注册元信息，可以继续叠加到 hover 上，但“被修饰声明”仍然是主定义目标。

这轮实现里还修了一个更底层的 parser 缺陷：顶层 `#decorator# class Foo {}` 之前会在 statement lookahead 时吃掉 decorator，导致 AST 上类声明没有保留 decorators。`parser_statements.c` 现在会在 decorator lookahead 前后保存并恢复 parser cursor，因此：

- 顶层 class decorator 会保留在 AST 上。
- 后续 LSP decorator definition / hover 不需要绕过损坏 AST 做位置猜测。
- `tests/parser/test_parser_extern.c` 新增了顶层 class decorator 回归，确保这条支持层不会再退化。

## `%import("...")` 字面量导航

`lsp_import_target_navigation.c` 现在把 `%import("module")` 里的字符串字面量当成独立语义入口处理，而不是只把导入后的别名变量当作可导航对象。

这条实现专门补了一个之前的结构性缺口：

- 语义分析里导入绑定本身是存在的。
- 但是 parser 当前给 string literal AST 节点留下的 `location` 在这条路径上并不可靠，`%import("greet")` 会把字面量位置漂到分号附近。
- 结果是 imported member 导航能工作，真正落在 `"greet"` 上的 hover / definition 却命不中。

当前行为改成：

- LSP 仍然复用 project/native metadata 去解析模块来源和目标记录。
- 但 `"module"` 字面量本身的命中范围，不再信任 AST string range。
- `lsp_import_target_navigation.c` 直接基于当前文档文本恢复字面量边界：
  - 从光标 offset 向左右收缩到当前字符串的引号范围。
  - 验证左侧语法前缀确实是 `%import(`。
  - 现场归一化模块名，再映射到 project source record 或 native builtin descriptor。
- definition 现在会把 `%import("greet")` 直接跳到 `greet.zr` 模块入口。
- hover 现在会把 `%import("greet")` / `%import("zr.system")` 统一展示成：
  - `module <...>`
  - `Source: project source` / `native builtin` / `external/unresolved`

这让 `%import` 字面量本身进入了和 imported member、decorator、`super(...)` 一致的第一类导航模型，也避免继续在 AST 位置不稳定的情况下做“命中不到就算了”的弱处理。

## 固定 Token 元信息

`lsp_token_metadata.c` 把 `%...` directive 和 `@...` meta method 的固定元信息表从 `lsp_interface_support.c` 里抽出来，避免继续把新职责堆进一个 2000+ 行文件。当前这层统一提供三类消费：

- 输入 `%` / `@` 时的固定 completion。
- `@constructor` 这类 token 的 hover 类别说明。
- semantic tokens 对 `%directive`、`@meta-method` 的识别辅助。

这意味着同一份固定表现在至少服务于 completion、hover、semantic tokens，不再出现“补全知道分类，hover/semantic token 不知道”的分裂状态。

## `@meta-method` Hover 与 Token 分类

这轮补齐了用户显式要求的 `@xxx` 类别说明能力。当前行为是：

- hover 落在 `@constructor`、`@add`、`@toString` 等 token 上时，会直接展示：
  - meta method 名称
  - 分类，例如 `lifecycle`、`arithmetic/operator`、`conversion`、`call/access`
  - 适用声明形态，当前统一写为 `class/struct meta function`
- semantic tokens 现在会把 `@constructor` 这类声明 token 分类成 `metaMethod`
- semantic tokens 现在会把 `#singleton#` 这类 decorator token 分类成 `decorator`
- `%compileTime`、`%import` 等保留字语义 token 继续保留为 `keyword`，以兼容现有 LSP token legend 与测试基线

这条实现虽然还没有把 meta method 自身接进 definition / references，但已经把“类别提示”和“可视分类”从 completion-only 扩展到了 hover 和 semantic tokens。

## 最窄引用优先

`reference_tracker.c` 的 `FindReferenceAt(...)` 以前采用“第一个包含当前位置的引用即返回”。这个策略在有宽范围定义引用时会产生系统性误判，例如：

- `%test("scope")` 的声明范围覆盖整个 test body
- 更具体的局部变量 / compile-time 变量 usage 引用虽然也存在，但因为排在后面，永远命不中

当前行为改成：

- 先收集所有包含当前位置的引用
- 再按“范围越窄越优先”选择最佳候选
- 若范围相同，则优先非 definition 引用

这使得以下行为回到一致状态：

- compile-time 变量 hover 不再被 `%test` 声明覆盖
- 局部变量 / lambda capture / class 内 receiver 的引用命中更稳定
- LSP definition / hover / references 的命中逻辑更接近真实语义事实，而不是注册顺序

## Native Import Metadata 预热

`$math.Vector3(4.0, 5.0, 6.0).y` 之前在 parser 单测里可推断，但在 LSP 文档分析里失败。根因不是语法不支持，而是 semantic analyzer 在建立

```zr
var math = %import("zr.math");
```

这个变量的类型时，只写入了模块名字符串，没有触发 parser 的 import metadata 加载流程。

当前行为改成：

- `semantic_analyzer_symbols.c` 里的 `infer_symbol_expression_type(...)` 在遇到 `ZR_AST_IMPORT_EXPRESSION` 时，优先调用 `ZrParser_ExpressionType_Infer(...)`
- parser 的 import inference 会进入 `ensure_import_module_compile_info(...)`
- native/source/binary module 的 prototype/type descriptor 会被注册到 compilerState 的 type prototype 集合
- 随后 `lsp_interface_support.c` 对 receiver 前缀做 AST 推断时，就能把 `$math.Vector3(...)` 识别成真实 `Vector3` value type

结果是：

- `$math.Vector3(...).` completion 可以列出 `x/y/z`
- `$math.Vector3(...).y` hover 可以展示 `field y: float` 和 `Receiver: Vector3`
- 这条能力直接复用 parser/type inference/native descriptor 的结构化元信息，不再靠 LSP 特判库名

## 统一模块元信息入口与优先级

`lsp_module_metadata.c` 现在是 server 侧统一的 imported-module metadata 入口，负责把四类来源归一到同一套 source-kind 判定上：

1. 当前工作区源码 record
2. `.zro/.zri` binary metadata
3. native builtin descriptor
4. native descriptor plugin

当前优先级固定为：

- `project source`
- `binary metadata`
- `native builtin` / `native descriptor plugin`
- `external/unresolved`

这条 helper 现在同时被以下消费层复用：

- `lsp_project_features.c` 的 import hover / completion
- `lsp_interface_support.c` 的 native receiver/type descriptor 查找
- `lsp_semantic_tokens.c` 的 imported module semantic token 解析
- `semantic_analyzer.c` 的 imported module completion 递归

结果是原来分散在 `NativeRegistry_FindModule(...)`、内建模块硬编码和 import 特判里的逻辑，开始收敛到同一份 source-kind / descriptor 解析路径。

## Imported Member References And Highlights

`lsp_project_navigation.c` 现在把 `%import(...)` alias 后的第一段 member 命中先还原成统一的 imported-member 事实，再让 `definition / references / document highlight` 复用这条路径，而不是继续依赖“source symbol 找得到就工作、找不到就失效”的分裂行为。

当前这条路径统一记录：

- `moduleName`
- `memberName`
- metadata source kind
- 可选的 binary metadata declaration 位置

消费结果按来源分层：

- source-backed imported member
  - definition 继续跳到真实源码声明
  - references 继续合并真实源码声明、真实源码引用、跨文件 import usage
  - document highlight 现在也会在当前文档里标出所有 imported usage
- binary-only imported member
  - definition 会优先落到 `.zri` 的 `EXPORTED_SYMBOLS` 声明
  - references 在 `includeDeclaration=true` 时会把 `.zri` 声明和项目 usage 归并到同一结果集
  - document highlight 会在当前文档里标出全部 `moduleAlias.member` usage
- native builtin / native descriptor plugin imported member
  - references 现在至少返回项目内或当前文档内的 usage 集合
  - document highlight 会返回当前文档的 usage 集合
  - 若当前还没有可导航声明元数据，则保持 usage-only，不伪造 definition

这意味着 imported member 不再只是 hover/completion 可用，references/highlights 失效；binary/native/plugin 也开始共享同一套结构化导航入口。

## Watched Metadata Refresh

`stdio_requests.c` 现在消费 `workspace/didChangeWatchedFiles`，覆盖：

- `.zr`
- `.zrp`
- `.zro`
- `.zri`
- `.dll`
- `.so`
- `.dylib`

server 不再只把 `.zrp` 刷新当成 document-sync 副作用。外部 metadata 变更会走：

1. `workspace/didChangeWatchedFiles`
2. `ZrLanguageServer_LspProject_ReloadOwningProjectForWatchedUri(...)`
3. `ZrLanguageServer_Lsp_ProjectRefreshForUpdatedDocument(...)`

刷新时不再只是替换 project index。当前行为还会：

- 失效旧 project record 持有的 analyzer
- 保留 parser 里已加载文档的最新内容
- 在新 project index 下重新分析这些已加载文档
- 重新挂回 import bindings / imported module facts
- 如果变更来自尚未预热的 `.zro/.zri/.dll/.so/.dylib`，会沿文件路径回溯最近 `.zrp` 并先自举 owning project，再走同一套 refresh

这条修复专门解决两类问题：

- binary/native metadata 已经变了，但 open 文档 hover / completion 仍然吃旧 analyzer
- metadata 先变、project 还没被任何 source doc 触发发现，导致 watched-files 事件根本进不了 project refresh

## `.zro` 与 `.zri` 的分流加载

当前 binary metadata 入口已经明确区分两种载体：

- `.zro`
  - 继续走 `ZrCore_Io_ReadSourceNew(...)`
  - 直接读取序列化 binary module/source
- `.zri`
  - 不再错误地复用 `.zro` 的 binary source loader
  - 改成按 intermediate 文本元数据读取 `EXPORTED_SYMBOLS`
  - LSP hover / completion 直接消费这些结构化 exported-symbol facts

这条分流很关键，因为 `.zri` 虽然是中间产物元数据，但它是文本格式，不是 `io.c` 里的 binary source 布局。之前把 `.zri` 当 `.zro` 读会在 refresh 后重新加载 imported member hover 时触发无效 IO 读取和断言崩溃。

现在 `.zri` 至少覆盖了 LSP 当前真正依赖的能力面：

- imported member hover
- imported member completion
- `%import("binary_only")` definition 到 metadata file entry
- `moduleAlias.member` definition 到 `.zri` 的 `EXPORTED_SYMBOLS` 声明行
- watched-files metadata refresh 后的重新提示

同时 `.zro` 仍然保持原来的完整 binary source 路径，不影响已有 `.zro` 工程。

## 相关回归

本轮回归覆盖了下面几类样例：

- 类实例方法中的 `this` / `super` / locals hover 与 completion
- 派生类构造函数里的 `super(...)` base constructor signature help
- 派生类构造函数里的 `super(...)` goto definition 到 base constructor
- 派生类构造函数里的 `super(...)` find references 命中 base constructor + super call
- 派生类构造函数里的 `super(...)` document highlight 命中 base constructor + super call
- `%compileTime` 变量与函数作用域
- `%test("scope")` 的局部作用域
- typed lambda 的局部参数和 capture
- directive `%...` 与 meta method `@...` completion
- `@constructor` meta method hover 类别说明
- semantic tokens 对 `#decorator#` 与 `@meta-method` 的分类
- native value constructor `$math.Vector3(...).y`
- watched binary metadata refresh 对 unopened project 的 bootstrap
- watched binary metadata refresh 后 open 文档 hover 的更新
- `.zri` 作为 binary metadata 载体的 imported member hover / completion
- binary import literal definition 到 `.zri` module entry
- binary imported member definition 到 `.zri` exported symbol declaration
- binary imported member references 到项目 usage + `.zri` exported symbol declaration
- binary imported member document highlight 到当前文档 usage
- native imported member references / document highlight 到 usage-only 结果
- 类成员 definition / references / comment hover

对应证据见：

- `tests/language_server/test_semantic_analyzer.c`
- `tests/language_server/test_lsp_interface.c`
- `tests/language_server/test_lsp_project_features.c`

## 验证证据

2026-04-04 这一轮 imported-module / external-symbol 导航修复的验证结果如下：

- WSL gcc `build/codex-wsl-gcc-debug`
  - 定向重编 `libzr_vm_language_server.so`
  - `test_lsp_project_features.c` 通过
  - `stdio_smoke.js` 通过
- WSL clang `build/codex-wsl-clang-debug`
  - 这一轮未重跑；当前验证以 WSL 定向构建为准
- Windows MSVC
  - 这一轮未重跑；仓库仍按已知基线限制处理

## 当前已知限制

- 当前工作区还存在独立的 parser 脏改动，完整 `cmake --build` 会在 `zr_vm_parser/src/zr_vm_parser/lexer.c` 的 `ZR_LEXER_EOZ` 未定义处失败。这不是本轮 `%import` literal 导航修复引入的问题，因此本轮验证采用了对象级别重编 + 定向重链。
- Windows MSVC 的 LSP 测试可执行体仍然存在独立的 `0xC0000005` 退出问题，这不是本轮 Linux/WSDL 语义修复引入的新回归。
- `zr_vm_language_server_lsp_interface_test` 在 WSL 通过时仍会打印两条 `Construct target must resolve to a registered prototype` 编译日志；当前没有导致目标测试失败，但说明 constructor prototype 解析路径仍有额外清理空间。
