---
related_code:
  - zr_vm_language_server/src/zr_vm_language_server/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_module_metadata.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_module_metadata.h
  - zr_vm_language_server/src/zr_vm_language_server/lsp_project.c
  - zr_vm_language_server/src/zr_vm_language_server/lsp_project_features.c
  - zr_vm_library/include/zr_vm_library/native_registry.h
  - zr_vm_library/src/zr_vm_library/native_binding.c
  - zr_vm_library/src/zr_vm_library/native_binding_support.c
  - zr_vm_library/src/zr_vm_library/native_binding_registry_plugin.c
  - zr_vm_library/src/zr_vm_library/native_binding_internal.h
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
  - zr_vm_parser/src/zr_vm_parser/compiler_bindings.c
  - zr_vm_parser/src/zr_vm_parser/compiler_extern_declaration.c
  - zr_vm_parser/src/zr_vm_parser/type_inference_core.c
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
  - zr_vm_library/include/zr_vm_library/native_registry.h
  - zr_vm_library/src/zr_vm_library/native_binding.c
  - zr_vm_library/src/zr_vm_library/native_binding_support.c
  - zr_vm_library/src/zr_vm_library/native_binding_registry_plugin.c
  - zr_vm_library/src/zr_vm_library/native_binding_internal.h
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
  - zr_vm_parser/src/zr_vm_parser/compiler_bindings.c
  - zr_vm_parser/src/zr_vm_parser/compiler_extern_declaration.c
  - zr_vm_parser/src/zr_vm_parser/type_inference_core.c
plan_sources:
  - user: 2026-04-04 实现“ZR LSP 语义内核与元信息推断增强计划”
  - user: 2026-04-05 继续把 plugin/native/binary metadata 统一链推进到更细粒度 completion/definition/references/watched refresh 覆盖
tests:
  - tests/language_server/test_semantic_analyzer.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_project_features.c
  - tests/language_server/descriptor_plugin_fixture_int.c
  - tests/language_server/descriptor_plugin_fixture_float.c
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

## `%extern` 函数与类型的统一语义入口

这一轮把 `%extern` 里的 function / delegate / struct / enum 从“只有 symbol table 看得见”继续推进到“navigation、references、signature help 共用同一组结构化事实”。

当前行为是：

- `semantic_analyzer_symbols.c` 会给 extern function / delegate / enum member 建立真实 source symbol，并把 definition reference 直接挂到 symbol 上。
- extern struct 声明现在也会补 definition reference，因此 type annotation 上的 `FindReferences` 不再只返回 usage，声明和 usage 会落到同一引用集合。
- server 自己维护的 `compilerState->typeEnv` / `compileTimeTypeEnv` 现在会显式注册 extern function callable binding，而不再只依赖 symbol table。
- 这一步会连同 declaration node 一起写入 type env，所以后续 overload resolution 和 signature help 能直接命中 extern declaration，而不是再回退到“源码里找不到普通 `func` 声明”。
- parser/type inference 侧的 candidate lookup 也补进了 extern function declaration，因此基于 declaration node 的参数列表和命名参数匹配不再只支持普通函数。

这条修复让 extern function 的以下能力开始共用同一路径：

- goto definition
- find references
- document highlight
- completion
- signature help

## Extern Call Context 与源类型保真

extern function 的 signature help 之前还有两个独立缺口：

1. bare function call 的 `FunctionCall.location` 没覆盖到“刚进入 `(` 后、还没进入第一个参数 AST 节点”的区间，导致 `NativeAdd(1, 2)` 这类位置虽然已经在调用里，signature help 仍然拿不到 call context。
2. 即使解析到 extern declaration，签名展示也会把 `i32` 这样的源类型正规化成 `int`，丢掉 FFI/source declaration 的原始拼写。

当前行为改成：

- `lsp_signature_help.c` 为普通函数调用统一构造 `signature_call_context_range(...)`，调用上下文范围会从 call node 起点延伸到最后一个实参或 generic 实参结尾。
- `signature_call_matches_position(...)` 不再只依赖裸 `callNode->location`，因此光标位于 `(` 后、逗号间隔区或第一个参数前时，也能正确触发 signature help。
- extern function 的 signature label 构建优先使用 declaration AST 上的参数/返回类型文本，而不是把 resolved inferred type 直接格式化成标准化名字。
- 结果是 `%extern` 源声明里的 `NativeAdd(lhs: i32, rhs: i32): i32;` 在 hover-independent signature help 里会保持 `i32`，而不是退化成 `int`。

这条链路和 `super(...)` 一样，已经不再是 hover markdown 或 native 特判的派生结果，而是基于真实 callable declaration、真实调用区间和真实类型来源做结构化拼装。

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
- references 现在也会把 import target literal 当成真实 module-level 入口：
- `includeDeclaration=true` 时先落到 source / native descriptor plugin 的 module entry
  - binary metadata 若 `.zro` typed export 已携带 declaration span，则优先落到 symbol-level declaration；旧 schema 才回退到 module entry
  - 然后回收 project 内所有匹配的 `%import("module")` 字面量位置
- document highlight 现在会在当前文档里标出同一 module target 的 import literal 范围，而不是退回普通 string token 语义

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

## Project-Local Descriptor Plugin 优先级

这一轮把 descriptor plugin 的“同名模块跨 project 漂移”问题补到了 registry 和 LSP metadata resolver 两层。

之前的行为是：

- native registry 只按逻辑模块名缓存 descriptor record。
- 一旦某个测试工程先加载了 `zr.pluginprobe`，后续另一个工程即使在自己的 `native/` 目录下放了同名 plugin，LSP 也会继续命中旧 record。
- hover 的 `Source: native descriptor plugin` 仍然是对的，但 definition / references 可能跳到上一个工程的 `.so/.dll` 路径。

当前行为改成：

- `native_registry` 增加了 `EnsureProjectDescriptorPlugin(...)` 这一类 project-aware 入口，优先检查当前 project `native/` 目录里的实际 plugin 文件。
- `lsp_module_metadata.c` 不再“先查到 registry 就立即返回”，而是先给当前 project 一次覆盖当前 record 的机会，再统一从 registry 读结构化 descriptor/source-kind/source-path。
- descriptor plugin record 现在持久保存 `moduleName/sourcePath` 副本，不再把 cache key 和 watched refresh 的身份信息借在插件导出的静态 descriptor 内存上。
- imported-member hover 的优先级也改成 `source > binary metadata > native/plugin descriptor > module prototype fallback`。这样即使 analyzer 里还保留旧的 module prototype，hover 也不会再被它盖过当前 project 的真实 plugin descriptor。
- imported-member completion 现在跟随同一套优先级：先用 source/binary/native/plugin metadata 生成主 completion 集，再只用 module prototype 补缺失 label，不再让旧 prototype detail 覆盖当前 plugin descriptor 的真实签名。

对应的回归固定覆盖在 `test_lsp_descriptor_plugin_project_local_definition_overrides_stale_registry(...)`：

- 先打开一个返回 `int` 的 `zr.pluginprobe`
- 再打开另一个返回 `float` 的同名 plugin 工程
- hover / completion / definition / references 都必须切到第二个工程自己的 plugin 副本

这条规则把 project-local native plugin 从“可能命中”提升为“同名模块下的第一优先元信息源”。

## Watched `.dll/.so/.dylib` Refresh 的加载路径

watched dynamic library refresh 现在仍然和 binary metadata refresh 走同一条 project discovery + project refresh 入口，但 plugin loader 本身已经改成“从 shadow copy 装载”，不再直接把 project 里的 `.dll/.so/.dylib` 原文件 `dlopen` 到当前进程。

这条修复针对的是一个真实崩溃场景：

1. open document 已经因为 `%import("zr.pluginprobe")` 把 project-local descriptor plugin 载入进程。
2. 外部构建工具在原路径上直接覆盖同名 `.so/.dll`。
3. server 收到 `workspace/didChangeWatchedFiles` 后尝试 `dlclose` 旧句柄。
4. 如果旧句柄直接映射的就是被覆盖中的原文件，卸载阶段可能读到被替换后的 fini / dynamic 元数据，最终在 `dlclose` 里崩溃。

当前行为改成：

- `native_registry_load_plugin_descriptor(...)` 会先把 project/source plugin 文件复制到 OS 临时目录下的 `zr_vm_native_plugin_cache/` shadow path。
- 真正 `dlopen` 的是这份 shadow copy，而不是 workspace 里的原始 plugin 文件。
- registry 继续把 workspace 中的真实 plugin 路径记录为 `sourcePath`，因此 watched-files invalidation 和 definition/source-kind 展示仍然指向用户工程里的原文件。
- plugin handle record 额外保存 `loadedPath`，invalidate 或按模块替换时会：
  - `dlclose` shadow-loaded handle
  - 删除对应 shadow copy
  - 清掉 module record / module cache
- `ZrLanguageServer_LspProject_ReloadOwningProjectForWatchedUri(...)` 不再对 `.dll/.so/.dylib` 走早退分支；它会继续执行完整 project refresh，并把已打开文档重新挂到新的 project metadata 上。

结果是：

- project 内的 plugin 文件可以被外部构建工具原地替换，而不会把当前进程里已加载句柄的卸载元数据破坏掉。
- watched plugin refresh 之后，open document 的 completion / hover / local inference 会在同一轮 project refresh 中切到新 descriptor，而不再依赖“下一次查询时再碰巧重新加载”。
- unopened-project 的 watched plugin bootstrap 语义保持不变，仍然可以沿路径回溯最近 `.zrp` 自举 owning project。

## 统一模块元信息入口与优先级

`lsp_module_metadata.c` 现在是 server 侧统一的 imported-module metadata 入口，负责把四类来源归一到同一套 source-kind 判定上：

1. 当前工作区源码 record
2. `.zro` binary metadata
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
- 可选的 binary metadata module-entry 位置

消费结果按来源分层：

- source-backed imported member
  - definition 继续跳到真实源码声明
  - references 继续合并真实源码声明、真实源码引用、跨文件 import usage
  - document highlight 现在也会在当前文档里标出所有 imported usage
- binary-only imported member
  - definition 会落到 `.zro` module entry
  - references 在 `includeDeclaration=true` 时会把 `.zro` module entry 和项目 usage 归并到同一结果集
  - document highlight 会在当前文档里标出全部 `moduleAlias.member` usage
- native builtin / native descriptor plugin imported member
  - references 现在至少返回项目内或当前文档内的 usage 集合
  - document highlight 会返回当前文档的 usage 集合
  - 若当前还没有可导航声明元数据，则保持 usage-only，不伪造 definition

这意味着 imported member 不再只是 hover/completion 可用，references/highlights 失效；binary/native/plugin 也开始共享同一套结构化导航入口。

这一轮又把“声明侧 metadata 文件本身”接回了同一条导航链，补掉了之前只支持“从 source usage 反推 declaration”的单向路径：

- `.zro` 路径现在会先按 project binary root 反推出 `moduleName`
- 若光标落在 binary metadata file entry，仍然统一成 module-entry 级结果
- 若光标落在 `.zro` typed export declaration span，resolver 会直接返回该 exported member，并把 references / document highlight 归并到同一条 imported-member 链
- descriptor plugin `.dll/.so/.dylib` 入口会先尝试走 owning project 的 import bindings 反解 `moduleName`，避免同名 plugin 跨 project 时只依赖全局 registry
- 若 project 侧暂时还没把 plugin 重新解析进 analyzer，再回退到 native registry 的 `sourcePath -> moduleName` 反查
- document highlight 现在也能落在 external metadata declaration 自身：
  - `.zro` module entry 会在 metadata 文档里高亮 module entry 自身
  - descriptor plugin file entry 会在 plugin 文档里高亮 module entry 自身
- module-entry 级 references 也补进了 import-binding 声明：
  - binary metadata / plugin file entry 除了继续聚合 `moduleAlias.member` usages，还会回收 project 内 `var moduleAlias = %import("...")` 的 alias declaration
  - 这样 module entry 不再只有“成员被访问过”的粗粒度引用，而开始具备真正的 module-binding 导航覆盖
- 这条 module-entry 聚合链现在也覆盖源码模块与 `%extern` wrapper 源文件：
  - `greet.zr` 这类 project source file entry 在 `0:0` module entry 上会回收到 `%import("greet")` 字面量、`greetModule` alias declaration，以及同模块的 imported-member usages
  - `native_api.zr` 这类 ffi source wrapper file entry 走同一条 declaration resolver，不再因为它是 source-backed wrapper 就退回“只能从 import literal 一侧导航”
  - project 级 import-target references 不再依赖“当前文件必须已打开”；若文档未打开，server 会先确认该 analyzer 是否真的导入了目标模块，再从磁盘文本恢复 `%import("...")` 的精确 inner-string range；必要时才回退到 AST binding range
  - source module entry / ffi wrapper module entry 的 references 也不再局限于 `projectIndex->files`；server 现在会递归 sourceRoot 下的 `.zr` 文件，按需加载 analyzer，把未打开源码里的 import literal、alias binding、imported-member usage 一起并回同一个 module target
  - 这样 source / ffi wrapper / binary metadata / descriptor plugin 四种 module entry 现在都共享同一套 declaration + import-target + alias-binding + imported-member usage 聚合模型

结果是 binary metadata module entry、plugin file entry、source/ffi module entry、source-side imported member 这几类入口现在都能回到同一套 definition/references/document highlight 目标模型，而不是继续分裂成“只能从 usage 侧工作”的半通路径。

## Watched Metadata Refresh

`stdio_requests.c` 现在消费 `workspace/didChangeWatchedFiles`，覆盖：

- `.zr`
- `.zrp`
- `.zro`
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
- 在 project-aware reanalyze 前，为这些已加载文档预加载当前 project `native/` 目录里的 descriptor plugin imports
- 重新挂回 import bindings / imported module facts
- 如果变更来自尚未预热的 `.zro/.dll/.so/.dylib`，会沿文件路径回溯最近 `.zrp` 并先自举 owning project，再走同一套 refresh

这条修复专门解决两类问题：

- binary/native metadata 已经变了，但 open 文档 hover / completion 仍然吃旧 analyzer
- metadata 先变、project 还没被任何 source doc 触发发现，导致 watched-files 事件根本进不了 project refresh
- watched plugin refresh 之后，importer locals 仍然停留在旧的 descriptor return type，而没有重新走 compiler-side local inference

## `.zro` 与 `.zri` 的职责分流

当前 binary metadata 的正式机器可消费入口固定为 `.zro`：

- `.zro`
  - 继续走 `ZrCore_Io_ReadSourceNew(...)`
  - 直接读取序列化 binary module/source
  - imported member hover / completion / definition / references / document highlight 都以这条路径为准
  - compiler/type inference 侧的 imported-module compile info 也只消费这条路径
- `.zri`
  - 保留为 debug / intermediate 文本产物
  - 不再作为 server 的语义推断、导航、hover、completion 事实源
  - 也不再被当作 `.zro` 的替代输入

这条分流很关键，因为 `.zri` 虽然仍然是有用的 debug 文件，但它不是 `io.c` 里的 binary source 布局，也不应该再驱动正式的 LSP 语义链。之前把 `.zri` 当 `.zro` 读会在 refresh 后重新加载 imported member hover 时触发无效 IO 读取和断言崩溃。

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
- `.zro` 作为 binary metadata 载体的 imported member hover / completion
- binary import literal definition 到 `.zro` module entry
- binary imported member definition 到 `.zro` exported declaration span（旧 schema 回退 module entry）
- binary imported member references 到项目 usage + `.zro` exported declaration span
- binary imported member document highlight 到当前文档 usage
- source module entry / ffi wrapper module entry references 到 `%import(...)` literal + alias binding + imported-member usage
- source module entry / ffi wrapper module entry references 覆盖未打开 project source files
- source module entry / ffi wrapper module entry document highlight 命中 module entry 自身
- native imported member references / document highlight 到 usage-only 结果
- 类成员 definition / references / comment hover

对应证据见：

- `tests/language_server/test_semantic_analyzer.c`
- `tests/language_server/test_lsp_interface.c`
- `tests/language_server/test_lsp_project_features.c`

## 验证证据

2026-04-05 这一轮 watched refresh / external metadata reanalysis 修复的验证结果如下：

- WSL gcc `build/codex-wsl-gcc-debug`
  - 定向重编 `libzr_vm_library.so`、`libzr_vm_language_server.so`
  - `./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_lsp_project_features_test_relaxed` 通过
  - `./build/codex-wsl-gcc-debug/bin/zr_vm_language_server_lsp_interface_test` 通过
  - watched binary metadata refresh 的 importer local inference 回归通过
  - watched descriptor plugin refresh 的 importer local inference 回归通过
  - plugin source overwrite 后的 unload/reload 不再在 `dlclose` 崩溃
- WSL clang `build/codex-wsl-clang-debug`
  - 这一轮未重跑；当前验证以 WSL 定向构建为准
- Windows MSVC
  - 这一轮未重跑；仓库仍按已知基线限制处理

## 当前已知限制

- 当前工作区还存在独立的 parser 脏改动，完整 `cmake --build` 会在 `zr_vm_parser/src/zr_vm_parser/lexer.c` 的 `ZR_LEXER_EOZ` 未定义处失败。这不是本轮 `%import` literal 导航修复引入的问题，因此本轮验证采用了对象级别重编 + 定向重链。
- Windows MSVC 的 LSP 测试可执行体仍然存在独立的 `0xC0000005` 退出问题，这不是本轮 Linux/WSDL 语义修复引入的新回归。
- `zr_vm_language_server_lsp_interface_test` 在 WSL 通过时仍会打印两条 `Construct target must resolve to a registered prototype` 编译日志；当前没有导致目标测试失败，但说明 constructor prototype 解析路径仍有额外清理空间。
