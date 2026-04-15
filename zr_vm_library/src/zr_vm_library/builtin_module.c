#include "native_binding/native_binding_internal.h"

#include <string.h>

static const TZrChar *kBuiltinBoxedValueField = "__zr_builtin_boxed_value";

static const TZrChar *builtin_wrapper_type_name_for_value_type(EZrValueType valueType) {
    switch (valueType) {
        case ZR_VALUE_TYPE_BOOL:
            return "Bool";
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_UINT8:
            return "Byte";
        case ZR_VALUE_TYPE_UINT64:
            return "UInt64";
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
            return "Integer";
        case ZR_VALUE_TYPE_FLOAT:
            return "Float";
        case ZR_VALUE_TYPE_DOUBLE:
            return "Double";
        case ZR_VALUE_TYPE_STRING:
            return "String";
        default:
            return ZR_NULL;
    }
}

static const SZrTypeValue *builtin_try_unbox_value(SZrState *state,
                                                   const SZrTypeValue *value) {
    SZrObject *object;
    const SZrTypeValue *boxedValue;

    if (state == ZR_NULL || value == ZR_NULL) {
        return value;
    }

    if ((value->type != ZR_VALUE_TYPE_OBJECT && value->type != ZR_VALUE_TYPE_ARRAY) || value->value.object == ZR_NULL) {
        return value;
    }

    object = ZR_CAST_OBJECT(state, value->value.object);
    if (object == ZR_NULL) {
        return value;
    }

    boxedValue = ZrLib_Object_GetFieldCString(state, object, kBuiltinBoxedValueField);
    return boxedValue != ZR_NULL ? boxedValue : value;
}

static TZrInt64 builtin_compare_values(SZrState *state,
                                       const SZrTypeValue *left,
                                       const SZrTypeValue *right) {
    const SZrTypeValue *stableLeft = builtin_try_unbox_value(state, left);
    const SZrTypeValue *stableRight = builtin_try_unbox_value(state, right);

    if (stableLeft == ZR_NULL || stableRight == ZR_NULL) {
        return 0;
    }

    if (ZrCore_Value_CompareDirectly(state, (SZrTypeValue *)stableLeft, (SZrTypeValue *)stableRight) ||
        ZrCore_Value_Equal(state, (SZrTypeValue *)stableLeft, (SZrTypeValue *)stableRight)) {
        return 0;
    }

    if (ZR_VALUE_IS_TYPE_NUMBER(stableLeft->type) && ZR_VALUE_IS_TYPE_NUMBER(stableRight->type)) {
        TZrFloat64 leftNumber = ZR_VALUE_IS_TYPE_FLOAT(stableLeft->type)
                                        ? stableLeft->value.nativeObject.nativeDouble
                                        : (TZrFloat64)stableLeft->value.nativeObject.nativeInt64;
        TZrFloat64 rightNumber = ZR_VALUE_IS_TYPE_FLOAT(stableRight->type)
                                         ? stableRight->value.nativeObject.nativeDouble
                                         : (TZrFloat64)stableRight->value.nativeObject.nativeInt64;
        return leftNumber < rightNumber ? -1 : 1;
    }

    if (stableLeft->type == ZR_VALUE_TYPE_BOOL && stableRight->type == ZR_VALUE_TYPE_BOOL) {
        return stableLeft->value.nativeObject.nativeBool ? 1 : -1;
    }

    if (stableLeft->type == ZR_VALUE_TYPE_STRING && stableRight->type == ZR_VALUE_TYPE_STRING) {
        SZrString *leftString = ZR_CAST_STRING(state, stableLeft->value.object);
        SZrString *rightString = ZR_CAST_STRING(state, stableRight->value.object);
        const TZrChar *leftText = leftString != ZR_NULL ? ZrCore_String_GetNativeString(leftString) : "";
        const TZrChar *rightText = rightString != ZR_NULL ? ZrCore_String_GetNativeString(rightString) : "";
        return strcmp(leftText != ZR_NULL ? leftText : "", rightText != ZR_NULL ? rightText : "") < 0 ? -1 : 1;
    }

    return ZrCore_Value_GetHash(state, stableLeft) < ZrCore_Value_GetHash(state, stableRight) ? -1 : 1;
}

static TZrBool builtin_make_boxed_wrapper(const ZrLibCallContext *context,
                                          const SZrTypeValue *source,
                                          SZrTypeValue *result) {
    SZrState *state;
    const TZrChar *wrapperTypeName;
    SZrObjectPrototype *prototype;
    SZrObject *boxedObject;
    ZrLibTempValueRoot root;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    state = context->state;
    if (state == ZR_NULL || source == ZR_NULL) {
        ZrLib_Value_SetNull(result);
        return ZR_TRUE;
    }

    wrapperTypeName = builtin_wrapper_type_name_for_value_type(source->type);
    if (wrapperTypeName == ZR_NULL) {
        if ((source->type == ZR_VALUE_TYPE_OBJECT || source->type == ZR_VALUE_TYPE_ARRAY) && source->value.object != ZR_NULL) {
            ZrLib_Value_SetObject(state, result, ZR_CAST_OBJECT(state, source->value.object), source->type);
            return ZR_TRUE;
        }
        ZrLib_Value_SetNull(result);
        return ZR_TRUE;
    }

    prototype = ZrLib_Type_FindPrototype(state, wrapperTypeName);
    if (prototype == ZR_NULL) {
        return ZR_FALSE;
    }

    boxedObject = ZrLib_Type_NewInstanceWithPrototype(state, prototype);
    if (boxedObject == ZR_NULL || !ZrLib_CallContext_BeginTempValueRoot(context, &root)) {
        return ZR_FALSE;
    }

    if (!ZrLib_TempValueRoot_SetObject(&root, boxedObject, ZR_VALUE_TYPE_OBJECT)) {
        ZrLib_TempValueRoot_End(&root);
        return ZR_FALSE;
    }

    ZrLib_Object_SetFieldCString(state, boxedObject, kBuiltinBoxedValueField, source);
    ZrLib_Value_SetObject(state, result, boxedObject, ZR_VALUE_TYPE_OBJECT);
    ZrLib_TempValueRoot_End(&root);
    return ZR_TRUE;
}

static TZrBool builtin_object_type(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrTypeValue *value;
    SZrObject *object;
    const TZrChar *typeName;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    value = ZrLib_CallContext_Argument(context, 0);
    if (value == ZR_NULL) {
        ZrLib_Value_SetString(context->state, result, "null");
        return ZR_TRUE;
    }

    object = ((value->type == ZR_VALUE_TYPE_OBJECT || value->type == ZR_VALUE_TYPE_ARRAY) && value->value.object != ZR_NULL)
                     ? ZR_CAST_OBJECT(context->state, value->value.object)
                     : ZR_NULL;
    if (object != ZR_NULL && object->prototype != ZR_NULL && object->prototype->name != ZR_NULL) {
        ZrLib_Value_SetStringObject(context->state, result, object->prototype->name);
        return ZR_TRUE;
    }

    typeName = native_binding_value_type_name(context->state, value);
    ZrLib_Value_SetString(context->state, result, typeName != ZR_NULL ? typeName : "value");
    return ZR_TRUE;
}

static TZrBool builtin_object_box(ZrLibCallContext *context, SZrTypeValue *result) {
    return builtin_make_boxed_wrapper(context, ZrLib_CallContext_Argument(context, 0), result);
}

static TZrBool builtin_type_info_box(ZrLibCallContext *context, SZrTypeValue *result) {
    return builtin_make_boxed_wrapper(context, ZrLib_CallContext_Argument(context, 0), result);
}

static TZrBool builtin_wrapper_equals(ZrLibCallContext *context, SZrTypeValue *result) {
    const SZrTypeValue *left;
    const SZrTypeValue *right;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    left = builtin_try_unbox_value(context->state, ZrLib_CallContext_Self(context));
    right = builtin_try_unbox_value(context->state, ZrLib_CallContext_Argument(context, 0));
    ZrLib_Value_SetBool(context->state,
                        result,
                        left != ZR_NULL && right != ZR_NULL &&
                                (ZrCore_Value_CompareDirectly(context->state, (SZrTypeValue *)left, (SZrTypeValue *)right) ||
                                 ZrCore_Value_Equal(context->state, (SZrTypeValue *)left, (SZrTypeValue *)right)));
    return ZR_TRUE;
}

static TZrBool builtin_wrapper_hash_code(ZrLibCallContext *context, SZrTypeValue *result) {
    const SZrTypeValue *value;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    value = builtin_try_unbox_value(context->state, ZrLib_CallContext_Self(context));
    ZrLib_Value_SetInt(context->state, result, (TZrInt64)ZrCore_Value_GetHash(context->state, value));
    return ZR_TRUE;
}

static TZrBool builtin_wrapper_compare_to(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrLib_Value_SetInt(context->state,
                       result,
                       builtin_compare_values(context->state,
                                              ZrLib_CallContext_Self(context),
                                              ZrLib_CallContext_Argument(context, 0)));
    return ZR_TRUE;
}

static TZrBool builtin_comparer_compare(ZrLibCallContext *context, SZrTypeValue *result) {
    ZrLib_Value_SetInt(context->state,
                       result,
                       builtin_compare_values(context->state,
                                              ZrLib_CallContext_Argument(context, 0),
                                              ZrLib_CallContext_Argument(context, 1)));
    return ZR_TRUE;
}

static const ZrLibGenericParameterDescriptor g_builtin_single_generic_parameter[] = {
        {.name = "T", .documentation = "Builtin protocol type parameter."},
};

static const ZrLibParameterDescriptor g_object_value_parameter[] = {
        {"value", "object", "Value to inspect or box."},
};

static const ZrLibParameterDescriptor g_generic_value_parameter[] = {
        {"other", "T", "Comparison target."},
};

static const ZrLibParameterDescriptor g_comparer_parameters[] = {
        {"left", "T", "Left value."},
        {"right", "T", "Right value."},
};

static const ZrLibParameterDescriptor g_array_like_get_item_parameters[] = {
        {"index", "int", "Element index."},
};

static const ZrLibParameterDescriptor g_array_like_set_item_parameters[] = {
        {"index", "int", "Element index."},
        {"value", "T", "Element value."},
};

static const ZrLibFieldDescriptor g_enumerator_fields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_ROLE_INIT("current", "T", "Current iterator element.", ZR_MEMBER_CONTRACT_ROLE_ITERATOR_CURRENT_FIELD),
};

static const ZrLibFieldDescriptor g_array_like_fields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_INIT("length", "int", "Logical element count."),
};

static const ZrLibFieldDescriptor g_type_info_fields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_INIT("name", "string", "Reflection short name."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("qualifiedName", "string", "Reflection qualified name."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("kind", "string", "Reflection kind label."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("hash", "UInt64", "Stable reflection hash."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("owner", "zr.builtin.TypeInfo", "Owning reflection target when applicable."),
        ZR_LIB_FIELD_DESCRIPTOR_INIT("module", "zr.builtin.TypeInfo", "Owning module reflection when applicable."),
};

static const ZrLibMethodDescriptor g_enumerable_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_ROLE_INIT("getIterator", 0, 0, ZR_NULL, "zr.builtin.IEnumerator<T>",
                                           "Create an iterator.", ZR_FALSE, ZR_NULL, 0,
                                           ZR_MEMBER_CONTRACT_ROLE_ITERABLE_INIT),
};

static const ZrLibMethodDescriptor g_enumerator_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_ROLE_INIT("moveNext", 0, 0, ZR_NULL, "bool",
                                           "Advance iterator state.", ZR_FALSE, ZR_NULL, 0,
                                           ZR_MEMBER_CONTRACT_ROLE_ITERATOR_MOVE_NEXT),
};

static const ZrLibMetaMethodDescriptor g_array_like_meta_methods[] = {
        {ZR_META_GET_ITEM, 1, 1, ZR_NULL, "T", "Read an indexed element.",
         g_array_like_get_item_parameters, ZR_ARRAY_COUNT(g_array_like_get_item_parameters), ZR_NULL, 0},
        {ZR_META_SET_ITEM, 2, 2, ZR_NULL, "T", "Write an indexed element.",
         g_array_like_set_item_parameters, ZR_ARRAY_COUNT(g_array_like_set_item_parameters), ZR_NULL, 0},
};

static const ZrLibMethodDescriptor g_equatable_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("equals", 1, 1, ZR_NULL, "bool",
                                      "Compare semantic equality.", ZR_FALSE,
                                      g_generic_value_parameter, ZR_ARRAY_COUNT(g_generic_value_parameter)),
};

static const ZrLibMethodDescriptor g_hashable_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("hashCode", 0, 0, ZR_NULL, "int",
                                      "Return a stable hash code.", ZR_FALSE, ZR_NULL, 0),
};

static const ZrLibMethodDescriptor g_comparable_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("compareTo", 1, 1, ZR_NULL, "int",
                                      "Compare ordering against another value.", ZR_FALSE,
                                      g_generic_value_parameter, ZR_ARRAY_COUNT(g_generic_value_parameter)),
};

static const ZrLibMethodDescriptor g_comparer_methods[] = {
        {"compare", 2, 2, builtin_comparer_compare, "int", "Compare two values.", ZR_FALSE,
         g_comparer_parameters, ZR_ARRAY_COUNT(g_comparer_parameters), 0U,
         ZR_NULL, 0},
};

static const ZrLibMethodDescriptor g_object_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("type", 1, 1, builtin_object_type, "string",
                                      "Return a runtime type label.", ZR_TRUE,
                                      g_object_value_parameter, ZR_ARRAY_COUNT(g_object_value_parameter)),
        ZR_LIB_METHOD_DESCRIPTOR_ROLE_INIT("box", 1, 1, builtin_object_box, "zr.builtin.Object",
                                           "Box a primitive value into its wrapper type.", ZR_TRUE,
                                           g_object_value_parameter, ZR_ARRAY_COUNT(g_object_value_parameter),
                                           ZR_MEMBER_CONTRACT_ROLE_BUILTIN_BOX),
};

static const ZrLibMethodDescriptor g_type_info_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_ROLE_INIT("box", 1, 1, builtin_type_info_box, "zr.builtin.Object",
                                           "Box a value through reflection metadata.", ZR_TRUE,
                                           g_object_value_parameter, ZR_ARRAY_COUNT(g_object_value_parameter),
                                           ZR_MEMBER_CONTRACT_ROLE_BUILTIN_BOX),
};

static const ZrLibMethodDescriptor g_wrapper_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("equals", 1, 1, builtin_wrapper_equals, "bool",
                                      "Compare wrapped value equality.", ZR_FALSE,
                                      g_generic_value_parameter, ZR_ARRAY_COUNT(g_generic_value_parameter)),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("compareTo", 1, 1, builtin_wrapper_compare_to, "int",
                                      "Compare wrapped value ordering.", ZR_FALSE,
                                      g_generic_value_parameter, ZR_ARRAY_COUNT(g_generic_value_parameter)),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("hashCode", 0, 0, builtin_wrapper_hash_code, "int",
                                      "Hash the wrapped value.", ZR_FALSE, ZR_NULL, 0),
};

static const TZrChar *g_integer_implements[] = {
        "zr.builtin.IEquatable<Integer>",
        "zr.builtin.IComparable<Integer>",
        "zr.builtin.IHashable",
};

static const TZrChar *g_float_implements[] = {
        "zr.builtin.IEquatable<Float>",
        "zr.builtin.IComparable<Float>",
        "zr.builtin.IHashable",
};

static const TZrChar *g_double_implements[] = {
        "zr.builtin.IEquatable<Double>",
        "zr.builtin.IComparable<Double>",
        "zr.builtin.IHashable",
};

static const TZrChar *g_string_implements[] = {
        "zr.builtin.IEquatable<String>",
        "zr.builtin.IComparable<String>",
        "zr.builtin.IHashable",
};

static const TZrChar *g_bool_implements[] = {
        "zr.builtin.IEquatable<Bool>",
        "zr.builtin.IHashable",
};

static const TZrChar *g_byte_implements[] = {
        "zr.builtin.IEquatable<Byte>",
        "zr.builtin.IComparable<Byte>",
        "zr.builtin.IHashable",
};

static const TZrChar *g_char_implements[] = {
        "zr.builtin.IEquatable<Char>",
        "zr.builtin.IComparable<Char>",
        "zr.builtin.IHashable",
};

static const TZrChar *g_uint64_implements[] = {
        "zr.builtin.IEquatable<UInt64>",
        "zr.builtin.IComparable<UInt64>",
        "zr.builtin.IHashable",
};

static const ZrLibTypeDescriptor g_builtin_types[] = {
        ZR_LIB_TYPE_DESCRIPTOR_PROTOCOL_INIT("IEnumerable", ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE, ZR_NULL, 0,
                                             g_enumerable_methods, ZR_ARRAY_COUNT(g_enumerable_methods), ZR_NULL, 0,
                                             "Canonical builtin enumerable protocol.", ZR_NULL, ZR_NULL, 0,
                                             ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, "IEnumerable<T>()",
                                             g_builtin_single_generic_parameter, ZR_ARRAY_COUNT(g_builtin_single_generic_parameter),
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERABLE)),
        ZR_LIB_TYPE_DESCRIPTOR_PROTOCOL_INIT("IEnumerator", ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE,
                                             g_enumerator_fields, ZR_ARRAY_COUNT(g_enumerator_fields),
                                             g_enumerator_methods, ZR_ARRAY_COUNT(g_enumerator_methods), ZR_NULL, 0,
                                             "Canonical builtin iterator protocol.", ZR_NULL, ZR_NULL, 0,
                                             ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, "IEnumerator<T>()",
                                             g_builtin_single_generic_parameter, ZR_ARRAY_COUNT(g_builtin_single_generic_parameter),
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ITERATOR)),
        ZR_LIB_TYPE_DESCRIPTOR_PROTOCOL_INIT("IArrayLike", ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE,
                                             g_array_like_fields, ZR_ARRAY_COUNT(g_array_like_fields),
                                             ZR_NULL, 0, g_array_like_meta_methods, ZR_ARRAY_COUNT(g_array_like_meta_methods),
                                             "Canonical builtin array-like protocol.", ZR_NULL, ZR_NULL, 0,
                                             ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, "IArrayLike<T>()",
                                             g_builtin_single_generic_parameter, ZR_ARRAY_COUNT(g_builtin_single_generic_parameter),
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_ARRAY_LIKE)),
        ZR_LIB_TYPE_DESCRIPTOR_PROTOCOL_INIT("IEquatable", ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE, ZR_NULL, 0,
                                             g_equatable_methods, ZR_ARRAY_COUNT(g_equatable_methods), ZR_NULL, 0,
                                             "Canonical builtin equality protocol.", ZR_NULL, ZR_NULL, 0,
                                             ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, "IEquatable<T>()",
                                             g_builtin_single_generic_parameter, ZR_ARRAY_COUNT(g_builtin_single_generic_parameter),
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_EQUATABLE)),
        ZR_LIB_TYPE_DESCRIPTOR_PROTOCOL_INIT("IHashable", ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE, ZR_NULL, 0,
                                             g_hashable_methods, ZR_ARRAY_COUNT(g_hashable_methods), ZR_NULL, 0,
                                             "Canonical builtin hash protocol.", ZR_NULL, ZR_NULL, 0,
                                             ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, "IHashable()",
                                             ZR_NULL, 0, ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_HASHABLE)),
        ZR_LIB_TYPE_DESCRIPTOR_PROTOCOL_INIT("IComparable", ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE, ZR_NULL, 0,
                                             g_comparable_methods, ZR_ARRAY_COUNT(g_comparable_methods), ZR_NULL, 0,
                                             "Canonical builtin comparable protocol.", ZR_NULL, ZR_NULL, 0,
                                             ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, "IComparable<T>()",
                                             g_builtin_single_generic_parameter, ZR_ARRAY_COUNT(g_builtin_single_generic_parameter),
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_COMPARABLE)),
        ZR_LIB_TYPE_DESCRIPTOR_INIT("IComparer", ZR_OBJECT_PROTOTYPE_TYPE_INTERFACE, ZR_NULL, 0,
                                    g_comparer_methods, ZR_ARRAY_COUNT(g_comparer_methods), ZR_NULL, 0,
                                    "Canonical builtin comparer protocol.", ZR_NULL, ZR_NULL, 0,
                                    ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, "IComparer<T>()",
                                    g_builtin_single_generic_parameter, ZR_ARRAY_COUNT(g_builtin_single_generic_parameter)),
        ZR_LIB_TYPE_DESCRIPTOR_INIT("Object", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0,
                                    g_object_methods, ZR_ARRAY_COUNT(g_object_methods), ZR_NULL, 0,
                                    "Builtin root object type.", ZR_NULL, ZR_NULL, 0,
                                    ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL, 0),
        ZR_LIB_TYPE_DESCRIPTOR_INIT("Module", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0,
                                    ZR_NULL, 0, ZR_NULL, 0,
                                    "Builtin root module type.", "zr.builtin.Object", ZR_NULL, 0,
                                    ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL, 0),
        ZR_LIB_TYPE_DESCRIPTOR_INIT("TypeInfo", ZR_OBJECT_PROTOTYPE_TYPE_CLASS,
                                    g_type_info_fields, ZR_ARRAY_COUNT(g_type_info_fields),
                                    g_type_info_methods, ZR_ARRAY_COUNT(g_type_info_methods), ZR_NULL, 0,
                                    "Builtin reflection root type.", "zr.builtin.Object", ZR_NULL, 0,
                                    ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL, 0),
        ZR_LIB_TYPE_DESCRIPTOR_PROTOCOL_INIT("Integer", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0,
                                             g_wrapper_methods, ZR_ARRAY_COUNT(g_wrapper_methods), ZR_NULL, 0,
                                             "Boxed integer wrapper.", "zr.builtin.Object",
                                             g_integer_implements, ZR_ARRAY_COUNT(g_integer_implements),
                                             ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL, 0,
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_EQUATABLE) |
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_COMPARABLE) |
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_HASHABLE)),
        ZR_LIB_TYPE_DESCRIPTOR_PROTOCOL_INIT("Float", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0,
                                             g_wrapper_methods, ZR_ARRAY_COUNT(g_wrapper_methods), ZR_NULL, 0,
                                             "Boxed float wrapper.", "zr.builtin.Object",
                                             g_float_implements, ZR_ARRAY_COUNT(g_float_implements),
                                             ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL, 0,
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_EQUATABLE) |
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_COMPARABLE) |
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_HASHABLE)),
        ZR_LIB_TYPE_DESCRIPTOR_PROTOCOL_INIT("Double", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0,
                                             g_wrapper_methods, ZR_ARRAY_COUNT(g_wrapper_methods), ZR_NULL, 0,
                                             "Boxed double wrapper.", "zr.builtin.Object",
                                             g_double_implements, ZR_ARRAY_COUNT(g_double_implements),
                                             ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL, 0,
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_EQUATABLE) |
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_COMPARABLE) |
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_HASHABLE)),
        ZR_LIB_TYPE_DESCRIPTOR_PROTOCOL_INIT("String", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0,
                                             g_wrapper_methods, ZR_ARRAY_COUNT(g_wrapper_methods), ZR_NULL, 0,
                                             "Boxed string wrapper.", "zr.builtin.Object",
                                             g_string_implements, ZR_ARRAY_COUNT(g_string_implements),
                                             ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL, 0,
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_EQUATABLE) |
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_COMPARABLE) |
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_HASHABLE)),
        ZR_LIB_TYPE_DESCRIPTOR_PROTOCOL_INIT("Bool", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0,
                                             g_wrapper_methods, ZR_ARRAY_COUNT(g_wrapper_methods), ZR_NULL, 0,
                                             "Boxed bool wrapper.", "zr.builtin.Object",
                                             g_bool_implements, ZR_ARRAY_COUNT(g_bool_implements),
                                             ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL, 0,
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_EQUATABLE) |
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_HASHABLE)),
        ZR_LIB_TYPE_DESCRIPTOR_PROTOCOL_INIT("Byte", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0,
                                             g_wrapper_methods, ZR_ARRAY_COUNT(g_wrapper_methods), ZR_NULL, 0,
                                             "Boxed byte wrapper.", "zr.builtin.Object",
                                             g_byte_implements, ZR_ARRAY_COUNT(g_byte_implements),
                                             ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL, 0,
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_EQUATABLE) |
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_COMPARABLE) |
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_HASHABLE)),
        ZR_LIB_TYPE_DESCRIPTOR_PROTOCOL_INIT("Char", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0,
                                             g_wrapper_methods, ZR_ARRAY_COUNT(g_wrapper_methods), ZR_NULL, 0,
                                             "Boxed char wrapper.", "zr.builtin.Object",
                                             g_char_implements, ZR_ARRAY_COUNT(g_char_implements),
                                             ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL, 0,
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_EQUATABLE) |
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_COMPARABLE) |
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_HASHABLE)),
        ZR_LIB_TYPE_DESCRIPTOR_PROTOCOL_INIT("UInt64", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0,
                                             g_wrapper_methods, ZR_ARRAY_COUNT(g_wrapper_methods), ZR_NULL, 0,
                                             "Boxed unsigned 64-bit wrapper.", "zr.builtin.Object",
                                             g_uint64_implements, ZR_ARRAY_COUNT(g_uint64_implements),
                                             ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL, 0,
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_EQUATABLE) |
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_COMPARABLE) |
                                             ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_HASHABLE)),
};

static const ZrLibTypeHintDescriptor g_builtin_hints[] = {
        {"IEnumerable", "type", "interface IEnumerable<T>", "Canonical builtin enumerable protocol."},
        {"IEnumerator", "type", "interface IEnumerator<T>", "Canonical builtin iterator protocol."},
        {"IArrayLike", "type", "interface IArrayLike<T>", "Canonical builtin array-like protocol."},
        {"IEquatable", "type", "interface IEquatable<T>", "Canonical builtin equality protocol."},
        {"IHashable", "type", "interface IHashable", "Canonical builtin hash protocol."},
        {"IComparable", "type", "interface IComparable<T>", "Canonical builtin comparable protocol."},
        {"IComparer", "type", "interface IComparer<T>", "Canonical builtin comparer protocol."},
        {"Object", "type", "class Object", "Builtin root object type."},
        {"Module", "type", "class Module", "Builtin root module type."},
        {"TypeInfo", "type", "class TypeInfo", "Builtin reflection root type."},
        {"Integer", "type", "class Integer", "Builtin boxed integer wrapper."},
        {"Float", "type", "class Float", "Builtin boxed float wrapper."},
        {"Double", "type", "class Double", "Builtin boxed double wrapper."},
        {"String", "type", "class String", "Builtin boxed string wrapper."},
        {"Bool", "type", "class Bool", "Builtin boxed bool wrapper."},
        {"Byte", "type", "class Byte", "Builtin boxed byte wrapper."},
        {"Char", "type", "class Char", "Builtin boxed char wrapper."},
        {"UInt64", "type", "class UInt64", "Builtin boxed unsigned 64-bit wrapper."},
};

static const TZrChar g_builtin_hints_json[] =
        "{\n"
        "  \"schema\": \"zr.native.hints/v1\",\n"
        "  \"module\": \"zr.builtin\"\n"
        "}\n";

static const ZrLibModuleDescriptor g_builtin_descriptor = {
        ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        "zr.builtin",
        ZR_NULL,
        0,
        ZR_NULL,
        0,
        g_builtin_types,
        ZR_ARRAY_COUNT(g_builtin_types),
        g_builtin_hints,
        ZR_ARRAY_COUNT(g_builtin_hints),
        g_builtin_hints_json,
        "Canonical builtin protocols, runtime roots, and boxed primitive wrappers.",
        ZR_NULL,
        0,
        "1.0.0",
        ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
        0,
        ZR_NULL,
};

const ZrLibModuleDescriptor *ZrLibrary_BuiltinModule_GetDescriptor(void) {
    return &g_builtin_descriptor;
}
