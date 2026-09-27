#include "semantic/semantic_analyzer_internal.h"
#include "type_environment_declaration_binding.h"
#include "interface/lsp_interface_internal.h"

/** 在符号收集后把声明的规范身份写入类型环境，供后续表达式推断按作用域查找。 */
void ZrLanguageServer_SemanticAnalyzer_RegisterDeclarationTypeBinding(
        SZrState *state, SZrSemanticAnalyzer *analyzer, SZrString *name,
        const SZrInferredType *typeInfo, SZrAstNode *declarationNode) {
    const SZrSemanticSymbolRecord *record;
    SZrSymbol *symbol;
    EZrSemanticSymbolKind kind = declarationNode != ZR_NULL &&
                                        declarationNode->type == ZR_AST_PARAMETER
                                ? ZR_SEMANTIC_SYMBOL_KIND_PARAMETER
                                : ZR_SEMANTIC_SYMBOL_KIND_VARIABLE;
    if (declarationNode != ZR_NULL &&
        (declarationNode->type == ZR_AST_FUNCTION_DECLARATION ||
         declarationNode->type == ZR_AST_EXTERN_FUNCTION_DECLARATION)) {
        kind = ZR_SEMANTIC_SYMBOL_KIND_FUNCTION;
    }

    if (state == ZR_NULL || analyzer == ZR_NULL ||
        analyzer->compilerState == ZR_NULL ||
        analyzer->compilerState->typeEnv == ZR_NULL || name == ZR_NULL ||
        declarationNode == ZR_NULL) {
        return;
    }
    record = type_environment_find_declaration_symbol(
            analyzer->semanticContext, declarationNode, name, kind);
    /* Exact callables already have the compiler's overload binding. */
    if (record != ZR_NULL && kind == ZR_SEMANTIC_SYMBOL_KIND_FUNCTION &&
        record->typeId != ZR_SEMANTIC_ID_INVALID) {
        return;
    }
    symbol = record != ZR_NULL
                     ? ZrLanguageServer_SymbolTable_FindBySemanticId(
                               analyzer->symbolTable, record->id)
                     : ZR_NULL;
    if (symbol != ZR_NULL) {
        if (typeInfo == ZR_NULL) {
            ZrLanguageServer_SemanticAnalyzer_RegisterVariableTypeBinding(
                    state, analyzer->compilerState->typeEnv, name, ZR_NULL, symbol);
        } else if (record->typeId != ZR_SEMANTIC_ID_INVALID) {
            (void)ZrParser_TypeEnvironment_RegisterCanonicalVariable(
                    state, analyzer->compilerState->typeEnv, name, typeInfo,
                    record->id, record->typeId,
                    ZrLanguageServer_Lsp_GetSymbolLookupRange(symbol));
        } else {
            (void)ZrParser_TypeEnvironment_RegisterVariableEx(
                    state, analyzer->compilerState->typeEnv, name, typeInfo,
                    declarationNode, ZrLanguageServer_Lsp_GetSymbolLookupRange(symbol));
        }
    }
}

/** 复用 parser 已建立的声明身份；展示层符号不能覆盖编译器完整的函数类型事实。 */
TZrBool ZrLanguageServer_SemanticAnalyzer_RegisterSymbolSemantics(SZrSemanticAnalyzer *analyzer,
                                       SZrSymbol *symbol,
                                       EZrSemanticSymbolKind semanticKind,
                                       const SZrInferredType *typeInfo,
                                       EZrSemanticTypeKind typeKind) {
    TZrTypeId typeId = 0;
    TZrOverloadSetId overloadSetId = 0;
    TZrSymbolId symbolId;

    if (analyzer == ZR_NULL || analyzer->semanticContext == ZR_NULL || symbol == ZR_NULL) {
        return ZR_FALSE;
    }

    /* The compiler's pre-inference pass may already own this declaration.
     * In particular a function's canonical type is its full callable type,
     * whereas the presentation symbol's typeInfo is its return type. */
    const SZrSemanticSymbolRecord *existing =
            type_environment_find_declaration_symbol(
                    analyzer->semanticContext, symbol->astNode,
                    symbol->name, semanticKind);
    if (existing != ZR_NULL) {
        symbol->semanticId = existing->id;
        symbol->semanticTypeId = existing->typeId;
        symbol->overloadSetId = existing->overloadSetId;
        return ZR_TRUE;
    }

    if (semanticKind == ZR_SEMANTIC_SYMBOL_KIND_TYPE && typeInfo == ZR_NULL) {
        typeId = ZrParser_Semantic_RegisterNamedType(analyzer->semanticContext,
                                             symbol->name,
                                             typeKind,
                                             symbol->astNode);
    } else if (typeInfo != ZR_NULL &&
               !(semanticKind == ZR_SEMANTIC_SYMBOL_KIND_FUNCTION &&
                 symbol->astNode != ZR_NULL &&
                 (symbol->astNode->type == ZR_AST_FUNCTION_DECLARATION ||
                  symbol->astNode->type == ZR_AST_EXTERN_FUNCTION_DECLARATION))) {
        typeId = ZrParser_Semantic_RegisterInferredType(analyzer->semanticContext,
                                                typeInfo,
                                                typeKind,
                                                typeInfo->typeName,
                                                symbol->astNode);
    }

    if (semanticKind == ZR_SEMANTIC_SYMBOL_KIND_FUNCTION) {
        overloadSetId = ZrParser_Semantic_GetOrCreateOverloadSet(analyzer->semanticContext, symbol->name);
    }

    symbolId = ZrParser_Semantic_RegisterSymbol(analyzer->semanticContext,
                                        symbol->name,
                                        semanticKind,
                                        typeId,
                                        overloadSetId,
                                        symbol->astNode,
                                        symbol->location);
    if (overloadSetId != 0) {
        ZrParser_Semantic_AddOverloadMember(analyzer->semanticContext, overloadSetId, symbolId);
    }

    symbol->semanticId = symbolId;
    symbol->semanticTypeId = typeId;
    symbol->overloadSetId = overloadSetId;
    return symbolId != 0;
}

/** 变量查询需要保留语义 ID 和可见范围；尚无精确类型时仍登记身份，避免引用漂移。 */
void ZrLanguageServer_SemanticAnalyzer_RegisterVariableTypeBinding(SZrState *state,
                                                  SZrTypeEnvironment *typeEnv,
                                                  SZrString *name,
                                                  SZrInferredType *typeInfo,
                                                  SZrSymbol *symbol) {
    SZrFileRange declarationRange;

    if (state == ZR_NULL || typeEnv == ZR_NULL || name == ZR_NULL) {
        return;
    }

    if (typeInfo == ZR_NULL) {
        if (symbol != ZR_NULL && symbol->astNode != ZR_NULL) {
            SZrInferredType unavailableType;
            ZrParser_InferredType_Init(state, &unavailableType, ZR_VALUE_TYPE_OBJECT);
            (void)ZrParser_TypeEnvironment_RegisterCanonicalVariable(
                    state, typeEnv, name, &unavailableType,
                    symbol->semanticId, symbol->semanticTypeId,
                    ZrLanguageServer_Lsp_GetSymbolLookupRange(symbol));
            ZrParser_InferredType_Free(state, &unavailableType);
        }
        return;
    }

    if (symbol != ZR_NULL &&
        symbol->semanticId != ZR_SEMANTIC_ID_INVALID &&
        symbol->semanticTypeId != ZR_SEMANTIC_ID_INVALID) {
        declarationRange = ZrLanguageServer_Lsp_GetSymbolLookupRange(symbol);
        if (declarationRange.source != ZR_NULL) {
            (void)ZrParser_TypeEnvironment_RegisterCanonicalVariable(state,
                                                                    typeEnv,
                                                                    name,
                                                                    typeInfo,
                                                                    symbol->semanticId,
                                                                    symbol->semanticTypeId,
                                                                    declarationRange);
        }
        return;
    }

    if (symbol != ZR_NULL && symbol->astNode != ZR_NULL) {
        (void)ZrParser_TypeEnvironment_RegisterVariableEx(
                state, typeEnv, name, typeInfo, symbol->astNode,
                ZrLanguageServer_Lsp_GetSymbolLookupRange(symbol));
        return;
    }
    ZrParser_TypeEnvironment_RegisterVariable(state, typeEnv, name, typeInfo);
}

