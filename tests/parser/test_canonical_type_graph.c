#include "unity.h"

#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"
#include "zr_vm_parser/semantic.h"
#include "zr_vm_parser/type_inference.h"

static SZrState *g_state;
static SZrSemanticContext *g_context;

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);

    g_context = ZrParser_SemanticContext_New(g_state);
    TEST_ASSERT_NOT_NULL(g_context);
}

void tearDown(void) {
    if (g_context != ZR_NULL) {
        ZrParser_SemanticContext_Free(g_context);
        g_context = ZR_NULL;
    }
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

static void test_primitive_types_are_structurally_interned(void) {
    const SZrCanonicalTypeNode *intNode;
    const SZrCanonicalTypeNode *boolNode;
    TZrTypeId firstInt;
    TZrTypeId secondInt;
    TZrTypeId boolType;

    firstInt = ZrParser_CanonicalType_InternPrimitive(g_context, ZR_VALUE_TYPE_INT64);
    secondInt = ZrParser_CanonicalType_InternPrimitive(g_context, ZR_VALUE_TYPE_INT64);
    boolType = ZrParser_CanonicalType_InternPrimitive(g_context, ZR_VALUE_TYPE_BOOL);

    TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, firstInt);
    TEST_ASSERT_EQUAL_UINT32(firstInt, secondInt);
    TEST_ASSERT_NOT_EQUAL(firstInt, boolType);

    intNode = ZrParser_CanonicalType_Find(g_context, firstInt);
    boolNode = ZrParser_CanonicalType_Find(g_context, boolType);
    TEST_ASSERT_NOT_NULL(intNode);
    TEST_ASSERT_NOT_NULL(boolNode);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_PRIMITIVE, intNode->kind);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_INT64, intNode->data.primitive.valueType);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, boolNode->data.primitive.valueType);
    TEST_ASSERT_NOT_EQUAL_UINT64(0, intNode->structuralHash);
    TEST_ASSERT_NOT_EQUAL_UINT64(intNode->structuralHash, boolNode->structuralHash);
}

static void test_nominal_and_generic_instance_identity_is_structural(void) {
    const SZrCanonicalTypeNode *genericNode;
    SZrString *appModuleFirst = ZrCore_String_Create(g_state, "app.model", 9);
    SZrString *appModuleSecond = ZrCore_String_Create(g_state, "app.model", 9);
    SZrString *otherModule = ZrCore_String_Create(g_state, "other.model", 11);
    SZrString *boxNameFirst = ZrCore_String_Create(g_state, "Box", 3);
    SZrString *boxNameSecond = ZrCore_String_Create(g_state, "Box", 3);
    SZrString *boxAliasName = ZrCore_String_Create(g_state, "BoxAlias", 8);
    TZrTypeId intType = ZrParser_CanonicalType_InternPrimitive(g_context, ZR_VALUE_TYPE_INT64);
    TZrTypeId boolType = ZrParser_CanonicalType_InternPrimitive(g_context, ZR_VALUE_TYPE_BOOL);
    TZrTypeId firstBox;
    TZrTypeId sameBox;
    TZrTypeId aliasedBox;
    TZrTypeId otherModuleBox;
    TZrTypeId otherTokenBox;
    TZrTypeId unresolvedBox;
    TZrTypeId unresolvedAlias;
    TZrTypeId intArguments[1];
    TZrTypeId boolArguments[1];
    TZrTypeId firstBoxOfInt;
    TZrTypeId sameBoxOfInt;
    TZrTypeId boxOfBool;

    firstBox = ZrParser_CanonicalType_InternNominal(
            g_context,
            appModuleFirst,
            boxNameFirst,
            0x02000001U);
    sameBox = ZrParser_CanonicalType_InternNominal(
            g_context,
            appModuleSecond,
            boxNameSecond,
            0x02000001U);
    aliasedBox = ZrParser_CanonicalType_InternNominal(
            g_context,
            appModuleSecond,
            boxAliasName,
            0x02000001U);
    otherModuleBox = ZrParser_CanonicalType_InternNominal(
            g_context,
            otherModule,
            boxNameFirst,
            0x02000001U);
    otherTokenBox = ZrParser_CanonicalType_InternNominal(
            g_context,
            appModuleFirst,
            boxNameFirst,
            0x02000002U);
    unresolvedBox = ZrParser_CanonicalType_InternNominal(
            g_context,
            appModuleFirst,
            boxNameFirst,
            0U);
    unresolvedAlias = ZrParser_CanonicalType_InternNominal(
            g_context,
            appModuleFirst,
            boxAliasName,
            0U);

    TEST_ASSERT_EQUAL_UINT32(firstBox, sameBox);
    TEST_ASSERT_EQUAL_UINT32(firstBox, aliasedBox);
    TEST_ASSERT_NOT_EQUAL(firstBox, otherModuleBox);
    TEST_ASSERT_NOT_EQUAL(firstBox, otherTokenBox);
    TEST_ASSERT_NOT_EQUAL(unresolvedBox, unresolvedAlias);

    intArguments[0] = intType;
    boolArguments[0] = boolType;
    firstBoxOfInt = ZrParser_CanonicalType_InternGenericInstance(g_context, firstBox, intArguments, 1);
    sameBoxOfInt = ZrParser_CanonicalType_InternGenericInstance(g_context, sameBox, intArguments, 1);
    boxOfBool = ZrParser_CanonicalType_InternGenericInstance(g_context, firstBox, boolArguments, 1);

    TEST_ASSERT_EQUAL_UINT32(firstBoxOfInt, sameBoxOfInt);
    TEST_ASSERT_NOT_EQUAL(firstBoxOfInt, boxOfBool);
    genericNode = ZrParser_CanonicalType_Find(g_context, firstBoxOfInt);
    TEST_ASSERT_NOT_NULL(genericNode);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_GENERIC_INSTANCE, genericNode->kind);
    TEST_ASSERT_EQUAL_UINT32(firstBox, genericNode->data.genericInstance.definitionTypeId);
    TEST_ASSERT_EQUAL_UINT32(1, (TZrUInt32)genericNode->data.genericInstance.arguments.length);
    TEST_ASSERT_EQUAL_INT(
            ZR_CANONICAL_GENERIC_ARGUMENT_TYPE,
            ((SZrCanonicalGenericArgument *)ZrCore_Array_Get(
                    (SZrArray *)&genericNode->data.genericInstance.arguments,
                    0))->kind);
    TEST_ASSERT_EQUAL_UINT32(
            intType,
            ((SZrCanonicalGenericArgument *)ZrCore_Array_Get(
                    (SZrArray *)&genericNode->data.genericInstance.arguments,
                    0))->data.typeId);
}

static void test_structural_composites_intern_all_identity_fields(void) {
    const SZrCanonicalTypeNode *arrayNode;
    const SZrCanonicalTypeNode *tupleNode;
    const SZrCanonicalTypeNode *unionNode;
    TZrTypeId intType = ZrParser_CanonicalType_InternPrimitive(g_context, ZR_VALUE_TYPE_INT64);
    TZrTypeId boolType = ZrParser_CanonicalType_InternPrimitive(g_context, ZR_VALUE_TYPE_BOOL);
    TZrTypeId unionDefinition = ZrParser_CanonicalType_InternNominal(
            g_context,
            ZrCore_String_Create(g_state, "app.model", 9),
            ZrCore_String_Create(g_state, "Result", 6),
            0x02000003U);
    TZrTypeId firstGenericParameter;
    TZrTypeId sameGenericParameter;
    TZrTypeId otherGenericParameter;
    TZrTypeId firstArray;
    TZrTypeId sameArray;
    TZrTypeId otherRankArray;
    TZrTypeId otherStorageArray;
    TZrTypeId orderedTypes[2] = {intType, boolType};
    TZrTypeId reversedTypes[2] = {boolType, intType};
    TZrTypeId firstTuple;
    TZrTypeId sameTuple;
    TZrTypeId reversedTuple;
    TZrTypeId firstUnion;
    TZrTypeId sameUnion;
    TZrTypeId reversedUnion;

    firstGenericParameter = ZrParser_CanonicalType_InternGenericParameter(g_context, 17U, 0U);
    sameGenericParameter = ZrParser_CanonicalType_InternGenericParameter(g_context, 17U, 0U);
    otherGenericParameter = ZrParser_CanonicalType_InternGenericParameter(g_context, 17U, 1U);
    TEST_ASSERT_EQUAL_UINT32(firstGenericParameter, sameGenericParameter);
    TEST_ASSERT_NOT_EQUAL(firstGenericParameter, otherGenericParameter);

    firstArray = ZrParser_CanonicalType_InternArray(
            g_context,
            intType,
            1U,
            ZR_CANONICAL_ARRAY_STORAGE_MANAGED);
    sameArray = ZrParser_CanonicalType_InternArray(
            g_context,
            intType,
            1U,
            ZR_CANONICAL_ARRAY_STORAGE_MANAGED);
    otherRankArray = ZrParser_CanonicalType_InternArray(
            g_context,
            intType,
            2U,
            ZR_CANONICAL_ARRAY_STORAGE_MANAGED);
    otherStorageArray = ZrParser_CanonicalType_InternArray(
            g_context,
            intType,
            1U,
            ZR_CANONICAL_ARRAY_STORAGE_INLINE);
    TEST_ASSERT_EQUAL_UINT32(firstArray, sameArray);
    TEST_ASSERT_NOT_EQUAL(firstArray, otherRankArray);
    TEST_ASSERT_NOT_EQUAL(firstArray, otherStorageArray);

    firstTuple = ZrParser_CanonicalType_InternTuple(g_context, orderedTypes, 2);
    sameTuple = ZrParser_CanonicalType_InternTuple(g_context, orderedTypes, 2);
    reversedTuple = ZrParser_CanonicalType_InternTuple(g_context, reversedTypes, 2);
    TEST_ASSERT_EQUAL_UINT32(firstTuple, sameTuple);
    TEST_ASSERT_NOT_EQUAL(firstTuple, reversedTuple);

    firstUnion = ZrParser_CanonicalType_InternUnion(g_context, unionDefinition, orderedTypes, 2);
    sameUnion = ZrParser_CanonicalType_InternUnion(g_context, unionDefinition, orderedTypes, 2);
    reversedUnion = ZrParser_CanonicalType_InternUnion(g_context, unionDefinition, reversedTypes, 2);
    TEST_ASSERT_EQUAL_UINT32(firstUnion, sameUnion);
    TEST_ASSERT_NOT_EQUAL(firstUnion, reversedUnion);

    arrayNode = ZrParser_CanonicalType_Find(g_context, firstArray);
    tupleNode = ZrParser_CanonicalType_Find(g_context, firstTuple);
    unionNode = ZrParser_CanonicalType_Find(g_context, firstUnion);
    TEST_ASSERT_NOT_NULL(arrayNode);
    TEST_ASSERT_NOT_NULL(tupleNode);
    TEST_ASSERT_NOT_NULL(unionNode);
    TEST_ASSERT_EQUAL_UINT32(intType, arrayNode->data.array.elementTypeId);
    TEST_ASSERT_EQUAL_UINT32(2, (TZrUInt32)tupleNode->data.typeList.elementTypeIds.length);
    TEST_ASSERT_EQUAL_UINT32(unionDefinition, unionNode->data.unionType.definitionTypeId);
    TEST_ASSERT_EQUAL_UINT32(2, (TZrUInt32)unionNode->data.unionType.variantTypeIds.length);

    TEST_ASSERT_EQUAL_UINT32(
            ZR_SEMANTIC_ID_INVALID,
            ZrParser_CanonicalType_InternArray(
                    g_context,
                    ZR_SEMANTIC_ID_INVALID,
                    1U,
                    ZR_CANONICAL_ARRAY_STORAGE_MANAGED));
}

static void test_invalid_enum_and_effect_values_are_rejected(void) {
    SZrCanonicalParameterContract parameter;
    TZrTypeId intType = ZrParser_CanonicalType_InternPrimitive(g_context, ZR_VALUE_TYPE_INT64);
    TZrTypeId nominalType = ZrParser_CanonicalType_InternNominal(
            g_context,
            ZR_NULL,
            ZrCore_String_Create(g_state, "InvalidBoundary", 15),
            0x02000040U);

    memset(&parameter, 0, sizeof(parameter));
    parameter.typeId = intType;
    parameter.passingForm = ZR_CANONICAL_PASSING_VALUE;
    parameter.escapeUpperBound = ZR_CANONICAL_ESCAPE_FUNCTION;
    parameter.entryInitialization = ZR_CANONICAL_ENTRY_INITIALIZED;
    parameter.exitInitialization = ZR_CANONICAL_EXIT_UNCHANGED;
    parameter.acceptsTemporary = ZR_TRUE;
    parameter.callSiteMarker = ZR_CANONICAL_CALL_SITE_NONE;

    TEST_ASSERT_EQUAL_UINT32(
            ZR_SEMANTIC_ID_INVALID,
            ZrParser_CanonicalType_InternPrimitive(g_context, (EZrValueType)-1));
    TEST_ASSERT_EQUAL_UINT32(
            ZR_SEMANTIC_ID_INVALID,
            ZrParser_CanonicalType_InternArray(
                    g_context,
                    intType,
                    1U,
                    (EZrCanonicalArrayStorageKind)-1));
    TEST_ASSERT_EQUAL_UINT32(
            ZR_SEMANTIC_ID_INVALID,
            ZrParser_CanonicalType_InternRef(
                    g_context,
                    intType,
                    (EZrCanonicalRefAccess)-1));
    TEST_ASSERT_EQUAL_UINT32(
            ZR_SEMANTIC_ID_INVALID,
            ZrParser_CanonicalType_InternOwner(
                    g_context,
                    intType,
                    (EZrCanonicalOwnerKind)-1));
    TEST_ASSERT_EQUAL_UINT32(
            ZR_SEMANTIC_ID_INVALID,
            ZrParser_CanonicalType_InternFunction(
                    g_context,
                    &parameter,
                    1U,
                    intType,
                    (EZrCanonicalReceiverEffect)-1,
                    ZR_CANONICAL_CALLABLE_EFFECT_NONE));
    TEST_ASSERT_EQUAL_UINT32(
            ZR_SEMANTIC_ID_INVALID,
            ZrParser_CanonicalType_InternFunction(
                    g_context,
                    &parameter,
                    1U,
                    intType,
                    ZR_CANONICAL_RECEIVER_NONE,
                    (TZrUInt32)1U << 31U));
    TEST_ASSERT_FALSE(ZrParser_CanonicalType_RegisterDefinition(
            g_context,
            nominalType,
            ZR_CANONICAL_TYPE_CAPABILITY_VALUE_TYPE,
            (EZrCanonicalGcScanKind)-1));
}

static void test_reference_owner_and_wrapper_shapes_are_distinct(void) {
    const SZrCanonicalTypeNode *readonlyRefNode;
    const SZrCanonicalTypeNode *ownerNode;
    TZrTypeId intType = ZrParser_CanonicalType_InternPrimitive(g_context, ZR_VALUE_TYPE_INT64);
    TZrTypeId errorType = ZrParser_CanonicalType_InternError(g_context);
    TZrTypeId sameErrorType = ZrParser_CanonicalType_InternError(g_context);
    TZrTypeId neverType = ZrParser_CanonicalType_InternNever(g_context);
    TZrTypeId writableRef = ZrParser_CanonicalType_InternRef(
            g_context,
            intType,
            ZR_CANONICAL_REF_WRITABLE);
    TZrTypeId sameWritableRef = ZrParser_CanonicalType_InternRef(
            g_context,
            intType,
            ZR_CANONICAL_REF_WRITABLE);
    TZrTypeId readonlyRef = ZrParser_CanonicalType_InternRef(
            g_context,
            intType,
            ZR_CANONICAL_REF_READONLY);
    TZrTypeId uniqueOwner = ZrParser_CanonicalType_InternOwner(
            g_context,
            intType,
            ZR_CANONICAL_OWNER_UNIQUE);
    TZrTypeId sharedOwner = ZrParser_CanonicalType_InternOwner(
            g_context,
            intType,
            ZR_CANONICAL_OWNER_SHARED);
    TZrTypeId weakOwner = ZrParser_CanonicalType_InternOwner(
            g_context,
            intType,
            ZR_CANONICAL_OWNER_WEAK);
    TZrTypeId atomicSharedOwner = ZrParser_CanonicalType_InternOwner(
            g_context,
            intType,
            ZR_CANONICAL_OWNER_ATOMIC_SHARED);
    TZrTypeId readonlyView = ZrParser_CanonicalType_InternReadonlyView(g_context, sharedOwner);
    TZrTypeId nullableReadonlyView = ZrParser_CanonicalType_InternNullable(g_context, readonlyView);
    TZrTypeId sameNullableReadonlyView = ZrParser_CanonicalType_InternNullable(g_context, readonlyView);

    TEST_ASSERT_EQUAL_UINT32(errorType, sameErrorType);
    TEST_ASSERT_NOT_EQUAL(errorType, neverType);
    TEST_ASSERT_EQUAL_UINT32(writableRef, sameWritableRef);
    TEST_ASSERT_NOT_EQUAL(writableRef, readonlyRef);
    TEST_ASSERT_NOT_EQUAL(uniqueOwner, sharedOwner);
    TEST_ASSERT_NOT_EQUAL(sharedOwner, weakOwner);
    TEST_ASSERT_NOT_EQUAL(weakOwner, atomicSharedOwner);
    TEST_ASSERT_EQUAL_UINT32(nullableReadonlyView, sameNullableReadonlyView);

    readonlyRefNode = ZrParser_CanonicalType_Find(g_context, readonlyRef);
    ownerNode = ZrParser_CanonicalType_Find(g_context, uniqueOwner);
    TEST_ASSERT_NOT_NULL(readonlyRefNode);
    TEST_ASSERT_NOT_NULL(ownerNode);
    TEST_ASSERT_EQUAL_UINT32(intType, readonlyRefNode->data.refType.pointeeTypeId);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_REF_READONLY, readonlyRefNode->data.refType.access);
    TEST_ASSERT_EQUAL_UINT32(intType, ownerNode->data.owner.targetTypeId);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_OWNER_UNIQUE, ownerNode->data.owner.ownerKind);

    TEST_ASSERT_EQUAL_UINT32(
            ZR_SEMANTIC_ID_INVALID,
            ZrParser_CanonicalType_InternOwner(
                    g_context,
                    intType,
                    (EZrCanonicalOwnerKind)99));
}

static void test_function_identity_uses_complete_callable_contract(void) {
    const SZrCanonicalTypeNode *functionNode;
    SZrCanonicalParameterContract parameters[2];
    SZrCanonicalParameterContract sameParameters[2];
    SZrCanonicalParameterContract changedParameters[2];
    SZrCanonicalParameterContract invalidParameters[2];
    TZrTypeId intType = ZrParser_CanonicalType_InternPrimitive(g_context, ZR_VALUE_TYPE_INT64);
    TZrTypeId boolType = ZrParser_CanonicalType_InternPrimitive(g_context, ZR_VALUE_TYPE_BOOL);
    TZrTypeId writableBoolRef = ZrParser_CanonicalType_InternRef(
            g_context,
            boolType,
            ZR_CANONICAL_REF_WRITABLE);
    TZrTypeId readonlyBoolRef = ZrParser_CanonicalType_InternRef(
            g_context,
            boolType,
            ZR_CANONICAL_REF_READONLY);
    TZrTypeId firstFunction;
    TZrTypeId sameFunction;
    TZrTypeId changedPassingFunction;
    TZrTypeId changedReceiverFunction;

    memset(parameters, 0, sizeof(parameters));
    parameters[0].typeId = intType;
    parameters[0].passingForm = ZR_CANONICAL_PASSING_VALUE;
    parameters[0].escapeUpperBound = ZR_CANONICAL_ESCAPE_FUNCTION;
    parameters[0].entryInitialization = ZR_CANONICAL_ENTRY_INITIALIZED;
    parameters[0].exitInitialization = ZR_CANONICAL_EXIT_UNCHANGED;
    parameters[0].acceptsTemporary = ZR_TRUE;
    parameters[0].callSiteMarker = ZR_CANONICAL_CALL_SITE_NONE;

    parameters[1].typeId = writableBoolRef;
    parameters[1].passingForm = ZR_CANONICAL_PASSING_OUT;
    parameters[1].escapeUpperBound = ZR_CANONICAL_ESCAPE_FUNCTION;
    parameters[1].entryInitialization = ZR_CANONICAL_ENTRY_UNINITIALIZED;
    parameters[1].exitInitialization = ZR_CANONICAL_EXIT_DEFINITELY_INITIALIZED;
    parameters[1].callSiteMarker = ZR_CANONICAL_CALL_SITE_OUT;

    memcpy(sameParameters, parameters, sizeof(parameters));
    memcpy(changedParameters, parameters, sizeof(parameters));
    changedParameters[1].passingForm = ZR_CANONICAL_PASSING_REF;
    changedParameters[1].escapeUpperBound = ZR_CANONICAL_ESCAPE_CALLER;
    changedParameters[1].entryInitialization = ZR_CANONICAL_ENTRY_INITIALIZED;
    changedParameters[1].exitInitialization = ZR_CANONICAL_EXIT_UNCHANGED;
    changedParameters[1].callSiteMarker = ZR_CANONICAL_CALL_SITE_REF;

    firstFunction = ZrParser_CanonicalType_InternFunction(
            g_context,
            parameters,
            2,
            boolType,
            ZR_CANONICAL_RECEIVER_MUTABLE,
            ZR_CANONICAL_CALLABLE_EFFECT_THROWS);
    sameFunction = ZrParser_CanonicalType_InternFunction(
            g_context,
            sameParameters,
            2,
            boolType,
            ZR_CANONICAL_RECEIVER_MUTABLE,
            ZR_CANONICAL_CALLABLE_EFFECT_THROWS);
    changedPassingFunction = ZrParser_CanonicalType_InternFunction(
            g_context,
            changedParameters,
            2,
            boolType,
            ZR_CANONICAL_RECEIVER_MUTABLE,
            ZR_CANONICAL_CALLABLE_EFFECT_THROWS);
    changedReceiverFunction = ZrParser_CanonicalType_InternFunction(
            g_context,
            parameters,
            2,
            boolType,
            ZR_CANONICAL_RECEIVER_READONLY,
            ZR_CANONICAL_CALLABLE_EFFECT_THROWS);

    TEST_ASSERT_EQUAL_UINT32(firstFunction, sameFunction);
    TEST_ASSERT_NOT_EQUAL(firstFunction, changedPassingFunction);
    TEST_ASSERT_NOT_EQUAL(firstFunction, changedReceiverFunction);
    functionNode = ZrParser_CanonicalType_Find(g_context, firstFunction);
    TEST_ASSERT_NOT_NULL(functionNode);
    TEST_ASSERT_EQUAL_INT(ZR_CANONICAL_TYPE_FUNCTION, functionNode->kind);
    TEST_ASSERT_EQUAL_UINT32(boolType, functionNode->data.function.returnTypeId);
    TEST_ASSERT_EQUAL_UINT32(2, (TZrUInt32)functionNode->data.function.parameterContracts.length);

    memcpy(invalidParameters, parameters, sizeof(parameters));
    invalidParameters[1].typeId = boolType;
    TEST_ASSERT_EQUAL_UINT32(
            ZR_SEMANTIC_ID_INVALID,
            ZrParser_CanonicalType_InternFunction(
                    g_context,
                    invalidParameters,
                    2,
                    boolType,
                    ZR_CANONICAL_RECEIVER_MUTABLE,
                    ZR_CANONICAL_CALLABLE_EFFECT_THROWS));

    invalidParameters[1].typeId = readonlyBoolRef;
    TEST_ASSERT_EQUAL_UINT32(
            ZR_SEMANTIC_ID_INVALID,
            ZrParser_CanonicalType_InternFunction(
                    g_context,
                    invalidParameters,
                    2,
                    boolType,
                    ZR_CANONICAL_RECEIVER_MUTABLE,
                    ZR_CANONICAL_CALLABLE_EFFECT_THROWS));

    memcpy(invalidParameters, parameters, sizeof(parameters));
    invalidParameters[1].acceptsTemporary = ZR_TRUE;
    TEST_ASSERT_EQUAL_UINT32(
            ZR_SEMANTIC_ID_INVALID,
            ZrParser_CanonicalType_InternFunction(
                    g_context,
                    invalidParameters,
                    2,
                    boolType,
                    ZR_CANONICAL_RECEIVER_MUTABLE,
                    ZR_CANONICAL_CALLABLE_EFFECT_THROWS));

    memcpy(invalidParameters, parameters, sizeof(parameters));
    invalidParameters[1].entryInitialization = ZR_CANONICAL_ENTRY_INITIALIZED;
    TEST_ASSERT_EQUAL_UINT32(
            ZR_SEMANTIC_ID_INVALID,
            ZrParser_CanonicalType_InternFunction(
                    g_context,
                    invalidParameters,
                    2,
                    boolType,
                    ZR_CANONICAL_RECEIVER_MUTABLE,
                    ZR_CANONICAL_CALLABLE_EFFECT_THROWS));
}

static void test_value_construction_requires_capability_and_public_constructor_contract(void) {
    TZrTypeId intType = ZrParser_CanonicalType_InternPrimitive(g_context, ZR_VALUE_TYPE_INT64);
    TZrTypeId boolType = ZrParser_CanonicalType_InternPrimitive(g_context, ZR_VALUE_TYPE_BOOL);
    TZrTypeId pointType = ZrParser_CanonicalType_InternNominal(
            g_context,
            ZrCore_String_Create(g_state, "app.model", 9),
            ZrCore_String_Create(g_state, "Point", 5),
            0x02000010U);
    TZrTypeId runtimeTypeDescriptor = ZrParser_CanonicalType_InternNominal(
            g_context,
            ZrCore_String_Create(g_state, "zr.reflection", 13),
            ZrCore_String_Create(g_state, "Type", 4),
            0x02000020U);
    TZrTypeId boxDefinition = ZrParser_CanonicalType_InternNominal(
            g_context,
            ZrCore_String_Create(g_state, "app.model", 9),
            ZrCore_String_Create(g_state, "Box", 3),
            0x02000021U);
    TZrTypeId boxAliasDefinition = ZrParser_CanonicalType_InternNominal(
            g_context,
            ZrCore_String_Create(g_state, "app.model", 9),
            ZrCore_String_Create(g_state, "BoxAlias", 8),
            0x02000021U);
    TZrSymbolId boxOwnerSymbolId = 71U;
    TZrTypeId boxParameter = ZrParser_CanonicalType_InternGenericParameter(
            g_context,
            boxOwnerSymbolId,
            0U);
    TZrTypeId boxArguments[1] = {intType};
    TZrTypeId boxOfInt = ZrParser_CanonicalType_InternGenericInstance(
            g_context,
            boxDefinition,
            boxArguments,
            1U);
    TZrTypeId aliasBoxOfInt = ZrParser_CanonicalType_InternGenericInstance(
            g_context,
            boxAliasDefinition,
            boxArguments,
            1U);
    TZrTypeId pointParameters[2] = {intType, intType};
    TZrTypeId privateParameters[1] = {boolType};
    TZrTypeId boxConstructorParameters[1] = {boxParameter};
    TZrSymbolId resolvedConstructor = ZR_SEMANTIC_ID_INVALID;

    TEST_ASSERT_TRUE(ZrParser_CanonicalType_RegisterDefinition(
            g_context,
            pointType,
            ZR_CANONICAL_TYPE_CAPABILITY_VALUE_TYPE |
                    ZR_CANONICAL_TYPE_CAPABILITY_VALUE_CONSTRUCTIBLE |
                    ZR_CANONICAL_TYPE_CAPABILITY_BLITTABLE,
            ZR_CANONICAL_GC_SCAN_FREE));
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_RegisterConstructor(
            g_context,
            pointType,
            51U,
            pointParameters,
            2,
            ZR_TRUE));
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_RegisterConstructor(
            g_context,
            pointType,
            52U,
            privateParameters,
            1,
            ZR_FALSE));

    TEST_ASSERT_TRUE(ZrParser_CanonicalType_HasCapabilities(
            g_context,
            pointType,
            ZR_CANONICAL_TYPE_CAPABILITY_VALUE_TYPE |
                    ZR_CANONICAL_TYPE_CAPABILITY_VALUE_CONSTRUCTIBLE));
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_ResolveValueConstructor(
            g_context,
            pointType,
            pointParameters,
            2,
            &resolvedConstructor));
    TEST_ASSERT_EQUAL_UINT32(51U, resolvedConstructor);

    resolvedConstructor = ZR_SEMANTIC_ID_INVALID;
    TEST_ASSERT_FALSE(ZrParser_CanonicalType_ResolveValueConstructor(
            g_context,
            pointType,
            privateParameters,
            1,
            &resolvedConstructor));
    TEST_ASSERT_EQUAL_UINT32(ZR_SEMANTIC_ID_INVALID, resolvedConstructor);
    TEST_ASSERT_FALSE(ZrParser_CanonicalType_ResolveValueConstructor(
            g_context,
            runtimeTypeDescriptor,
            pointParameters,
            2,
            &resolvedConstructor));

    TEST_ASSERT_EQUAL_UINT32(boxDefinition, boxAliasDefinition);
    TEST_ASSERT_EQUAL_UINT32(boxOfInt, aliasBoxOfInt);
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_RegisterGenericDefinition(
            g_context,
            boxDefinition,
            boxOwnerSymbolId,
            1U,
            ZR_CANONICAL_TYPE_CAPABILITY_VALUE_TYPE |
                    ZR_CANONICAL_TYPE_CAPABILITY_VALUE_CONSTRUCTIBLE,
            ZR_CANONICAL_GC_SCAN_FREE));
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_RegisterConstructor(
            g_context,
            boxDefinition,
            61U,
            boxConstructorParameters,
            1U,
            ZR_TRUE));
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_HasCapabilities(
            g_context,
            boxOfInt,
            ZR_CANONICAL_TYPE_CAPABILITY_VALUE_CONSTRUCTIBLE));
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_ResolveValueConstructor(
            g_context,
            boxOfInt,
            boxArguments,
            1U,
            &resolvedConstructor));
    TEST_ASSERT_EQUAL_UINT32(61U, resolvedConstructor);
    TEST_ASSERT_FALSE(ZrParser_CanonicalType_ResolveValueConstructor(
            g_context,
            boxOfInt,
            privateParameters,
            1U,
            &resolvedConstructor));

    ZrParser_CanonicalTypeDefinition_Reset(g_context);
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)g_context->canonicalTypeDefinitions.length);
    TEST_ASSERT_FALSE(ZrParser_CanonicalType_HasCapabilities(
            g_context,
            boxOfInt,
            ZR_CANONICAL_TYPE_CAPABILITY_VALUE_CONSTRUCTIBLE));
}

static void assert_canonical_type_format(TZrTypeId typeId, const TZrChar *expected) {
    TZrChar buffer[256];

    TEST_ASSERT_TRUE(ZrParser_CanonicalType_Format(g_context, typeId, buffer, sizeof(buffer)));
    TEST_ASSERT_EQUAL_STRING(expected, buffer);
}

static void test_formatter_renders_every_current_canonical_type_shape(void) {
    TZrTypeId intType = ZrParser_CanonicalType_InternPrimitive(g_context, ZR_VALUE_TYPE_INT64);
    TZrTypeId boolType = ZrParser_CanonicalType_InternPrimitive(g_context, ZR_VALUE_TYPE_BOOL);
    TZrTypeId boxDefinition = ZrParser_CanonicalType_InternNominal(
            g_context,
            ZrCore_String_Create(g_state, "app.collections", 15),
            ZrCore_String_Create(g_state, "Box", 3),
            0x02000030U);
    TZrTypeId choiceDefinition = ZrParser_CanonicalType_InternNominal(
            g_context,
            ZrCore_String_Create(g_state, "app", 3),
            ZrCore_String_Create(g_state, "Choice", 6),
            0x02000031U);
    TZrTypeId genericArguments[1] = {intType};
    TZrTypeId tupleElements[2] = {intType, boolType};
    TZrTypeId genericType = ZrParser_CanonicalType_InternGenericInstance(
            g_context,
            boxDefinition,
            genericArguments,
            1);
    TZrTypeId arrayType = ZrParser_CanonicalType_InternArray(
            g_context,
            genericType,
            2U,
            ZR_CANONICAL_ARRAY_STORAGE_MANAGED);
    TZrTypeId nestedArrayType = ZrParser_CanonicalType_InternArray(
            g_context,
            ZrParser_CanonicalType_InternArray(
                    g_context,
                    genericType,
                    1U,
                    ZR_CANONICAL_ARRAY_STORAGE_MANAGED),
            1U,
            ZR_CANONICAL_ARRAY_STORAGE_MANAGED);
    TZrTypeId tupleType = ZrParser_CanonicalType_InternTuple(g_context, tupleElements, 2);
    TZrTypeId unionType = ZrParser_CanonicalType_InternUnion(
            g_context,
            choiceDefinition,
            tupleElements,
            2);
    TZrTypeId writableRef = ZrParser_CanonicalType_InternRef(
            g_context,
            intType,
            ZR_CANONICAL_REF_WRITABLE);
    TZrTypeId readonlyRef = ZrParser_CanonicalType_InternRef(
            g_context,
            intType,
            ZR_CANONICAL_REF_READONLY);
    TZrTypeId writableBoolRef = ZrParser_CanonicalType_InternRef(
            g_context,
            boolType,
            ZR_CANONICAL_REF_WRITABLE);
    TZrTypeId uniqueOwner = ZrParser_CanonicalType_InternOwner(
            g_context,
            genericType,
            ZR_CANONICAL_OWNER_UNIQUE);
    TZrTypeId readonlyView = ZrParser_CanonicalType_InternReadonlyView(g_context, genericType);
    TZrTypeId nullableType = ZrParser_CanonicalType_InternNullable(g_context, genericType);
    SZrCanonicalParameterContract parameters[2];
    TZrTypeId functionType;
    TZrChar tooSmall[4];

    memset(parameters, 0, sizeof(parameters));
    parameters[0].typeId = intType;
    parameters[0].passingForm = ZR_CANONICAL_PASSING_VALUE;
    parameters[0].escapeUpperBound = ZR_CANONICAL_ESCAPE_FUNCTION;
    parameters[0].entryInitialization = ZR_CANONICAL_ENTRY_INITIALIZED;
    parameters[0].exitInitialization = ZR_CANONICAL_EXIT_UNCHANGED;
    parameters[0].acceptsTemporary = ZR_TRUE;
    parameters[0].callSiteMarker = ZR_CANONICAL_CALL_SITE_NONE;
    parameters[1].typeId = writableBoolRef;
    parameters[1].passingForm = ZR_CANONICAL_PASSING_OUT;
    parameters[1].escapeUpperBound = ZR_CANONICAL_ESCAPE_FUNCTION;
    parameters[1].entryInitialization = ZR_CANONICAL_ENTRY_UNINITIALIZED;
    parameters[1].exitInitialization = ZR_CANONICAL_EXIT_DEFINITELY_INITIALIZED;
    parameters[1].callSiteMarker = ZR_CANONICAL_CALL_SITE_OUT;
    functionType = ZrParser_CanonicalType_InternFunction(
            g_context,
            parameters,
            2,
            boolType,
            ZR_CANONICAL_RECEIVER_READONLY,
            ZR_CANONICAL_CALLABLE_EFFECT_THROWS | ZR_CANONICAL_CALLABLE_EFFECT_ASYNC);

    assert_canonical_type_format(intType, "int");
    assert_canonical_type_format(boxDefinition, "app.collections.Box");
    assert_canonical_type_format(genericType, "app.collections.Box<int>");
    TEST_ASSERT_NOT_EQUAL(arrayType, nestedArrayType);
    assert_canonical_type_format(arrayType, "app.collections.Box<int>[,]");
    assert_canonical_type_format(nestedArrayType, "app.collections.Box<int>[][]");
    assert_canonical_type_format(tupleType, "(int, bool)");
    assert_canonical_type_format(unionType, "app.Choice{int | bool}");
    assert_canonical_type_format(writableRef, "ref int");
    assert_canonical_type_format(readonlyRef, "ref readonly int");
    assert_canonical_type_format(uniqueOwner, "Unique<app.collections.Box<int>>");
    assert_canonical_type_format(readonlyView, "readonly app.collections.Box<int>");
    assert_canonical_type_format(nullableType, "app.collections.Box<int>?");
    assert_canonical_type_format(functionType, "async const fn(int, out bool) -> bool throws");
    assert_canonical_type_format(ZrParser_CanonicalType_InternError(g_context), "<error>");
    assert_canonical_type_format(ZrParser_CanonicalType_InternNever(g_context), "never");

    TEST_ASSERT_FALSE(ZrParser_CanonicalType_Format(
            g_context,
            genericType,
            tooSmall,
            sizeof(tooSmall)));
    TEST_ASSERT_EQUAL_STRING("", tooSmall);
}

#include "test_canonical_type_graph_semantic_cases.h"
#include "test_canonical_type_graph_union_cases.h"

static void test_tuple_ast_projects_to_tuple_type_id(void) {
    const TZrChar *source = "pair(): [int, bool] { return 0; }";
    SZrString *sourceName = ZrCore_String_Create(g_state, "tuple_type.zr", 13);
    SZrAstNode *ast = ZrParser_Parse(g_state, source, strlen(source), sourceName);
    SZrCompilerState compiler;
    SZrFunctionDeclaration *function;
    SZrInferredType inferredType;
    TZrTypeId typeId;

    TEST_ASSERT_NOT_NULL(ast);
    TEST_ASSERT_EQUAL_INT(ZR_AST_SCRIPT, ast->type);
    TEST_ASSERT_NOT_NULL(ast->data.script.statements);
    TEST_ASSERT_EQUAL_UINT32(1U, (TZrUInt32)ast->data.script.statements->count);
    TEST_ASSERT_EQUAL_INT(
            ZR_AST_FUNCTION_DECLARATION,
            ast->data.script.statements->nodes[0]->type);

    ZrParser_CompilerState_Init(&compiler, g_state);
    function = &ast->data.script.statements->nodes[0]->data.functionDeclaration;
    ZrParser_InferredType_Init(g_state, &inferredType, ZR_VALUE_TYPE_OBJECT);
    TEST_ASSERT_TRUE(ZrParser_AstTypeToInferredType_Convert(
            &compiler,
            function->returnType,
            &inferredType));
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, inferredType.baseType);
    TEST_ASSERT_NULL(inferredType.typeName);
    TEST_ASSERT_EQUAL_UINT32(2U, (TZrUInt32)inferredType.elementTypes.length);

    typeId = ZrParser_CanonicalType_FromInferred(g_context, &inferredType);
    TEST_ASSERT_EQUAL_INT(
            ZR_CANONICAL_TYPE_TUPLE,
            ZrParser_CanonicalType_Find(g_context, typeId)->kind);
    assert_canonical_type_format(typeId, "(int, bool)");

    ZrParser_InferredType_Free(g_state, &inferredType);
    ZrParser_Ast_Free(g_state, ast);
    ZrParser_CompilerState_Free(&compiler);
}

static void test_intern_index_scales_to_large_and_deep_type_graphs(void) {
    const TZrUInt32 uniqueTypeCount = 100000U;
    const TZrUInt32 deepTypeCount = 256U;
    TZrTypeId sampledTypeIds[3] = {
            ZR_SEMANTIC_ID_INVALID,
            ZR_SEMANTIC_ID_INVALID,
            ZR_SEMANTIC_ID_INVALID,
    };
    TZrTypeId deepType;
    TZrChar deepFormat[1024];
    TZrBool collisionFound = ZR_FALSE;
    TZrBool collisionLookupVerified = ZR_FALSE;
    TZrUInt32 index;

    for (index = 0; index < uniqueTypeCount; index++) {
        TZrTypeId typeId = ZrParser_CanonicalType_InternGenericParameter(
                g_context,
                index + 1U,
                0U);

        if (typeId == ZR_SEMANTIC_ID_INVALID) {
            TEST_FAIL_MESSAGE("large canonical type graph rejected a unique type");
        }
        if (index == 0U) {
            sampledTypeIds[0] = typeId;
        } else if (index == uniqueTypeCount / 2U) {
            sampledTypeIds[1] = typeId;
        } else if (index == uniqueTypeCount - 1U) {
            sampledTypeIds[2] = typeId;
        }
    }

    TEST_ASSERT_EQUAL_UINT32(uniqueTypeCount, (TZrUInt32)g_context->canonicalTypes.length);
    TEST_ASSERT_EQUAL_UINT32(
            sampledTypeIds[0],
            ZrParser_CanonicalType_InternGenericParameter(g_context, 1U, 0U));
    TEST_ASSERT_EQUAL_UINT32(
            sampledTypeIds[1],
            ZrParser_CanonicalType_InternGenericParameter(g_context, uniqueTypeCount / 2U + 1U, 0U));
    TEST_ASSERT_EQUAL_UINT32(
            sampledTypeIds[2],
            ZrParser_CanonicalType_InternGenericParameter(g_context, uniqueTypeCount, 0U));
    TEST_ASSERT_EQUAL_UINT32(uniqueTypeCount, (TZrUInt32)g_context->canonicalTypes.length);

    deepType = ZrParser_CanonicalType_InternPrimitive(g_context, ZR_VALUE_TYPE_INT64);
    for (index = 0; index < deepTypeCount; index++) {
        TZrTypeId nextType = ZrParser_CanonicalType_InternArray(
                g_context,
                deepType,
                1U,
                ZR_CANONICAL_ARRAY_STORAGE_MANAGED);

        TEST_ASSERT_NOT_EQUAL(ZR_SEMANTIC_ID_INVALID, nextType);
        deepType = nextType;
    }

    TEST_ASSERT_NOT_NULL(ZrParser_CanonicalType_Find(g_context, deepType));
    TEST_ASSERT_EQUAL_UINT32(
            deepType,
            ZrParser_CanonicalType_InternArray(
                    g_context,
                    ZrParser_CanonicalType_Find(g_context, deepType)->data.array.elementTypeId,
                    1U,
                    ZR_CANONICAL_ARRAY_STORAGE_MANAGED));
    TEST_ASSERT_EQUAL_UINT32(
            uniqueTypeCount + deepTypeCount + 1U,
            (TZrUInt32)g_context->canonicalTypes.length);
    TEST_ASSERT_TRUE(ZrParser_CanonicalType_Format(
            g_context,
            deepType,
            deepFormat,
            sizeof(deepFormat)));
    TEST_ASSERT_EQUAL_UINT32(
            3U + deepTypeCount * 2U,
            (TZrUInt32)strlen(deepFormat));

    for (index = 0; index < g_context->canonicalTypes.length; index++) {
        const SZrCanonicalTypeNode *node =
                (const SZrCanonicalTypeNode *)ZrCore_Array_Get(
                        &g_context->canonicalTypes,
                        index);
        TZrSize bucketHead;
        TZrSize nextIndex;
        const SZrCanonicalTypeNode *candidate;
        TZrTypeId reinterned = ZR_SEMANTIC_ID_INVALID;

        if (node == ZR_NULL) {
            continue;
        }
        bucketHead = ZrParser_CanonicalTypeIndex_First(g_context, node->structuralHash);
        nextIndex = ZrParser_CanonicalTypeIndex_Next(g_context, bucketHead);
        if (nextIndex == ZR_MAX_SIZE) {
            continue;
        }
        collisionFound = ZR_TRUE;
        candidate = (const SZrCanonicalTypeNode *)ZrCore_Array_Get(
                &g_context->canonicalTypes,
                nextIndex);
        if (candidate != ZR_NULL && candidate->kind == ZR_CANONICAL_TYPE_GENERIC_PARAMETER) {
            reinterned = ZrParser_CanonicalType_InternGenericParameter(
                    g_context,
                    candidate->data.genericParameter.ownerSymbolId,
                    candidate->data.genericParameter.ordinal);
        } else if (candidate != ZR_NULL && candidate->kind == ZR_CANONICAL_TYPE_ARRAY) {
            reinterned = ZrParser_CanonicalType_InternArray(
                    g_context,
                    candidate->data.array.elementTypeId,
                    candidate->data.array.rank,
                    candidate->data.array.storageKind);
        }
        if (candidate != ZR_NULL && reinterned == candidate->id) {
            collisionLookupVerified = ZR_TRUE;
            break;
        }
    }
    TEST_ASSERT_TRUE(collisionFound);
    TEST_ASSERT_TRUE(collisionLookupVerified);

    ZrParser_SemanticContext_Reset(g_context);
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)g_context->canonicalTypes.length);
    TEST_ASSERT_EQUAL_UINT32(0U, (TZrUInt32)g_context->canonicalTypeHashNext.length);
    TEST_ASSERT_NULL(ZrParser_CanonicalType_Find(g_context, deepType));
    TEST_ASSERT_EQUAL_UINT32(
            ZR_SEMANTIC_ID_FIRST,
            ZrParser_CanonicalType_InternPrimitive(g_context, ZR_VALUE_TYPE_INT64));
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_primitive_types_are_structurally_interned);
    RUN_TEST(test_nominal_and_generic_instance_identity_is_structural);
    RUN_TEST(test_structural_composites_intern_all_identity_fields);
    RUN_TEST(test_invalid_enum_and_effect_values_are_rejected);
    RUN_TEST(test_reference_owner_and_wrapper_shapes_are_distinct);
    RUN_TEST(test_function_identity_uses_complete_callable_contract);
    RUN_TEST(test_value_construction_requires_capability_and_public_constructor_contract);
    RUN_TEST(test_formatter_renders_every_current_canonical_type_shape);
    RUN_TEST(test_legacy_inferred_types_project_to_structural_type_ids);
    RUN_TEST(test_function_registration_uses_function_type_id);
    RUN_TEST(test_generic_constructor_substitution_is_closed_and_kind_aware);
    RUN_TEST(test_tuple_ast_projects_to_tuple_type_id);
    RUN_TEST(test_union_compiler_path_unifies_declaration_and_use_type_ids);
    RUN_TEST(test_open_const_function_and_closed_member_returns_preserve_structure);
    RUN_TEST(test_class_generic_kind_and_constraint_failures_are_atomic);
    RUN_TEST(test_known_generic_contract_mismatches_fail_compiler_canonicalization);
    RUN_TEST(test_union_registration_failure_does_not_publish_partial_state);
    RUN_TEST(test_intern_index_scales_to_large_and_deep_type_graphs);
    return UNITY_END();
}
