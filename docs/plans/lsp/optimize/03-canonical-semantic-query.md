# Compiler Canonical Semantic Query 收敛实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use `zr-language-feature-design`, `test-driven-development`, `support-first-regression-testing`, `cross-session-coordination`, `code-module-docs-maintenance`, and `verification-before-completion` while executing this plan.

**Goal:** 让 parser/compiler/metadata projection 成为类型、符号、引用、调用、属性、所有权和诊断的唯一事实来源；LSP 只执行 snapshot acquisition、query、协议投影与展示格式化。

**Architecture:** 扩展 `zr_vm_parser/semantic_query.h` 为稳定的 read-only semantic model API。source、`.zro` 与 native descriptor 先投影到同一 `TypeId`/`SymbolId`/ModuleIdentity/relation graph；这些 id 在所属 semantic snapshot 内稳定，跨代际身份由 ModuleIdentity + generation 管理。LSP 不允许根据 token/name/type text 补造语义。Roslyn `SemanticModel` 的 SymbolInfo/TypeInfo/DeclaredSymbol/LookupSymbols/Diagnostics 边界作为仓库内参考，但 API 采用 ZR 现有数据结构。

**Tech Stack:** parser semantic facts、canonical type system、artifact metadata projection、project index、C11。

---

## Task 1：冻结 semantic query 契约与缺口测试

**Files:**
- Modify: `zr_vm_parser/include/zr_vm_parser/semantic_query.h`
- Create: `tests/parser/test_semantic_query_contract.c`
- Create: `tests/language_server/test_lsp_semantic_query_parity.c`
- Modify: `tests/CMakeLists.txt`

- [x] 为现有 TypeAt、CanonicalTypeAt、CallAt、DefinitionOf、DeclarationOf、ReferencesOf、Diagnostics、PropertyAt 建立 source/binary/native 三路同构测试。
- [x] 每个 query 明确 ownership：返回 borrowed view 的有效期绑定 semantic snapshot；需要跨 snapshot 保存的结果必须复制 stable ids/ranges，不保存 AST/raw pointer。
- [x] 查询不得修改 compiler/analyzer 状态；重复调用结果顺序稳定。
- [x] exactness 为 UNKNOWN/APPROXIMATE 时上层必须 fail closed 或明确标记，不得回退到 LSP type text reconstruction。

## Task 2：补齐 symbol-at-position 与 visible symbols

**Files:**
- Modify: `zr_vm_parser/include/zr_vm_parser/semantic_query.h`
- Create: `zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_symbols.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/semantic/semantic_query.c`
- Create: `tests/parser/test_semantic_query_symbols.c`

- [x] 新增契约：

```c
typedef struct SZrParserSemanticSymbolQuery {
    TZrSymbolId symbolId;
    TZrTypeId typeId;
    TZrSymbolId ownerSymbolId;
    EZrSemanticSymbolKind kind;
    EZrSemanticReferenceKind role;
    SZrFileRange declarationRange;
    SZrFileRange definitionRange;
    SZrString *displayName;
    SZrString *signatureDisplay;
} SZrParserSemanticSymbolQuery;

typedef struct SZrParserSemanticVisibleSymbolOptions {
    TZrBool includeReceiverMembers;
    TZrBool includeImports;
    TZrBool includeInaccessible;
} SZrParserSemanticVisibleSymbolOptions;

ZR_PARSER_API TZrBool ZrParser_SemanticQuery_SymbolAt(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        SZrParserSemanticSymbolQuery *outSymbol);
ZR_PARSER_API TZrBool ZrParser_SemanticQuery_VisibleSymbols(
        const SZrSemanticContext *context,
        SZrFileRange position,
        const SZrParserSemanticQueryScope *scope,
        const SZrParserSemanticVisibleSymbolOptions *options,
        SZrArray *outSymbols);
```

- [x] VisibleSymbols 处理 lexical scope、shadowing、receiver members、imports/aliases、generic parameters、overload sets、visibility 和 static/instance 上下文。
- [x] 返回稳定排序 key：scope distance、declaration order、SymbolId；completion 不再遍历 LSP symbol table 后按名称拼接。
- [x] Lua/QuickJS reference check：变量身份必须由 compiler scope state 决定；不得在 LSP token scanner 中复制 `searchvar`/scope walk。

## Task 3：补齐 declaration/definition/implementation relations

**Files:**
- Create: `zr_vm_parser/include/zr_vm_parser/semantic_relations.h`
- Create: `zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c`
- Modify: `zr_vm_parser/include/zr_vm_parser/semantic_query.h`
- Create: `tests/parser/test_semantic_query_relations.c`

- [x] relation graph 使用 snapshot-scoped stable ids：declaration↔definition、override/implementation、type base/interface、constructor、property accessor、alias target、import export origin。
- [x] 新增 `RelationsOfSymbol`、`ImplementationsOf`、`BaseTypesOf`、`DerivedTypesOf`；返回 relation kind + source/target SymbolId/TypeId + exact range。
- [ ] 处理多定义、partial/extern/native/binary 无 source definition 情形；没有 source range 时返回明确 external origin 和 virtual declaration URI，由 metadata projection 提供，不由 LSP 编造。
- [ ] 测试同名不同模块、重载、generic open/closed type、receiver method、alias chain、base/interface、多项目 provider generation。

## Task 4：补齐 call graph 与 overload facts

**Files:**
- Create: `zr_vm_parser/include/zr_vm_parser/semantic_calls.h`
- Create: `zr_vm_parser/src/zr_vm_parser/semantic/semantic_calls.c`
- Modify: `zr_vm_parser/include/zr_vm_parser/semantic_query.h`
- Create: `tests/parser/test_semantic_query_calls.c`

- [ ] CallAt 返回 selected target、overload candidate set、receiver TypeId、closed callable TypeId、argument-to-parameter mapping、conversion/exactness 和 call-site range。
- [x] semantic context 构建 caller SymbolId → call edges 索引；incoming/outgoing query 不扫描源文本。
- [x] 动态/无法解析调用返回 unresolved edge 与 reason，不把第一个同名函数当目标。
- [ ] source、`.zro`、native descriptor external callable 共用同一 callable contract；当前 L8 overlay 的 `isExternalCallable`/signatureDisplay 必须在 parser 层封闭，不再由 hover/signature 各自拼装。

## Task 5：补齐 formatter/display 与 documentation facts

**Files:**
- Modify: `zr_vm_parser/include/zr_vm_parser/canonical_type.h`
- Create: `zr_vm_parser/include/zr_vm_parser/semantic_display.h`
- Create: `zr_vm_parser/src/zr_vm_parser/semantic/semantic_display.c`
- Create: `tests/parser/test_semantic_display.c`

- [x] 为 canonical type、symbol signature、call signature、property contract 提供结构化 display API；输出由 TypeId/SymbolId 生成。
- [x] primitive、union、nullable、ref/owner/readonly、generic const/type args、tuple、function effects/passing modes 全覆盖。
- [ ] 删除 LSP `semantic_type_prototypes.c` 中把 `int/i64`、`string/str` 等名称映射回类型的职责；展示 alias 与 canonical identity 分开返回。
- [x] documentation 作为 symbol metadata fact 进入 query；completion/hover/signature 不再彼此提取文本。

## Task 6：让结构化诊断成为唯一语义诊断

**Files:**
- Modify: `zr_vm_parser/include/zr_vm_parser/semantic_query.h`
- Modify: `zr_vm_parser/src/zr_vm_parser/semantic/semantic_query.c`
- Modify: `zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_typecheck.c`
- Modify: `zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer_ownership_diagnostics.c`
- Create: `tests/parser/test_semantic_query_diagnostics.c`
- Create: `tests/language_server/test_lsp_diagnostic_projection.c`

- [ ] parser/compiler 负责 const assignment context、call compatibility、field existence、ownership/loan/region/effect、unresolved reference 等规则。
- [ ] diagnostic 包含 stable code、severity、primary range、related ranges、typed machine fix 或明确 no-fix reason。
- [ ] LSP 仅做 severity/range/code/relatedInformation/codeDescription/data 投影；删除 LSP analyzer 中已知不完整的重复检查。
- [ ] compiler diagnostic 与 LSP diagnostic 做 golden parity；不得通过调整 LSP message 掩盖 compiler 漏报。

## Task 7：迁移 LSP consumers 并删除第二套语义

**Files:**
- Modify: `zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c`
- Modify: `zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface_support.c`
- Modify: `zr_vm_language_server/src/zr_vm_language_server/semantic/lsp_semantic_query.c`
- Modify: `zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer*.c`
- Modify: `zr_vm_language_server/src/zr_vm_language_server/symbol_table.c`
- Modify: `zr_vm_language_server/src/zr_vm_language_server/reference_tracker.c`
- Test: `tests/language_server/test_lsp_semantic_query_parity.c`

- [ ] 按 consumer 迁移：definition/references/highlights/rename → completion → hover/signature/inlay → semantic tokens → diagnostics。
- [x] `reference_tracker` 改为 SymbolId/relation index projection；修复 source URI 为空即相等的问题。跨文件引用不能靠同名与坐标匹配。
- [ ] 每迁移一个 consumer，添加 source/binary/native + stale snapshot + unresolved exactness 测试，再删除对应 LSP fallback。
- [ ] 最终删除 LSP 自有 typecheck/reference collection/symbol scope 推断；如果少量语法恢复仍需 LSP analyzer，命名为 syntax recovery 并禁止产生 canonical semantic facts。
- [ ] source contract test 阻止新增生产代码调用 token/name/type text semantic reconstruction helper。

## Task 8：完成门禁

- [ ] parser query contract tests 在 GCC/Clang/MSVC 通过。
- [ ] 同一 fixture 的 source、`.zro`、native descriptor 返回相同 canonical signature/TypeId relation；允许 declaration origin URI 不同。
- [ ] declaration/definition/implementation、references、rename、call/type hierarchy 的目标集合只按 stable ids 产生。
- [ ] LSP semantic/type/reference 重复实现的生产代码显著删除；不以移动到另一个大文件代替收敛。
- [ ] 更新 parser 与 language server module docs，列出 API lifetime、exactness 和 snapshot ownership。

## 状态与产出记录

- 最近更新时间：2026-08-31 11:37 +08:00。
- 总体状态：进行中。Task 7.55 让LSP symbols analyzer在已有canonical declaration
  SymbolId/TypeId/range时，通过`RegisterCanonicalVariable`注册type binding；Web URI local
  use不再同时发布一个无声明的竞争SymbolId，GCC/Clang interface由fixed8精确降为fixed7。
  Task 6.41 删除interface const-field在LSP symbols analyzer中的
  violation枚举、diagnostic builder与fact append循环，只调用parser-owned persistent-fact
  publisher；GCC/Clang source-contract与LSP diagnostics `70/19`均真实exit 0，既有
  interface const-field双诊断专项保持通过。Task 1.5 将`PropertyAt` reference/contract range匹配收紧为exact
  optional source identity，并修复property code-action consumer使其保留当前document URI；
  sourceless同offset请求不再命中sourced property，LSP不按名称恢复。GCC/Clang property/facts/
  query/contract/canonical/parity/source-contract `11/17/30/6/21/15/70`均真实exit 0，property
  refactor恢复PASS，interface保持fixed8、delta 0。Task 1.4 将ownership dataflow的statement归属、region release与
  weak receiver wake range fallback收紧为exact optional source identity；不同AST节点即使
  offset相同，也不能靠单边缺source跨snapshot产生move/release/wake observation。GCC/Clang
  facts/query/parser diagnostic/compiler diagnostic/LSP diagnostic/parity/source-contract/
  type-inference `17/30/13/64/19/15/70/124`均真实exit 0，interface保持fixed8、delta 0。
  Task 1.3 将CFG-backed definite-assignment与reaching-definition
  的AST/fact source比较从one-sided `NULL`通配收紧为exact optional identity；sourceless
  reference fact不再凭相同offset进入sourced CFG并获得assignment state或definition range。
  GCC/Clang facts/query/parser diagnostic/compiler diagnostic/LSP diagnostic/parity/source-contract/
  type-inference `16/30/13/64/19/15/70/124`均真实exit 0，interface保持fixed8、delta 0。
  Task 6.40 将resolved reference抑制unresolved diagnostic的duplicate key
  收紧为exact source/range/name identity；单边缺source不再跨snapshot隐藏诊断，GCC/Clang
  parser/compiler/LSP diagnostics与parity/source-contract `13/64/19/15/70`均真实exit 0，
  interface保持fixed8、delta 0。Task 2.4 将`VisibleSymbols` lexical scope selection的source比较从
  one-sided `NULL`通配收紧为exact optional identity；sourceless position不再按相同offset进入
  sourced scope，GCC/Clang symbols/parity/source-contract `22/15/70`均真实exit 0，interface
  保持fixed8、delta 0。Task 1.2 将底层semantic fact lookup与canonical position query的
  source比较统一收紧为exact optional identity；`FactsAt`与`CanonicalTypeAt`不再让单边缺失
  source的range凭相同offset跨snapshot命中，GCC/Clang facts/query/contract/calls/parser diagnostic/
  compiler diagnostic/reference/property/canonical/parity/source-contract/type-inference
  `15/30/6/30/12/64/6/11/21/15/70/124`均真实exit 0，interface仍为fixed8、delta 0。
  Task 4.27 让同一最优range/exactness priority下结构互相矛盾的
  call expression facts fail closed，不再按append order选择call target；GCC/Clang calls/query/
  canonical/parity/source-contract `30/30/21/15/70`均真实exit 0。Task 6.39 将公共semantic-query source比较从one-sided `NULL`
  通配收紧为exact optional identity；source-unknown node scope不再仅凭重叠offset吸入
  source-known diagnostic，GCC/Clang query/contract/parser diagnostic/compiler diagnostic
  `30/4/12/64`均真实exit 0。Task 4.26 让等宽、同callsite但owner SymbolId不同的function scopes
  fail closed为`CALLER_UNAVAILABLE`，不再按scope append order归属call edge；GCC/Clang
  relations/query/symbols/calls/parity/source-contract `23/30/21/29/15/70`全部真实exit 0，
  caller index/incoming/outgoing项已勾选。Task 3.17 收紧relation append结构门禁，source/target endpoint各自必须
  提供有效SymbolId或TypeId；GCC/Clang relation/query/symbol/call/parity/source-contract
  `23/30/21/28/15/70`全部真实exit 0，并据实勾选relation graph与四查询API两项。Task 2.3 已把`SymbolAt`/`VisibleSymbols` API与compiler-owned scope审计
  checkbox对齐；当前GCC/Clang parser symbols `21/21`、semantic parity `15/15`、LSP source
  contracts `70/70`真实exit 0，lexical completion不再调用analyzer/symbol-table scope扫描。Task 1.1 已把主计划Task1的四个stale checkbox与既有完成record对齐；
  当前GCC/Clang query contract `4/4`、source/binary/native parity `15/15`真实exit 0，borrowed
  snapshot、query purity、repeat stability与exactness fail-closed合同未改变。Task 4.25 已冻结unresolved call edge reason矩阵：有效target缺声明坐标时
  保留SymbolId并返回`TARGET_DECLARATION_UNAVAILABLE`，resolved id指向非函数时清零target并返回
  `TARGET_UNRESOLVED`，同名function不能替代canonical id。Task 4.24 已让source `ref/out` call mapping消费parser保存的structured
  marker range，返回完整`ref value`/`out value` argument range；`in`保持expression range，
  passing mode与两侧TypeId继续来自selected canonical callable，LSP无需扫描source keyword。
  Task 5.17 已把canonical graph唯一tuple marker的旧keywordless
  fixture切换到当前`fn pair(): [int, bool]`合同，tuple AST/TypeId/formatter链恢复真实GREEN，
  未在生产parser或formatter兼容旧语法；Task 6.38 已让一个canonical query fact清除同identity的多重stale
  analyzer rows，保留最早槽位并完整替换，最终只发布一个protocol diagnostic；Task 6.37
  已把source identity纳入canonical diagnostic duplicate key；
  一侧source缺失时，即使offset/code相同也fail closed并保留两条diagnostic，禁止跨document/
  snapshot替换。Task 6.36 已让canonical semantic-query diagnostic在同一exact range与
  stable code命中旧LSP analyzer diagnostic时替换完整投影，不再只合并relatedInformation并保留
  stale severity/message/descriptor/fix disposition；Task 5.16 已以semantic display public API直接覆盖
  `Unique<int>`、`Shared<int>`、`Weak<int>`与`AtomicShared<int>`；四种owner variants在首轮
  GCC/Clang characterization即全部通过，未制造production修复。Task 5.15 已让direct type-value aliases在`readonly`、`ref`和
  `ref readonly` source wrappers下发布inner target alias；outer canonical分别保持
  `readonly Document`、`ref Document`、`ref readonly Document`，exact `Word` range只绑定
  inner `Document` TypeId。Task 5.14 已在GcBridge target通过canonical ordinary/resource class world
  校验后、附加bridge kind前重放type-value alias producer；`Gc<DocAlias>`与
  `GcBox<ResourceAlias>`分别保持canonical `Gc<Document>`/`GcBox<BoxedResource>`，inner exact
  ranges返回source aliases且不会绑定outer wrapper TypeId。Task 5.13 已在ownership wrapper恢复semantic context后重放structured
  type-value alias producer；`Unique<Word>`保持canonical文本`Unique<int>`，inner canonical
  `int` TypeId在exact `Word` range查询到source alias，且outer/inner identity不混用。Task 5.12
  已在canonical type-value alias table命中后发布direct use-site alias；
  `Word -> int`保持canonical文本`int`，exact `Word` annotation range查询到source alias `Word`，
  不注册同名nominal type。Task 5.11 已把generic source alias构造与canonical const evaluator分离；
  `Matrix<i64, 2 + 2>`的canonical文本为`Matrix<int, 4>`，exact whole-use alias仍保留structured
  source presentation `Matrix<i64, 2 + 2>`，不切source文本。Task 5.10 已为generic type use发布独立authoritative whole range；
  `Box<i64>`的canonical文本为`Box<int>`，source alias只在包含closing angle的exact range可用，
  nested `Box<Box<i64>>`对outer/inner分别保留精确range，并维持既有generic AST node location
  合同不变。Task 5.9 已让structured qualified resolver在最终canonical inferred type
  形成后发布whole name-chain use alias；`declaration.Patch`的canonical文本为
  `zr.compile.declaration.Patch`，source alias只在exact whole-use range可用，wrapped-qualified
  缺authoritative整体range时fail closed。Task 5.8 已补齐 ownership generic 临时关闭 semantic context造成的内层
  alias缺口：`Unique<i64>`的外层canonical文本为`Unique<int>`，内层primitive TypeId在exact
  `i64` range查询到source alias，outer/inner identity不混用。Task 5.7 已让显式 primitive type-use 在完成 canonical inference 后，
  以 exact type-name range 发布 source spelling alias；`i64` 的 canonical display 仍为 `int`，
  alias query仅在相同 TypeId/source/range返回`i64`，不改变推断、兼容性或诊断。Task 5.6 已发布 snapshot-scoped `(TypeId, exact use range) -> alias`
  display fact，canonical identity文本与use-site alias不再共用formatter字段；Task 5.5 已让 nominal interner 与 formatter共同拒绝空 type name，避免
  分配有效 TypeId或成功输出空 canonical label；Task 5.4 已让 SymbolId callable signature 从同一 canonical function
  `TypeId` 投影 `async`、`generator` 与 `throws`，并以 value/in/ref/ref readonly/out 完整
  parameter contract 冻结 effect/passing display；Task 5.3 已让 generic parameter/instance、array 与 union formatter
  校验完整 composite identity/cardinality，rank 0、unknown storage、invalid owner 与空集合均
  fail closed；Task 5.2 已将 const generic parameter 的 canonical display 从可选
  `displayName` 分离，统一输出稳定 owner SymbolId + ordinal identity，避免同一 TypeId 文本依赖
  alias intern 顺序；Task 5.1 已把 canonical parameter contract 的完整校验发布为
  intern/formatter/callable display 共用的只读 API，并让 malformed ref access、receiver/effect、
  passing/escape/init/temporary/call-site snapshot 一律 fail closed；Task 4.23 已把 non-empty
  structured argument mapping 从“字段非空”收紧为
  selected callable contract一致性门禁：parameter binding必须唯一，`TypeId`、passing mode与
  exact/implicit conversion必须彼此一致，损坏snapshot清空输出并fail closed。
- 固定 GCC/Clang 快照中的 parser/display/call/query/query-contract/relation/symbol/parity/source-contract 门禁分别为
  `74/22/30/30/6/23/22/15/70`，并补 canonical consumers `21/21`、semantic-facts `15/15`、
  type inference `124/124`，均真实
  exit 0；interface 当前保留7个既有producer marker，Task 7.55关闭Web URI local identity
  marker。
  receiver/member 与 `.zro`/native mapping parity、receiver `TypeId`、完整 16-target matrix、
  三套 stdio smoke 和 Syntax05 imported declaration identity producer
  尚未完成，Task 7/Task 8 不声明 Plan 03 GREEN或完成。
- Task 6.36 fixed GCC/Clang diagnostics gate：parser semantic-query diagnostics `11/11`、
  compiler semantic-query diagnostics `64/64`、LSP semantic-query diagnostics、parity与
  source-contract均真实exit 0；interface仅保留同一8个既有producer markers，delta 0。未运行
  MSVC、完整16-target matrix或三套stdio smoke。
- Task 6.37 fixed GCC/Clang LSP diagnostic `18/18`，并重放parser diagnostics `11/11`、
  compiler diagnostics `64/64`、parity/source-contract真实exit 0；interface保持同一8个既有
  producer markers，delta 0。未运行MSVC、完整16-target matrix或三套stdio smoke。
- Task 6.38 fixed GCC/Clang LSP diagnostic `19/19`，parser/compiler diagnostics仍为
  `11/11`与`64/64`，parity/source-contract真实exit 0；interface fixed 8 markers，delta 0。
  未运行MSVC、完整16-target matrix或三套stdio smoke。
- Task 6.39 fixed GCC/Clang semantic query `30/30`、query contract `4/4`、parser
  diagnostics `12/12`与compiler diagnostics `64/64`，均真实exit 0。未运行MSVC、完整
  16-target matrix或三套stdio smoke。
- Task 1.2 fixed GCC/Clang semantic facts/query/query contract/calls/parser diagnostic/compiler
  diagnostic/reference/property/canonical/parity/source-contract/type-inference
  `15/30/6/30/12/64/6/11/21/15/70/124`，均真实exit 0；interface保持同一fixed8，
  delta 0。未运行MSVC、完整16-target matrix或三套stdio smoke。
- Task 2.4 fixed GCC/Clang semantic query symbols `22/22`、parity `15/15`与source-contract
  `70/70`，均真实exit 0；interface保持同一fixed8，delta 0。未运行MSVC、完整16-target
  matrix或三套stdio smoke。
- Task 6.40 fixed GCC/Clang parser/compiler/LSP semantic-query diagnostics `13/64/19`、
  parity `15/15`与source-contract `70/70`，均真实exit 0；interface保持同一fixed8，delta 0。
  未运行MSVC、完整16-target matrix或三套stdio smoke。
- Task 7.55 fixed GCC/Clang semantic-query symbols/parity/LSP diagnostics/property
  consumers/source contracts `22/15/19/11/70`，均真实exit 0；两套interface均真实exit 1，
  Web URI local navigation转为PASS且失败集合从fixed8精确降为fixed7。未运行MSVC、完整
  16-target matrix或三套stdio smoke。
- Task 5.17 fixed GCC/Clang canonical graph `19/19`，parser/display分别`74/74`与
  `22/22`，均真实exit 0；仅修正测试fixture，未重跑interface、MSVC、完整16-target matrix或
  三套stdio smoke。
- 本阶段完成项目：Task 7.55 canonical local binding identity；Task 6.40 unresolved diagnostic source identity；Task 2.4 visible-symbol source identity；Task 1.2 semantic fact source identity；Task 4.27 ambiguous call expression fail-closed；Task 6.39 query-scope source identity；Task 4.26 ambiguous caller identity fail-closed；Task 3.17 relation endpoint identity integrity；Task 2.3 symbol-query state reconciliation；Task 1.1 query-contract state reconciliation；Task 4.25 unresolved call reason matrix；Task 4.24 source argument passing ranges；Task 5.17 canonical tuple fixture contract；Task 6.38 canonical diagnostic multiplicity collapse；Task 6.37 diagnostic source identity fail-closed；Task 6.36 canonical diagnostic duplicate replacement；Task 5.16 owner variant display acceptance；Task 5.15 reference/readonly type-value alias producer；Task 5.14 GcBridge type-value alias producer；Task 5.13 wrapped type-value alias producer；Task 5.12 type-value alias producer；Task 5.11 const-generic expression alias；Task 5.10 generic type-use alias range；Task 5.9 qualified type-use alias producer；Task 5.8 ownership wrapper inner primitive alias producer；Task 5.7 primitive type-use alias producer；Task 5.6 use-site type display alias fact foundation；Task 5.5 nominal
  display identity integrity；Task 5.4 callable
  effect/passing display integrity；Task 5.3 composite
  display integrity；Task 5.2 const generic display
  identity；Task 5.1 canonical display
  integrity；Task 4.23 argument mapping
  canonical-contract integrity；Task 4.22 source
  super-constructor argument mapping；Task 4.21 source
  constructor argument mapping/conversion；Task 4.20
  source free-call argument mapping/conversion；Task 4.19
  call-expression exactness refinement；Task 4.18 call source
  identity exactness；Task 4.17 resolved call-target
  conflict detection；Task 4.16 resolved
  call-reference refinement；Task 4.15 overload member
  completeness；Task 4.14 overload-set record
  exactness；Task 4.13 overload selected-target
  atomic consistency；Task 3.16 relation
  deterministic structured ordering；Task 3.15 relation
  node-scope exact source identity；Task 4.12 call-edge
  deterministic coordinate ordering；Task 4.11 call-edge
  line-only range identity；Task 4.10 call-edge exact
  source identity；Task 4.9 call-edge refinement
  merge；Task 3.14 relation append exact
  identity deduplication；
  Task 6.30 exact-type inference diagnostic query projection；
  Task 6.31 raw semantic-analyzer diagnostic escape-hatch removal；Task 6.32
  ownership return-escape canonical producer/query/LSP projection；Task 6.33
  variance persistent-fact producer migration；Task 6.34 const-assignment
  persistent-fact producer migration；Task 6.35 interface const-field parser
  producer support。Task 6.35 已通过三工具链 `1/11/64` focused 门禁，覆盖
  drop-const 与 missing-field 两类 persistent facts，且不改变 compiler error；
  Task 7.1 reference source identity fail-closed，已通过三工具链
  `4/2/3/54` 与三处 `2/2` byte audit；Task 7.2 已把 reference tracker 从
  name/wrapper storage 迁移到 copied SymbolId index，并在 analyzer 的单一 AST
  snapshot 边界补全 query source，三工具链通过 `5/2/3/55` 与三处 `7/7`
  byte audit；Task 7.3 已把 source-local references/highlights 从 tracker
  迁移到 `DeclarationOf/ReferencesOf`，覆盖 tracker-detached、version 2
  re-resolution 与 invalid SymbolId exactness，三工具链通过 `4/56/2/5` 与
  三处 `5/5` byte audit；Task 7.4 已把 project source-symbol fallback 从
  tracker 迁移到同一 SymbolId relation projector，并区分 query success 与
  zero-local append，三工具链通过 `56/4` focused 门禁、project process exit
  0，且 GCC project parent/overlay 的 6 个既有 marker delta 为 0，三处
  `4/4` byte audit；Task 7.5 已删除 local definition 的 request URI、LSP
  symbol location 与 enum/name range fallback，统一消费
  `DefinitionsOf/DeclarationOf(SymbolId)` 和 analyzer snapshot source，三工具链
  通过 `57/3/4` focused 门禁，interface 保持 `109 Pass / 4` 既有 marker、
  delta 0，三处 `4/4` byte audit；Task 7.6 已由 compiler prototype contract
  发布 source type/interface/member/override relation，LSP implementation 仅消费
  `ImplementationsOf(SymbolId)`，同名无关 member 不进入结果，三工具链通过
  `19/5/58/3/5` focused 门禁，interface 保持 `109 Pass / 4` 既有 marker、
  delta 0，三处 `11/11` byte audit；Task 7.7 已删除 source-local type
  hierarchy 的 inheritance header/document-symbol/name scan，改为携带
  SymbolId/TypeId/version 并消费 `BaseTypesOf/DerivedTypesOf`，覆盖 same-name、
  stale snapshot 与 stdio identity roundtrip，三工具链通过 `19/6/59/73`
  focused 门禁和 stdio smoke，interface 仍为 `109 Pass / 4`、delta 0，三处
  `10/10` byte audit；Task 7.8 已删除 source-local call hierarchy 的
  source-line/document-symbol/name scan，改为消费 `IncomingCalls/OutgoingCalls`
  并携带 SymbolId/TypeId/version，覆盖 same-AST duplicate records、display-name
  mutation、multiple fromRanges 与 stale snapshot，三工具链通过
  `11/7/60/73` focused 门禁和 combined stdio smoke，interface 仍为
  `109 Pass / 4`、delta 0，三处 `10/10` byte audit；Task 7.9 已将
  source-local receiver method 与 variable-bound lambda call hierarchy 接入
  同一 parser call-edge consumer，lambda item 仅由 SymbolId/TypeId、
  `DeclarationOf`、exact ranges 与 version 重解析，覆盖 same-name method、
  returned lambda outgoing、display-name mutation 与 range tampering；三工具链
  通过 `11/9/60/73` focused 门禁和 combined stdio smoke，interface 仍为
  `109 Pass / 4`、delta 0，三处 `5/5` byte audit；Task 7.10 新增 exact
  `DeclaredSymbols` snapshot query，并把 inlay declaration enumeration 从 LSP
  symbol table 与 request-time inference 迁移到 SymbolId/TypeId/declaration fact，
  三工具链通过 parser `20/20`、focused inlay `11/11`、source contract
  `61 Pass` 与 focused stdio smoke；interface 保持同一 `109 Pass / 4`、delta 0，
  full stdio 在本项场景前被既有 generic short-circuit diagnostic 缺失阻断，
  两者均未计 GREEN；Task 7.11 删除 completion detail 的 request-time exact
  inference/materializer，缺失 initializer fact 时保持 snapshot 不变并省略
  optional detail，三工具链通过 focused semantic facts `12/12` 与 source
  contracts `62 Pass`，interface 仍为同一 `109 Pass / 4`、delta 0。
  Task 7.12 同样删除 signature argument documentation 的 request-time exact
  inference/materializer，缺失 argument fact 时保持 snapshot 不变并省略
  optional documentation；三工具链通过 focused semantic facts `13/13` 与
  source contracts `63 Pass`，interface 仍为同一 `109 Pass / 4`、delta 0。
  Task 7.13 删除 local expression/hover query 的 request-time inference、
  inferred-type registration、symbol-table lookup 与 position type resolver
  回填；三工具链 expanded GREEN `7/7`、source contracts `64 Pass`，fixed
  baseline 对照确认 local-query 的 3 个、local-hover 的 2 个既有 producer
  marker 集合不变，interface 仍为同一 `109 Pass / 4`、delta 0。
  Task 7.14 将 CodeLens declaration/reference count 从 LSP symbol-table scope
  walk 与 position-based `FindReferences` 重入迁移到
  `DeclaredSymbols + ReferencesOf(SymbolId)`；三工具链通过 focused `4/4`、
  source contracts `65 Pass`、advanced editor features `73/73`；project
  features 保持 `54 Pass / 6` 个既有 marker、watched-project CodeLens 通过；
  interface 保持同一 `109 Pass / 4`、delta 0，WSL/MSVC overlay 均 `5/5`
  byte-exact；
  stale snapshot 与 unresolved declaration identity 均 fail closed，同一精确
  reference range/declaration AST 的重复 producer row 只投影一次。跨项目
  imported、binary/native CodeLens 仍等待 ModuleIdentity relation producer，
  未复用 name-keyed imported aggregation。Task 7.15 删除 call hierarchy
  follow-up 的 `symbolTable->allScopes` display-symbol 回查与 position-based
  re-resolution，改为按 URI/version/SymbolId/TypeId、semantic declaration range
  和 `DeclarationOf` selection range 重解析；三工具链通过 semantic parity
  `9/9`、source contracts `65/65`、advanced editor `73/73` 与 combined stdio
  smoke，project 保持 `54 Pass / 6`、interface 保持 `109 Pass / 4`，两者
  marker delta 均为 0。detached symbol table 与 unresolved declaration 均
  fail closed，未增加名称 fallback。Task 7.16 同样删除 type hierarchy
  follow-up 的 position re-resolution 与 `symbolTable->allScopes` relation-target
  回查，改由 URI/version/SymbolId/TypeId、semantic declaration range、
  `DeclarationOf` selection range 和 `BaseTypesOf/DerivedTypesOf` target ids
  重解析；三工具链通过同一 `9/65/73` 与 combined stdio 门禁，project/interface
  marker delta 仍为 0。detached symbol table、unresolved declaration 与 stale
  version 均 fail closed。Task 7.17 进一步删除 call/type hierarchy prepare 对
  LSP symbol table 与通用 position resolver 的依赖，改由 source-bound
  `SymbolAt`、exact declaration AST identity 和 `DeclarationOf` 构造初始 item；
  三工具链继续通过同一 `9/65/73` 与 combined stdio 门禁，project/interface
  固定 marker delta 为 0。prepare 前 detached symbol table 的 type/method RED
  均已转 GREEN，未增加 name/token fallback。Task 7.18 将 signature help 的
  compiler-state 门禁后移到 canonical local/external query之后；detached
  compilerState/symbolTable 的 direct-call RED 已转 GREEN，call payload缺失仍
  fail closed。三工具链通过 `9/65` focused门禁，interface保持`109/4`且新增
  case三套均PASS，project保持`54/6`，两组marker delta均为0。Task 7.19 删除
  source direct-function signature的compiler overload、symbol-table、callee-name、
  initializer AST与argument-count fallback；parser用结构化callable-value binding
  identity让identifier alias/lambda在lexical shadowing下继续发布canonical call
  facts，同时保持普通变量遮蔽同名函数。三工具链通过parser/query
  `19/124/30/127`与LSP`9/65` focused门禁；interface均为外部overlay固定
  `111 Pass / 2 Fail`且本项signature cases全部PASS。MSVC external 47-case的
  单一Weak callable marker在parent/overlay一致，delta 0，未计GREEN。Task 7.20
  继续删除receiver method signature的临时receiver AST、request-time
  `ExpressionType_Infer`、prototype/AST member-name搜索与本地generic闭合，共净删
  647行第二套语义；三工具链通过`65/9` focused门禁，interface固定`111/2`、
  project固定`56/4`，GCC parent/overlay A/B及三套marker集合均无delta。
  Task 7.21 为source class `new`与struct `init`发布closed constructor callable TypeId、
  resolved SymbolId/declaration range、参数/命名实参与`@constructor` display；signature
  consumer在compiler-state门禁前只消费`CallAt/FormatCall`，detached snapshot保持可用，
  payload移除后fail closed。三工具链通过`20/30/9/65` focused门禁；interface固定
  `111/2`、project固定`56/4`，source/native constructor cases全部PASS且marker delta为0。
  Task 7.22 为 source class `super(...)` 保存 exact call range 并发布 closed constructor
  callable TypeId、resolved SymbolId/declaration range 与 `@constructor` display；signature
  consumer 删除 request-time base prototype/constructor resolver，在 compiler-state 门禁前
  只消费 `CallAt/FormatCall`，payload 缺失时 fail closed。三工具链 canonical consumer
  均 `21/21`、source contracts 均 exit 0；interface 固定 `111/2` 且 super
  signature/navigation cases 全部 PASS。三套 full stdio 仍被既有 generic
  `short_circuit_unreachable` 缺失前置阻断，未计 GREEN；三套 CLI `--version` 均 exit 0。
  Task 7.23 将 source lexical completion 从 LSP symbol table/request-time analyzer completion
  迁移到 `VisibleSymbols`，并给 symbol query 增加 structured kind；scope producer 补齐 extern
  function/delegate/struct/class/interface/enum 与 source callable named signature。三工具链均通过
  parser symbols `21/21`、semantic parity `10/10`、hover `11/11`、inlay `13/13` 与 source
  contracts；interface 固定 `111/2`、project 固定 `56/4`，marker delta 为 0。三套 full stdio
  仍在同一既有 `short_circuit_unreachable` 缺失处退出 1，未计 GREEN；三套 CLI `--version`
  均 exit 0。Task 7.24 将 source-local hover 从 analyzer `GetHoverInfo`、token-name lookup 与
  symbol-table semantic fallback 迁移到 `SymbolAt` copied view；query 新增 exact reference range，
  独立 projector 只格式化 stable SymbolId、TypeId、signature 与 declaration identity。三工具链
  semantic parity 均 `11/11`、parser symbols 均 `21/21`；interface 均保持 `111/2`，extern
  delegate/struct/enum 与 layout metadata 回归全部 PASS，marker delta 0。detached analyzer
  symbol table/reference tracker/AST 后 source hover 仍可用，缺失 fact 时 fail closed。Task 7.26
  收紧 external metadata type-member reference matcher：任一侧有 declaration identity 时必须双方
  URI/range 完全一致，不一致后不再按 module/type/member spelling 回退；双方都无 declaration 时
  才保留 structured metadata contract。三工具链 semantic parity 均 `12/12`、source contracts
  均真实 exit 0，project suite 均真实 exit 0 且保持同一 9 条既有 marker；binary/native
  references 与 highlights 全部 PASS，GCC parent/overlay marker delta 为 0。
  Task 7.27 为 resolved semantic query 增加 source document version identity：version 1 query
  在同一 URI 更新到 version 2 后，hover/definition/references/document highlights 全部统一
  fail closed，不再读取 stale analyzer/fact view。三工具链 semantic parity 均 `13/13`、source
  contracts 均真实 exit 0，project suite 均保持同一 9 条既有 marker，新增 marker 0；binary/
  plugin metadata URI 无 source version 时保留既有 provider consumer 边界，不按虚拟 AST/name
  伪造版本。Task 7.28 进一步关闭 non-TYPE unresolved reference 的 local symbol fallback：parser
  已发布 fact 但 `isResolved=false` 或 SymbolId invalid 时，`ResolveAtPosition` 立即 fail closed，
  不再把 LSP symbol table 同名项提升为 semantic target。三工具链 semantic parity 均 `14/14`、
  source contracts 均真实 exit 0；interface 保持 `111 Pass / 2`、project 保持 `51 Pass / 9`，
  marker delta 均为 0。过宽的 all-reference 实现因 closed-generic unresolved TYPE producer 新增
  1 marker而被否决，未计 GREEN。
  Task 7.29 将 local definition/references/document highlights 的主 identity 从 raw `SZrSymbol *`
  迁移到 copied `canonicalSymbol.symbolId`；detached raw symbol 后 version 1 仍返回 `1/3/3`，version 2
  返回 `1/4/4`，invalid copied id fail closed。三工具链 semantic parity 均 `14/14`、source
  contracts均真实exit 0，interface保持`111 Pass / 2`、project保持`51 Pass / 9`，marker delta为0。
  GDB审计确认 extern function canonical/raw id=`10/3`、web URI local=`2/1`；consumer未猜测哪一侧
  正确，冲突时保留旧identity并等待producer收口。
   Task 7.30 将 local implementation navigation 的 relation key 迁移到 copied `canonicalSymbol.symbolId`；
   detached analyzer symbol table 后仍返回相同 implementation location。三工具链 semantic parity 继续
   为 `14/14`、source contracts 真实 exit 0，interface/project marker 集与 Task 7.29 完全一致。
   Task 7.31 将 lexical completion 的 documentation metadata 迁移到 parser
   `DocumentationOfSymbol(SymbolId)`：completion projector 在 query snapshot 内按 exact id 读取并复制
   documentation，脱离 analyzer symbol table 与 document AST 后仍保留文档。GCC parity 为 `14/14`、
   source contracts 真实 exit 0，未新增 marker。
   Task 7.32 将 source hover 的 documentation metadata 同样迁移到 parser
   `DocumentationOfSymbol(SymbolId)`：hover projector 在 canonical `SymbolAt` 后按 exact id 合并并复制
   documentation，脱离 analyzer symbol table/reference tracker/AST 后仍保留文档。GCC parity 为
   `14/14`、source contracts 真实 exit 0，未新增 marker。
   Task 7.33 完成 diagnostics/semantic-token consumer audit：semantic analyzer 的 query
   diagnostics 路径已由 `MaterializeDiagnostics`/`Diagnostics` 取得结构化 facts，并通过
   `Diagnostic_FromStructured` 投影 severity、range、code、descriptor、related information 与
   fixes；本次未发现新的 LSP 语义重建缺口。项目级跨模块 import/member unresolved diagnostics
   仍等待 producer/metadata ownership 收口，未计 consumer migration GREEN。semantic tokens 的
   source symbol-table/metadata-chain fallback 仍在 Syntax05 Task4 exact-owned
   `lsp_semantic_tokens.c`，本阶段不编辑、不增加兼容，也不声明迁移完成。
   Task 7.34 完成最终门禁复核（2026-08-30 05:56 +08:00）：GCC/Clang/MSVC 的
   canonical semantic-query parity 均为 `14/14`，source-contract、inlay 与 CLI
   版本检查均真实 exit 0；interface 每套仍有 `111 Pass / 2 Fail`，GCC/Clang 的
   analyzer 分别为 `68 Pass / 2 Fail`，project 每套为 `51 Pass / 9 Fail`。三套
   stdio smoke 均在通用 generic fixture 缺少 `short_circuit_unreachable` warning
   处 exit 1。该结果只记录门禁状态，不用 LSP 名称、类型文本或消息重建事实。
   Task 7.35（2026-08-30 06:56 +08:00）收口 analyzer typecheck 的 parser diagnostic
   bridge：exact expression inference 失败时，若 compiler 已发布 structured persistent
   diagnostic，LSP 只消费该 fact 并抑制重复的 `cannot infer exact type`；primary-call
   typecheck 也会排空同一 current compiler diagnostic。GCC/Clang/MSVC 的 interface
   reference-call case 均通过，且完整 16-target 构建/运行保持三工具链一致。剩余
   canonical graph、closed-generic/borrow-return analyzer、class-member/local/project
   fixtures 仍失败；三套 stdio smoke 仍在 `short_circuit_unreachable` producer warning
   缺失处 exit 1。该子里程碑已完成，但 Plan 03 Task 7/8 仍未完成。
   Task 7.36（2026-08-30 07:08 +08:00）修正 local semantic query 的 ownership 投影：
   查询点若落在结构化表达式的前缀（例如 `ref`），LSP 先按 expression fact 的 exact
   AST node 查询 ownership，再按该 expression 的 canonical range 选择最窄 ownership fact，
   不按名称、消息或源码文本推断。GCC/Clang/MSVC local-query 的 ownership violation
   case 均通过；短路 reachability 与 member-write reference 的 producer facts 仍失败，
   因而该子里程碑不扩大为全局 GREEN。
   Task 7.37（2026-08-30 07:29 +08:00）补齐 local semantic query 的短路 reachability
   投影：请求点没有直接 reachability fact 时，LSP 仅沿已有 logical fact 的结构化
   `relatedNode` 查询右操作数范围，不按名称、消息或源码文本推断。GCC/Clang/MSVC
   的 local-query short-circuit case 均通过；member-write reference 仍返回 parser
   producer 发布的错误 kind，未在 LSP 侧伪造或兜底。interface 仍保留既有 class-member
   fixture 失败，stdio smoke 仍在 `short_circuit_unreachable` producer warning 缺失处
   失败，Plan 03 Task 7/8 继续未完成。
   07:44 的有效三工具链 16-target 回放保持相同的 `10 PASS / 6 FAIL`，且各测试进程真实
   exit 与 marker 一致；三套 CLI `--version` 真实 exit 0。剩余失败均归属于已记录的
   parser/metadata producer 或既有 fixture，包括 tuple canonical graph、member-write
   reference、class-member interface、imported-type matrix、pooling guard 和 analyzer
   marker；不在 LSP 侧增加名称、类型文本或消息 fallback。
   Task 7.38 移除 local-symbol references consumer 中按 `query->symbol->name` 扩展
   跨项目 imported-member aggregation 的分支。source-contract regression 只检查
   `AppendReferences` local branch 不再读取该 name；local 结果完全来自 parser relation
   query，缺失 canonical cross-project relation 时保持 fail-closed。imported-member 与
   external-metadata 分支的 structured declaration identity 不变。Task 7.38 focused GREEN
   已于 2026-08-30 08:31 +08:00 完成；09:50 +08:00 的 post-commit gate 在 GCC/Clang/MSVC
   均确认 source-contract `70/70` 真实 exit 0，原定义 16-target 集合均为 `10 PASS / 6 FAIL`。
   三套 CLI `--version` 真实 exit 0；三套 stdio smoke 均在 generic fixture 缺失
   `short_circuit_unreachable` warning 处真实 exit 1。失败项全部落在已知 parser/metadata
   producer 或 fixture：tuple canonical graph、semantic analyzer producer markers、interface
   class/import fixture、local member-write reference query/hover、imported-type matrix；
   LSP 未增加名称、类型文本或消息 fallback。
- 当前边界：两项 direct-Weak receiver guard 失败在固定 parent 与 overlay
  均存在，不计入 Task 6.32 GREEN；Task 6.33 的 GCC analyzer parent/overlay
  另有相同 closed-generic 与 borrow-range 两项 marker，不计入 GREEN。后续
  必须在各自 canonical producer 层修复。其余 analyzer-owned semantic
  producers 继续逐项迁移；并行 Syntax05 当前持有 interface const-field 所在
  `semantic_analyzer_symbols.c`，所以 6.35 只完成 parser support，LSP local
  enumerator/builder/append loop 尚未删除，释放前不进入。禁止 LSP
  name/type-text/message/AST-pair fallback。Task 7 后续仍须把 navigation
  consumers 迁移到 canonical relation index，并覆盖 stale/unresolved exactness；
  source-local definition/references/highlights 与 project source-symbol fallback
  以及 source-local implementation 已完成，
  external metadata type-member references 已在 exact declaration 可用时 fail closed；
  跨项目 imported function references/highlights consumer 已删除 module/member 名称聚合，
  仅在 parser `SymbolAt` 提供有效 SymbolId，且 declaration URI/range 与 structured metadata
  完全一致时消费 canonical relation；Task 7.25 RED 已证明当前 imported declaration range
  仍可能为零，因此 identity 缺失时保持 fail-closed，并等待 Syntax05 producer 路径释放、
  closed-generic source type annotation 仍可能只发布 unresolved TYPE fact，等待同一 support-first
  producer 层收口；non-TYPE unresolved value/symbol query 已在 Task 7.28 fail closed；
  local navigation 已能在 canonical/raw id一致或raw pointer缺失时只消费copied SymbolId；extern/web
  URI 的 identity mismatch 仍等待 producer 修复，未计作 raw symbol 全面删除；
  implementation navigation 已能在 canonical/raw id一致或raw pointer缺失时只消费copied SymbolId；
  extern/web URI 的 identity mismatch 仍等待 producer 修复，未计作 raw symbol 全面删除；
  cross-project/binary/native external implementation/hierarchy、rename 和其余
  navigation consumers 仍未完成；source-local type hierarchy 与 free-function/
  method/lambda call hierarchy 已完成；
  source-local inlay declaration enumeration 已完成；
   source lexical completion 已仅消费 canonical VisibleSymbols，LSP symbol-table fallback 已删除；
   completion documentation 已仅消费 exact SymbolId documentation fact，并在 LSP item 内复制 snapshot view；
   source hover documentation 已仅消费 exact SymbolId documentation fact，并在 LSP-owned contents 内复制 snapshot view；
  source-local hover 已仅消费 canonical SymbolAt，analyzer hover 与 token-name fallback 已删除；
  completion semantic-fact enrichment 的 request-time mutation 已删除；
  signature semantic-fact enrichment 的 request-time mutation已删除；
  local expression/hover semantic query 的 request-time mutation 已删除；
  CodeLens declaration enumeration/reference count 已迁移到 canonical query；
  source-local call hierarchy follow-up 已脱离 LSP symbol-table scope；
  source-local type hierarchy follow-up 已脱离 LSP symbol-table scope；
  source-local call/type hierarchy prepare 已直接消费 parser SymbolAt；
  source direct-call signature dispatch 已可只依赖 semantic snapshot；
  source callable alias/lambda已在lexical shadowing下发布canonical call facts，source
  signature dispatch的compiler overload、symbol-table、callee-name与initializer AST
  fallback已删除；source receiver method的临时AST、request-time type inference、
  prototype/member-name搜索、AST method搜索与本地generic闭合fallback也已删除，
  receiver signature仅保留canonical/external adapters；
  source class/struct constructor已发布canonical call/target/declaration facts并在detached
  snapshot中消费；native/imported constructor仍保留structured metadata adapter，等待其
  producer parity后再迁移；
  reference tracker 的 null-source 与 name-keyed storage 缺口已关闭。三工具链
  full analyzer 仍仅保留相同两条既有 closed-generic/borrow-range marker，未计
  GREEN，也未在 LSP 增加兼容。Task 7.3 full interface 在 fixed parent 与
  overlay 均为相同 4 条既有 marker，delta 0，同样未计 GREEN。

- 本轮完成时间：2026-08-30 09:50 +08:00。
- 本轮状态：Task 7.38 focused GREEN；post-commit overall gate 未通过。完成三工具链
  source-contract `70/70`、原定义 16-target `10 PASS / 6 FAIL`、三套 stdio smoke 与
  CLI `--version` 的真实退出复核，并保持 LSP 不按名称、类型文本或消息重建语义。
- 本轮未完成项目：canonical tuple、member-write、imported-type、interface/analyzer
  producer 或 fixture 缺口，以及 `short_circuit_unreachable` producer warning；需待
  Syntax05/parser/metadata ownership 释放后再重跑完整门禁。

- 支持提交后复核时间：2026-08-30 10:07 +08:00。`7b7996d` 的 semantic-fact helper
  Windows 导出声明已同步到独立 GCC/Clang/MSVC 快照并重链；三工具链 16-target 结果
  仍为 `10 PASS / 6 FAIL`，source-contract 仍为 `70/70`，不改变上述阻塞归属。

- 补充完成时间：2026-08-30 10:26 +08:00。external type-member references/highlights
  现在要求 query 与 candidate 都有完整 declaration URI/range 且 exact 相等；缺失
  identity 或 range mismatch 均 fail closed。已删除 LSP 中按 member/module/owner-type
  name 匹配的 fallback，新增 parity regression；GCC/Clang/MSVC parity 与 source-contract
  均真实 exit 0。interface 与 stdio 的既有 producer/fixture failures 保持未完成，Task 7/8
  不标记 GREEN。

- 补充完成时间：2026-08-30 11:35 +08:00。Task 7.39 已将通用 stdio smoke 的 generic
  reachability 断言对齐 parser canonical `unreachable_code` fact 与 `Unreachable code`
  message，并保留 warning severity 校验；未增加 `short_circuit_unreachable` 兼容或任何
  名称/文本 fallback。`node --check` 与 `git diff --check` 通过。独立 GCC binary 因缺少
  `libzr_vm_lib_math.so` 未能提供有效运行时 smoke 证据，故不宣称 GREEN；补齐三工具链
  快照后仍须重跑 16-target matrix、stdio 与 CLI smoke，Task 7/Task 8 继续未完成。

- 补充完成时间：2026-08-30 12:26 +08:00。Task 7.40 将 local rename consumer
  收紧为 `hasCanonicalSymbol + canonicalSymbol.symbolId`，声明和引用统一通过
  `LspSemanticReferenceQuery` 投影 parser relation facts；canonical identity 缺失或与
  raw LSP symbol 不一致时 fail closed，不再使用 symbol lookup range/name。WSL GCC 对
  两个 production source 与 source-contract test 的 `-fsyntax-only` 均真实 exit 0，
  手工链接的 source-contract executable 真实 exit 0。该项未重跑完整 runtime matrix
  或 stdio smoke，Plan 03 Task 7/Task 8 继续未完成。

- 补充完成时间：2026-08-30 13:10 +08:00。Task 7.41 将 source-local semantic
  tokens 的 declaration enumeration 与 identifier/member classification 迁移到
  parser `DeclaredSymbols`/`SymbolAt` canonical facts；删除 analyzer symbol-table
  遍历、request-time semantic query、parameter name lookup 和 owner-type inference
  fallback。声明 role 投影为 declaration modifier，并同步 stdio token legend；新增
  source-contract 与 interface runtime regression。WSL GCC/Clang syntax checks 和
  source-contract executable 真实 exit 0，focused Ninja 因 CMake glob 校验 184 秒
  超时，尚无有效 interface runtime exit。外部 metadata chain producer、三工具链
  完整 16-target matrix 与三套 stdio smoke 仍未完成，Task 7/Task 8 不标记全局 GREEN。

- 补充完成时间：2026-08-30 13:55 +08:00。Task 7.42 将 imported-member references
  与 document highlights 的身份入口收紧为 parser `SymbolAt` 返回的有效 SymbolId 和
  exact declaration range；该范围还必须与 structured metadata 的 declaration URI/range
  完全一致。consumer 随后只调用 canonical `DeclarationOf/ReferencesOf` projector，已删除
  module/member name aggregation helper；identity 缺失或不一致时 fail closed。WSL GCC/Clang
  对 production source 的 `-fsyntax-only` 均真实 exit 0，重新编译运行的 source-contract
  executable 真实 exit 0。Syntax05 imported declaration producer、完整三工具链 16-target
  matrix 与三套 stdio smoke 仍未完成，Task 7/Task 8 不标记全局 GREEN。

- 补充完成时间：2026-08-30 14:22 +08:00。Task 7.43 删除已经没有调用者的
  `ProjectTryGetDefinition/FindReferences/GetDocumentHighlights` API、内部声明和
  `SZrLspProjectResolvedSymbol` raw-symbol transport；同时删除其独占的 imported-member
  AST/name resolver、global symbol name lookup 与 name-based cross-project reference bridge。
  主接口继续只走 `LspSemanticQuery` canonical consumer，active binary/native metadata
  adapters 保持不变。production 净删 519 行，source-contract RED 精确失败 9 项后转
  GREEN；WSL GCC/Clang production `-fsyntax-only` 与重新编译运行的 source-contract
  executable 均真实 exit 0。完整三工具链 16-target matrix、三套 stdio smoke 与 Syntax05
  producer 仍未完成，Task 7/Task 8 不标记全局 GREEN。

- 补充完成时间：2026-08-30 14:58 +08:00。Task 7.44 删除全仓无生产调用的
  `ReferenceTracker_FindReferences/GetReferenceCount/GetReferenceLocations`，并移除其
  唯一依赖的 `symbolToReferencesMap`、哈希池生命周期和无读取的 tracker state/table
  字段。`AddReference`、exact-source/range `FindReferenceAt` 与每条 reference 保存的
  SymbolId 保持不变。source-contract RED 精确失败 4 项后转 GREEN；固定
  `67bcd96 + 5 code/test overlays` 快照完成三目标重链，reference tracker 5/5 与
  source-contract 均真实 exit 0。semantic analyzer 中本任务触及的 creation/free 与
  local-reference case 均 PASS，整目标仍仅有计划已登记的 closed-generic 与 owner-generic
  两项 producer marker，真实 exit 1，不计本任务 GREEN，也未在 LSP 增加兼容。完整
  三工具链 16-target matrix、三套 stdio smoke 与 Syntax05 producer 仍未完成。

- 补充完成时间：2026-08-30 15:06 +08:00。Task 7.45 删除全仓无生产调用的
  `ZrLanguageServer_Symbol_GetReferenceCount` API，symbol-table focused test改为直接验证
  唯一仍在使用的 `references` range数组。source-contract RED 精确失败 getter一项后
  转 GREEN；最初拟删除 `referenceCount` 字段的草案在全仓审计中发现 Syntax05 exact-owned
  `test_lsp_interface.c`仍读取该字段作为失败诊断，因此字段、初始化、递增与数组均保留，
  未越权修改该测试。固定 `cdb214a + 4 code/test overlays` 快照完成 symbol-table、tracker、
  source-contract 与 interface test目标重链，interface目标编译/链接 exit 0；前三个运行目标
  分别4/4、5/5、全套PASS且真实 exit 0。完整三工具链16-target matrix、三套stdio smoke、
  Syntax05 producer及per-symbol数组最终删除仍未完成。

- 补充完成时间：2026-08-30 15:47 +08:00。Task 7.46 删除全仓无生产调用的
  `ZrLanguageServer_SemanticAnalyzer_GetCompletions` 声明/实现、其七个 completion helper
  闭包、两个末端常量/helper及三项纯死路径测试；九项混合 analyzer 测试只移除 completion
  分支，保留 hover、type、diagnostic 与 symbol-scope 断言。source-contract RED 在旧代码上
  精确失败声明/实现两项后转 GREEN。固定 `eb77fae + 5 code/test overlays` 的 GCC/Clang
  静态快照均完成 analyzer、source-contract、semantic parity 目标重链；source-contract
  69/69、semantic parity 15/15 均真实 exit 0，analyzer 两套均为 65 Pass/2 个相同既有
  closed-generic/owner-generic producer marker，真实 exit 1且不计本任务 GREEN。生产/测试
  净删约1016行第二套 completion 语义；完整三工具链16-target matrix、三套stdio smoke、
  Syntax05 producer及其余 symbol-table/analyzer consumers仍未完成。

- 补充完成时间：2026-08-30 16:06 +08:00。Task 7.47 删除全仓无调用的
  `ZrLanguageServer_SymbolTable_GetVisibleSymbolsAtPosition/GetSymbolsInRange` 公开声明、生产
  实现及范围查询独占 helper；completion 继续只消费 parser `VisibleSymbols`，未新增 scope、
  name 或 token fallback。source-contract RED 在旧代码上精确失败声明/实现四项后转 GREEN。
  固定 `64b8cff + 3 code/test overlays` 的 GCC/Clang 静态快照均完成 source-contract、
  symbol-table、semantic parity 与 interface 目标重链；前三个运行目标分别为 69/69、4/4、
  15/15 且真实 exit 0。GCC interface parent/overlay 失败集合均为同一 8 个已登记 producer
  marker，delta 0且不计本任务 GREEN。完整三工具链16-target matrix、三套stdio smoke、
  Syntax05 producer及其余 symbol-table/analyzer consumers仍未完成。

- 补充完成时间：2026-08-30 16:18 +08:00。Task 7.48 删除全仓无生产调用的
  `ZrLanguageServer_SymbolTable_AddSymbol` 公开声明与 wrapper 实现；五处 legacy
  symbol-table/reference-tracker test setup 改为直接调用活跃 `AddSymbolEx` 并忽略可选
  out-symbol，未新增兼容 helper。source-contract RED 在旧代码上精确失败声明/实现两项后
  转 GREEN。固定 `5826e0a + 5 code/test overlays` 的 GCC/Clang 静态快照均完成
  source-contract、symbol-table、reference-tracker、semantic parity 与 interface 目标重链；
  前四个运行目标分别为 69/69、4/4、5/5、15/15 且真实 exit 0。GCC interface 与 Task 7.47
  parent 的失败测试名称均为同一 8 个已登记 producer marker，delta 0且不计本任务 GREEN。
  完整三工具链16-target matrix、三套stdio smoke、Syntax05 producer及其余
  symbol-table/analyzer consumers仍未完成。

- 补充完成时间：2026-08-30 16:28 +08:00。Task 7.49 删除全仓零调用且编译器已报告
  unused 的 `semantic_member_property_text`、`semantic_import_chain_string_text`，并在重链后
  继续删除失去唯一调用者的 `semantic_identifier_node_text`。source-contract 首轮在旧代码
  上精确失败两项，follow-up RED 再精确失败尾随 helper 一项，最终转 GREEN；未新增名称、
  token 或类型文本 fallback。固定 `c575d8a + 3 code/test overlays` 的 GCC/Clang 静态快照
  均完成 source-contract、semantic parity、analyzer 与 interface 目标重链；前两项分别为
  69/69、15/15且真实 exit 0，analyzer 两套均保持同一 65 Pass/2 个已登记 producer marker。
  GCC interface 与 Task 7.48 parent 的失败测试名称均为同一 8 个已登记 producer marker，
  delta 0且不计本任务 GREEN。完整三工具链16-target matrix、三套stdio smoke、Syntax05
  producer及其余 analyzer/symbol-table consumers仍未完成。

- 补充完成时间：2026-08-30 16:44 +08:00。Task 7.50 删除全仓零调用的
  `virtual_builder_append_format` 及其独占 `<stdarg.h>` 依赖，保留活跃的 structured
  virtual-document renderer 与 native descriptor adapter。source-contract 在旧生产代码上
  精确 RED 一项后转 GREEN；固定 `eaad830 + 2 code/test overlays` 的 GCC/Clang 快照均完成
  language-server static library 与 source-contract 重链，source-contract 70/70 且真实
  exit 0。GCC interface 中五个 virtual-document/navigation case 全部 PASS；整体失败测试
  名称与 Task 7.47 parent 的 8 个已登记 producer marker 完全一致，delta 0且不计本任务
  GREEN。完整三工具链16-target matrix、三套stdio smoke、Syntax05 producer及其余
  analyzer/symbol-table consumers仍未完成。

- 补充完成时间：2026-08-30 16:55 +08:00。Task 7.51 删除全仓无调用的
  `ZrLanguageServer_LspSemanticReferenceQuery_AppendReferencesForSymbol` 声明与 wrapper
  实现；活跃 `AppendReferences(query)` 继续从 canonical SymbolId 直接调用内部 relation
  projector，未恢复 `SZrSymbol` 或 name fallback。source-contract 在旧生产代码上精确 RED
  header/source 两项后转 GREEN。固定 `c20a968 + 3 code/test overlays` 的 GCC/Clang 快照均
  完成 source-contract、local semantic query 与 semantic parity 重链；前后两项分别为
  70/70、15/15且真实 exit 0，local-query 两套均只保留同一 member-write unresolved producer
  marker。GCC interface 与固定 parent 的失败测试名称均为同一 8 个已登记 producer marker，
  delta 0且不计本任务 GREEN。完整三工具链16-target matrix、三套stdio smoke、Syntax05
  producer及其余 analyzer/symbol-table consumers仍未完成。

- 补充完成时间：2026-08-30 17:08 +08:00。Task 7.52 删除全仓无调用的
  `ZrLanguageServer_LspProject_FindImportedMemberHit` 声明/实现及其独占两层 request-time
  AST position walker；活跃 import-binding walker 与 imported-location projector保持不变，
  未新增 module/member name fallback。source-contract 在旧生产代码上精确 RED 四项后转
  GREEN。初次连续区间删除误含活跃 binding walker，链接失败轮已作废；按函数边界恢复后，
  固定 `78d862c + 3 code/test overlays` 的 GCC/Clang 快照均完成 source-contract、semantic
  parity 与 project features重链，前两项分别为70/70、15/15且真实 exit 0。project runner
  两套进程均 exit 0但日志含同一42 Pass/18个已登记 producer marker；GCC同构 parent/overlay
  及 GCC/Clang marker名称均delta 0，不计本任务 GREEN。GCC interface 与固定 parent仍为同一
  8个marker。完整三工具链16-target matrix、三套stdio smoke、Syntax05 producer及其余
  analyzer/symbol-table consumers仍未完成。

- 补充完成时间：2026-08-30 17:14 +08:00。Task 7.53 删除全仓无调用的
  `ZrLanguageServer_LspVirtualDocuments_FindModuleLinkDeclaration/FindTypeDeclaration` 声明与
  实现，保留活跃 `FindTypeMemberDeclaration/FindDeclarationAtPosition`、record collector及
  structured descriptor renderer。source-contract 在旧生产代码上精确RED source/header四项后
  转GREEN。固定 `2ad5abb + 3 code/test overlays` 的GCC/Clang快照均完成language-server
  static library与source-contract重链，source-contract 70/70且真实exit 0。GCC interface中
  五个virtual-document/navigation case全部PASS，整体与固定parent保持同一8个已登记producer
  marker，delta 0且不计本任务GREEN。Clang仍报告Syntax05 exact-owned metadata provider的五个
  既有unused helper，本任务未越权修改。完整三工具链16-target matrix、三套stdio smoke、
  Syntax05 producer及其余analyzer/symbol-table consumers仍未完成。

- 补充完成时间：2026-08-30 17:22 +08:00。Task 7.54 删除全仓无调用的
  `ZrLanguageServer_Lsp_ProjectEnsureProjectByProjectUri` 声明与完整实现；活跃
  `ZrLanguageServer_LspProject_GetOrCreateByProjectUri` 仍由interface调用并保留既有project
  load语义。source-contract在旧生产代码上精确RED source/header两项后转GREEN。固定
  `1cab9f1 + 3 code/test overlays` 的GCC/Clang快照均完成source-contract与project features
  重链；source-contract 70/70且真实exit 0，project两套均为进程exit 0、42 Pass/18个已登记
  producer marker，GCC/Clang失败名称delta 0。GCC interface与固定parent仍为同一8个marker，
  delta 0且不计本任务GREEN。完整三工具链16-target matrix、三套stdio smoke、Syntax05
  producer及其余analyzer/symbol-table consumers仍未完成。

- 补充完成时间：2026-08-30 18:05 +08:00。Task 4.10 收紧 parser call-edge 的 source
  identity：`NULL` 只与 `NULL` 相等，非空 source 按字符串值比较；缺失 source 的 call fact
  保留为 `CALLER_UNAVAILABLE`，但不能匹配已知文档 scope、位置查询或 callsite refinement。
  RED 为 call-query `13 Tests / 1 Failure`、`Expected 0 Was 1`；GREEN 后 GCC/Clang 均通过
  `13/30/20/15/70` focused 门禁。两套 interface 仍为同一8个既有producer marker，失败名称
  对 fixed parent 的 delta 0且不计 GREEN。本项未运行 MSVC、完整16-target matrix或三套
  stdio smoke，Plan 03 Task 7/Task 8 继续未完成。

- 补充完成时间：2026-08-30 18:11 +08:00。Task 4.11 修正 call-edge refinement 的
  callsite key：存在 offset 时比较 source + start/end offsets；双方均无 offset 时比较 source
  与完整 line/column range。RED 为 call-query `14 Tests / 1 Failure`、`Expected 2 Was 1`；
  GREEN 后 GCC/Clang 均通过 `14/30/20/15/70` focused 门禁。两套 interface 仍为同一8个
  既有producer marker，失败名称对 fixed parent 的 delta 0且不计 GREEN。本项未运行 MSVC、
  完整16-target matrix或三套stdio smoke，Plan 03 Task 7/Task 8继续未完成。

- 补充完成时间：2026-08-30 18:18 +08:00。Task 4.12 将 call-edge query 排序改为
  source identity + authoritative offset range / line-only完整坐标，再比较caller/target/resolution。
  RED 逆序发布第3行与第2行 facts，call-query `14 Tests / 1 Failure`、`Expected 2 Was 3`；
  GREEN 后 GCC/Clang 均通过 `14/30/20/15/70` focused 门禁。两套 interface 仍为同一8个
  既有producer marker，失败名称对 fixed parent 的 delta 0且不计 GREEN。本项未运行 MSVC、
  完整16-target matrix或三套stdio smoke，Plan 03 Task 7/Task 8继续未完成。

- 补充完成时间：2026-08-30 18:23 +08:00。Task 3.15 将 relation node-scope containment
  的 source 比较从 `NULL` 通配收紧为 exact optional identity。RED 为 relation-query
  `21 Tests / 1 Failure`、`Expected FALSE Was TRUE`；GREEN 后 GCC/Clang 均通过
  `21/30/21/14/15/70` relation/query/symbol/call/parity/source-contract 门禁。两套 interface
  仍为同一8个既有producer marker，失败名称对 fixed parent 的 delta 0且不计 GREEN。本项
  未运行 MSVC、完整16-target matrix或三套stdio smoke，Plan 03 Task 7/Task 8继续未完成。

- 补充完成时间：2026-08-30 18:33 +08:00。Task 3.16 将 relation query 排序从
  kind/id/offset 的不完整键收敛为 kind、source/target SymbolId/TypeId、module identity、
  source/target 完整 range、external classification 与 URI 的 structured total order；line-only
  range 使用完整 line/column 坐标，不再保留 append 顺序。RED 逆序发布第3行与第2行 facts，
  relation-query `22 Tests / 1 Failure`、`Expected 2 Was 3`；GREEN 后 GCC/Clang 均通过
  `22/30/21/14/15/70` relation/query/symbol/call/parity/source-contract 门禁。排序逻辑提取至
  `semantic_relations_order.c`，主 orchestrator 降至约860行。两套 interface 仍为同一8个既有
  producer marker，失败名称对 fixed parent 的 delta 0且不计 GREEN。本项未运行 MSVC、
  完整16-target matrix或三套stdio smoke，Plan 03 Task 7/Task 8继续未完成。

- 补充完成时间：2026-08-30 18:44 +08:00。Task 4.13 将 overload candidate projection 与
  `CallAt` 的已解析 target 绑定为原子契约：若 structured overload-set row 未包含 selected
  SymbolId，即使仍有其他有效 function candidates，query 也会清空复用数组并 fail closed，不返回
  零个 `isSelected` 的不一致集合。RED 在真实重载调用 snapshot 中把 overload members 收敛为
  未选中的另一候选，call-query `15 Tests / 1 Failure`、`Expected FALSE Was TRUE`；GREEN 后
  GCC/Clang 均通过 `15/30/22/21/15/70` call/query/relation/symbol/parity/source-contract 门禁。
  两套 interface 仍为同一8个既有producer marker，失败名称对 fixed parent 的 delta 0且不计
  GREEN。本项未运行 MSVC、完整16-target matrix或三套stdio smoke，Plan 03 Task 7/Task 8
  继续未完成。

- 补充完成时间：2026-08-30 18:49 +08:00。Task 4.14 将 invalid `overloadSetId` 表示的
  single-candidate 调用与 non-invalid id 指向缺失 row 的 snapshot corruption 明确区分。后一种
  情况不再退化为 selected-only candidate，而是清空输出并 fail closed。RED 在真实重载调用中
  把 selected symbol 指向不存在的 overload-set id，call-query `16 Tests / 1 Failure`、
  `Expected FALSE Was TRUE`；GREEN 后 GCC/Clang 均通过 `16/30/22/21/15/70`
  call/query/relation/symbol/parity/source-contract 门禁。两套 interface 仍为同一8个既有producer
  marker，失败名称对 fixed parent 的 delta 0且不计 GREEN。本项未运行 MSVC、完整16-target
  matrix或三套stdio smoke，Plan 03 Task 7/Task 8继续未完成。

- 补充完成时间：2026-08-30 18:56 +08:00。Task 4.15 将 overload-set member projection 从
  best-effort skip 收紧为原子完整性：每个 member SymbolId 必须解析为 registered function 与有效
  callable TypeId；任一缺失/malformed row 都清空整个输出并 fail closed，重复 member仍幂等。
  RED 保留 selected member、把另一 member 改为不存在的 SymbolId，call-query
  `17 Tests / 1 Failure`、`Expected FALSE Was TRUE`；GREEN 后 GCC/Clang 均通过
  `17/30/22/21/15/70` call/query/relation/symbol/parity/source-contract 门禁。两套 interface
  仍为同一8个既有producer marker，失败名称对 fixed parent 的 delta 0且不计 GREEN。本项未
  运行 MSVC、完整16-target matrix或三套stdio smoke，Plan 03 Task 7/Task 8继续未完成。

- 补充完成时间：2026-08-30 19:14 +08:00。Task 4.16 将 `CallAt` 的 reference 选择从
  “遇到首个 `signatureDisplay` 即停止”收紧为 structured completeness：resolved 且具有有效
  SymbolId 的事实优先于 display-only 事实，declaration range 与 display 仅用于同级完整度，
  相同完整度保持稳定发布顺序。RED 先发布未解析 display fact、后发布 resolved identity fact，
  call-query `18 Tests / 1 Failure`、`Expected TRUE Was FALSE`；GREEN 后 GCC/Clang 均通过
  `18/30/22/21/15/70` call/query/relation/symbol/parity/source-contract 门禁。两套 interface
  仍为同一8个既有producer marker，失败名称对 fixed parent 的 delta 0且不计 GREEN。本项未
  运行 MSVC、完整16-target matrix或三套stdio smoke，Plan 03 Task 7/Task 8继续未完成。

- 补充完成时间：2026-08-30 21:52 +08:00。Task 4.17 将同一 call target、同一精确 reference
  range 的 resolved identity 从“完整度相同时保留首项”收紧为一致性门禁：扫描到两个不同有效
  SymbolId 时清零输出并 fail closed；不同 nested ranges 保持独立，同一 target 的重复事实仍可
  按 declaration range/display 完整度 refinement。
  RED 为同一 call-site 发布两个不同 resolved function facts，call-query `19 Tests / 1 Failure`、
  `Expected FALSE Was TRUE`；反向 RED 为同一容器内不同 nested ranges，`20 Tests / 1 Failure`、
  `Expected TRUE Was FALSE`；单遍 range-scoped 实现再由“先发布同 range fact、后以更高完整度
  选中冲突 fact”的 `20 Tests / 1 Failure`、`Expected FALSE Was TRUE` 证明仍会漏检。最终
  query 先稳定选择，再第二遍复核 selected exact range。固定 `1e8584c + 3 overlays` 的
  GCC/Clang 快照均通过
  `20/30/22/21/15/70` call/query/relation/symbol/parity/source-contract 门禁。两套 interface
  仍为同一8个既有producer marker，失败名称对 fixed parent 的 delta 0且不计 GREEN。本项未
  运行 MSVC、完整16-target matrix或三套stdio smoke，Plan 03 Task 7/Task 8继续未完成。

- 补充完成时间：2026-08-30 21:58 +08:00。Task 4.18 将 `CallAt` 的 source identity 从
  `NULL` 通配收紧为 request↔expression、expression↔callTarget、callTarget↔reference 三层 exact
  optional identity；一侧缺 source、另一侧有明确 source 时清零并 fail closed。三个 focused RED
  分别移除 expression、reference、callTarget source，call-query `23 Tests / 3 Failures`，三项均为
  `Expected FALSE Was TRUE`；GREEN 后固定 `1e8584c + Task 4.17/4.18 overlays` 的 GCC/Clang
  快照均通过 `23/30/22/21/15/70` call/query/relation/symbol/parity/source-contract 门禁。两套
  interface 仍为同一8个既有producer marker，失败名称对 fixed parent 的 delta 0且不计 GREEN。
  本项未运行 MSVC、完整16-target matrix或三套stdio smoke，Plan 03 Task 7/Task 8继续未完成。

- 补充完成时间：2026-08-30 22:03 +08:00。Task 4.19 将 `CallAt` 的 expression 选择从等宽
  `<=` 后项覆盖收紧为 range width优先、等宽时 `EXACT` 优先、同 exactness 保留首项。RED 在同一
  exact call range 先发布 exact、后发布 approximate fact，call-query `24 Tests / 1 Failure`，
  唯一失败为 selected expression pointer不一致；GREEN 后固定 GCC/Clang 快照均通过
  `24/30/22/21/15/70` call/query/relation/symbol/parity/source-contract 门禁。两套 interface
  仍为同一8个既有producer marker，失败名称对 fixed parent 的 delta 0且不计 GREEN。本项未
  运行 MSVC、完整16-target matrix或三套stdio smoke，Plan 03 Task 7/Task 8继续未完成。

- 补充完成时间：2026-08-31 00:03 +08:00。Task 4.20 为 resolved source free-call 的同一
  canonical reference fact 发布 dense argument-to-parameter rows，覆盖 named reorder、精确
  argument range、argument/parameter `TypeId`、passing mode 与 exact/implicit conversion；
  `CallAt` 对 borrowed mapping 做 call-count、callable parameter、type id、conversion、source/range
  完整性校验，malformed payload 清零并 fail closed。RED 首次编译因 public mapping 类型/API 缺失
  真实 exit 1；测试定位修正后 GREEN。固定 GCC/Clang 快照均通过
  `25/30/22/21/15/70` call/query/relation/symbol/parity/source-contract 与 semantic-facts `15/15`，
  全部真实 exit 0；两套 interface 仍为同一8个既有producer marker，delta 0且不计 GREEN。
  receiver/member、`.zro`/native mapping parity、receiver `TypeId`、MSVC、完整16-target matrix及
  三套stdio smoke尚未完成，Plan 03 Task 7/Task 8继续未完成。

- 补充完成时间：2026-08-31 00:28 +08:00。Task 4.21 为 source class/struct constructor 的
  canonical reference fact 复用 structured argument mapping。首个 RED 为 constructor query mapping
  `NULL`，canonical consumers `21 Tests / 1 Failure`；首次 producer 接线后 mapping index/range存在但
  argument/parameter `TypeId` 为0、conversion为UNKNOWN，query按既有完整性门禁 fail closed。GREEN
  在缺 exact argument fact时仅于 parser producer阶段调用 canonical expression inference，并对
  resolved parameter执行 compatibility gate；class `int -> double` 发布 implicit row，struct named
  `y/x` 发布 `arg0 -> param1`、`arg1 -> param0` exact rows。固定 GCC/Clang 快照均通过 canonical/
  calls/query/relations/symbols/parity/source-contract/facts
  `21/25/30/22/21/15/70/15`，独占串行 type-inference均`124/124`、真实 exit 0；interface仍为同一
  8个既有producer marker，delta 0且不计GREEN。receiver/member、`.zro`/native mapping parity、
  receiver `TypeId`、MSVC、完整16-target matrix与三套stdio smoke尚未完成。

- 补充完成时间：2026-08-31 00:45 +08:00。Task 4.22 为 source `super(...)` 的 canonical
  reference fact发布base-constructor argument mapping。RED 将base参数设为canonical `double`、derived
  `seed`保留`int`，canonical consumers `21 Tests / 1 Failure`，唯一失败为mapping `NULL`。初次GREEN
  接线后focused全绿，但interface新增`LSP Signature Help Resolves Super Constructor` marker；根因是
  analyzer snapshot无法证明argument TypeId时producer仍发布UNKNOWN row，`CallAt`按malformed规则拒绝
  整个call。最终修复使mapping publication原子化：任一row不完整即清空可选数组，保留基础signature，
  不允许LSP补推。固定GCC/Clang快照均通过canonical/calls/query/relations/symbols/parity/
  source-contract/facts `21/25/30/22/21/15/70/15`，独占串行type-inference均`124/124`；super
  signature interface case恢复PASS，两套interface仅余固定8个既有marker、delta 0且不计GREEN。
  receiver/member、`.zro`/native mapping parity、receiver `TypeId`、MSVC、完整16-target matrix与
  三套stdio smoke尚未完成。

- 补充完成时间：2026-08-31 01:13 +08:00。Task 4.23 将 `CallAt` 对 non-empty
  `argumentMappings` 的校验收紧到 selected callable parameter contract。RED 在合法named reorder
  fact中依次注入另一个有效parameter `TypeId`、错误passing mode与和相等TypeId矛盾的IMPLICIT
  conversion；旧query接受首个损坏row，calls真实exit 1、`25 Tests / 1 Failure`、
  `Expected FALSE Was TRUE`。第二个RED把两条自洽row绑定到同一parameter，仍为`25/1`和同一
  failure。GREEN 要求parameter binding唯一、parameter type匹配canonical contract（non-value可为
  ref wrapper或其pointee）、passing mode匹配passing form、argument type存在且EXACT/IMPLICIT与
  TypeId相等性一致。固定GCC/Clang快照均通过calls/query/relations/symbols/parity/source-contract/facts/
  canonical `25/30/22/21/15/70/15/21`、真实exit 0；两套interface仍仅固定8个既有producer
  marker，delta 0且不计GREEN。只读反向审计确认source `in/ref/out`调用当前尚未发布`hasCallInfo`
  expression fact，未进入mapping query；该producer缺口、receiver/member、`.zro`/native parity、
  receiver `TypeId`、MSVC、完整16-target matrix与三套stdio smoke继续未完成。

- 补充完成时间：2026-08-31 04:50 +08:00。Task 5.10 为generic type use增加独立
  `SZrGenericType.wholeRange`，从generic name覆盖到exact closing angle；nested `>>`分别保存outer/inner
  range。首个RED中`Box<i64>`仍只有从`i64`开始的legacy generic node location，display为
  `15 Tests / 1 Failure`。直接扩大node location的初版虽使display GREEN，却让interface新增
  `LSP Closed Generic Type Display And Definition` marker，因此被门禁拒绝；最终保留legacy node location，
  仅由alias producer消费wholeRange。固定GCC/Clang快照均通过parser/display/calls/query/relations/
  symbols/parity/source-contract/facts/canonical/type-inference
  `74/16/26/30/22/21/15/70/15/21/124`、真实exit 0；两套canonical graph仍只含同一既有tuple
  marker，两套interface恢复固定8个既有producer marker，delta 0且均不计GREEN。本项未运行MSVC、
  完整16-target matrix或三套stdio smoke；const-expression/nominal/GcBridge alias producer、LSP alias
  consumer、receiver/member与binary/native parity及Plan 03 Task 7/Task 8继续未完成。

- 补充完成时间：2026-08-31 05:20 +08:00。Task 5.11 将generic use-site alias从
  `extract_generic_argument_name_string`的const求值路径分离，改由alias专属structured AST renderer
  生成presentation。RED中canonical `Matrix<int, 4>`已正确，但alias为`Matrix<i64, 4>`，display
  `17 Tests / 1 Failure`；GREEN保留canonical value identity并返回exact whole-use alias
  `Matrix<i64, 2 + 2>`，不切source文本。固定GCC/Clang快照均通过parser/display/calls/query/
  relations/symbols/parity/source-contract/facts/canonical/type-inference
  `74/17/26/30/22/21/15/70/15/21/124`、真实exit 0；两套canonical graph仍只有既有tuple
  marker，两套interface仍为fixed parent同一8个producer marker，delta 0且均不计GREEN。本项未
  运行MSVC、完整16-target matrix或三套stdio smoke；nominal/GcBridge alias producer、LSP alias
  consumer、receiver/member与binary/native parity及Plan 03 Task 7/Task 8继续未完成。

- 补充完成时间：2026-08-31 05:29 +08:00。Task 5.12 在
  `type_inference_resolve_type_value_alias`成功复制canonical inferred target后，以已有explicit alias publisher
  发布direct identifier use。RED中`Word -> int`推断与canonical display `int`均通过，但exact range
  alias query为`NULL`，display `18 Tests / 1 Failure`；GREEN返回`Word`且不注册同名nominal identity。
  固定GCC/Clang快照均通过parser/display/calls/query/relations/symbols/parity/source-contract/facts/
  canonical/type-inference `74/18/26/30/22/21/15/70/15/21/124`、真实exit 0；两套canonical
  graph仍只有既有tuple marker，两套interface仍为fixed parent同一8个producer marker，delta 0且均
  不计GREEN。本项未运行MSVC、完整16-target matrix或三套stdio smoke；wrapped alias propagation、
  GcBridge alias producer、LSP alias consumer、receiver/member与binary/native parity及Plan 03
  Task 7/Task 8继续未完成。

- 补充完成时间：2026-08-31 05:40 +08:00。Task 5.13 将type-value alias table lookup
  提取为structured binding查询，并在ownership generic内层推断恢复semantic context后重放
  explicit alias producer。RED中`Unique<Word>`已正确推断并格式化为`Unique<int>`，但inner
  `int` TypeId在exact `Word` range的alias query为`NULL`，display `19 Tests / 1 Failure`；GREEN
  返回`Word`，不为outer ownership TypeId发布alias，也不注册`Word` nominal identity。固定
  GCC/Clang快照均通过parser/display/calls/query/relations/symbols/parity/source-contract/facts/
  canonical/type-inference `74/19/26/30/22/21/15/70/15/21/124`、真实exit 0；两套canonical
  graph仍只有既有tuple marker，两套interface仍为fixed parent同一8个producer marker，delta 0且
  均不计GREEN。本项未运行MSVC、完整16-target matrix或三套stdio smoke；GcBridge alias
  producer、LSP alias consumer、receiver/member与binary/native parity及Plan 03 Task 7/Task 8继续
  未完成。

- 补充完成时间：2026-08-31 05:59 +08:00。Task 5.14 在GcBridge inner type完成
  structured alias resolution并通过ordinary/resource class prototype world校验后、设置
  `gcBridgeKind`前发布inner alias。RED中`Gc<DocAlias>`已正确格式化为`Gc<Document>`，但inner
  `Document` TypeId的exact alias query为`NULL`，display `20 Tests / 1 Failure`；GREEN同时覆盖
  `GcBox<ResourceAlias>`到`GcBox<BoxedResource>`，分别返回`DocAlias`/`ResourceAlias`，无效world
  不发布fact且outer wrapper TypeId不混用。固定GCC/Clang快照均通过parser/display/calls/query/
  relations/symbols/parity/source-contract/facts/canonical/type-inference
  `74/20/26/30/22/21/15/70/15/21/124`、真实exit 0；两套canonical graph仍只有既有tuple
  marker，两套interface仍为fixed parent同一8个producer marker，delta 0且均不计GREEN。本项未
  运行MSVC、完整16-target matrix或三套stdio smoke；LSP alias consumer、receiver/member与
  binary/native parity及Plan 03 Task 7/Task 8继续未完成。

- 补充完成时间：2026-08-31 06:10 +08:00。Task 5.15 让type-value alias producer对
  parsed type-use的局部副本清除`ownershipQualifier`、`referenceAccess`、`isReadonlyView`与array
  dimensions，再以原identifier exact range和resolved binding target发布inner alias；AST与最终
  inferred wrapper不变。RED中`readonly Word`已正确格式化为`readonly Document`，但inner
  `Document` TypeId的alias query为`NULL`，display `21 Tests / 1 Failure`；GREEN同时覆盖
  `ref Word`与`ref readonly Word`，三种outer canonical均保持。固定GCC/Clang快照均通过parser/
  display/calls/query/relations/symbols/parity/source-contract/facts/canonical/type-inference
  `74/21/26/30/22/21/15/70/15/21/124`、真实exit 0；两套canonical graph仍只有既有tuple
  marker，两套interface仍为fixed parent同一8个producer marker，delta 0且均不计GREEN。本项未
  运行MSVC、完整16-target matrix或三套stdio smoke；Shared/Weak/AtomicShared全变体display gate、
  LSP alias consumer、receiver/member与binary/native parity及Plan 03 Task 7/Task 8继续未完成。

- 补充完成时间：2026-08-31 06:16 +08:00。Task 5.16 新增独立owner display cases，
  通过`ZrParser_SemanticDisplay_FormatType`验证同一primitive target下Unique、Shared、Weak与
  AtomicShared四种canonical owner labels。该characterization在GCC/Clang首轮即为
  `22 Tests / 0 Failures`，无需production修复；既有invalid owner fail-closed测试继续有效。
  固定GCC/Clang快照均通过parser/display/calls/query/relations/symbols/parity/source-contract/
  facts/canonical/type-inference `74/22/26/30/22/21/15/70/15/21/124`、真实exit 0；两套canonical
  graph仍只有既有tuple marker，两套interface仍为fixed parent同一8个producer marker，delta 0且
  均不计GREEN。本项未运行MSVC、完整16-target matrix或三套stdio smoke；Task 5仅剩LSP旧名称
  映射删除，按Syntax05 ownership与Task 7 consumer迁移顺序继续；receiver/member与binary/native
  parity及Plan 03 Task 7/Task 8仍未完成。

- 补充完成时间：2026-08-31 07:40 +08:00。Task 4.24 以独立source
  `in/ref/out` call query case复核Task 4.23后的producer状态：`hasCallInfo`、passing mode与canonical
  TypeId已存在，但显式`ref/out` mapping range遗漏marker。RED为calls `26 Tests / 1 Failure`、
  `Expected 236 Was 240`；GREEN直接消费`SZrFunctionCall.argumentMarkers`中的structured
  `markerLocation`并与argument AST range合并，named label与无marker `in` range保持原合同，不允许
  LSP扫描source补偿。固定GCC/Clang snapshot均通过parser/display/calls/query/relations/symbols/
  parity/source-contract/facts/canonical/type-inference
  `74/22/26/30/22/21/15/70/15/21/124`、真实exit 0；type-inference两套串行。
  两套interface均真实exit 1且精确保持fixed parent同一8个producer marker，delta 0、不计GREEN。
  本项未运行MSVC、完整16-target matrix或三套stdio smoke；receiver/member、receiver `TypeId`、
  binary/native mapping parity、Syntax05 imported identity producer及Plan 03 Task 7/Task 8继续未完成。

- 补充完成时间：2026-08-31 07:55 +08:00。Task 4.25 新增unresolved call reason
  matrix：有效target function没有declaration coordinates时保留target SymbolId并返回
  `TARGET_DECLARATION_UNAVAILABLE`；resolved reference指向variable SymbolId时，即使registry存在
  同名function也清零target并返回`TARGET_UNRESOLVED`。现有production首轮满足合同，calls
  `28 Tests / 0 Failures`，因此无需生产补丁。固定GCC/Clang snapshot均通过parser/display/calls/
  query/relations/symbols/parity/source-contract/facts/canonical/type-inference
  `74/22/28/30/22/21/15/70/15/21/124`、真实exit 0，type-inference两套串行；两套
  interface均真实exit 1且精确保持fixed parent同一8个producer marker，delta 0、不计GREEN。
  本项未运行MSVC、完整16-target matrix或三套stdio smoke；receiver/member、receiver `TypeId`、
  binary/native mapping parity、Syntax05 imported identity producer及Plan 03 Task 7/Task 8继续未完成。

- 补充完成时间：2026-08-31 08:03 +08:00。Task 1.1 对账发现
  `2026-08-24-plan03-task01-query-purity.md`已记录Task1三工具链完成，但主计划四个checkbox仍未勾选。
  本项不改生产代码或测试语义，只按既有记录和当前重放证据修复状态：GCC/Clang
  `zr_vm_semantic_query_contract_test`均`4/4`，`zr_vm_language_server_semantic_query_parity_test`
  均`15/15`，真实exit 0。borrowed snapshot view、diagnostics query purity、重复调用稳定、
  UNKNOWN/APPROXIMATE fail-closed及source/binary/native parity继续由既有测试冻结。历史MSVC证据仅
  引用Task1原record，本项没有重跑MSVC、完整16-target matrix或三套stdio smoke；Plan 03整体仍因
  Task4 receiver/binary-native producer、Task7 consumer迁移与Task8总门禁未完成而保持进行中。

- 补充完成时间：2026-08-31 08:09 +08:00。Task 2.3 对账确认
  `SymbolAt`、`VisibleSymbols`、snapshot-borrowed display与稳定scope-distance/declaration-order/
  SymbolId排序已由既有Task2 records和当前API/tests冻结；canonical lexical completion只消费
  parser `VisibleSymbols`，source-contract禁止恢复analyzer completion及symbol-table visible/range
  scope扫描。固定GCC/Clang snapshot均通过parser symbols `21/21`、semantic parity `15/15`、
  LSP source contracts `70/70`，真实exit 0，因此勾选Task2剩余两个stale checkbox。历史MSVC
  evidence保留在Task2原records，本项未重跑MSVC、完整16-target matrix或三套stdio smoke；
  relation external producer、call receiver/binary-native、其余Task7 consumer与Task8仍未完成。

- 补充完成时间：2026-08-31 08:19 +08:00。Task 3.17 为公共relation append新增
  endpoint identity integrity：source与target各自必须至少有一个有效SymbolId或TypeId；
  symbol→type alias/import与type→type base edge继续合法，单边edge在写snapshot前fail closed。
  RED中既有22项全通过、新case唯一失败，relations `23 Tests / 1 Failure`；GREEN后固定GCC/Clang
  snapshot均通过relations/query/symbols/calls/parity/source-contract
  `23/30/21/28/15/70`、真实exit 0。据此勾选Task3 relation graph与四查询API两项。未运行
  MSVC、完整16-target matrix或三套stdio smoke；external/virtual metadata producer、跨provider
  generation矩阵、Task4 receiver/binary-native、其余Task7 consumer与Task8继续未完成。

- 补充完成时间：2026-08-31 08:30 +08:00。Task 4.26 冻结caller scope identity：
  最窄function scope正常获胜，同owner等宽重复fact保持有效；同callsite存在两个等宽但owner
  SymbolId不同的function scopes时，caller归属冲突并返回`CALLER_UNAVAILABLE`，resolved target
  仍可进入incoming query，两个候选owner的outgoing query均为空。RED中原28项全通过、新case
  唯一失败，calls `29 Tests / 1 Failure`；GREEN后固定GCC/Clang snapshot均通过relations/query/
  symbols/calls/parity/source-contract `23/30/21/29/15/70`、真实exit 0。据此勾选Task4
  caller index/incoming/outgoing项。未运行MSVC、完整16-target matrix或三套stdio smoke；CallAt
  receiver TypeId/member mapping、binary/native callable parity、其余Task7 consumer与Task8未完成。

- 补充完成时间：2026-08-31 08:46 +08:00。Task 6.39 将公共
  semantic-query range/position source比较收紧为exact optional identity：两个`NULL`相等，两个
  非空source按字符串值比较，一侧缺失时fail closed。RED中既有11个parser diagnostic用例全过，
  新node-scope case唯一失败为`12 Tests / 1 Failure`；GREEN后source-unknown root不再仅凭重叠
  offset吸入source-known persistent diagnostic。固定GCC/Clang snapshot均通过semantic query/
  contract/parser diagnostic/compiler diagnostic `30/4/12/64`、真实exit 0。未运行MSVC、完整
  16-target matrix或三套stdio smoke；analyzer-side structured producer迁移、compiler/LSP golden
  parity总门禁及Plan 03 Task 7/Task 8继续未完成。

- 补充完成时间：2026-08-31 09:12 +08:00。Task 4.27 为`CallAt`增加
  equal-rank call expression一致性门禁：同一最窄width与exactness priority下，只有call-site/
  target完整range、kind/exactness、optional target name、result TypeId、argument count、named/member
  flags全部一致才视为重复；冲突时清空输出并fail closed，不再按append order选首项。RED中原29项
  全过、新case唯一失败为`30 Tests / 1 Failure`；GREEN后固定GCC/Clang snapshot均通过calls/
  query/canonical consumers/parity/source-contract `30/30/21/15/70`、真实exit 0。未运行MSVC、
  完整16-target matrix、interface runtime或三套stdio smoke；receiver/member与binary/native producer、
  Task 4首项和跨来源parity、Plan 03 Task 7/Task 8继续未完成。

- 补充完成时间：2026-08-31 09:33 +08:00。Task 1.2 将底层semantic facts与canonical
  query的source比较统一为exact optional identity：两个缺失source相等，两个非空source按值相等，
  单边缺失一律fail closed。RED中原4项全过，新增`FactsAt`与`CanonicalTypeAt`两项唯一失败为
  `6 Tests / 2 Failures`；GREEN后同offset不再让sourceless position或node scope选中sourced fact。
  四个reaching-definition测试移除手工清空snapshot source的旧夹具，继续使用position converter返回的
  canonical identity。固定GCC/Clang snapshot均通过facts/query/contract/calls/parser diagnostic/
  compiler diagnostic/reference/property/canonical/parity/source-contract/type-inference
  `15/30/6/30/12/64/6/11/21/15/70/124`、真实exit 0；interface保持同一fixed8，delta 0。
  未运行MSVC、完整16-target matrix或三套stdio smoke；receiver/member与binary/native producer、
  Task 7 consumer迁移和Task 8总门禁继续未完成。

- 补充完成时间：2026-08-31 09:48 +08:00。Task 2.4 将`VisibleSymbols`的scope与
  declaration source比较统一为exact optional identity；sourceless position不再按重叠offset进入
  sourced lexical scope。RED中原21项全过，新case唯一失败为`22 Tests / 1 Failure`，具体为
  `Expected FALSE Was TRUE`；GREEN后复用输出数组在失败时清零。固定GCC/Clang snapshot均通过
  symbols/parity/source-contract `22/15/70`、真实exit 0，interface保持同一fixed8、delta 0。
  未运行MSVC、完整16-target matrix或三套stdio smoke；binary/native scope producer、Task 7
  consumer迁移和Task 8总门禁继续未完成。

- 补充完成时间：2026-08-31 09:55 +08:00。Task 6.40 将resolved reference抑制
  unresolved diagnostic的duplicate key收紧为exact optional source identity；同name/range只有在
  source也匹配时才可视为同一事实，单边缺source时保留unresolved diagnostic。RED中原12项全过，
  新case唯一失败为`13 Tests / 1 Failure`，具体为`Expected 1 Was 0`；GREEN后固定GCC/Clang
  snapshot均通过parser/compiler/LSP diagnostics、parity与source-contract
  `13/64/19/15/70`、真实exit 0，interface保持同一fixed8、delta 0。未运行MSVC、完整
  16-target matrix或三套stdio smoke；其余analyzer producer迁移、compiler/LSP golden parity
  总门禁、Task 7 consumer与Task 8继续未完成。

- 补充完成时间：2026-08-31 11:04 +08:00。Task 6.41 删除LSP symbols analyzer中
  interface const-field violation枚举、diagnostic builder与semantic-fact append循环，改为只调用
  `ZrParser_InterfaceContract_PublishConstFieldDiagnostics`；descriptor 2014、exact primary/related
  ranges与no-fix disposition均继续由parser persistent fact唯一生产。source-contract RED精确为
  4项失败；GREEN后固定GCC/Clang snapshot均通过source-contract与LSP semantic diagnostics
  `70/19`及parser publisher `1/1`、真实exit 0，GCC interface const-field双诊断专项保持通过；
  interface保持同一fixed8、delta 0。semantic-analyzer完整目标仍有既有非本片失败，不计为
  GREEN；未运行MSVC、完整16-target matrix或三套stdio smoke。其余
  analyzer producer迁移、compiler/LSP golden parity总门禁、Task 7 consumer与Task 8继续未完成。

- 补充完成时间：2026-08-31 11:37 +08:00。Task 7.55 修复LSP local symbol与
  type-environment binding的双重身份：symbols analyzer已注册canonical declaration后，变量、
  参数、foreach、隐式runtime symbol与property setter/init参数通过
  `ZrParser_TypeEnvironment_RegisterCanonicalVariable`复用同一SymbolId、TypeId与exact
  declaration range；仅无declared symbol的临时return-inference scope保留普通注册。Web URI
  RED在同一`x` use range精确观察到SymbolId 1 declaration、孤立SymbolId 2 read与两个
  SymbolId 1 reads；GREEN后fact-level断言要求所有resolved reads匹配`SymbolAt` identity，
  definition/references/highlights均恢复且没有URI scheme或name fallback。固定GCC/Clang
  snapshot均通过symbols/parity/LSP diagnostics/property/source-contract
  `22/15/19/11/70`、真实exit 0；两套interface均真实exit 1，Web URI case转PASS且失败集合
  从fixed8精确降为fixed7。本项未运行MSVC、完整16-target matrix或三套stdio smoke；其余
  七个producer marker、source/binary/native parity与Task 8继续未完成。
