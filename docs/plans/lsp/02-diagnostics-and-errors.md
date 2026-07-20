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
