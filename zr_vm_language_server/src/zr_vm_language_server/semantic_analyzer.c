//
// Created by Auto on 2025/01/XX.
//

#include "semantic_analyzer_internal.h"
#include "zr_vm_library/native_registry.h"

static void semantic_get_string_view(SZrString *value, TZrNativeString *text, TZrSize *length) {
    if (text == ZR_NULL || length == ZR_NULL) {
        return;
    }

    *text = ZR_NULL;
    *length = 0;
    if (value == ZR_NULL) {
        return;
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        *text = ZrCore_String_GetNativeStringShort(value);
        *length = value->shortStringLength;
    } else {
        *text = ZrCore_String_GetNativeString(value);
        *length = value->longStringLength;
    }
}

static const TZrChar *semantic_symbol_kind_text(EZrSymbolType type) {
    switch (type) {
        case ZR_SYMBOL_FUNCTION: return "function";
        case ZR_SYMBOL_CLASS: return "class";
        case ZR_SYMBOL_STRUCT: return "struct";
        case ZR_SYMBOL_METHOD: return "method";
        case ZR_SYMBOL_PROPERTY: return "property";
        case ZR_SYMBOL_FIELD: return "field";
        case ZR_SYMBOL_PARAMETER: return "parameter";
        case ZR_SYMBOL_ENUM: return "enum";
        case ZR_SYMBOL_INTERFACE: return "interface";
        case ZR_SYMBOL_MODULE: return "module";
        default: return "variable";
    }
}

static const TZrChar *semantic_access_modifier_text(EZrAccessModifier accessModifier) {
    switch (accessModifier) {
        case ZR_ACCESS_PUBLIC: return "public";
        case ZR_ACCESS_PROTECTED: return "protected";
        case ZR_ACCESS_PRIVATE:
        default:
            return "private";
    }
}

static void semantic_build_symbol_detail(SZrState *state,
                                         SZrSymbol *symbol,
                                         TZrChar *buffer,
                                         TZrSize bufferSize) {
    TZrChar typeBuffer[128];
    const TZrChar *typeText = semantic_symbol_kind_text(symbol != ZR_NULL ? symbol->type : ZR_SYMBOL_VARIABLE);
    const TZrChar *accessText = semantic_access_modifier_text(symbol != ZR_NULL
                                                              ? symbol->accessModifier
                                                              : ZR_ACCESS_PRIVATE);

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    if (state != ZR_NULL && symbol != ZR_NULL && symbol->typeInfo != ZR_NULL) {
        typeText = ZrParser_TypeNameString_Get(state, symbol->typeInfo, typeBuffer, sizeof(typeBuffer));
    }

    snprintf(buffer, bufferSize, "%s %s", accessText, typeText != ZR_NULL ? typeText : "object");
}

static TZrBool semantic_completion_has_label(SZrArray *items, const TZrChar *label) {
    if (items == ZR_NULL || label == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrSize index = 0; index < items->length; index++) {
        SZrCompletionItem **itemPtr = (SZrCompletionItem **)ZrCore_Array_Get(items, index);
        TZrNativeString itemLabel;
        TZrSize itemLabelLength;
        TZrSize labelLength;

        if (itemPtr == ZR_NULL || *itemPtr == ZR_NULL || (*itemPtr)->label == ZR_NULL) {
            continue;
        }

        semantic_get_string_view((*itemPtr)->label, &itemLabel, &itemLabelLength);
        labelLength = strlen(label);
        if (itemLabel != ZR_NULL &&
            itemLabelLength == labelLength &&
            memcmp(itemLabel, label, labelLength) == 0) {
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static void semantic_append_completion_item(SZrState *state,
                                            SZrArray *result,
                                            const TZrChar *label,
                                            const TZrChar *kind,
                                            const TZrChar *detail,
                                            SZrInferredType *typeInfo) {
    SZrCompletionItem *item;

    if (state == ZR_NULL || result == ZR_NULL || label == ZR_NULL ||
        semantic_completion_has_label(result, label)) {
        return;
    }

    item = ZrLanguageServer_CompletionItem_New(state, label, kind, detail, ZR_NULL, typeInfo);
    if (item != ZR_NULL) {
        ZrCore_Array_Push(state, result, &item);
    }
}

static void semantic_append_symbol_completion(SZrState *state,
                                              SZrArray *result,
                                              SZrSymbol *symbol) {
    TZrNativeString nameText;
    TZrSize nameLength;
    TZrChar label[256];
    TZrChar detail[160];

    if (state == ZR_NULL || result == ZR_NULL || symbol == ZR_NULL || symbol->name == ZR_NULL) {
        return;
    }

    semantic_get_string_view(symbol->name, &nameText, &nameLength);
    if (nameText == ZR_NULL || nameLength == 0 || nameLength >= sizeof(label)) {
        return;
    }

    memcpy(label, nameText, nameLength);
    label[nameLength] = '\0';
    semantic_build_symbol_detail(state, symbol, detail, sizeof(detail));
    semantic_append_completion_item(state,
                                    result,
                                    label,
                                    semantic_symbol_kind_text(symbol->type),
                                    detail,
                                    symbol->typeInfo);
}

static const ZrLibModuleDescriptor *semantic_resolve_native_module_descriptor(SZrState *state,
                                                                              const TZrChar *moduleName) {
    if (state == ZR_NULL || state->global == ZR_NULL || moduleName == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrLibrary_NativeRegistry_FindModule(state->global, moduleName);
}

static void semantic_append_native_module_descriptor_completions(SZrState *state,
                                                                 const ZrLibModuleDescriptor *descriptor,
                                                                 SZrArray *result,
                                                                 TZrSize depth) {
    if (state == ZR_NULL || descriptor == ZR_NULL || result == ZR_NULL || depth > 4) {
        return;
    }

    for (TZrSize index = 0; index < descriptor->typeHintCount; index++) {
        const ZrLibTypeHintDescriptor *hint = &descriptor->typeHints[index];
        if (hint->symbolName != ZR_NULL) {
            semantic_append_completion_item(state,
                                            result,
                                            hint->symbolName,
                                            hint->symbolKind != ZR_NULL ? hint->symbolKind : "symbol",
                                            hint->signature,
                                            ZR_NULL);
        }
    }

    for (TZrSize index = 0; index < descriptor->functionCount; index++) {
        const ZrLibFunctionDescriptor *functionDescriptor = &descriptor->functions[index];
        TZrChar detail[192];

        if (functionDescriptor->name == ZR_NULL) {
            continue;
        }

        snprintf(detail,
                 sizeof(detail),
                 "%s(...): %s",
                 functionDescriptor->name,
                 functionDescriptor->returnTypeName != ZR_NULL ? functionDescriptor->returnTypeName : "object");
        semantic_append_completion_item(state, result, functionDescriptor->name, "function", detail, ZR_NULL);
    }

    for (TZrSize index = 0; index < descriptor->typeCount; index++) {
        const ZrLibTypeDescriptor *typeDescriptor = &descriptor->types[index];
        const TZrChar *kind;
        TZrChar detail[192];

        if (typeDescriptor->name == ZR_NULL) {
            continue;
        }

        kind = (typeDescriptor->prototypeType == ZR_OBJECT_PROTOTYPE_TYPE_CLASS) ? "class" : "struct";
        snprintf(detail, sizeof(detail), "%s %s", kind, typeDescriptor->name);
        semantic_append_completion_item(state, result, typeDescriptor->name, kind, detail, ZR_NULL);

        for (TZrSize methodIndex = 0; methodIndex < typeDescriptor->methodCount; methodIndex++) {
            const ZrLibMethodDescriptor *methodDescriptor = &typeDescriptor->methods[methodIndex];
            TZrChar methodDetail[192];

            if (methodDescriptor->name == ZR_NULL) {
                continue;
            }

            snprintf(methodDetail,
                     sizeof(methodDetail),
                     "%s(...): %s",
                     methodDescriptor->name,
                     methodDescriptor->returnTypeName != ZR_NULL ? methodDescriptor->returnTypeName : "object");
            semantic_append_completion_item(state, result, methodDescriptor->name, "method", methodDetail, ZR_NULL);
        }
    }

    for (TZrSize index = 0; index < descriptor->moduleLinkCount; index++) {
        const ZrLibModuleLinkDescriptor *linkDescriptor = &descriptor->moduleLinks[index];
        const ZrLibModuleDescriptor *linkedDescriptor;

        if (linkDescriptor->moduleName == ZR_NULL) {
            continue;
        }

        linkedDescriptor = semantic_resolve_native_module_descriptor(state, linkDescriptor->moduleName);
        semantic_append_native_module_descriptor_completions(state, linkedDescriptor, result, depth + 1);
    }
}

static void semantic_append_imported_module_completions(SZrState *state,
                                                        SZrAstNode *node,
                                                        SZrArray *result) {
    const ZrLibModuleDescriptor *descriptor;
    TZrNativeString moduleText;
    TZrSize moduleLength;
    TZrChar moduleName[256];

    if (state == ZR_NULL || node == ZR_NULL || result == ZR_NULL) {
        return;
    }

    switch (node->type) {
        case ZR_AST_SCRIPT:
            if (node->data.script.statements != ZR_NULL && node->data.script.statements->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.script.statements->count; index++) {
                    semantic_append_imported_module_completions(state,
                                                                node->data.script.statements->nodes[index],
                                                                result);
                }
            }
            return;

        case ZR_AST_BLOCK:
            if (node->data.block.body != ZR_NULL && node->data.block.body->nodes != ZR_NULL) {
                for (TZrSize index = 0; index < node->data.block.body->count; index++) {
                    semantic_append_imported_module_completions(state,
                                                                node->data.block.body->nodes[index],
                                                                result);
                }
            }
            return;

        case ZR_AST_FUNCTION_DECLARATION:
            semantic_append_imported_module_completions(state, node->data.functionDeclaration.body, result);
            return;

        case ZR_AST_TEST_DECLARATION:
            semantic_append_imported_module_completions(state, node->data.testDeclaration.body, result);
            return;

        case ZR_AST_VARIABLE_DECLARATION:
            if (node->data.variableDeclaration.value == ZR_NULL ||
                node->data.variableDeclaration.value->type != ZR_AST_IMPORT_EXPRESSION ||
                node->data.variableDeclaration.value->data.importExpression.modulePath == ZR_NULL ||
                node->data.variableDeclaration.value->data.importExpression.modulePath->type != ZR_AST_STRING_LITERAL ||
                node->data.variableDeclaration.value->data.importExpression.modulePath->data.stringLiteral.value == ZR_NULL) {
                return;
            }

            semantic_get_string_view(node->data.variableDeclaration.value->data.importExpression.modulePath->data.stringLiteral.value,
                                     &moduleText,
                                     &moduleLength);
            if (moduleText == ZR_NULL || moduleLength == 0 || moduleLength >= sizeof(moduleName)) {
                return;
            }

            memcpy(moduleName, moduleText, moduleLength);
            moduleName[moduleLength] = '\0';
            descriptor = semantic_resolve_native_module_descriptor(state, moduleName);
            semantic_append_native_module_descriptor_completions(state, descriptor, result, 0);
            return;

        default:
            return;
    }
}

SZrSemanticAnalyzer *ZrLanguageServer_SemanticAnalyzer_New(SZrState *state) {
    if (state == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrSemanticAnalyzer *analyzer = (SZrSemanticAnalyzer *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrSemanticAnalyzer));
    if (analyzer == ZR_NULL) {
        return ZR_NULL;
    }
    
    analyzer->state = state;
    analyzer->symbolTable = ZrLanguageServer_SymbolTable_New(state);
    analyzer->referenceTracker = ZR_NULL;
    analyzer->ast = ZR_NULL;
    analyzer->cache = ZR_NULL;
    analyzer->enableCache = ZR_TRUE; // 默认启用缓存
    analyzer->compilerState = ZR_NULL; // 延迟创建
    analyzer->semanticContext = ZR_NULL;
    analyzer->hirModule = ZR_NULL;
    
    if (analyzer->symbolTable == ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, analyzer, sizeof(SZrSemanticAnalyzer));
        return ZR_NULL;
    }
    
    analyzer->referenceTracker = ZrLanguageServer_ReferenceTracker_New(state, analyzer->symbolTable);
    if (analyzer->referenceTracker == ZR_NULL) {
        ZrLanguageServer_SymbolTable_Free(state, analyzer->symbolTable);
        ZrCore_Memory_RawFree(state->global, analyzer, sizeof(SZrSemanticAnalyzer));
        return ZR_NULL;
    }
    
    ZrCore_Array_Init(state, &analyzer->diagnostics, sizeof(SZrDiagnostic *), 8);
    
    // 创建缓存
    analyzer->cache = (SZrAnalysisCache *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrAnalysisCache));
    if (analyzer->cache != ZR_NULL) {
        analyzer->cache->isValid = ZR_FALSE;
        analyzer->cache->astHash = 0;
        ZrCore_Array_Init(state, &analyzer->cache->cachedDiagnostics, sizeof(SZrDiagnostic *), 8);
        ZrCore_Array_Init(state, &analyzer->cache->cachedSymbols, sizeof(SZrSymbol *), 8);
    }
    
    return analyzer;
}

// 释放语义分析器
void ZrLanguageServer_SemanticAnalyzer_Free(SZrState *state, SZrSemanticAnalyzer *analyzer) {
    if (state == ZR_NULL || analyzer == ZR_NULL) {
        return;
    }

    // 释放所有诊断
    for (TZrSize i = 0; i < analyzer->diagnostics.length; i++) {
        SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->diagnostics, i);
        if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
            ZrLanguageServer_Diagnostic_Free(state, *diagPtr);
        }
    }
    
    ZrCore_Array_Free(state, &analyzer->diagnostics);
    
    // 释放缓存
    if (analyzer->cache != ZR_NULL) {
        // 释放缓存的诊断信息
        for (TZrSize i = 0; i < analyzer->cache->cachedDiagnostics.length; i++) {
            SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->cache->cachedDiagnostics, i);
            if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
                // 注意：诊断可能在 analyzer->diagnostics 中，避免重复释放
            }
        }
        ZrCore_Array_Free(state, &analyzer->cache->cachedDiagnostics);
        ZrCore_Array_Free(state, &analyzer->cache->cachedSymbols);
        ZrCore_Memory_RawFree(state->global, analyzer->cache, sizeof(SZrAnalysisCache));
    }

    if (analyzer->referenceTracker != ZR_NULL) {
        ZrLanguageServer_ReferenceTracker_Free(state, analyzer->referenceTracker);
    }

    if (analyzer->symbolTable != ZR_NULL) {
        ZrLanguageServer_SymbolTable_Free(state, analyzer->symbolTable);
    }

    // 释放编译器状态
    if (analyzer->compilerState != ZR_NULL) {
        ZrParser_CompilerState_Free(analyzer->compilerState);
        ZrCore_Memory_RawFree(state->global, analyzer->compilerState, sizeof(SZrCompilerState));
    }

    analyzer->semanticContext = ZR_NULL;
    analyzer->hirModule = ZR_NULL;

    ZrCore_Memory_RawFree(state->global, analyzer, sizeof(SZrSemanticAnalyzer));
}

// 辅助函数：从 AST 节点提取标识符名称

TZrBool ZrLanguageServer_SemanticAnalyzer_Analyze(SZrState *state, 
                                 SZrSemanticAnalyzer *analyzer,
                                 SZrAstNode *ast) {
    if (state == ZR_NULL || analyzer == ZR_NULL || ast == ZR_NULL) {
        return ZR_FALSE;
    }

    analyzer->ast = ast;
    
    // 检查缓存
    TZrSize astHash = 0;
    if (analyzer->enableCache && analyzer->cache != ZR_NULL) {
        astHash = ZrLanguageServer_SemanticAnalyzer_ComputeAstHash(ast);
        if (analyzer->cache->isValid && analyzer->cache->astHash == astHash) {
            // 缓存有效，使用缓存结果
            // 复制缓存的诊断信息
            for (TZrSize i = 0; i < analyzer->cache->cachedDiagnostics.length; i++) {
                SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->cache->cachedDiagnostics, i);
                if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
                    ZrCore_Array_Push(state, &analyzer->diagnostics, diagPtr);
                }
            }
            return ZR_TRUE;
        }
    }

    // 清除旧的诊断信息
    for (TZrSize i = 0; i < analyzer->diagnostics.length; i++) {
        SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->diagnostics, i);
        if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
            ZrLanguageServer_Diagnostic_Free(state, *diagPtr);
        }
    }
    analyzer->diagnostics.length = 0;
    
    if (!ZrLanguageServer_SemanticAnalyzer_PrepareState(state, analyzer, ast)) {
        return ZR_FALSE;
    }
    
    // 第一阶段：收集符号定义
    ZrLanguageServer_SemanticAnalyzer_CollectSymbolsFromAst(state, analyzer, ast);
    
    // 第二阶段：收集引用
    ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(state, analyzer, ast);
    
    // 第三阶段：类型检查（集成类型推断系统）
    // 遍历 AST 进行类型检查
    if (analyzer->compilerState != ZR_NULL) {
        ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(state, analyzer, ast);
    }
    
    // 更新缓存
    if (analyzer->enableCache && analyzer->cache != ZR_NULL) {
        analyzer->cache->astHash = astHash;
        analyzer->cache->isValid = ZR_TRUE;
        
        // 复制诊断信息到缓存
        analyzer->cache->cachedDiagnostics.length = 0;
        for (TZrSize i = 0; i < analyzer->diagnostics.length; i++) {
            SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->diagnostics, i);
            if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
                ZrCore_Array_Push(state, &analyzer->cache->cachedDiagnostics, diagPtr);
            }
        }
    }
    
    return ZR_TRUE;
}

// 获取诊断信息
TZrBool ZrLanguageServer_SemanticAnalyzer_GetDiagnostics(SZrState *state,
                                        SZrSemanticAnalyzer *analyzer,
                                        SZrArray *result) {
    if (state == ZR_NULL || analyzer == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    // 初始化结果数组
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrDiagnostic *), analyzer->diagnostics.length);
    }
    
    // 复制所有诊断
    for (TZrSize i = 0; i < analyzer->diagnostics.length; i++) {
        SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->diagnostics, i);
        if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
            ZrCore_Array_Push(state, result, diagPtr);
        }
    }
    
    return ZR_TRUE;
}

// 获取位置的符号
SZrSymbol *ZrLanguageServer_SemanticAnalyzer_GetSymbolAt(SZrSemanticAnalyzer *analyzer,
                                         SZrFileRange position) {
    SZrReference *reference;

    if (analyzer == ZR_NULL) {
        return ZR_NULL;
    }

    if (analyzer->referenceTracker != ZR_NULL) {
        reference = ZrLanguageServer_ReferenceTracker_FindReferenceAt(analyzer->referenceTracker, position);
        if (reference != ZR_NULL) {
            return reference->symbol;
        }
    }
    
    return ZrLanguageServer_SymbolTable_FindDefinition(analyzer->symbolTable, position);
}

// 获取悬停信息
TZrBool ZrLanguageServer_SemanticAnalyzer_GetHoverInfo(SZrState *state,
                                     SZrSemanticAnalyzer *analyzer,
                                     SZrFileRange position,
                                     SZrHoverInfo **result) {
    TZrNativeString nameStr;
    TZrSize nameLen;
    TZrChar typeBuffer[128];
    TZrChar buffer[512];
    const TZrChar *kindText;
    const TZrChar *typeText;
    const TZrChar *accessText;

    if (state == ZR_NULL || analyzer == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    SZrSymbol *symbol = ZrLanguageServer_SemanticAnalyzer_GetSymbolAt(analyzer, position);
    if (symbol == ZR_NULL) {
        return ZR_FALSE;
    }

    semantic_get_string_view(symbol->name, &nameStr, &nameLen);
    if (nameStr == ZR_NULL || nameLen == 0) {
        return ZR_FALSE;
    }

    kindText = semantic_symbol_kind_text(symbol->type);
    accessText = semantic_access_modifier_text(symbol->accessModifier);
    typeText = (symbol->typeInfo != ZR_NULL)
               ? ZrParser_TypeNameString_Get(state, symbol->typeInfo, typeBuffer, sizeof(typeBuffer))
               : kindText;
    snprintf(buffer,
             sizeof(buffer),
             "**%s**: %.*s\n\nType: %s\nAccess: %s",
             kindText,
             (int)nameLen,
             nameStr,
             typeText != ZR_NULL ? typeText : "object",
             accessText);

    *result = ZrLanguageServer_HoverInfo_New(state, buffer, symbol->selectionRange, symbol->typeInfo);
    return *result != ZR_NULL;
}

// 获取代码补全
TZrBool ZrLanguageServer_SemanticAnalyzer_GetCompletions(SZrState *state,
                                       SZrSemanticAnalyzer *analyzer,
                                       SZrFileRange position,
                                       SZrArray *result) {
    SZrArray visibleSymbols;

    if (state == ZR_NULL || analyzer == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    
    if (!result->isValid) {
        ZrCore_Array_Init(state, result, sizeof(SZrCompletionItem *), 8);
    }

    ZrCore_Array_Construct(&visibleSymbols);
    if (ZrLanguageServer_SymbolTable_GetVisibleSymbolsAtPosition(state,
                                                                 analyzer->symbolTable,
                                                                 position,
                                                                 &visibleSymbols)) {
        for (TZrSize index = 0; index < visibleSymbols.length; index++) {
            SZrSymbol **symbolPtr = (SZrSymbol **)ZrCore_Array_Get(&visibleSymbols, index);
            if (symbolPtr != ZR_NULL && *symbolPtr != ZR_NULL) {
                semantic_append_symbol_completion(state, result, *symbolPtr);
            }
        }
    }
    ZrCore_Array_Free(state, &visibleSymbols);

    if (analyzer->ast != ZR_NULL) {
        semantic_append_imported_module_completions(state, analyzer->ast, result);
    }

    return ZR_TRUE;
}

// 添加诊断
TZrBool ZrLanguageServer_SemanticAnalyzer_AddDiagnostic(SZrState *state,
                                     SZrSemanticAnalyzer *analyzer,
                                     EZrDiagnosticSeverity severity,
                                     SZrFileRange location,
                                     const TZrChar *message,
                                     const TZrChar *code) {
    if (state == ZR_NULL || analyzer == ZR_NULL || message == ZR_NULL) {
        return ZR_FALSE;
    }
    
    SZrDiagnostic *diagnostic = ZrLanguageServer_Diagnostic_New(state, severity, location, message, code);
    if (diagnostic == ZR_NULL) {
        return ZR_FALSE;
    }
    
    ZrCore_Array_Push(state, &analyzer->diagnostics, &diagnostic);
    
    return ZR_TRUE;
}

// 创建诊断
SZrDiagnostic *ZrLanguageServer_Diagnostic_New(SZrState *state,
                                EZrDiagnosticSeverity severity,
                                SZrFileRange location,
                                const TZrChar *message,
                                const TZrChar *code) {
    if (state == ZR_NULL || message == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrDiagnostic *diagnostic = (SZrDiagnostic *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrDiagnostic));
    if (diagnostic == ZR_NULL) {
        return ZR_NULL;
    }
    
    diagnostic->severity = severity;
    diagnostic->location = location;
    diagnostic->message = ZrCore_String_Create(state, (TZrNativeString)message, strlen(message));
    diagnostic->code = code != ZR_NULL ? ZrCore_String_Create(state, (TZrNativeString)code, strlen(code)) : ZR_NULL;
    
    if (diagnostic->message == ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, diagnostic, sizeof(SZrDiagnostic));
        return ZR_NULL;
    }
    
    return diagnostic;
}

// 释放诊断
void ZrLanguageServer_Diagnostic_Free(SZrState *state, SZrDiagnostic *diagnostic) {
    if (state == ZR_NULL || diagnostic == ZR_NULL) {
        return;
    }
    
    if (diagnostic->message != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    if (diagnostic->code != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    ZrCore_Memory_RawFree(state->global, diagnostic, sizeof(SZrDiagnostic));
}

// 创建补全项
SZrCompletionItem *ZrLanguageServer_CompletionItem_New(SZrState *state,
                                       const TZrChar *label,
                                       const TZrChar *kind,
                                       const TZrChar *detail,
                                       const TZrChar *documentation,
                                       SZrInferredType *typeInfo) {
    if (state == ZR_NULL || label == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrCompletionItem *item = (SZrCompletionItem *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrCompletionItem));
    if (item == ZR_NULL) {
        return ZR_NULL;
    }
    
    item->label = ZrCore_String_Create(state, (TZrNativeString)label, strlen(label));
    item->kind = kind != ZR_NULL ? ZrCore_String_Create(state, (TZrNativeString)kind, strlen(kind)) : ZR_NULL;
    item->detail = detail != ZR_NULL ? ZrCore_String_Create(state, (TZrNativeString)detail, strlen(detail)) : ZR_NULL;
    item->documentation = documentation != ZR_NULL ? ZrCore_String_Create(state, (TZrNativeString)documentation, strlen(documentation)) : ZR_NULL;
    item->typeInfo = typeInfo; // 不复制，只是引用
    
    if (item->label == ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, item, sizeof(SZrCompletionItem));
        return ZR_NULL;
    }
    
    return item;
}

// 释放补全项
void ZrLanguageServer_CompletionItem_Free(SZrState *state, SZrCompletionItem *item) {
    if (state == ZR_NULL || item == ZR_NULL) {
        return;
    }
    
    if (item->label != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    if (item->kind != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    if (item->detail != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    if (item->documentation != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    ZrCore_Memory_RawFree(state->global, item, sizeof(SZrCompletionItem));
}

// 创建悬停信息
SZrHoverInfo *ZrLanguageServer_HoverInfo_New(SZrState *state,
                              const TZrChar *contents,
                              SZrFileRange range,
                              SZrInferredType *typeInfo) {
    if (state == ZR_NULL || contents == ZR_NULL) {
        return ZR_NULL;
    }
    
    SZrHoverInfo *info = (SZrHoverInfo *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrHoverInfo));
    if (info == ZR_NULL) {
        return ZR_NULL;
    }
    
    info->contents = ZrCore_String_Create(state, (TZrNativeString)contents, strlen(contents));
    info->range = range;
    info->typeInfo = typeInfo; // 不复制，只是引用
    
    if (info->contents == ZR_NULL) {
        ZrCore_Memory_RawFree(state->global, info, sizeof(SZrHoverInfo));
        return ZR_NULL;
    }
    
    return info;
}

// 释放悬停信息
void ZrLanguageServer_HoverInfo_Free(SZrState *state, SZrHoverInfo *info) {
    if (state == ZR_NULL || info == ZR_NULL) {
        return;
    }
    
    if (info->contents != ZR_NULL) {
        // SZrString 由 GC 管理，不需要手动释放
    }
    ZrCore_Memory_RawFree(state->global, info, sizeof(SZrHoverInfo));
}

// 启用/禁用缓存
void ZrLanguageServer_SemanticAnalyzer_SetCacheEnabled(SZrSemanticAnalyzer *analyzer, TZrBool enabled) {
    if (analyzer == ZR_NULL) {
        return;
    }
    analyzer->enableCache = enabled;
}

// 清除缓存
void ZrLanguageServer_SemanticAnalyzer_ClearCache(SZrState *state, SZrSemanticAnalyzer *analyzer) {
    if (state == ZR_NULL || analyzer == ZR_NULL || analyzer->cache == ZR_NULL) {
        return;
    }
    
    analyzer->cache->isValid = ZR_FALSE;
    analyzer->cache->astHash = 0;
    
    // 清除缓存的诊断信息
    for (TZrSize i = 0; i < analyzer->cache->cachedDiagnostics.length; i++) {
        SZrDiagnostic **diagPtr = (SZrDiagnostic **)ZrCore_Array_Get(&analyzer->cache->cachedDiagnostics, i);
        if (diagPtr != ZR_NULL && *diagPtr != ZR_NULL) {
            // 注意：诊断可能在 analyzer->diagnostics 中，避免重复释放
        }
    }
    analyzer->cache->cachedDiagnostics.length = 0;
    analyzer->cache->cachedSymbols.length = 0;
}
