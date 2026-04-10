#ifndef ZR_VM_LANGUAGE_SERVER_SEMANTIC_ANALYZER_INTERNAL_H
#define ZR_VM_LANGUAGE_SERVER_SEMANTIC_ANALYZER_INTERNAL_H

#include "zr_vm_language_server/semantic_analyzer.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"

#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/type_inference.h"
#include "zr_vm_parser/type_system.h"

#include <stdio.h>
#include <string.h>

ZR_FORCE_INLINE const TZrChar *semantic_string_native(SZrString *value) {
    if (value == ZR_NULL) {
        return ZR_NULL;
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        return ZrCore_String_GetNativeStringShort(value);
    }

    return ZrCore_String_GetNativeString(value);
}

ZR_FORCE_INLINE TZrBool ZrLanguageServer_SemanticAnalyzer_IsWeakObjectType(const SZrInferredType *typeInfo) {
    return typeInfo != ZR_NULL &&
           typeInfo->baseType == ZR_VALUE_TYPE_OBJECT &&
           typeInfo->typeName == ZR_NULL &&
           (!typeInfo->elementTypes.isValid || typeInfo->elementTypes.length == 0);
}

ZR_FORCE_INLINE TZrBool ZrLanguageServer_SemanticAnalyzer_IsPreciseInferredType(const SZrInferredType *typeInfo) {
    return typeInfo != ZR_NULL && !ZrLanguageServer_SemanticAnalyzer_IsWeakObjectType(typeInfo);
}

void ZrLanguageServer_SemanticAnalyzer_PerformTypeChecking(SZrState *state,
                                                           SZrSemanticAnalyzer *analyzer,
                                                           SZrAstNode *node);

SZrString *ZrLanguageServer_SemanticAnalyzer_ExtractIdentifierName(SZrState *state, SZrAstNode *node);

TZrBool ZrLanguageServer_SemanticAnalyzer_IsImplicitRuntimeIdentifier(SZrString *name);

SZrString *ZrLanguageServer_SemanticAnalyzer_GetClassPropertySymbolName(SZrAstNode *node);

void ZrLanguageServer_SemanticAnalyzer_AddDefinitionReferenceForSymbol(SZrState *state,
                                                                       SZrSemanticAnalyzer *analyzer,
                                                                       SZrSymbol *symbol);

TZrSize ZrLanguageServer_SemanticAnalyzer_ComputeAstHash(SZrAstNode *ast);

TZrBool ZrLanguageServer_SemanticAnalyzer_PrepareState(SZrState *state,
                                                       SZrSemanticAnalyzer *analyzer,
                                                       SZrAstNode *ast);

TZrBool ZrLanguageServer_SemanticAnalyzer_RegisterSymbolSemantics(SZrSemanticAnalyzer *analyzer,
                                                                  SZrSymbol *symbol,
                                                                  EZrSemanticSymbolKind semanticKind,
                                                                  const SZrInferredType *typeInfo,
                                                                  EZrSemanticTypeKind typeKind);

void ZrLanguageServer_SemanticAnalyzer_RecordTemplateStringSegments(SZrSemanticAnalyzer *analyzer,
                                                                    SZrAstNode *node);

void ZrLanguageServer_SemanticAnalyzer_RecordUsingCleanupStep(SZrSemanticAnalyzer *analyzer,
                                                              SZrAstNode *resource);

void ZrLanguageServer_SemanticAnalyzer_ConsumeCompilerErrorDiagnostic(SZrState *state,
                                                                      SZrSemanticAnalyzer *analyzer,
                                                                      SZrFileRange fallbackLocation);

TZrBool ZrLanguageServer_SemanticAnalyzer_InferExactExpressionType(SZrState *state,
                                                                   SZrSemanticAnalyzer *analyzer,
                                                                   SZrAstNode *node,
                                                                   SZrInferredType *outType);

void ZrLanguageServer_SemanticAnalyzer_RegisterFieldSymbolFromAst(SZrState *state,
                                                                  SZrSemanticAnalyzer *analyzer,
                                                                  SZrAstNode *fieldNode,
                                                                  TZrLifetimeRegionId ownerRegionId,
                                                                  EZrDeterministicCleanupKind cleanupKind,
                                                                  TZrInt32 declarationOrder);

void ZrLanguageServer_SemanticAnalyzer_CollectSymbolsFromAst(SZrState *state,
                                                             SZrSemanticAnalyzer *analyzer,
                                                             SZrAstNode *node);

void ZrLanguageServer_SemanticAnalyzer_CollectReferencesFromAst(SZrState *state,
                                                                SZrSemanticAnalyzer *analyzer,
                                                                SZrAstNode *node);

#endif
