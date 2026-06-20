---
doc_type: plan-detail
related_code:
  - zr_vm_parser/include/zr_vm_parser/diagnostic_builder.h
  - zr_vm_parser/src/zr_vm_parser/compiler/compiler_diagnostics.c
  - zr_vm_parser/include/zr_vm_parser/location.h
  - zr_vm_language_server/src/zr_vm_language_server/interface/lsp_interface.c
  - zr_vm_language_server/src/zr_vm_language_server/semantic/semantic_analyzer.c
---

# 02 · 诊断与错误表达强化

用户诉求精确版：**不要只说「预期 token / 预期符号」，要说清「具体哪里错、为什么错、怎么改」。** 现状其实已经有 `code/message/cause/suggestion`（40+ builder），所以本篇不是「从零做诊断」，而是补三个结构性缺口：(1) 关联位置链 relatedInformation；(2) 机器可应用 fix-it；(3) 类型/语义诊断的「期望 vs 实得 + 根因 + 最近修复」具体化。再加一个诊断码注册表做规范化与本地化。

---

## 1. 扩展诊断数据结构

在 `diagnostic_builder.h` 的 `SZrStructuredDiagnostic` 上增量扩展（**不破坏现有字段**）：

```c
// 关联位置（多位置诊断链）
typedef struct SZrDiagnosticRelated {
    SZrFileRange location;   // 相关位置（如「在此处声明」）
    SZrString   *message;    // 该位置的说明
} SZrDiagnosticRelated;

// 机器可应用修复
typedef struct SZrDiagnosticFix {
    SZrString   *title;      // 如「将 x 转换为 int」
    SZrFileRange editRange;  // 要替换的范围
    SZrString   *editText;   // 替换文本
    EZrFixApplicability applicability; // MACHINE_APPLICABLE / HAS_PLACEHOLDERS / MAYBE_INCORRECT
} SZrDiagnosticFix;

typedef struct SZrStructuredDiagnostic {
    EZrStructuredDiagnosticSeverity severity;
    SZrFileRange location;
    SZrString *code;
    SZrString *message;
    SZrString *cause;
    SZrString *suggestion;
    // 新增：
    SZrDiagnosticRelated *related;   TZrSize relatedCount;
    SZrDiagnosticFix     *fixes;     TZrSize fixCount;
    TZrUInt32 descriptorId;          // 指向诊断码注册表（见 §4）
} SZrStructuredDiagnostic;
```

`Applicability` 借鉴 Rust（`lua/rust/compiler/rustc_borrowck/src/borrowck_errors.rs`）：只有 `MACHINE_APPLICABLE` 允许 IDE 静默批量应用；其余需用户确认。

---

## 2. 关联位置链（relatedInformation）

让诊断从「单点」变「故事」。典型场景与挂接点：

| 场景 | 主位置 | related 位置 |
| --- | --- | --- |
| use after move（01 篇所有权） | 再次使用处 | 「值在此处被移动」move 点 |
| 重复定义 | 第二个定义 | 「已在此处定义」第一个定义 |
| 类型不匹配 | 实参/赋值处 | 「期望类型来自此处声明」 |
| borrow escape | 逃逸使用处 | 「借用源在此处」+「source 在此处结束」 |
| 未初始化使用 | READ 处 | 「在此处声明但未赋值」 |

LSP 侧映射到 `Diagnostic.relatedInformation`（`semantic_analyzer.c` / `lsp_interface.c` 的诊断序列化处）。借鉴 Rust `span_label`：每个 related 都是「带文字的位置标签」。

---

## 3. 类型 / 语义诊断具体化

替换 `compiler_diagnostics.c:249` 附近的「`strstr("Type mismatch")` -> 通用建议」模板，改为结构化构造。新增 builder（接 01 篇的推断结果）：

- `BuildTypeMismatchDetailed(loc, expectedType, actualType, fromLoc, conversionHint)`：
  - message：`期望 'int' 但得到 'string'`
  - cause：`'string' 不能隐式转换为 'int'`（说明为什么）
  - related：期望类型来自哪个声明/签名
  - fix（若存在最近可行转换）：`string -> int` 的显式 cast 编辑，applicability = HAS_PLACEHOLDERS
- `BuildUndefinedSymbol(loc, name, nearestCandidates)`：列出最接近的已知符号（编辑距离），fix = 改名编辑。
- `BuildUseBeforeInit` / `BuildUseAfterMove` / `BuildUnreachableCode`：接 01 篇 CFG/dataflow 事实，全部带 cause + related。
- union 模式诊断（已有 `BuildPatternUnknownField` / `BuildPatternArityMismatch` / `BuildPatternVariantMismatch`）补 `availableFields` 的 fix-it（补全字段）。

原则：每条诊断回答三问——**哪里**（location + related）、**为什么**（cause）、**怎么改**（suggestion 文本 + fix 编辑）。

---

## 4. 诊断码注册表（规范化 + 本地化）

新增 `diagnostic_registry.h` / `.c`，把今天的裸字符串 code 升级为注册表项（借鉴 Roslyn `DiagnosticDescriptor`，`lua/runtime/.../Roslyn/DiagnosticDescriptorHelper.cs`）：

```c
typedef struct SZrDiagnosticDescriptor {
    TZrUInt32   id;              // 稳定数值 ID，如 ZR1001
    const char *code;           // "use_after_move"
    const char *titleKey;       // 本地化 key
    const char *messageFormatKey;
    EZrStructuredDiagnosticSeverity defaultSeverity;
    const char *helpUri;        // 文档链接
    EZrLintCategory category;    // 可被项目配置覆盖严重级
} SZrDiagnosticDescriptor;
```

收益：
- 严重级可被项目配置覆盖（warning -> error / 关闭）。
- 中英文消息：`titleKey/messageFormatKey` 走消息表，先英文，预留中文（呼应「错误信息」诉求）。
- `helpUri` 让 LSP 诊断带「了解更多」链接。
- 可恢复性标记（借鉴 javac `DiagnosticFlag`）：标记哪些错误可继续编译产出更多诊断，而非首错即停。

---

## 5. 落地顺序

1. 扩展 `SZrStructuredDiagnostic`（related/fixes/descriptorId）+ init/free + LSP 序列化。
2. 诊断码注册表骨架，把现有 40+ builder 的 code 注册进表。
3. 关联位置：先给所有权 / 未初始化 / 重复定义三类接 related（依赖 01 篇）。
4. 类型不匹配具体化 + fix-it。
5. 本地化消息表（英文先行，留中文槽）。

测试：扩 `tests/language_server/test_union_pattern_diagnostics.c`、`test_ownership_diagnostics.c`，断言 related/fix/code 字段；新增类型不匹配与未初始化用例。
