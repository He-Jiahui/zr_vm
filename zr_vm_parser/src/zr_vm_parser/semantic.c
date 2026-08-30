#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/diagnostic_builder.h"
#include "zr_vm_parser/semantic_calls.h"
#include "zr_vm_parser/semantic_display.h"

#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"

static SZrString *semantic_context_clone_string(
        SZrSemanticContext *context,
        SZrString *value) {
    TZrNativeString text;

    if (context == ZR_NULL || context->state == ZR_NULL || value == ZR_NULL) {
        return ZR_NULL;
    }
    text = ZrCore_String_GetNativeString(value);
    if (text == ZR_NULL) {
        return ZR_NULL;
    }
    return ZrCore_String_Create(
            context->state, text, ZrCore_String_GetByteLength(value));
}

static void semantic_context_init_arrays(SZrSemanticContext *context) {
    ZrCore_Array_Init(context->state,
                &context->canonicalTypes,
                sizeof(SZrCanonicalTypeNode),
                ZR_PARSER_INITIAL_CAPACITY_SMALL);
    ZrParser_CanonicalTypeIndex_Init(context);
    ZrParser_CanonicalTypeDefinition_Init(context);
    ZrCore_Array_Init(context->state,
                &context->types,
                sizeof(SZrSemanticTypeRecord),
                ZR_PARSER_INITIAL_CAPACITY_SMALL);
    ZrCore_Array_Init(context->state,
                &context->symbols,
                sizeof(SZrSemanticSymbolRecord),
                ZR_PARSER_INITIAL_CAPACITY_SMALL);
    ZrCore_Array_Init(context->state,
                &context->scopeFacts,
                sizeof(SZrSemanticScopeFact),
                ZR_PARSER_INITIAL_CAPACITY_SMALL);
    ZrCore_Array_Init(context->state,
                &context->visibleSymbolFacts,
                sizeof(SZrSemanticVisibleSymbolFact),
                ZR_PARSER_INITIAL_CAPACITY_SMALL);
    ZrCore_Array_Init(context->state,
                &context->overloadSets,
                sizeof(SZrSemanticOverloadSetRecord),
                ZR_PARSER_INITIAL_CAPACITY_TINY);
    ZrCore_Array_Init(context->state,
                &context->cleanupPlan,
                sizeof(SZrDeterministicCleanupStep),
                ZR_PARSER_INITIAL_CAPACITY_SMALL);
    ZrCore_Array_Init(context->state,
                &context->templateSegments,
                sizeof(SZrTemplateSegment),
                ZR_PARSER_INITIAL_CAPACITY_SMALL);
    ZrCore_Array_Init(context->state,
                &context->queryDiagnostics,
                sizeof(SZrStructuredDiagnostic),
                ZR_PARSER_INITIAL_CAPACITY_SMALL);
    ZrCore_Array_Init(context->state,
                &context->propertyContracts,
                sizeof(SZrSemanticPropertyContract),
                ZR_PARSER_INITIAL_CAPACITY_TINY);
    ZrCore_Array_Init(context->state,
                &context->typeDisplayAliasFacts,
                sizeof(SZrSemanticTypeDisplayAliasFact),
                ZR_PARSER_INITIAL_CAPACITY_TINY);
    ZrCore_Array_Init(context->state,
                &context->documentationFacts,
                sizeof(SZrSemanticDocumentationFact),
                ZR_PARSER_INITIAL_CAPACITY_TINY);
    ZrParser_SemanticFacts_Init(context);
    ZrParser_SemanticCalls_Init(context);
}

static void semantic_context_reset_query_diagnostics(SZrSemanticContext *context) {
    TZrSize i;

    if (context == ZR_NULL || !context->queryDiagnostics.isValid) {
        return;
    }

    for (i = 0; i < context->queryDiagnostics.length; i++) {
        SZrStructuredDiagnostic *diagnostic =
            (SZrStructuredDiagnostic *)ZrCore_Array_Get(&context->queryDiagnostics, i);
        if (diagnostic != ZR_NULL) {
            ZrParser_StructuredDiagnostic_Free(context->state, diagnostic);
        }
    }

    context->queryDiagnostics.length = 0;
    context->queryDiagnosticsMaterialized = ZR_FALSE;
    context->queryDiagnosticsScopeRoot = ZR_NULL;
}

SZrSemanticContext *ZrParser_SemanticContext_New(SZrState *state) {
    SZrSemanticContext *context;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    context = (SZrSemanticContext *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrSemanticContext));
    if (context == ZR_NULL) {
        return ZR_NULL;
    }

    context->state = state;
    semantic_context_init_arrays(context);
    ZrParser_SemanticContext_Reset(context);
    return context;
}

void ZrParser_SemanticContext_Free(SZrSemanticContext *context) {
    TZrSize i;

    if (context == ZR_NULL || context->state == ZR_NULL) {
        return;
    }

    if (context->types.isValid && context->types.head != ZR_NULL) {
        for (i = 0; i < context->types.length; i++) {
            SZrSemanticTypeRecord *record =
                (SZrSemanticTypeRecord *)ZrCore_Array_Get(&context->types, i);
            if (record != ZR_NULL) {
                ZrParser_InferredType_Free(context->state, &record->inferredType);
            }
        }
        ZrCore_Array_Free(context->state, &context->types);
    }
    ZrParser_CanonicalTypeDefinition_Free(context);
    ZrParser_CanonicalType_Free(context);
    if (context->symbols.isValid && context->symbols.head != ZR_NULL) {
        ZrCore_Array_Free(context->state, &context->symbols);
    }
    ZrCore_Array_Free(context->state, &context->scopeFacts);
    ZrCore_Array_Free(context->state, &context->visibleSymbolFacts);
    if (context->overloadSets.isValid && context->overloadSets.head != ZR_NULL) {
        for (i = 0; i < context->overloadSets.length; i++) {
            SZrSemanticOverloadSetRecord *record =
                (SZrSemanticOverloadSetRecord *)ZrCore_Array_Get(&context->overloadSets, i);
            if (record != ZR_NULL && record->members.isValid && record->members.head != ZR_NULL) {
                ZrCore_Array_Free(context->state, &record->members);
            }
        }
        ZrCore_Array_Free(context->state, &context->overloadSets);
    }
    ZrCore_Array_Free(context->state, &context->cleanupPlan);
    ZrCore_Array_Free(context->state, &context->templateSegments);
    semantic_context_reset_query_diagnostics(context);
    ZrCore_Array_Free(context->state, &context->queryDiagnostics);
    ZrParser_SemanticCalls_Free(context);
    ZrParser_SemanticFacts_Free(context);
    ZrCore_Array_Free(context->state, &context->typeDisplayAliasFacts);
    ZrCore_Array_Free(context->state, &context->documentationFacts);
    ZrCore_Array_Free(context->state, &context->propertyContracts);
    ZrCore_Memory_RawFree(context->state->global, context, sizeof(SZrSemanticContext));
}

void ZrParser_SemanticContext_Reset(SZrSemanticContext *context) {
    TZrSize i;

    if (context == ZR_NULL) {
        return;
    }

    context->nextTypeId = ZR_SEMANTIC_ID_FIRST;
    context->nextSymbolId = ZR_SEMANTIC_ID_FIRST;
    context->nextOverloadSetId = ZR_SEMANTIC_ID_FIRST;
    context->nextLifetimeRegionId = ZR_SEMANTIC_ID_FIRST;
    context->nextScopeId = ZR_SEMANTIC_ID_FIRST;
    ZrParser_CanonicalTypeDefinition_Reset(context);
    ZrParser_CanonicalType_Reset(context);
    for (i = 0; i < context->types.length; i++) {
        SZrSemanticTypeRecord *record =
            (SZrSemanticTypeRecord *)ZrCore_Array_Get(&context->types, i);
        if (record != ZR_NULL) {
            ZrParser_InferredType_Free(context->state, &record->inferredType);
        }
    }
    for (i = 0; i < context->overloadSets.length; i++) {
        SZrSemanticOverloadSetRecord *record =
            (SZrSemanticOverloadSetRecord *)ZrCore_Array_Get(&context->overloadSets, i);
        if (record != ZR_NULL && record->members.isValid && record->members.head != ZR_NULL) {
            ZrCore_Array_Free(context->state, &record->members);
        }
    }
    context->types.length = 0;
    context->symbols.length = 0;
    context->scopeFacts.length = 0;
    context->visibleSymbolFacts.length = 0;
    context->overloadSets.length = 0;
    context->cleanupPlan.length = 0;
    context->templateSegments.length = 0;
    context->propertyContracts.length = 0;
    context->typeDisplayAliasFacts.length = 0;
    context->documentationFacts.length = 0;
    semantic_context_reset_query_diagnostics(context);
    ZrParser_SemanticCalls_Reset(context);
    ZrParser_SemanticFacts_Reset(context);
}

TZrTypeId ZrParser_Semantic_ReserveTypeId(SZrSemanticContext *context) {
    if (context == ZR_NULL) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    return context->nextTypeId++;
}

TZrSymbolId ZrParser_Semantic_ReserveSymbolId(SZrSemanticContext *context) {
    if (context == ZR_NULL) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    return context->nextSymbolId++;
}

TZrOverloadSetId ZrParser_Semantic_ReserveOverloadSetId(SZrSemanticContext *context) {
    if (context == ZR_NULL) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    return context->nextOverloadSetId++;
}

TZrLifetimeRegionId ZrParser_Semantic_ReserveLifetimeRegionId(SZrSemanticContext *context) {
    if (context == ZR_NULL) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    return context->nextLifetimeRegionId++;
}

TZrSemanticScopeId ZrParser_Semantic_ReserveScopeId(SZrSemanticContext *context) {
    if (context == ZR_NULL) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    return context->nextScopeId++;
}

static EZrSemanticTypeKind semantic_type_kind_from_inferred_type(const SZrInferredType *type,
                                                                 EZrSemanticTypeKind fallback) {
    if (type == ZR_NULL) {
        return fallback;
    }

    if (fallback != ZR_SEMANTIC_TYPE_KIND_UNKNOWN) {
        return fallback;
    }

    switch (type->baseType) {
        case ZR_VALUE_TYPE_STRING:
        case ZR_VALUE_TYPE_OBJECT:
        case ZR_VALUE_TYPE_FUNCTION:
        case ZR_VALUE_TYPE_ARRAY:
            return ZR_SEMANTIC_TYPE_KIND_REFERENCE;
        default:
            return ZR_SEMANTIC_TYPE_KIND_VALUE;
    }
}

static TZrBool semantic_names_equal(SZrString *left, SZrString *right) {
    if (left == right) {
        return ZR_TRUE;
    }
    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_FALSE;
    }
    return ZrCore_String_Equal(left, right);
}

static void semantic_inferred_type_keep_structural_fields_only(
        SZrState *state,
        SZrInferredType *type) {
    TZrSize index;

    if (state == ZR_NULL || type == ZR_NULL) {
        return;
    }
    for (index = 0; index < type->elementTypes.length; index++) {
        SZrInferredType *element = (SZrInferredType *)ZrCore_Array_Get(
                &type->elementTypes,
                index);
        semantic_inferred_type_keep_structural_fields_only(state, element);
    }

    type->minValue = 0;
    type->maxValue = 0;
    type->hasRangeConstraint = ZR_FALSE;
    ZrParser_NumericRangeSegments_Free(
            state,
            &type->rangeSegmentCount,
            type->rangeSegments,
            &type->rangeExtraSegments);
    type->knownBoolValue = ZR_FALSE;
    type->hasKnownBoolValue = ZR_FALSE;
    type->arrayFixedSize = 0U;
    type->arrayMinSize = 0U;
    type->arrayMaxSize = 0U;
    type->hasArraySizeConstraint = ZR_FALSE;
}

TZrTypeId ZrParser_Semantic_RegisterInferredType(SZrSemanticContext *context,
                                         const SZrInferredType *type,
                                         EZrSemanticTypeKind kind,
                                         SZrString *name,
                                         SZrAstNode *astNode) {
    TZrSize i;
    SZrSemanticTypeRecord record;
    TZrTypeId canonicalTypeId;

    if (context == ZR_NULL || type == ZR_NULL) {
        return ZR_SEMANTIC_ID_INVALID;
    }

    canonicalTypeId = ZrParser_CanonicalType_FromInferred(context, type);
    if (canonicalTypeId == ZR_SEMANTIC_ID_INVALID) {
        return canonicalTypeId;
    }

    for (i = 0; i < context->types.length; i++) {
        SZrSemanticTypeRecord *existing =
            (SZrSemanticTypeRecord *)ZrCore_Array_Get(&context->types, i);
        if (existing != ZR_NULL && existing->id == canonicalTypeId) {
            return existing->id;
        }
    }

    record.id = canonicalTypeId;
    record.kind = semantic_type_kind_from_inferred_type(type, kind);
    record.baseType = type->baseType;
    record.ownershipQualifier = type->ownershipQualifier;
    (void)name;
    (void)astNode;
    record.name = type->typeName;
    record.astNode = ZR_NULL;
    ZrParser_InferredType_Copy(context->state, &record.inferredType, type);
    semantic_inferred_type_keep_structural_fields_only(context->state, &record.inferredType);

    ZrCore_Array_Push(context->state, &context->types, &record);
    return record.id;
}

TZrTypeId ZrParser_Semantic_RegisterNamedType(SZrSemanticContext *context,
                                      SZrString *name,
                                      EZrSemanticTypeKind kind,
                                      SZrAstNode *astNode) {
    TZrSize i;
    SZrSemanticTypeRecord record;
    TZrTypeId canonicalTypeId;

    if (context == ZR_NULL || name == ZR_NULL) {
        return ZR_SEMANTIC_ID_INVALID;
    }

    canonicalTypeId = ZrParser_CanonicalType_FromName(context, name);
    if (canonicalTypeId == ZR_SEMANTIC_ID_INVALID) {
        return canonicalTypeId;
    }

    for (i = 0; i < context->types.length; i++) {
        SZrSemanticTypeRecord *existing =
            (SZrSemanticTypeRecord *)ZrCore_Array_Get(&context->types, i);
        if (existing != ZR_NULL && existing->id == canonicalTypeId) {
            return existing->id;
        }
    }

    record.id = canonicalTypeId;
    record.kind = kind;
    record.baseType = ZR_VALUE_TYPE_OBJECT;
    record.ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    record.name = name;
    record.astNode = astNode;
    ZrParser_InferredType_InitFull(context->state, &record.inferredType, ZR_VALUE_TYPE_OBJECT, ZR_FALSE, name);

    ZrCore_Array_Push(context->state, &context->types, &record);
    return record.id;
}

TZrBool ZrParser_Semantic_RegisterCanonicalType(
        SZrSemanticContext *context,
        TZrTypeId typeId,
        EZrSemanticTypeKind kind,
        SZrString *name,
        SZrAstNode *astNode) {
    SZrSemanticTypeRecord record;
    TZrSize index;

    if (context == ZR_NULL ||
        typeId == ZR_SEMANTIC_ID_INVALID ||
        ZrParser_CanonicalType_Find(context, typeId) == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = 0; index < context->types.length; index++) {
        const SZrSemanticTypeRecord *existing =
                (const SZrSemanticTypeRecord *)ZrCore_Array_Get(&context->types, index);
        if (existing != ZR_NULL && existing->id == typeId) {
            return existing->kind == kind;
        }
    }

    record.id = typeId;
    record.kind = kind;
    record.baseType = ZR_VALUE_TYPE_OBJECT;
    record.ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    record.name = name;
    record.astNode = astNode;
    ZrParser_InferredType_InitFull(
            context->state,
            &record.inferredType,
            ZR_VALUE_TYPE_OBJECT,
            ZR_FALSE,
            name);
    ZrCore_Array_Push(context->state, &context->types, &record);
    return ZR_TRUE;
}

TZrSymbolId ZrParser_Semantic_RegisterSymbol(SZrSemanticContext *context,
                                     SZrString *name,
                                     EZrSemanticSymbolKind kind,
                                     TZrTypeId typeId,
                                     TZrOverloadSetId overloadSetId,
                                     SZrAstNode *astNode,
                                     SZrFileRange location) {
    TZrSymbolId symbolId;

    if (context == ZR_NULL || name == ZR_NULL) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    symbolId = ZrParser_Semantic_ReserveSymbolId(context);
    return ZrParser_Semantic_RegisterSymbolWithId(
            context,
            symbolId,
            name,
            kind,
            typeId,
            overloadSetId,
            astNode,
            location);
}

TZrSymbolId ZrParser_Semantic_RegisterSymbolWithId(
        SZrSemanticContext *context,
        TZrSymbolId symbolId,
        SZrString *name,
        EZrSemanticSymbolKind kind,
        TZrTypeId typeId,
        TZrOverloadSetId overloadSetId,
        SZrAstNode *astNode,
        SZrFileRange location) {
    SZrSemanticSymbolRecord record;
    TZrSize index;

    if (context == ZR_NULL ||
        symbolId == ZR_SEMANTIC_ID_INVALID ||
        name == ZR_NULL) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    for (index = 0; index < context->symbols.length; index++) {
        const SZrSemanticSymbolRecord *existing =
                (const SZrSemanticSymbolRecord *)ZrCore_Array_Get(&context->symbols, index);
        if (existing != ZR_NULL && existing->id == symbolId) {
            return ZR_SEMANTIC_ID_INVALID;
        }
    }

    record.id = symbolId;
    record.kind = kind;
    record.name = name;
    record.typeId = typeId;
    record.overloadSetId = overloadSetId;
    record.astNode = astNode;
    record.location = location;

    ZrCore_Array_Push(context->state, &context->symbols, &record);
    return record.id;
}

const SZrSemanticSymbolRecord *ZrParser_Semantic_FindSymbolById(
        const SZrSemanticContext *context,
        TZrSymbolId symbolId) {
    TZrSize index;

    if (context == ZR_NULL || symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_NULL;
    }
    for (index = 0U; index < context->symbols.length; index++) {
        const SZrSemanticSymbolRecord *symbol =
                (const SZrSemanticSymbolRecord *)ZrCore_Array_Get(
                        (SZrArray *)&context->symbols, index);
        if (symbol != ZR_NULL && symbol->id == symbolId) {
            return symbol;
        }
    }
    return ZR_NULL;
}

const SZrSemanticScopeFact *ZrParser_Semantic_FindScopeFactById(
        const SZrSemanticContext *context,
        TZrSemanticScopeId scopeId) {
    TZrSize index;

    if (context == ZR_NULL || scopeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_NULL;
    }
    for (index = 0U; index < context->scopeFacts.length; index++) {
        const SZrSemanticScopeFact *scope =
                (const SZrSemanticScopeFact *)ZrCore_Array_Get(
                        (SZrArray *)&context->scopeFacts, index);
        if (scope != ZR_NULL && scope->id == scopeId) {
            return scope;
        }
    }
    return ZR_NULL;
}

TZrSemanticScopeId ZrParser_Semantic_PublishScopeFact(
        SZrSemanticContext *context,
        const SZrSemanticScopeFact *fact) {
    SZrSemanticScopeFact copy;

    if (context == ZR_NULL || fact == ZR_NULL || !context->scopeFacts.isValid ||
        (fact->parentScopeId != ZR_SEMANTIC_ID_INVALID &&
         ZrParser_Semantic_FindScopeFactById(context, fact->parentScopeId) == ZR_NULL)) {
        return ZR_SEMANTIC_ID_INVALID;
    }

    copy = *fact;
    copy.id = ZrParser_Semantic_ReserveScopeId(context);
    if (copy.id == ZR_SEMANTIC_ID_INVALID) {
        return ZR_SEMANTIC_ID_INVALID;
    }
    ZrCore_Array_Push(context->state, &context->scopeFacts, &copy);
    return copy.id;
}

TZrBool ZrParser_Semantic_PublishVisibleSymbolFact(
        SZrSemanticContext *context,
        const SZrSemanticVisibleSymbolFact *fact) {
    SZrSemanticVisibleSymbolFact copy;

    if (context == ZR_NULL || fact == ZR_NULL || !context->visibleSymbolFacts.isValid ||
        fact->symbolId == ZR_SEMANTIC_ID_INVALID || fact->declarationOrder == 0U ||
        ZrParser_Semantic_FindScopeFactById(context, fact->scopeId) == ZR_NULL ||
        ZrParser_Semantic_FindSymbolById(context, fact->symbolId) == ZR_NULL) {
        return ZR_FALSE;
    }

    copy = *fact;
    copy.externalOriginUri = semantic_context_clone_string(
            context, fact->externalOriginUri);
    if (fact->externalOriginUri != ZR_NULL && copy.externalOriginUri == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Array_Push(context->state, &context->visibleSymbolFacts, &copy);
    return ZR_TRUE;
}

const SZrSemanticSymbolRecord *ZrParser_Semantic_FindSymbolByNameAndKind(
        const SZrSemanticContext *context,
        SZrString *name,
        EZrSemanticSymbolKind kind) {
    TZrSize index;

    if (context == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }
    for (index = 0; index < context->symbols.length; index++) {
        const SZrSemanticSymbolRecord *symbol =
                (const SZrSemanticSymbolRecord *)ZrCore_Array_Get(
                        (SZrArray *)&context->symbols,
                        index);
        if (symbol != ZR_NULL &&
            symbol->kind == kind &&
            symbol->name != ZR_NULL &&
            ZrCore_String_Equal(symbol->name, name)) {
            return symbol;
        }
    }
    return ZR_NULL;
}

TZrBool ZrParser_Semantic_RebindSymbolType(
        SZrSemanticContext *context,
        TZrSymbolId symbolId,
        TZrTypeId typeId) {
    TZrSize index;

    if (context == ZR_NULL ||
        symbolId == ZR_SEMANTIC_ID_INVALID ||
        ZrParser_CanonicalType_Find(context, typeId) == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = 0; index < context->symbols.length; index++) {
        SZrSemanticSymbolRecord *symbol = (SZrSemanticSymbolRecord *)ZrCore_Array_Get(
                &context->symbols,
                index);
        if (symbol != ZR_NULL && symbol->id == symbolId) {
            symbol->typeId = typeId;
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

TZrBool ZrParser_Semantic_PublishCanonicalTypeSymbol(
        SZrSemanticContext *context,
        TZrTypeId typeId,
        EZrSemanticTypeKind typeKind,
        SZrString *name,
        SZrAstNode *astNode,
        TZrSymbolId symbolId,
        SZrFileRange location) {
    SZrSemanticTypeRecord typeRecord;
    SZrSemanticSymbolRecord symbolRecord;
    TZrSize index;

    if (context == ZR_NULL ||
        typeId == ZR_SEMANTIC_ID_INVALID ||
        symbolId == ZR_SEMANTIC_ID_INVALID ||
        name == ZR_NULL ||
        ZrParser_CanonicalType_Find(context, typeId) == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = 0; index < context->types.length; index++) {
        const SZrSemanticTypeRecord *existing =
                (const SZrSemanticTypeRecord *)ZrCore_Array_Get(&context->types, index);
        if (existing != ZR_NULL && existing->id == typeId) {
            return ZR_FALSE;
        }
    }
    for (index = 0; index < context->symbols.length; index++) {
        const SZrSemanticSymbolRecord *existing =
                (const SZrSemanticSymbolRecord *)ZrCore_Array_Get(&context->symbols, index);
        if (existing != ZR_NULL &&
            (existing->id == symbolId ||
             (existing->kind == ZR_SEMANTIC_SYMBOL_KIND_TYPE &&
              existing->name != ZR_NULL &&
              ZrCore_String_Equal(existing->name, name)))) {
            return ZR_FALSE;
        }
    }

    typeRecord.id = typeId;
    typeRecord.kind = typeKind;
    typeRecord.baseType = ZR_VALUE_TYPE_OBJECT;
    typeRecord.ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    typeRecord.name = name;
    typeRecord.astNode = astNode;
    ZrParser_InferredType_InitFull(
            context->state,
            &typeRecord.inferredType,
            ZR_VALUE_TYPE_OBJECT,
            ZR_FALSE,
            name);

    symbolRecord.id = symbolId;
    symbolRecord.kind = ZR_SEMANTIC_SYMBOL_KIND_TYPE;
    symbolRecord.name = name;
    symbolRecord.typeId = typeId;
    symbolRecord.overloadSetId = ZR_SEMANTIC_ID_INVALID;
    symbolRecord.astNode = astNode;
    symbolRecord.location = location;

    ZrCore_Array_Push(context->state, &context->types, &typeRecord);
    ZrCore_Array_Push(context->state, &context->symbols, &symbolRecord);
    return ZR_TRUE;
}

TZrOverloadSetId ZrParser_Semantic_GetOrCreateOverloadSet(SZrSemanticContext *context,
                                                  SZrString *name) {
    TZrSize i;
    SZrSemanticOverloadSetRecord record;

    if (context == ZR_NULL || name == ZR_NULL) {
        return ZR_SEMANTIC_ID_INVALID;
    }

    for (i = 0; i < context->overloadSets.length; i++) {
        SZrSemanticOverloadSetRecord *existing =
            (SZrSemanticOverloadSetRecord *)ZrCore_Array_Get(&context->overloadSets, i);
        if (existing != ZR_NULL && semantic_names_equal(existing->name, name)) {
            return existing->id;
        }
    }

    record.id = ZrParser_Semantic_ReserveOverloadSetId(context);
    record.name = name;
    ZrCore_Array_Init(context->state, &record.members, sizeof(TZrSymbolId), ZR_PARSER_INITIAL_CAPACITY_TINY);

    ZrCore_Array_Push(context->state, &context->overloadSets, &record);
    return record.id;
}

TZrBool ZrParser_Semantic_AddOverloadMember(SZrSemanticContext *context,
                                  TZrOverloadSetId overloadSetId,
                                  TZrSymbolId symbolId) {
    TZrSize i;

    if (context == ZR_NULL ||
        overloadSetId == ZR_SEMANTIC_ID_INVALID ||
        symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }

    for (i = 0; i < context->overloadSets.length; i++) {
        SZrSemanticOverloadSetRecord *record =
            (SZrSemanticOverloadSetRecord *)ZrCore_Array_Get(&context->overloadSets, i);
        if (record != ZR_NULL && record->id == overloadSetId) {
            TZrSize memberIndex;
            for (memberIndex = 0; memberIndex < record->members.length; memberIndex++) {
                TZrSymbolId *existingSymbolId =
                    (TZrSymbolId *)ZrCore_Array_Get(&record->members, memberIndex);
                if (existingSymbolId != ZR_NULL && *existingSymbolId == symbolId) {
                    return ZR_TRUE;
                }
            }
            ZrCore_Array_Push(context->state, &record->members, &symbolId);
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

TZrBool ZrParser_Semantic_AppendCleanupStep(SZrSemanticContext *context,
                                  const SZrDeterministicCleanupStep *step) {
    SZrDeterministicCleanupStep copy;

    if (context == ZR_NULL || step == ZR_NULL) {
        return ZR_FALSE;
    }

    copy = *step;
    ZrCore_Array_Push(context->state, &context->cleanupPlan, &copy);
    return ZR_TRUE;
}

TZrBool ZrParser_Semantic_AppendTemplateSegment(SZrSemanticContext *context,
                                      const SZrTemplateSegment *segment) {
    SZrTemplateSegment copy;

    if (context == ZR_NULL || segment == ZR_NULL) {
        return ZR_FALSE;
    }

    copy = *segment;
    ZrCore_Array_Push(context->state, &context->templateSegments, &copy);
    return ZR_TRUE;
}

SZrHirModule *ZrParser_HirModule_New(SZrState *state,
                             SZrSemanticContext *context,
                             SZrAstNode *rootAst) {
    SZrHirModule *module;

    if (state == ZR_NULL || context == ZR_NULL) {
        return ZR_NULL;
    }

    module = (SZrHirModule *)ZrCore_Memory_RawMalloc(state->global, sizeof(SZrHirModule));
    if (module == ZR_NULL) {
        return ZR_NULL;
    }

    module->rootAst = rootAst;
    module->semantic = context;
    return module;
}

void ZrParser_HirModule_Free(SZrState *state, SZrHirModule *module) {
    if (state == ZR_NULL || module == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawFree(state->global, module, sizeof(SZrHirModule));
}
