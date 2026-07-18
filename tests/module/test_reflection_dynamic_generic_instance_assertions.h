#ifndef ZR_VM_TEST_REFLECTION_DYNAMIC_GENERIC_INSTANCE_ASSERTIONS_H
#define ZR_VM_TEST_REFLECTION_DYNAMIC_GENERIC_INSTANCE_ASSERTIONS_H

static const SZrTypeValue *get_object_field_value(SZrState *state,
                                                   SZrObject *object,
                                                   const TZrChar *fieldName) {
    SZrString *fieldString;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }
    fieldString = ZrCore_String_CreateFromNative(state, (TZrNativeString)fieldName);
    TEST_ASSERT_NOT_NULL(fieldString);
    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(state, object, &key);
}

static void assert_object_string_field(SZrState *state,
                                       SZrObject *object,
                                       const TZrChar *fieldName,
                                       const TZrChar *expectedValue) {
    const SZrTypeValue *value = get_object_field_value(state, object, fieldName);

    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_STRING, value->type);
    TEST_ASSERT_NOT_NULL(value->value.object);
    TEST_ASSERT_EQUAL_STRING(expectedValue, ZrCore_String_GetNativeString(ZR_CAST_STRING(state, value->value.object)));
}

static void assert_object_int_field(SZrState *state,
                                    SZrObject *object,
                                    const TZrChar *fieldName,
                                    TZrInt64 expectedValue) {
    const SZrTypeValue *value = get_object_field_value(state, object, fieldName);

    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_TRUE(ZR_VALUE_IS_TYPE_INT(value->type));
    TEST_ASSERT_EQUAL_INT64(expectedValue, value->value.nativeObject.nativeInt64);
}

static void assert_object_bool_field(SZrState *state,
                                     SZrObject *object,
                                     const TZrChar *fieldName,
                                     TZrBool expectedValue) {
    const SZrTypeValue *value = get_object_field_value(state, object, fieldName);

    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_BOOL, value->type);
    TEST_ASSERT_EQUAL_INT(expectedValue ? ZR_TRUE : ZR_FALSE, value->value.nativeObject.nativeBool);
}

static void assert_object_native_pointer_field(SZrState *state,
                                               SZrObject *object,
                                               const TZrChar *fieldName,
                                               TZrPtr expectedValue) {
    const SZrTypeValue *value = get_object_field_value(state, object, fieldName);

    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NATIVE_POINTER, value->type);
    TEST_ASSERT_EQUAL_PTR(expectedValue, value->value.nativeObject.nativePointer);
}

static SZrObject *assert_object_object_field(SZrState *state,
                                             SZrObject *object,
                                             const TZrChar *fieldName,
                                             EZrValueType valueType) {
    const SZrTypeValue *value = get_object_field_value(state, object, fieldName);

    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_INT(valueType, value->type);
    TEST_ASSERT_NOT_NULL(value->value.object);
    return ZR_CAST_OBJECT(state, value->value.object);
}

static SZrObject *assert_array_object_entry(SZrState *state, SZrObject *array, TZrUInt32 index) {
    SZrTypeValue key;
    const SZrTypeValue *value;

    TEST_ASSERT_NOT_NULL(array);
    TEST_ASSERT_EQUAL_INT(ZR_OBJECT_INTERNAL_TYPE_ARRAY, array->internalType);
    ZrCore_Value_InitAsInt(state, &key, (TZrInt64)index);
    value = ZrCore_Object_GetValue(state, array, &key);
    TEST_ASSERT_NOT_NULL(value);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_OBJECT, value->type);
    TEST_ASSERT_NOT_NULL(value->value.object);
    return ZR_CAST_OBJECT(state, value->value.object);
}

static void assert_compound_generic_arguments(SZrState *state, SZrObject *argumentsArray) {
    SZrObject *tupleObject;
    SZrObject *tupleChildrenArray;
    SZrObject *ownershipObject;
    SZrObject *unionObject;
    SZrObject *unionChildrenArray;
    SZrObject *nullableObject;
    SZrObject *primitiveObject;

    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)argumentsArray->nodeMap.elementCount);
    tupleObject = assert_array_object_entry(state, argumentsArray, 0u);
    assert_object_string_field(state, tupleObject, "genericArgumentKind", "tuple");
    tupleChildrenArray = assert_object_object_field(state, tupleObject, "children", ZR_VALUE_TYPE_ARRAY);
    TEST_ASSERT_EQUAL_UINT32(2u, (TZrUInt32)tupleChildrenArray->nodeMap.elementCount);

    ownershipObject = assert_array_object_entry(state, tupleChildrenArray, 0u);
    assert_object_string_field(state, ownershipObject, "genericArgumentKind", "ownership");
    assert_object_int_field(state,
                            ownershipObject,
                            "ownershipQualifier",
                            ZR_REFLECTION_OWNERSHIP_QUALIFIER_UNIQUE);
    assert_object_int_field(state,
                            assert_object_object_field(
                                    state, ownershipObject, "elementType", ZR_VALUE_TYPE_OBJECT),
                            "typeToken",
                            TEST_TYPE_DEF_TOKEN);

    unionObject = assert_array_object_entry(state, tupleChildrenArray, 1u);
    assert_object_string_field(state, unionObject, "genericArgumentKind", "union");
    assert_object_int_field(state, unionObject, "unionValueType", ZR_VALUE_TYPE_OBJECT);
    assert_object_int_field(state, unionObject, "unionNameStringOffset", TEST_UNION_NAME_STRING_OFFSET);
    unionChildrenArray = assert_object_object_field(state, unionObject, "children", ZR_VALUE_TYPE_ARRAY);
    TEST_ASSERT_EQUAL_UINT32(1u, (TZrUInt32)unionChildrenArray->nodeMap.elementCount);
    nullableObject = assert_array_object_entry(state, unionChildrenArray, 0u);
    assert_object_string_field(state, nullableObject, "genericArgumentKind", "nullable");
    primitiveObject = assert_object_object_field(state, nullableObject, "elementType", ZR_VALUE_TYPE_OBJECT);
    assert_object_string_field(state, primitiveObject, "genericArgumentKind", "primitive");
    assert_object_int_field(state, primitiveObject, "primitiveValueType", ZR_VALUE_TYPE_INT64);
}

#endif
