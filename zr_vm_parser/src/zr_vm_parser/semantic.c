#include "zr_vm_parser/semantic.h"

#include "zr_vm_core/memory.h"

static void semantic_context_init_arrays(SZrSemanticContext *context) {
    ZrArrayInit(context->state,
                &context->types,
                sizeof(SZrSemanticTypeRecord),
                8);
    ZrArrayInit(context->state,
                &context->symbols,
                sizeof(SZrSemanticSymbolRecord),
                8);
    ZrArrayInit(context->state,
                &context->overloadSets,
                sizeof(SZrSemanticOverloadSetRecord),
                4);
    ZrArrayInit(context->state,
                &context->cleanupPlan,
                sizeof(SZrDeterministicCleanupStep),
                8);
    ZrArrayInit(context->state,
                &context->templateSegments,
                sizeof(SZrTemplateSegment),
                8);
}

SZrSemanticContext *ZrSemanticContextNew(SZrState *state) {
    SZrSemanticContext *context;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    context = (SZrSemanticContext *)ZrMemoryRawMalloc(state->global, sizeof(SZrSemanticContext));
    if (context == ZR_NULL) {
        return ZR_NULL;
    }

    context->state = state;
    semantic_context_init_arrays(context);
    ZrSemanticContextReset(context);
    return context;
}

void ZrSemanticContextFree(SZrSemanticContext *context) {
    TZrSize i;

    if (context == ZR_NULL || context->state == ZR_NULL) {
        return;
    }

    if (context->types.isValid && context->types.head != ZR_NULL) {
        for (i = 0; i < context->types.length; i++) {
            SZrSemanticTypeRecord *record =
                (SZrSemanticTypeRecord *)ZrArrayGet(&context->types, i);
            if (record != ZR_NULL) {
                ZrInferredTypeFree(context->state, &record->inferredType);
            }
        }
        ZrArrayFree(context->state, &context->types);
    }
    if (context->symbols.isValid && context->symbols.head != ZR_NULL) {
        ZrArrayFree(context->state, &context->symbols);
    }
    if (context->overloadSets.isValid && context->overloadSets.head != ZR_NULL) {
        for (i = 0; i < context->overloadSets.length; i++) {
            SZrSemanticOverloadSetRecord *record =
                (SZrSemanticOverloadSetRecord *)ZrArrayGet(&context->overloadSets, i);
            if (record != ZR_NULL && record->members.isValid && record->members.head != ZR_NULL) {
                ZrArrayFree(context->state, &record->members);
            }
        }
        ZrArrayFree(context->state, &context->overloadSets);
    }
    ZrArrayFree(context->state, &context->cleanupPlan);
    ZrArrayFree(context->state, &context->templateSegments);
    ZrMemoryRawFree(context->state->global, context, sizeof(SZrSemanticContext));
}

void ZrSemanticContextReset(SZrSemanticContext *context) {
    TZrSize i;

    if (context == ZR_NULL) {
        return;
    }

    context->nextTypeId = 1;
    context->nextSymbolId = 1;
    context->nextOverloadSetId = 1;
    context->nextLifetimeRegionId = 1;
    for (i = 0; i < context->types.length; i++) {
        SZrSemanticTypeRecord *record =
            (SZrSemanticTypeRecord *)ZrArrayGet(&context->types, i);
        if (record != ZR_NULL) {
            ZrInferredTypeFree(context->state, &record->inferredType);
        }
    }
    for (i = 0; i < context->overloadSets.length; i++) {
        SZrSemanticOverloadSetRecord *record =
            (SZrSemanticOverloadSetRecord *)ZrArrayGet(&context->overloadSets, i);
        if (record != ZR_NULL && record->members.isValid && record->members.head != ZR_NULL) {
            ZrArrayFree(context->state, &record->members);
        }
    }
    context->types.length = 0;
    context->symbols.length = 0;
    context->overloadSets.length = 0;
    context->cleanupPlan.length = 0;
    context->templateSegments.length = 0;
}

TZrTypeId ZrSemanticReserveTypeId(SZrSemanticContext *context) {
    if (context == ZR_NULL) {
        return 0;
    }
    return context->nextTypeId++;
}

TZrSymbolId ZrSemanticReserveSymbolId(SZrSemanticContext *context) {
    if (context == ZR_NULL) {
        return 0;
    }
    return context->nextSymbolId++;
}

TZrOverloadSetId ZrSemanticReserveOverloadSetId(SZrSemanticContext *context) {
    if (context == ZR_NULL) {
        return 0;
    }
    return context->nextOverloadSetId++;
}

TZrLifetimeRegionId ZrSemanticReserveLifetimeRegionId(SZrSemanticContext *context) {
    if (context == ZR_NULL) {
        return 0;
    }
    return context->nextLifetimeRegionId++;
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

static TBool semantic_names_equal(SZrString *left, SZrString *right) {
    if (left == right) {
        return ZR_TRUE;
    }
    if (left == ZR_NULL || right == ZR_NULL) {
        return ZR_FALSE;
    }
    return ZrStringEqual(left, right);
}

TZrTypeId ZrSemanticRegisterInferredType(SZrSemanticContext *context,
                                         const SZrInferredType *type,
                                         EZrSemanticTypeKind kind,
                                         SZrString *name,
                                         SZrAstNode *astNode) {
    TZrSize i;
    SZrSemanticTypeRecord record;

    if (context == ZR_NULL || type == ZR_NULL) {
        return 0;
    }

    for (i = 0; i < context->types.length; i++) {
        SZrSemanticTypeRecord *existing =
            (SZrSemanticTypeRecord *)ZrArrayGet(&context->types, i);
        if (existing != ZR_NULL &&
            existing->kind == semantic_type_kind_from_inferred_type(type, kind) &&
            semantic_names_equal(existing->name, name != ZR_NULL ? name : type->typeName) &&
            ZrInferredTypeEqual(&existing->inferredType, type)) {
            return existing->id;
        }
    }

    record.id = ZrSemanticReserveTypeId(context);
    record.kind = semantic_type_kind_from_inferred_type(type, kind);
    record.baseType = type->baseType;
    record.ownershipQualifier = type->ownershipQualifier;
    record.name = name != ZR_NULL ? name : type->typeName;
    record.astNode = astNode;
    ZrInferredTypeCopy(context->state, &record.inferredType, type);

    ZrArrayPush(context->state, &context->types, &record);
    return record.id;
}

TZrTypeId ZrSemanticRegisterNamedType(SZrSemanticContext *context,
                                      SZrString *name,
                                      EZrSemanticTypeKind kind,
                                      SZrAstNode *astNode) {
    TZrSize i;
    SZrSemanticTypeRecord record;

    if (context == ZR_NULL || name == ZR_NULL) {
        return 0;
    }

    for (i = 0; i < context->types.length; i++) {
        SZrSemanticTypeRecord *existing =
            (SZrSemanticTypeRecord *)ZrArrayGet(&context->types, i);
        if (existing != ZR_NULL && semantic_names_equal(existing->name, name)) {
            return existing->id;
        }
    }

    record.id = ZrSemanticReserveTypeId(context);
    record.kind = kind;
    record.baseType = ZR_VALUE_TYPE_OBJECT;
    record.ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    record.name = name;
    record.astNode = astNode;
    ZrInferredTypeInitFull(context->state, &record.inferredType, ZR_VALUE_TYPE_OBJECT, ZR_FALSE, name);

    ZrArrayPush(context->state, &context->types, &record);
    return record.id;
}

TZrSymbolId ZrSemanticRegisterSymbol(SZrSemanticContext *context,
                                     SZrString *name,
                                     EZrSemanticSymbolKind kind,
                                     TZrTypeId typeId,
                                     TZrOverloadSetId overloadSetId,
                                     SZrAstNode *astNode,
                                     SZrFileRange location) {
    SZrSemanticSymbolRecord record;

    if (context == ZR_NULL || name == ZR_NULL) {
        return 0;
    }

    record.id = ZrSemanticReserveSymbolId(context);
    record.kind = kind;
    record.name = name;
    record.typeId = typeId;
    record.overloadSetId = overloadSetId;
    record.astNode = astNode;
    record.location = location;

    ZrArrayPush(context->state, &context->symbols, &record);
    return record.id;
}

TZrOverloadSetId ZrSemanticGetOrCreateOverloadSet(SZrSemanticContext *context,
                                                  SZrString *name) {
    TZrSize i;
    SZrSemanticOverloadSetRecord record;

    if (context == ZR_NULL || name == ZR_NULL) {
        return 0;
    }

    for (i = 0; i < context->overloadSets.length; i++) {
        SZrSemanticOverloadSetRecord *existing =
            (SZrSemanticOverloadSetRecord *)ZrArrayGet(&context->overloadSets, i);
        if (existing != ZR_NULL && semantic_names_equal(existing->name, name)) {
            return existing->id;
        }
    }

    record.id = ZrSemanticReserveOverloadSetId(context);
    record.name = name;
    ZrArrayInit(context->state, &record.members, sizeof(TZrSymbolId), 4);

    ZrArrayPush(context->state, &context->overloadSets, &record);
    return record.id;
}

TBool ZrSemanticAddOverloadMember(SZrSemanticContext *context,
                                  TZrOverloadSetId overloadSetId,
                                  TZrSymbolId symbolId) {
    TZrSize i;

    if (context == ZR_NULL || overloadSetId == 0 || symbolId == 0) {
        return ZR_FALSE;
    }

    for (i = 0; i < context->overloadSets.length; i++) {
        SZrSemanticOverloadSetRecord *record =
            (SZrSemanticOverloadSetRecord *)ZrArrayGet(&context->overloadSets, i);
        if (record != ZR_NULL && record->id == overloadSetId) {
            TZrSize memberIndex;
            for (memberIndex = 0; memberIndex < record->members.length; memberIndex++) {
                TZrSymbolId *existingSymbolId =
                    (TZrSymbolId *)ZrArrayGet(&record->members, memberIndex);
                if (existingSymbolId != ZR_NULL && *existingSymbolId == symbolId) {
                    return ZR_TRUE;
                }
            }
            ZrArrayPush(context->state, &record->members, &symbolId);
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

TBool ZrSemanticAppendCleanupStep(SZrSemanticContext *context,
                                  const SZrDeterministicCleanupStep *step) {
    SZrDeterministicCleanupStep copy;

    if (context == ZR_NULL || step == ZR_NULL) {
        return ZR_FALSE;
    }

    copy = *step;
    ZrArrayPush(context->state, &context->cleanupPlan, &copy);
    return ZR_TRUE;
}

TBool ZrSemanticAppendTemplateSegment(SZrSemanticContext *context,
                                      const SZrTemplateSegment *segment) {
    SZrTemplateSegment copy;

    if (context == ZR_NULL || segment == ZR_NULL) {
        return ZR_FALSE;
    }

    copy = *segment;
    ZrArrayPush(context->state, &context->templateSegments, &copy);
    return ZR_TRUE;
}

SZrHirModule *ZrHirModuleNew(SZrState *state,
                             SZrSemanticContext *context,
                             SZrAstNode *rootAst) {
    SZrHirModule *module;

    if (state == ZR_NULL || context == ZR_NULL) {
        return ZR_NULL;
    }

    module = (SZrHirModule *)ZrMemoryRawMalloc(state->global, sizeof(SZrHirModule));
    if (module == ZR_NULL) {
        return ZR_NULL;
    }

    module->rootAst = rootAst;
    module->semantic = context;
    return module;
}

void ZrHirModuleFree(SZrState *state, SZrHirModule *module) {
    if (state == ZR_NULL || module == ZR_NULL) {
        return;
    }

    ZrMemoryRawFree(state->global, module, sizeof(SZrHirModule));
}
