#ifndef ZR_TEST_SEMANTIC_QUERY_CALL_TYPE_LIFETIME_CASES_H
#define ZR_TEST_SEMANTIC_QUERY_CALL_TYPE_LIFETIME_CASES_H

#include "zr_vm_core/global.h"
#include "zr_vm_parser/type_inference.h"
#include "../../zr_vm_parser/src/zr_vm_parser/type_inference/type_inference_semantic_facts.h"

static struct {
    FZrAllocator original;
    TZrPtr target;
    TZrPtr retired;
    TZrSize retiredSize;
    TZrUInt32 relocations;
} g_call_type_relocation;

static TZrPtr call_type_relocating_allocator(
        TZrPtr userData, TZrPtr pointer, TZrSize originalSize,
        TZrSize newSize, TZrInt64 flag) {
    if (pointer != ZR_NULL && pointer == g_call_type_relocation.target &&
        newSize > originalSize) {
        TZrPtr replacement = g_call_type_relocation.original(
                userData, ZR_NULL, 0U, newSize, flag);
        if (replacement == ZR_NULL) {
            return ZR_NULL;
        }
        memcpy(replacement, pointer, originalSize);
        /* Clear quarantined records so stale kind checks fail on every allocator. */
        memset(pointer, 0, originalSize);
        g_call_type_relocation.retired = pointer;
        g_call_type_relocation.retiredSize = originalSize;
        g_call_type_relocation.target = ZR_NULL;
        g_call_type_relocation.relocations++;
        return replacement;
    }
    return g_call_type_relocation.original(
            userData, pointer, originalSize, newSize, flag);
}

static void test_call_fact_preserves_declaration_contract_after_type_growth(void) {
    SZrCompilerState cs = {0};
    SZrFunctionTypeInfo functionInfo = {0};
    SZrResolvedCallSignature resolved = {0};
    SZrAstNode target = {0};
    SZrAstNode callNode = {0};
    SZrAstNode primary = {0};
    SZrAstNode *memberNodes[] = {&callNode};
    SZrAstNodeArray members = {0};
    SZrSemanticContext *context = ZrParser_SemanticContext_New(g_state);
    SZrString *sourceName = ZrCore_String_CreateFromNative(g_state, "call_type_growth.zr");
    TZrTypeId intTypeId;
    TZrTypeId boolTypeId;
    TZrTypeId readonlyIntId;
    TZrTypeId fillerId;
    const SZrSemanticReferenceFact *fact;
    const SZrCanonicalTypeNode *callType;
    const SZrCanonicalTypeNode *returnType;
    TZrTypeId pointeeId = ZR_SEMANTIC_ID_INVALID;
    EZrCanonicalRefAccess returnAccess = ZR_CANONICAL_REF_WRITABLE;
    TZrUInt32 effectFlags = ZR_CANONICAL_CALLABLE_EFFECT_NONE;
    TZrBool foundCall = ZR_FALSE;

    TEST_ASSERT_NOT_NULL(context);
    intTypeId = ZrParser_CanonicalType_InternPrimitive(context, ZR_VALUE_TYPE_INT64);
    boolTypeId = ZrParser_CanonicalType_InternPrimitive(context, ZR_VALUE_TYPE_BOOL);
    readonlyIntId = ZrParser_CanonicalType_InternRef(
            context, intTypeId, ZR_CANONICAL_REF_READONLY);
    functionInfo.name = ZrCore_String_CreateFromNative(g_state, "call");
    functionInfo.typeId = ZrParser_CanonicalType_InternFunction(
            context, ZR_NULL, 0U, readonlyIntId, ZR_CANONICAL_RECEIVER_NONE,
            ZR_CANONICAL_CALLABLE_EFFECT_THROWS);
    TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, functionInfo.typeId);
    functionInfo.declarationRange = call_source_position("call", sourceName, "call", 0U);
    functionInfo.hasDeclarationRange = ZR_TRUE;
    functionInfo.symbolId = ZrParser_Semantic_RegisterSymbol(
            context, functionInfo.name, ZR_SEMANTIC_SYMBOL_KIND_FUNCTION,
            functionInfo.typeId, ZR_SEMANTIC_ID_INVALID, ZR_NULL,
            functionInfo.declarationRange);
    ZrParser_InferredType_Init(g_state, &resolved.returnType, ZR_VALUE_TYPE_BOOL);
    resolved.returnType.referenceAccess = ZR_REFERENCE_ACCESS_WRITABLE;
    cs.state = g_state;
    cs.semanticContext = context;
    target.type = ZR_AST_IDENTIFIER_LITERAL;
    target.data.identifier.name = functionInfo.name;
    target.location = call_source_position("call call", sourceName, "call", 1U);
    callNode.type = ZR_AST_FUNCTION_CALL;
    callNode.location = target.location;
    members.nodes = memberNodes;
    members.count = 1U;
    primary.type = ZR_AST_PRIMARY_EXPRESSION;
    primary.location = target.location;
    primary.data.primaryExpression.property = &target;
    primary.data.primaryExpression.members = &members;
    fillerId = intTypeId;
    while (context->canonicalTypes.length < context->canonicalTypes.capacity) {
        fillerId = ZrParser_CanonicalType_InternNullable(context, fillerId);
        TEST_ASSERT_NOT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, fillerId);
    }

    memset(&g_call_type_relocation, 0, sizeof(g_call_type_relocation));
    g_call_type_relocation.original = g_state->global->allocator;
    g_call_type_relocation.target = context->canonicalTypes.head;
    g_state->global->allocator = call_type_relocating_allocator;
    type_inference_record_primary_call_reference_fact(
            &cs, &primary, &callNode, &functionInfo, &resolved);
    g_state->global->allocator = g_call_type_relocation.original;
    if (g_call_type_relocation.retired != ZR_NULL) {
        ZrCore_Memory_RawFree(g_state->global,
                             g_call_type_relocation.retired,
                             g_call_type_relocation.retiredSize);
    }
    fact = ZrParser_SemanticFacts_FindReferenceByNodeAndKind(
            context, &primary, ZR_SEMANTIC_REFERENCE_CALL);
    callType = fact != ZR_NULL ? ZrParser_CanonicalType_Find(context, fact->typeId) : ZR_NULL;
    if (callType != ZR_NULL && callType->kind == ZR_CANONICAL_TYPE_FUNCTION) {
        effectFlags = callType->data.function.effectFlags;
        returnType = ZrParser_CanonicalType_Find(context, callType->data.function.returnTypeId);
        if (returnType != ZR_NULL && returnType->kind == ZR_CANONICAL_TYPE_REF) {
            foundCall = fact->isResolved && fact->symbolId == functionInfo.symbolId;
            returnAccess = returnType->data.refType.access;
            pointeeId = returnType->data.refType.pointeeTypeId;
        }
    }
    ZrParser_InferredType_Free(g_state, &resolved.returnType);
    ZrParser_SemanticContext_Free(context);

    TEST_ASSERT_EQUAL_UINT32(1U, g_call_type_relocation.relocations);
    TEST_ASSERT_TRUE(foundCall);
    TEST_ASSERT_EQUAL_UINT32(boolTypeId, pointeeId);
    TEST_ASSERT_EQUAL_UINT32(ZR_CANONICAL_CALLABLE_EFFECT_THROWS, effectFlags);
    TEST_ASSERT_EQUAL_INT_MESSAGE(ZR_CANONICAL_REF_READONLY, returnAccess,
                                 "Call fact lost the declared readonly return after type growth");
}

#endif
