# LSP 02：Structured Diagnostic 与 Safe Fix

## Diagnostic schema

```text
SemanticDiagnostic {
  code; severity; messageId; arguments[];
  primaryRange;
  relatedLocations[];
  factIds[];
  fixes[];
  snapshotId;
}

DiagnosticFix {
  titleId;
  applicability: always | maybeIncorrect | requiresReview;
  edits[];
  preconditionChecksums[];
}
```

compiler/parser产生结构化原因；LSP负责协议映射和本地化，不通过message字符串分类错误。

## 必须覆盖

- syntax：missing semicolon、delimiter混用、invalid ModuleSpecifier、legacy `%xxx`。
- type/call：mismatch、generic constraint、receiver effect、constructor/call边界。
- Place/borrow：uninitialized/out、moved/dropped、loan conflict、ref/ref struct escape、readonly写入。
- property：missing accessor、accessibility、ref-return lifetime、非法auto backing/ref setter。
- ownership：resource/new/own错误、active borrow drop、GC bridge、Shared thread能力。
- module/package：unknown alias/package、export denied、artifact contract mismatch、native capability。
- pattern：non-exhaustive、unreachable variant、payload move。

## Range 与 fix

primary指向用户能改的token；related location展示declaration/origin/last use/move/loan/manifest entry。missing `;`只插入token；newline不作为terminator。`$runtimeType(...)`迁移到reflection API必须`requiresReview`。跨文件/manifest fix必须检查document version与checksum，不能编辑过期snapshot。

## Recovery

parser recovery产生poisoned/unknown facts并限制级联；后续diagnostic标记依赖root error。LSP不能为了减少红线而伪造any type或吞掉所有ownership错误。

## 完成记录

[2026-07-19 structured diagnostic baseline](./02-diagnostics/2026-07-19-structured-diagnostic-baseline.md) 记录registry/JSON/fix基础。

[2026-07-20 resolved callable consumer convergence](./03-robustness/2026-07-20-resolved-callable-consumer-convergence.md) 完成source named-call compiler current diagnostic到persistent semantic query fact再到LSP的投影，并移除同call range的`cannot_infer_exact_type`占位级联；registry完整性、safe fix和binary/native同原因链仍需后续覆盖。

[2026-07-21 semicolon safe-fix convergence](./02-diagnostics/2026-07-21-semicolon-safe-fix-convergence.md) 完成`missing_statement_semicolon`从parser structured diagnostic到machine-applicable fix、LSP quickfix、snapshot resolve和apply-edit-rebind的闭环；placeholder fix保持不可自动应用。该记录只关闭semicolon局部插入，不代表delimiter、registry全覆盖或L3整体完成。

[2026-07-21 condition-close safe-fix convergence](./02-diagnostics/2026-07-21-condition-close-safe-fix-convergence.md) 完成`missing_condition_close`由parser在block opener前发布精确`)` machine fix，并由通用LSP code-action/stdio consumer投影；primary range与零宽edit range保持独立，修复后重新绑定会清除该code。其他delimiter family、replacement、registry全覆盖和L3整体仍未完成。

[2026-07-21 index-close safe-fix convergence](./02-diagnostics/2026-07-21-index-close-safe-fix-convergence.md) 完成`missing_index_close`保留opening `[` primary range、在current token起点发布精确`]` machine fix，并由同一通用LSP code-action/stdio consumer完成apply-edit-rebind；不按code/message/source重建delimiter。parameter-list close、其他delimiter family、replacement、registry全覆盖和L3整体仍未完成。

[2026-07-21 parameter-list-close safe-fix convergence](./02-diagnostics/2026-07-21-parameter-list-close-safe-fix-convergence.md) 完成`missing_parameter_list_close`在unexpected token起点发布精确`)` machine fix，并由function/method/interface/extern共享producer和通用LSP/stdio consumer完成apply-edit-rebind；不按declaration kind/code/message/source重建。call/group close、其他delimiter family、replacement、registry全覆盖和L3整体仍未完成。

[2026-07-21 call-close safe-fix convergence](./02-diagnostics/2026-07-21-call-close-safe-fix-convergence.md) 完成`missing_call_close`保留opening `(` primary range并在current token起点发布精确`)` machine fix，由通用LSP/stdio consumer完成apply-edit-rebind；不按call AST/code/message/source重建。group close、其他delimiter family、replacement、registry全覆盖和L3整体仍未完成。

[2026-07-21 group-close safe-fix convergence](./02-diagnostics/2026-07-21-group-close-safe-fix-convergence.md) 完成`missing_group_close`保留group opening `(` primary range并在current token起点发布精确`)` machine fix；failed lambda lookahead改用完整parser cursor恢复token-start identity，通用LSP/stdio consumer完成apply-edit-rebind且不按group AST/code/message/source重建。其他delimiter family、replacement、registry全覆盖和L3整体仍未完成。

[2026-07-21 array-close safe-fix convergence](./02-diagnostics/2026-07-21-array-close-safe-fix-convergence.md) 完成`missing_array_close`以exact token identity保留opening `[` primary range并在current token起点发布精确`]` machine fix，由通用LSP/stdio consumer完成apply-edit-rebind；不按array AST/code/message/source重建。object及其他delimiter family、replacement、registry全覆盖和L3整体仍未完成。

[2026-07-21 object-close safe-fix convergence](./02-diagnostics/2026-07-21-object-close-safe-fix-convergence.md) 完成`missing_object_close`在object literal、using destructuring和switch struct pattern producer间统一exact opening `{` identity，并在current token起点发布精确`}` machine fix；通用LSP/stdio consumer不按object AST/producer/code/message/source重建。computed-key及其他delimiter family、replacement、registry全覆盖和L3整体仍未完成。

[2026-07-21 object computed-key close safe-fix convergence](./02-diagnostics/2026-07-21-object-computed-key-close-safe-fix-convergence.md) 完成`missing_object_computed_key_close`在首属性与后续属性producer间保留exact opening `[`，并在property colon起点发布精确`]` machine fix；通用LSP/stdio consumer不按property位置、object AST、code/message/source重建。其他delimiter family、replacement、registry全覆盖和L3整体仍未完成。

[2026-07-21 object property-colon safe-fix convergence](./02-diagnostics/2026-07-21-object-property-colon-safe-fix-convergence.md) 完成`missing_object_property_colon`在首属性与后续属性producer间保留unexpected value token primary，并在同token起点发布精确`:` machine fix；通用LSP/stdio consumer不按property位置、object AST、code/message/source重建。其他delimiter family、replacement、registry全覆盖和L3整体仍未完成。

[2026-07-21 object property-separator safe-fix convergence](./02-diagnostics/2026-07-21-object-property-separator-safe-fix-convergence.md) 完成`missing_object_property_separator`保留reachable identifier/string next-key token primary，并在同token起点发布精确`,` machine fix；通用LSP/stdio consumer不按key kind、object AST、code/message/source重建。Postfix `[`语法边界、其他delimiter family、replacement、registry全覆盖和L3整体仍未完成。

[2026-07-21 conditional-colon safe-fix convergence](./02-diagnostics/2026-07-21-conditional-colon-safe-fix-convergence.md) 完成`missing_conditional_colon`保留`?` token primary，并仅在current token可开始alternate expression时发布精确`:` machine fix；`return true ? 1;`与missing consequent/alternate不发布无效fix，通用LSP/stdio consumer不按conditional AST/code/message/source重建。其他delimiter family、replacement、registry全覆盖和L3整体仍未完成。

[2026-07-21 array element-separator safe-fix convergence](./02-diagnostics/2026-07-21-array-element-separator-safe-fix-convergence.md) 完成`missing_array_element_separator`保留next-element token primary并在同token起点发布规范`,` machine fix；`array_element_assignment`保持无fix，通用LSP/stdio consumer不按array AST/code/message/source重建。其他delimiter family、replacement、registry全覆盖和L3整体仍未完成。

[2026-08-05 declaration-body-close safe-fix convergence](./02-diagnostics/2026-08-05-declaration-body-close-safe-fix-convergence.md) 完成`missing_declaration_body_close`保留opening `{` primary，并在recovery EOF lexer cursor发布零宽`}` machine fix；通用LSP code-action直接消费structured `fixes[]`，应用后以新document version重新绑定并清除该code，不按declaration kind、message、AST或源码文本重建。stdio/CLI全链验收、其他delimiter/replacement family、registry全覆盖和L3整体仍未完成。

[2026-08-05 declaration-body-open safe-fix convergence](./02-diagnostics/2026-08-05-declaration-body-open-safe-fix-convergence.md) 已完成`missing_declaration_body_open`验证与主树集成：保留existing recovery primary并在同cursor发布零宽`{}` machine fix；`{}`保证自动修复不立即制造body-close diagnostic，通用LSP code-action直接消费structured `fixes[]`并在v2重新绑定后清除该code，不按declaration kind、message、AST或源码文本重建。主树三工具链定向parser 38/38和advanced editor suite均真实exit 0；stdio/CLI全链验收、其他delimiter/replacement family、registry全覆盖和L3整体仍未完成。

[2026-08-05 statement-body-open safe-fix convergence](./02-diagnostics/2026-08-05-statement-body-open-safe-fix-convergence.md) 已完成`missing_statement_body_open`验证与主树集成：parser保留recovery primary、在current lexer cursor发布零宽`{}` machine fix，通用LSP code-action消费structured `fixes[]`并在v2重新绑定后清除该code，不按statement kind、message、AST或源码文本重建。主树三工具链定向parser 38/38和advanced editor suite均真实exit 0；stdio/CLI全链验收、其他delimiter/replacement family、registry全覆盖和L3整体仍未完成。

[2026-08-05 block-close safe-fix convergence](./02-diagnostics/2026-08-05-block-close-safe-fix-convergence.md) 已完成`missing_block_close`验证与主树集成：parser保留opening `{` primary、在current lexer cursor发布零宽`}` machine fix，通用LSP code-action消费structured `fixes[]`并在v2重新绑定后清除该code，不按block AST、message或源码文本重建。主树三工具链定向parser 38/38和advanced editor suite均真实exit 0；stdio/CLI全链验收、其他delimiter/replacement family、registry全覆盖和L3整体仍未完成。

[2026-08-05 catch-pattern-close safe-fix convergence](./02-diagnostics/2026-08-05-catch-pattern-close-safe-fix-convergence.md) 已完成`missing_catch_pattern_close`验证与主树集成：parser保留catch-body `{` primary、在current lexer cursor发布零宽`)` machine fix，通用LSP code-action消费structured `fixes[]`并在v2重新绑定后清除该code，不按catch AST、message或源码文本重建。主树三工具链定向parser 38/38和advanced editor suite均真实exit 0；stdio/CLI全链验收、其他delimiter/replacement family、registry全覆盖和L3整体仍未完成。

[2026-08-05 using-resource-close safe-fix convergence](./02-diagnostics/2026-08-05-using-resource-close-safe-fix-convergence.md) 已完成隔离叶子验证，待合入主树：`missing_using_resource_close`保留using body `{` primary、在current lexer cursor发布零宽`)` machine fix；通用LSP code-action只消费structured `fixes[]`并在v2重新绑定后清除该code，不按using AST、diagnostic code、message或源码文本重建。GCC、Clang和MSVC定向parser 39/39及advanced editor suite均真实exit 0；stdio/CLI全链验收、其他delimiter/replacement family、registry全覆盖和L3整体仍未完成。

## Diagnostic Registry 与验收

每个diagnostic registry entry必须声明：stable code、message id/arguments、severity、producing fact/query、primary role、related role集合、suppression/recovery policy和允许的fix applicability。协议层不得从英文message反推code。

| 组 | 必须实现的关联信息 | Fix边界 |
|---|---|---|
| parser/semicolon/delimiter | missing token、definition/type delimiter role | always，仅局部插入/替换且checksum匹配 |
| type/call/construct | expected/actual TypeId、candidate/rejection reason | maybeIncorrect，禁止跨类别静默改构造 |
| borrow/move/out | origin、loan/move、conflict、exit path | safe时插marker；lifetime重构requiresReview |
| property/receiver | declaration/accessor、receiver effect、referent region | accessor生成/field创建通常requiresReview |
| owner/resource/bridge | type category、Drop/bridge source | `new/own/init`明确错误可修，bridge需review |
| pattern | missing/redundant variant与payload move | arm skeleton可生成，不猜业务body |
| module/package/native | literal、manifest/export/dependency/ABI entry | manifest跨文件edit需version/checksum验证 |
| migration | legacy AST role与目标feature id | 依syntax 06 applicability，不做全文替换 |

验证入口：`tests/parser/test_compiler_semantic_query_diagnostics.c`、`tests/language_server/test_lsp_semantic_query_diagnostics.c`、`test_ownership_diagnostics.c`、`test_union_pattern_diagnostics.c`和stdio diagnostic fix smoke。每个case断言code/range/related/fix edits，并在应用edit后重parse/rebind。

退出条件：目标fixture所有negative都有唯一primary diagnostic；poison recovery不产生无界级联；过期snapshot不发布/应用fix；同一compiler diagnostic在CLI/LSP保持相同code和原因链。
