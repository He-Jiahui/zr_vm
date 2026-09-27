#ifndef ZR_VM_LANGUAGE_SERVER_SEMANTIC_ANALYZER_UNION_PATTERNS_H
#define ZR_VM_LANGUAGE_SERVER_SEMANTIC_ANALYZER_UNION_PATTERNS_H

#include "semantic/semantic_analyzer_internal.h"

/** 模式解析借用 AST 名称和绑定数组；仅 resourceType 是需由调用方配对释放的类型副本。 */
typedef struct SZrSemanticUnionPatternResolution {
    SZrString *variantName;
    SZrAstNodeArray *bindings;
    SZrAstNode *variant;
    SZrInferredType resourceType;
    TZrBool hasResourceType;
} SZrSemanticUnionPatternResolution;

/** 每次模式查询前初始化；重复使用同一结果前先调用 Free。 */
void ZrLanguageServer_SemanticAnalyzer_UnionPatternResolutionInit(
        SZrState *state,
        SZrSemanticUnionPatternResolution *resolution);

void ZrLanguageServer_SemanticAnalyzer_UnionPatternResolutionFree(
        SZrState *state,
        SZrSemanticUnionPatternResolution *resolution);

/** using 守卫按资源实际类型解析 union 变体，结果供符号收集和类型检查共用。 */
TZrBool ZrLanguageServer_SemanticAnalyzer_ResolveUsingUnionPattern(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrUsingStatement *usingStmt,
        SZrSemanticUnionPatternResolution *resolution);

/** switch case 需借助主表达式类型确定简写变体；失败不会生成 payload 绑定。 */
TZrBool ZrLanguageServer_SemanticAnalyzer_ResolveSwitchUnionPattern(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *caseValue,
        const SZrInferredType *subjectType,
        SZrSemanticUnionPatternResolution *resolution);

/** 仅当所有变体被覆盖时把 default 标为不可达，供控制流事实和诊断查询使用。 */
void ZrLanguageServer_SemanticAnalyzer_AnalyzeSwitchUnionExhaustiveness(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        SZrAstNode *switchNode,
        const SZrInferredType *subjectType);

/** 按调用阶段选择登记展示符号或类型环境绑定；payload 只在对应分支内可见。 */
void ZrLanguageServer_SemanticAnalyzer_RegisterUnionPatternBindings(
        SZrState *state,
        SZrSemanticAnalyzer *analyzer,
        const SZrSemanticUnionPatternResolution *resolution,
        TZrBool registerSymbols,
        TZrBool registerTypeEnv);

#endif
