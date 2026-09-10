---
related_code:
  - zr_vm_parser/include/zr_vm_parser/diagnostic_builder.h
  - zr_vm_parser/include/zr_vm_parser/diagnostic_registry.h
  - zr_vm_parser/include/zr_vm_parser/diagnostic_messages.h
  - zr_vm_parser/include/zr_vm_parser/parser.h
  - zr_vm_language_server/include/zr_vm_language_server/lsp_interface.h
  - zr_vm_language_server/include/zr_vm_language_server/lsp_diagnostic_store.h
  - zr_vm_core/include/zr_vm_core/exception.h
implementation_files:
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_builder.c
  - zr_vm_parser/src/zr_vm_parser/diagnostics/diagnostic_registry.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_diagnostic_fixes.c
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - zr_vm_core/src/zr_vm_core/exception.c
plan_sources:
  - user: 2026-09-10 继续完善 ZrVm Wiki，要求详细介绍语法规则、用例和实现机制
  - docs/plans/lsp/02-diagnostics-and-errors.md
tests:
  - tests/parser/test_parser_recovery_ownership.c
  - tests/parser/test_reference_syntax_contract.c
  - tests/language_server/test_lsp_interface.c
  - tests/language_server/test_lsp_project_features.c
  - tests/core/test_runtime_crash_recovery.c
  - tests/ffi/test_native_extern_contract.c
doc_type: reference
---

# 诊断生命周期与修复契约参考

ZrVm 将 parser、semantic/compiler、native contract、runtime exception 和 LSP UI 的失败信息
分层处理。结构化 diagnostic 的目标不是把一段英文错误文本包装成 JSON，而是携带稳定 code、
source range、原因链、可执行编辑和“为什么没有安全修复”的信息。后端、CLI、LSP 和测试
应传递这些结构化事实，不应重新根据 message 文本分类。

本页补充 [错误目录](error-catalog.md) 与 [语言诊断与错误恢复](../02-language/diagnostics.md)：
前者列举常见 code，本页说明对象如何创建、复制、映射、缓存、释放和安全应用。

## 1. 失败层级

```text
lexer/parser shape error
  -> SZrStructuredDiagnostic
semantic/type/flow/ownership error
  -> SZrStructuredDiagnostic or compiler diagnostic projection
artifact/native/FFI admission failure
  -> structured diagnostic or registry/artifact status + fields
runtime throw / execution failure
  -> exception object + stack, then host/UI projection
LSP request
  -> SZrLspDiagnostic / JSON-RPC result, with revision/result-id lifetime
```

一个源文件可以同时含多个可恢复 parser diagnostic；但 fatal parser state、semantic error、
artifact contract mismatch 和 runtime exception 不是相同的“警告等级”。上层不应在有
`hasError`/fatal 状态时把部分 AST 交给 production compiler，也不应把 native ABI failure
伪装成 source spelling error。

## 2. `SZrStructuredDiagnostic` 数据模型

```c
typedef struct SZrStructuredDiagnostic {
    EZrStructuredDiagnosticSeverity severity;
    SZrFileRange location;
    SZrString *code;
    SZrString *message;
    SZrString *cause;
    SZrString *suggestion;
    SZrArray relatedInformation;
    SZrArray fixes;
    TZrUInt32 descriptorId;
    EZrDiagnosticNoFixReason noFixReason;
} SZrStructuredDiagnostic;
```

| 字段 | 规则 |
| --- | --- |
| `severity` | parser enum 为 Error、Warning、Info、Hint，不能直接当 LSP 数字使用 |
| `location` | `SZrFileRange`，保留原始 source 坐标/provenance |
| `code` | 稳定的 machine-readable 分类；测试、suppress、文档应优先使用它 |
| `message` | 当前 locale 的可呈现文本，不是稳定 key |
| `cause` / `suggestion` | 解释与建议，不等于自动 fix |
| `relatedInformation` | 其他 range/message，表示根因/被关联声明 |
| `fixes` | 每项 title、edit range、edit text、applicability |
| `descriptorId` | registry 中可查的数字身份 |
| `noFixReason` | 没有安全 edit 的明确原因 |

诊断 registry 将 code 映射到 `SZrDiagnosticDescriptor`，其中有 id、title/message format key、
default severity、help URI、lint category。category 可为 syntax、semantic、type、flow、
ownership、style。message catalog 则按 locale 解析文本。这三层关系如下：

```text
code <-> descriptor id/category/help URI
     -> localized title/message keys
     -> rendered message/cause/suggestion
```

因此客户端应保存 code/descriptor id，而不是把英语 message 当作数据库主键。

## 3. 构造、复制和释放

创建 diagnostic 前先初始化；构造失败时不应读半填充字段：

```c
SZrStructuredDiagnostic diagnostic;
ZrParser_StructuredDiagnostic_Init(&diagnostic);

if (ZrParser_DiagnosticBuilder_Build(
        state,
        &diagnostic,
        ZR_STRUCTURED_DIAGNOSTIC_ERROR,
        range,
        "example_contract_error",
        "The imported contract is incompatible.",
        "The provider signature differs from the compiled call site.",
        "Rebuild the consumer against the current provider.")) {
    /* Add related locations/fixes only while diagnostic is owned by this state. */
}

ZrParser_StructuredDiagnostic_Free(state, &diagnostic);
```

`ZrParser_StructuredDiagnostic_Copy` 深复制可跨 callback/queue 保留的 payload；不复制时，
strings、related arrays 和 fix arrays 的生命周期由原 owner/state 管理。`AddRelatedInformation`
与 `AddFix` 负责分配/追加，不能让调用者手工把 stack-local `SZrString` 或 `SZrArray` 插入。
不要在 `Free` 后读取 `code`、`message`、fix text 或 related range。

## 4. Fix applicability 和 no-fix reason

每个 `SZrStructuredDiagnosticFix` 有 edit range、replacement text、title 与 applicability：

| applicability | UI/自动化含义 |
| --- | --- |
| `UNKNOWN` | 不应自动应用 |
| `MACHINE_APPLICABLE` | 在确认同一 document revision 后可提供一键应用 |
| `HAS_PLACEHOLDERS` | 可应用为模板，但需要用户补全 |
| `MAYBE_INCORRECT` | 只能作为建议，必须明确确认 |

当没有 fix 时，`noFixReason` 不能默认为“没有实现”。可用值包括 not applicable、insufficient
context、requires user decision、unsafe edit。ownership intrinsic 的错误就是典型例子：parser
可以指出缺少/多余参数，却不能自动选择用户希望 share、drop 还是改变 owner 类型，因此用
`REQUIRES_USER_DECISION` 标明不能安全改写。

应用 edit 前，client 必须验证 URI、document version、position encoding 与 range。即使一个
fix 标为 machine-applicable，也不能把它应用到已被用户修改的 snapshot；应重新请求
diagnostic/code action。

## 5. Parser 恢复与 compiler 失败

parser builder 对常见 shape 缺失有专用构造器，例如 missing expression after assignment、
missing right operand、missing condition、missing call/index/group close、array/object separator、
declaration/body close、using binder、import static path、pattern shape/arity/variant、for/foreach/
switch header、native extern spec、legacy property/ownership syntax。它们保证 code/range/suggestion
格式一致，但不保证 parser 恢复后的 AST 可执行。

正确的消费顺序是：

```text
parse source
  -> collect/copy structured diagnostics
  -> check parser error/fatal state
  -> only then compile semantic facts
  -> merge compiler diagnostics by source/provenance
  -> do not execute if an error prevents valid artifact
```

semantic/ownership diagnostics 通常依赖 canonical type、Place、loan、CFG 或 native provider
contract，因而不能由 lexer/parser 的 token rule 替代。类似地，artifact status 的 byte offset
和 expected/actual hash 应以 artifact diagnostic 投影，而不是伪造为一个 source range。

## 6. LSP 映射

LSP adapter 将 structured diagnostic 映射为 `SZrLspDiagnostic`：

| structured 字段 | LSP 字段/处理 |
| --- | --- |
| `location` | 按协商 UTF-16/UTF-8 codec 转为 `SZrLspRange` |
| parser severity 0..3 | LSP severity 1..4 |
| `code`、`descriptorId` | `code`、`descriptorId`、可选 `codeDescriptionHref` |
| message/cause/suggestion | `message` 和 UI 展示内容 |
| related info | `relatedInformation`（URI + LSP range + message） |
| fix | `fixes`（title/range/text/applicability） |
| no-fix reason | `noFixReason` |

LSP `GetDiagnostics` 返回的 `SZrArray` 需要 `ZrLanguageServer_Lsp_FreeDiagnostics` 释放。pull
diagnostic result id 从 immutable semantic snapshot 和完整 payload 计算，故 document edit、
semantic rebuild、provider reload 或 fix 应用都会使旧 result id 无效。push 和 pull 必须从同一
diagnostic store payload 投影，不能让一个通道丢失 fixes/no-fix reason。

## 7. Native、FFI 与 runtime failure

native registry 和 artifact loader 的失败常不具备一个直接的 `.zr` range。正确的处理方式是
保留原始结构化事实：

| 来源 | 应保留的关键数据 |
| --- | --- |
| native registry | error code、message、module name/source path、phase/ABI/capability/contract context |
| artifact reader | `EZrArtifactStatus`、section/row/offset、expected/actual token/version/hash |
| FFI contract | library locator、entry point、source mapping、target ABI/layout/callable mismatch |
| runtime exception | exception type/message、stack frames、execution/cancellation context |

若 native extern contract 附带 `SZrFfiSourceMapping`，LSP/CLI 可关联回原始 declaration；否则
不要制造一个看似精确却错误的 source range。native callback 必须在 C 边界清理临时 root/handle
后再 raise/return error，避免 exception 本身掩盖资源泄漏或 stack corruption。

## 8. 建议的 C diagnostic adapter

```c
static void publish_diagnostic(SZrState *state,
                               const SZrStructuredDiagnostic *source) {
    SZrStructuredDiagnostic stable;

    ZrParser_StructuredDiagnostic_Init(&stable);
    if (!ZrParser_StructuredDiagnostic_Copy(state, &stable, source)) {
        return;
    }

    /* Queue/serialize stable here; do not keep `source` pointers. */
    ZrParser_StructuredDiagnostic_Free(state, &stable);
}
```

队列消费者如果跨线程，除深复制外还必须遵守 `SZrState`/allocator 的线程和 lifetime 约束；
copy 不会让 parser state、source buffer、native provider 或 GC object 自动变成跨线程安全。
对于前端，请通过 LSP context/request API 生成自己的 immutable response，而不是将 parser
callback 的内部地址直接塞进 JSON。

## 9. 测试策略

每个新 diagnostic contract 至少覆盖：稳定 code/descriptor id、主 range、related information、
fix applicability 或 no-fix reason、parser recovery/semantic stop 行为、LSP position encoding、
push/pull payload 等价、native/artifact/FFI 的 expected-vs-actual 数据以及正确 free。测试应
断言结构化字段，不要只 `contains("error")`；文字本地化或措辞调整不应破坏正确的工具行为。
