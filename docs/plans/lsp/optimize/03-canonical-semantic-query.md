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

- [ ] 为现有 TypeAt、CanonicalTypeAt、CallAt、DefinitionOf、DeclarationOf、ReferencesOf、Diagnostics、PropertyAt 建立 source/binary/native 三路同构测试。
- [ ] 每个 query 明确 ownership：返回 borrowed view 的有效期绑定 semantic snapshot；需要跨 snapshot 保存的结果必须复制 stable ids/ranges，不保存 AST/raw pointer。
- [ ] 查询不得修改 compiler/analyzer 状态；重复调用结果顺序稳定。
- [ ] exactness 为 UNKNOWN/APPROXIMATE 时上层必须 fail closed 或明确标记，不得回退到 LSP type text reconstruction。

## Task 2：补齐 symbol-at-position 与 visible symbols

**Files:**
- Modify: `zr_vm_parser/include/zr_vm_parser/semantic_query.h`
- Create: `zr_vm_parser/src/zr_vm_parser/semantic/semantic_query_symbols.c`
- Modify: `zr_vm_parser/src/zr_vm_parser/semantic/semantic_query.c`
- Create: `tests/parser/test_semantic_query_symbols.c`

- [ ] 新增契约：

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
- [ ] Lua/QuickJS reference check：变量身份必须由 compiler scope state 决定；不得在 LSP token scanner 中复制 `searchvar`/scope walk。

## Task 3：补齐 declaration/definition/implementation relations

**Files:**
- Create: `zr_vm_parser/include/zr_vm_parser/semantic_relations.h`
- Create: `zr_vm_parser/src/zr_vm_parser/semantic/semantic_relations.c`
- Modify: `zr_vm_parser/include/zr_vm_parser/semantic_query.h`
- Create: `tests/parser/test_semantic_query_relations.c`

- [ ] relation graph 使用 snapshot-scoped stable ids：declaration↔definition、override/implementation、type base/interface、constructor、property accessor、alias target、import export origin。
- [ ] 新增 `RelationsOfSymbol`、`ImplementationsOf`、`BaseTypesOf`、`DerivedTypesOf`；返回 relation kind + source/target SymbolId/TypeId + exact range。
- [ ] 处理多定义、partial/extern/native/binary 无 source definition 情形；没有 source range 时返回明确 external origin 和 virtual declaration URI，由 metadata projection 提供，不由 LSP 编造。
- [ ] 测试同名不同模块、重载、generic open/closed type、receiver method、alias chain、base/interface、多项目 provider generation。

## Task 4：补齐 call graph 与 overload facts

**Files:**
- Create: `zr_vm_parser/include/zr_vm_parser/semantic_calls.h`
- Create: `zr_vm_parser/src/zr_vm_parser/semantic/semantic_calls.c`
- Modify: `zr_vm_parser/include/zr_vm_parser/semantic_query.h`
- Create: `tests/parser/test_semantic_query_calls.c`

- [ ] CallAt 返回 selected target、overload candidate set、receiver TypeId、closed callable TypeId、argument-to-parameter mapping、conversion/exactness 和 call-site range。
- [ ] semantic context 构建 caller SymbolId → call edges 索引；incoming/outgoing query 不扫描源文本。
- [ ] 动态/无法解析调用返回 unresolved edge 与 reason，不把第一个同名函数当目标。
- [ ] source、`.zro`、native descriptor external callable 共用同一 callable contract；当前 L8 overlay 的 `isExternalCallable`/signatureDisplay 必须在 parser 层封闭，不再由 hover/signature 各自拼装。

## Task 5：补齐 formatter/display 与 documentation facts

**Files:**
- Modify: `zr_vm_parser/include/zr_vm_parser/canonical_type.h`
- Create: `zr_vm_parser/include/zr_vm_parser/semantic_display.h`
- Create: `zr_vm_parser/src/zr_vm_parser/semantic/semantic_display.c`
- Create: `tests/parser/test_semantic_display.c`

- [ ] 为 canonical type、symbol signature、call signature、property contract 提供结构化 display API；输出由 TypeId/SymbolId 生成。
- [ ] primitive、union、nullable、ref/owner/readonly、generic const/type args、tuple、function effects/passing modes 全覆盖。
- [ ] 删除 LSP `semantic_type_prototypes.c` 中把 `int/i64`、`string/str` 等名称映射回类型的职责；展示 alias 与 canonical identity 分开返回。
- [ ] documentation 作为 symbol metadata fact 进入 query；completion/hover/signature 不再彼此提取文本。

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

- 最近更新时间：2026-08-30 05:22 +08:00。
- 总体状态：进行中。Task 6 的最后一个已知 LSP producer consumer loop 因
  Syntax05 exact ownership 等待释放；Task 7 已开始无重叠 consumer 边界迁移，
  Task 8 尚未完成，不声明 Plan 03 完成。
- 本阶段完成项目：Task 6.30 exact-type inference diagnostic query projection；
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
  跨项目 imported function reference 的 module/member 聚合仍按名称实现，Task 7.25 RED 已证明
  parser `SymbolAt` imported declaration range 为零，等待 Syntax05 producer 路径释放、
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
