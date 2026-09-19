#include "compiler_internal.h"
#include "type_inference_internal.h"

static const SZrAstNode *compiler_semantic_cfg_catch_parameter(const SZrAstNode *catchClause) {
    if (catchClause == ZR_NULL || catchClause->type != ZR_AST_CATCH_CLAUSE ||
        catchClause->data.catchClause.pattern == ZR_NULL || catchClause->data.catchClause.pattern->count != 1U) {
        return ZR_NULL;
    }
    return catchClause->data.catchClause.pattern->nodes[0];
}

TZrBool compiler_semantic_cfg_catch_type_is_resolvable(SZrCompilerState *cs, const SZrType *typeInfo) {
    const SZrAstNode *nameNode;
    SZrString *typeName;
    TZrNativeString nativeName;
    TZrSize nativeNameLength;
    EZrValueType primitiveType;

    if (typeInfo == ZR_NULL) {
        return ZR_TRUE;
    }
    if (cs == ZR_NULL || typeInfo->name == ZR_NULL || typeInfo->subType != ZR_NULL || typeInfo->dimensions != 0 ||
        typeInfo->ownershipQualifier != ZR_OWNERSHIP_QUALIFIER_NONE ||
        typeInfo->referenceAccess != ZR_REFERENCE_ACCESS_NONE || typeInfo->isScopedReference ||
        typeInfo->isReadonlyView) {
        return ZR_FALSE;
    }
    nameNode = typeInfo->name;
    if (nameNode->type != ZR_AST_IDENTIFIER_LITERAL || nameNode->data.identifier.name == ZR_NULL) {
        return ZR_FALSE;
    }
    typeName = nameNode->data.identifier.name;
    if (typeName->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        nativeName = ZrCore_String_GetNativeStringShort(typeName);
        nativeNameLength = typeName->shortStringLength;
    } else {
        nativeName = ZrCore_String_GetNativeString(typeName);
        nativeNameLength = typeName->longStringLength;
    }
    if (nativeName != ZR_NULL && inferred_type_try_map_primitive_name(nativeName, nativeNameLength, &primitiveType)) {
        return ZR_TRUE;
    }
    return type_name_is_explicitly_available_in_context_inference(cs, typeName);
}

static TZrBool compiler_semantic_cfg_resolve_catch_match_type(SZrCompilerState *cs, const SZrAstNode *catchClause,
                                                              TZrTypeId *outMatchTypeId) {
    const SZrAstNode *parameter = compiler_semantic_cfg_catch_parameter(catchClause);
    SZrInferredType inferredType;
    TZrTypeId typeId;

    if (cs == ZR_NULL || parameter == ZR_NULL || parameter->type != ZR_AST_PARAMETER || outMatchTypeId == ZR_NULL) {
        return ZR_FALSE;
    }
    *outMatchTypeId = ZR_SEMANTIC_ID_INVALID;
    if (parameter->data.parameter.typeInfo == ZR_NULL) {
        return ZR_TRUE;
    }
    if (!compiler_semantic_cfg_catch_type_is_resolvable(cs, parameter->data.parameter.typeInfo) ||
        !ZrParser_AstTypeToInferredType_Convert(cs, parameter->data.parameter.typeInfo, &inferredType)) {
        return ZR_FALSE;
    }
    typeId = ZrParser_Semantic_RegisterInferredType(cs->semanticContext, &inferredType, ZR_SEMANTIC_TYPE_KIND_UNKNOWN,
                                                    ZR_NULL, ZR_NULL);
    ZrParser_InferredType_Free(cs->state, &inferredType);
    if (typeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    *outMatchTypeId = typeId;
    return ZR_TRUE;
}

static void compiler_semantic_cfg_catch_plan_free(SZrCompilerState *cs, SZrCompilerSemanticCatchPlan *plan) {
    if (cs == ZR_NULL || plan == ZR_NULL) {
        return;
    }
    if (plan->entries.isValid) {
        ZrCore_Array_Free(cs->state, &plan->entries);
    }
    memset(plan, 0, sizeof(*plan));
}

static TZrBool compiler_semantic_cfg_emit_exception_payload(SZrCompilerState *cs, SZrFileRange sourceRange,
                                                            TZrTypeId *outTypeId, TZrValueId *outValueId) {
    SZrInferredType payloadType;
    SZrSemanticIrInstructionSpec spec;
    TZrTypeId typeId;
    TZrValueId valueId;

    ZrParser_InferredType_Init(cs->state, &payloadType, ZR_VALUE_TYPE_OBJECT);
    typeId = ZrParser_Semantic_RegisterInferredType(cs->semanticContext, &payloadType, ZR_SEMANTIC_TYPE_KIND_UNKNOWN,
                                                    ZR_NULL, ZR_NULL);
    ZrParser_InferredType_Free(cs->state, &payloadType);
    if (typeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    valueId = ZrParser_SemanticIr_AddValue(&cs->preSemanticIr, typeId, sourceRange);
    if (valueId == ZR_VALUE_ID_INVALID) {
        return ZR_FALSE;
    }
    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_EXCEPTION_PAYLOAD;
    spec.typeId = typeId;
    spec.resultValueId = valueId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    if (!compiler_semantic_ir_emit(cs, &spec)) {
        return ZR_FALSE;
    }
    *outTypeId = typeId;
    *outValueId = valueId;
    return ZR_TRUE;
}

static TZrBool compiler_semantic_cfg_emit_type_test(SZrCompilerState *cs, TZrValueId payloadValueId,
                                                    TZrTypeId matchTypeId, SZrFileRange sourceRange,
                                                    TZrValueId *outResultValueId) {
    SZrSemanticIrInstructionSpec spec;
    TZrTypeId boolTypeId;
    TZrValueId resultValueId;

    if (cs == ZR_NULL || payloadValueId == ZR_VALUE_ID_INVALID || matchTypeId == ZR_SEMANTIC_ID_INVALID ||
        outResultValueId == ZR_NULL) {
        return ZR_FALSE;
    }
    boolTypeId = ZrParser_CanonicalType_InternPrimitive(cs->semanticContext, ZR_VALUE_TYPE_BOOL);
    if (boolTypeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    resultValueId = ZrParser_SemanticIr_AddValue(&cs->preSemanticIr, boolTypeId, sourceRange);
    if (resultValueId == ZR_VALUE_ID_INVALID) {
        return ZR_FALSE;
    }
    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_TYPE_TEST;
    spec.typeId = boolTypeId;
    spec.matchTypeId = matchTypeId;
    spec.valueId = payloadValueId;
    spec.resultValueId = resultValueId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = sourceRange;
    if (!compiler_semantic_ir_emit(cs, &spec)) {
        return ZR_FALSE;
    }
    *outResultValueId = resultValueId;
    return ZR_TRUE;
}

static TZrBool compiler_semantic_cfg_bind_catch_payload(SZrCompilerState *cs, const SZrAstNode *catchClause,
                                                        TZrUInt32 bindingSlot, TZrTypeId typeId, TZrValueId valueId) {
    const SZrAstNode *parameter;
    SZrString *bindingName;
    SZrCompilerSemanticIrSlot slot;
    SZrParserPlaceBase base;
    SZrSemanticIrInstructionSpec spec;

    parameter = compiler_semantic_cfg_catch_parameter(catchClause);
    if (cs == ZR_NULL || parameter == ZR_NULL || parameter->type != ZR_AST_PARAMETER ||
        parameter->data.parameter.name == ZR_NULL || parameter->data.parameter.name->name == ZR_NULL ||
        bindingSlot == ZR_PARSER_SLOT_NONE || typeId == ZR_SEMANTIC_ID_INVALID || valueId == ZR_VALUE_ID_INVALID) {
        return ZR_FALSE;
    }
    bindingName = parameter->data.parameter.name->name;

    memset(&slot, 0, sizeof(slot));
    memset(&base, 0, sizeof(base));
    base.kind = ZR_PARSER_PLACE_BASE_LOCAL;
    base.identity = bindingSlot;
    slot.stackSlot = bindingSlot;
    slot.typeId = typeId;
    slot.valueId = valueId;
    slot.symbolId =
            ZrParser_Semantic_RegisterSymbol(cs->semanticContext, bindingName, ZR_SEMANTIC_SYMBOL_KIND_VARIABLE, typeId,
                                             ZR_SEMANTIC_ID_INVALID, (SZrAstNode *) parameter, parameter->location);
    if (slot.symbolId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }
    slot.placeId = ZrParser_SemanticIr_AddLocal(&cs->preSemanticIr, slot.symbolId, &base, typeId, parameter->location,
                                                ZR_FALSE);
    if (slot.placeId == ZR_PLACE_ID_INVALID) {
        return ZR_FALSE;
    }
    ZrCore_Array_Push(cs->state, &cs->preSemanticIrSlots, &slot);

    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_PLACE_BASE;
    spec.typeId = typeId;
    spec.placeId = slot.placeId;
    spec.symbolId = slot.symbolId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = parameter->location;
    if (!compiler_semantic_ir_emit(cs, &spec)) {
        return ZR_FALSE;
    }
    memset(&spec, 0, sizeof(spec));
    spec.opcode = ZR_SEMANTIC_IR_INITIALIZE;
    spec.typeId = typeId;
    spec.placeId = slot.placeId;
    spec.valueId = valueId;
    spec.symbolId = slot.symbolId;
    spec.targetBlockId = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    spec.sourceRange = parameter->location;
    return compiler_semantic_ir_emit(cs, &spec);
}

static void compiler_semantic_cfg_catch_plan_fail(SZrCompilerState *cs, SZrCompilerSemanticCatchPlan *plan,
                                                  SZrArray *entrySlots) {
    if (cs->preSemanticIrCfgActive) {
        (void) compiler_semantic_cfg_abandon(cs);
    }
    cs->preSemanticIrCfgCatchBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    cs->preSemanticIrCfgCatchUsed = ZR_FALSE;
    cs->preSemanticIrCfgStartupBlocked = ZR_TRUE;
    if (entrySlots != ZR_NULL && entrySlots->isValid) {
        compiler_semantic_cfg_free_slots(cs, entrySlots);
    }
    compiler_semantic_cfg_catch_plan_free(cs, plan);
}

TZrBool compiler_semantic_cfg_begin_try_catch(SZrCompilerState *cs, SZrAstNode *node,
                                              SZrCompilerSemanticCatchPlan *plan, SZrArray *entrySlots) {
    const SZrTryCatchFinallyStatement *statement;
    const SZrCompilerSemanticCatchPlanEntry *lastEntry;
    SZrParserCfg *cfg;
    TZrSize index;

    if (cs == ZR_NULL || node == ZR_NULL || plan == ZR_NULL || entrySlots == ZR_NULL ||
        !compiler_semantic_cfg_try_catch_is_supported(cs, node) ||
        cs->preSemanticIrCfgCatchBlock != ZR_PARSER_CFG_INVALID_BLOCK_ID) {
        return ZR_FALSE;
    }
    memset(plan, 0, sizeof(*plan));
    memset(entrySlots, 0, sizeof(*entrySlots));
    plan->exceptionBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    plan->unmatchedBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    plan->joinBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
    plan->payloadTypeId = ZR_SEMANTIC_ID_INVALID;
    plan->payloadValueId = ZR_VALUE_ID_INVALID;
    statement = &node->data.tryCatchFinallyStatement;
    ZrCore_Array_Init(cs->state, &plan->entries, sizeof(SZrCompilerSemanticCatchPlanEntry),
                      statement->catchClauses->count);
    plan->initialized = ZR_TRUE;

    for (index = 0U; index < statement->catchClauses->count; index++) {
        const SZrAstNode *catchClause = statement->catchClauses->nodes[index];
        SZrCompilerSemanticCatchPlanEntry entry;

        memset(&entry, 0, sizeof(entry));
        entry.catchClause = catchClause;
        entry.dispatchBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
        entry.handlerBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
        entry.matchTypeId = ZR_SEMANTIC_ID_INVALID;
        if (!compiler_semantic_cfg_resolve_catch_match_type(cs, catchClause, &entry.matchTypeId)) {
            compiler_semantic_cfg_catch_plan_free(cs, plan);
            return ZR_FALSE;
        }
        ZrCore_Array_Push(cs->state, &plan->entries, &entry);
    }
    if (!compiler_semantic_cfg_ensure_active(cs)) {
        compiler_semantic_cfg_catch_plan_free(cs, plan);
        return ZR_FALSE;
    }

    cfg = &cs->preSemanticIr.cfg;
    for (index = 0U; index < plan->entries.length; index++) {
        SZrCompilerSemanticCatchPlanEntry *entry =
                (SZrCompilerSemanticCatchPlanEntry *) ZrCore_Array_Get(&plan->entries, index);

        if (entry->matchTypeId != ZR_SEMANTIC_ID_INVALID) {
            entry->dispatchBlock = ZrParser_Cfg_AppendBlock(cs->state, cfg, ZR_PARSER_CFG_BLOCK_STATEMENT,
                                                            (SZrAstNode *) entry->catchClause);
            entry->handlerBlock = ZrParser_Cfg_AppendBlock(cs->state, cfg, ZR_PARSER_CFG_BLOCK_STATEMENT,
                                                           (SZrAstNode *) entry->catchClause);
        } else {
            entry->handlerBlock = ZrParser_Cfg_AppendBlock(cs->state, cfg, ZR_PARSER_CFG_BLOCK_STATEMENT,
                                                           (SZrAstNode *) entry->catchClause);
            entry->dispatchBlock = entry->handlerBlock;
        }
        if (index == 0U) {
            plan->exceptionBlock = entry->dispatchBlock;
        }
        if (entry->dispatchBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
            entry->handlerBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID) {
            compiler_semantic_cfg_catch_plan_fail(cs, plan, entrySlots);
            return ZR_FALSE;
        }
    }
    lastEntry = (const SZrCompilerSemanticCatchPlanEntry *) ZrCore_Array_Get(&plan->entries, plan->entries.length - 1U);
    if (lastEntry == ZR_NULL) {
        compiler_semantic_cfg_catch_plan_fail(cs, plan, entrySlots);
        return ZR_FALSE;
    }
    if (lastEntry->matchTypeId != ZR_SEMANTIC_ID_INVALID) {
        plan->unmatchedBlock = ZrParser_Cfg_AppendBlock(cs->state, cfg, ZR_PARSER_CFG_BLOCK_STATEMENT,
                                                        (SZrAstNode *) lastEntry->catchClause);
    }
    plan->joinBlock = ZrParser_Cfg_AppendBlock(cs->state, cfg, ZR_PARSER_CFG_BLOCK_JOIN, node);
    if ((plan->unmatchedBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID && lastEntry->matchTypeId != ZR_SEMANTIC_ID_INVALID) ||
        plan->joinBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID || !compiler_semantic_cfg_capture_slots(cs, entrySlots)) {
        compiler_semantic_cfg_catch_plan_fail(cs, plan, entrySlots);
        return ZR_FALSE;
    }
    cs->preSemanticIrCfgCatchBlock = plan->exceptionBlock;
    cs->preSemanticIrCfgCatchUsed = ZR_FALSE;
    return ZR_TRUE;
}

static TZrBool compiler_semantic_cfg_emit_catch_dispatch(SZrCompilerState *cs, SZrCompilerSemanticCatchPlan *plan) {
    const SZrCompilerSemanticCatchPlanEntry *lastEntry;
    TZrSize index;

    compiler_semantic_cfg_enter(cs, plan->exceptionBlock);
    if (!compiler_semantic_cfg_emit_exception_payload(
                cs,
                ((const SZrCompilerSemanticCatchPlanEntry *) ZrCore_Array_Get(&plan->entries, 0U))
                        ->catchClause->location,
                &plan->payloadTypeId, &plan->payloadValueId)) {
        return ZR_FALSE;
    }
    for (index = 0U; index < plan->entries.length; index++) {
        const SZrCompilerSemanticCatchPlanEntry *entry =
                (const SZrCompilerSemanticCatchPlanEntry *) ZrCore_Array_Get(&plan->entries, index);
        const SZrCompilerSemanticCatchPlanEntry *nextEntry =
                index + 1U < plan->entries.length
                        ? (const SZrCompilerSemanticCatchPlanEntry *) ZrCore_Array_Get(&plan->entries, index + 1U)
                        : ZR_NULL;
        TZrUInt32 missBlock;
        TZrValueId typeTestValueId = ZR_VALUE_ID_INVALID;

        if (entry->matchTypeId == ZR_SEMANTIC_ID_INVALID) {
            break;
        }
        if (index != 0U) {
            compiler_semantic_cfg_enter(cs, entry->dispatchBlock);
        }
        missBlock = nextEntry != ZR_NULL ? nextEntry->dispatchBlock : plan->unmatchedBlock;
        if (missBlock == ZR_PARSER_CFG_INVALID_BLOCK_ID ||
            !compiler_semantic_cfg_emit_type_test(cs, plan->payloadValueId, entry->matchTypeId,
                                                  entry->catchClause->location, &typeTestValueId) ||
            !compiler_semantic_cfg_branch_value(cs, typeTestValueId, entry->handlerBlock, missBlock,
                                                (SZrAstNode *) entry->catchClause, entry->catchClause->location)) {
            return ZR_FALSE;
        }
    }
    lastEntry = (const SZrCompilerSemanticCatchPlanEntry *) ZrCore_Array_Get(&plan->entries, plan->entries.length - 1U);
    if (lastEntry->matchTypeId != ZR_SEMANTIC_ID_INVALID) {
        TZrBool previousAbruptIsLocal = cs->preSemanticIrCfgAbruptIsLocal;
        compiler_semantic_cfg_enter(cs, plan->unmatchedBlock);
        cs->preSemanticIrCfgAbruptIsLocal = ZR_TRUE;
        if (!compiler_semantic_cfg_terminate_throw_value(cs, plan->payloadValueId, lastEntry->catchClause->location)) {
            cs->preSemanticIrCfgAbruptIsLocal = previousAbruptIsLocal;
            return ZR_FALSE;
        }
        cs->preSemanticIrCfgAbruptIsLocal = previousAbruptIsLocal;
    }
    plan->dispatchEmitted = ZR_TRUE;
    return ZR_TRUE;
}

TZrBool compiler_semantic_cfg_enter_try_catch_handler(SZrCompilerState *cs, SZrAstNode *node,
                                                      SZrCompilerSemanticCatchPlan *plan, TZrSize catchIndex,
                                                      TZrUInt32 bindingSlot, SZrArray *entrySlots,
                                                      TZrBool *outEntered) {
    const SZrCompilerSemanticCatchPlanEntry *entry;

    if (outEntered != ZR_NULL) {
        *outEntered = ZR_FALSE;
    }
    if (cs == ZR_NULL || node == ZR_NULL || plan == ZR_NULL || entrySlots == ZR_NULL || outEntered == ZR_NULL ||
        !plan->initialized || catchIndex >= plan->entries.length) {
        return ZR_FALSE;
    }
    if (catchIndex == 0U) {
        if (!cs->preSemanticIrCfgActive || !cs->preSemanticIrCfgCatchUsed ||
            cs->preSemanticIrCfgCatchBlock != plan->exceptionBlock) {
            compiler_semantic_cfg_catch_plan_fail(cs, plan, entrySlots);
            return ZR_TRUE;
        }
        if (!compiler_semantic_cfg_jump(cs, plan->joinBlock, node->location)) {
            goto fail;
        }
        cs->preSemanticIrCfgCatchBlock = ZR_PARSER_CFG_INVALID_BLOCK_ID;
        cs->preSemanticIrCfgCatchUsed = ZR_FALSE;
        if (!compiler_semantic_cfg_restore_slots(cs, entrySlots) ||
            !compiler_semantic_cfg_emit_catch_dispatch(cs, plan)) {
            goto fail;
        }
    } else if (!plan->dispatchEmitted) {
        goto fail;
    }

    entry = (const SZrCompilerSemanticCatchPlanEntry *) ZrCore_Array_Get(&plan->entries, catchIndex);
    if (entry == ZR_NULL) {
        goto fail;
    }
    if (cs->preSemanticIrCfgBlock != entry->handlerBlock) {
        compiler_semantic_cfg_enter(cs, entry->handlerBlock);
    }
    if (!compiler_semantic_cfg_bind_catch_payload(cs, entry->catchClause, bindingSlot,
                                                  entry->matchTypeId == ZR_SEMANTIC_ID_INVALID ? plan->payloadTypeId
                                                                                               : entry->matchTypeId,
                                                  plan->payloadValueId)) {
        goto fail;
    }
    *outEntered = ZR_TRUE;
    return ZR_TRUE;

fail:
    compiler_semantic_cfg_catch_plan_fail(cs, plan, entrySlots);
    return ZR_FALSE;
}

TZrBool compiler_semantic_cfg_complete_try_catch(SZrCompilerState *cs, SZrAstNode *node,
                                                 SZrCompilerSemanticCatchPlan *plan, TZrSize catchIndex,
                                                 TZrBool handlerTerminates, SZrArray *entrySlots) {
    const SZrCompilerSemanticCatchPlanEntry *entry;
    TZrBool isLast;

    if (cs == ZR_NULL || node == ZR_NULL || plan == ZR_NULL || entrySlots == ZR_NULL || !plan->initialized ||
        catchIndex >= plan->entries.length) {
        return ZR_FALSE;
    }
    entry = (const SZrCompilerSemanticCatchPlanEntry *) ZrCore_Array_Get(&plan->entries, catchIndex);
    isLast = (TZrBool) (catchIndex + 1U == plan->entries.length);
    if (!cs->preSemanticIrCfgActive || entry == ZR_NULL ||
        cs->preSemanticIrCfgCatchBlock != ZR_PARSER_CFG_INVALID_BLOCK_ID ||
        (handlerTerminates ? cs->preSemanticIrCfgBlock != ZR_PARSER_CFG_INVALID_BLOCK_ID
                           : cs->preSemanticIrCfgBlock != entry->handlerBlock)) {
        goto fail;
    }
    if (!handlerTerminates && !compiler_semantic_cfg_jump(cs, plan->joinBlock, node->location)) {
        goto fail;
    }
    if (!compiler_semantic_cfg_restore_slots(cs, entrySlots)) {
        goto fail;
    }
    if (isLast) {
        compiler_semantic_cfg_enter(cs, plan->joinBlock);
        compiler_semantic_cfg_free_slots(cs, entrySlots);
        compiler_semantic_cfg_catch_plan_free(cs, plan);
    }
    return ZR_TRUE;

fail:
    compiler_semantic_cfg_catch_plan_fail(cs, plan, entrySlots);
    return ZR_FALSE;
}
