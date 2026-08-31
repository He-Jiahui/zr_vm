//
// Public reflection object materialization for constructed generic requests.
//

#include "zr_vm_core/reflection.h"

#include "reflection_object_internal.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

#include <string.h>

#define ZR_REFLECTION_GENERIC_TYPE_OBJECT_MAX_RECURSION_DEPTH 64u

#define generic_type_object_pin_raw ZrCore_Reflection_ObjectPinRaw
#define generic_type_object_unpin_raw ZrCore_Reflection_ObjectUnpinRaw
#define generic_type_object_pin_value ZrCore_Reflection_ObjectPinValue
#define generic_type_object_unpin_value ZrCore_Reflection_ObjectUnpinValue
#define generic_type_object_make_string ZrCore_Reflection_ObjectMakeString
#define generic_type_object_set_field_value ZrCore_Reflection_ObjectSetFieldValue
#define generic_type_object_set_string ZrCore_Reflection_ObjectSetString
#define generic_type_object_set_bool ZrCore_Reflection_ObjectSetBool
#define generic_type_object_set_object ZrCore_Reflection_ObjectSetObject

static TZrBool generic_type_object_set_int(SZrState *state,
                                           SZrObject *object,
                                           const TZrChar *fieldName,
                                           TZrInt64 value) {
    SZrTypeValue fieldValue;
    ZrCore_Value_InitAsInt(state, &fieldValue, value);
    return generic_type_object_set_field_value(state, object, fieldName, &fieldValue);
}

static TZrBool generic_type_object_set_uint(SZrState *state,
                                            SZrObject *object,
                                            const TZrChar *fieldName,
                                            TZrUInt64 value) {
    SZrTypeValue fieldValue;
    ZrCore_Value_InitAsUInt(state, &fieldValue, value);
    return generic_type_object_set_field_value(state, object, fieldName, &fieldValue);
}

static TZrBool generic_type_object_set_native_pointer(SZrState *state,
                                                      SZrObject *object,
                                                      const TZrChar *fieldName,
                                                      TZrPtr value) {
    SZrTypeValue fieldValue;
    ZrCore_Value_InitAsNativePointer(state, &fieldValue, value);
    return generic_type_object_set_field_value(state, object, fieldName, &fieldValue);
}

static SZrObject *generic_type_object_new_array(SZrState *state) {
    SZrObject *array;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }
    array = ZrCore_Object_NewCustomized(state, sizeof(SZrObject), ZR_OBJECT_INTERNAL_TYPE_ARRAY);
    if (array != ZR_NULL) {
        ZrCore_Object_Init(state, array);
    }
    return array;
}

static TZrBool generic_type_object_array_push(SZrState *state,
                                              SZrObject *array,
                                              SZrObject *object) {
    SZrTypeValue key;
    SZrTypeValue value;
    TZrBool arrayPinned = ZR_FALSE;
    TZrBool objectPinned = ZR_FALSE;

    if (state == ZR_NULL || array == ZR_NULL || object == ZR_NULL ||
        array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return ZR_FALSE;
    }
    if (!ZrCore_Object_SuperArrayMaterializeGeneric(state, array)) {
        return ZR_FALSE;
    }
    ZrCore_Value_InitAsRawObject(state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(object));
    value.type = ZR_VALUE_TYPE_OBJECT;
    if (!generic_type_object_pin_raw(state, ZR_CAST_RAW_OBJECT_AS_SUPER(array), &arrayPinned) ||
        !generic_type_object_pin_value(state, &value, &objectPinned)) {
        generic_type_object_unpin_value(state->global, &value, objectPinned);
        generic_type_object_unpin_raw(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(array), arrayPinned);
        return ZR_FALSE;
    }
    ZrCore_Value_InitAsInt(
            state, &key, (TZrInt64)ZrCore_Object_SuperArrayLength(array));
    ZrCore_Object_SetValue(state, array, &key, &value);
    generic_type_object_unpin_value(state->global, &value, objectPinned);
    generic_type_object_unpin_raw(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(array), arrayPinned);
    return ZR_TRUE;
}

static SZrObject *generic_type_object_build_literal(SZrState *state, const TZrChar *name) {
    SZrString *nameString;
    SZrObject *result = ZR_NULL;
    TZrBool namePinned = ZR_FALSE;

    nameString = generic_type_object_make_string(state, name);
    if (nameString == ZR_NULL ||
        !generic_type_object_pin_raw(state, ZR_CAST_RAW_OBJECT_AS_SUPER(nameString), &namePinned)) {
        return ZR_NULL;
    }
    result = ZrCore_Reflection_BuildTypeLiteralObject(state, nameString);
    generic_type_object_unpin_raw(state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(nameString), namePinned);
    return result;
}

static SZrObject *generic_type_object_build_metadata_literal(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        TZrUInt32 nameStringOffset,
        const TZrChar *fallbackName) {
    SZrZrpMetadataStringView stringView;
    SZrString *nameString;
    SZrObject *result = ZR_NULL;
    TZrBool namePinned = ZR_FALSE;

    if (state == ZR_NULL || runtime == ZR_NULL || fallbackName == ZR_NULL ||
        !runtime->hasZrpMetadata ||
        !ZrCore_ZrpMetadata_GetString(runtime->zrpMetadataBuffer,
                                      runtime->zrpMetadataBufferLength,
                                      &runtime->zrpMetadataHeader,
                                      nameStringOffset,
                                      &stringView)) {
        return generic_type_object_build_literal(state, fallbackName);
    }

    nameString = ZrCore_String_Create(
            state,
            (TZrNativeString)(void *)stringView.data,
            stringView.byteLength);
    if (nameString == ZR_NULL ||
        !generic_type_object_pin_raw(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(nameString), &namePinned)) {
        return ZR_NULL;
    }
    result = ZrCore_Reflection_BuildTypeLiteralObject(state, nameString);
    generic_type_object_unpin_raw(
            state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(nameString), namePinned);
    return result;
}

static const TZrChar *generic_type_object_argument_kind_name(EZrReflectionGenericTypeArgumentKind kind) {
    switch (kind) {
        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_PRIMITIVE:
            return "primitive";
        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TYPE_TOKEN:
            return "typeToken";
        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_ARRAY:
            return "array";
        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TUPLE:
            return "tuple";
        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_OWNERSHIP:
            return "ownership";
        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_NULLABLE:
            return "nullable";
        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_UNION:
            return "union";
        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_NONE:
        default:
            return ZR_NULL;
    }
}

static SZrObject *generic_type_object_begin_argument(
        SZrState *state,
        EZrReflectionGenericTypeArgumentKind kind,
        TZrBool *argumentPinned) {
    const TZrChar *kindName = generic_type_object_argument_kind_name(kind);
    SZrObject *argumentObject;

    if (state == ZR_NULL || kindName == ZR_NULL || argumentPinned == ZR_NULL) {
        return ZR_NULL;
    }
    *argumentPinned = ZR_FALSE;
    argumentObject = generic_type_object_build_literal(state, kindName);
    if (argumentObject == ZR_NULL ||
        !generic_type_object_pin_raw(state,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(argumentObject),
                                     argumentPinned) ||
        !generic_type_object_set_string(state, argumentObject, "genericArgumentKind", kindName) ||
        !generic_type_object_set_int(state, argumentObject, "genericArgumentKindValue", kind)) {
        generic_type_object_unpin_raw(state->global,
                                      argumentObject != ZR_NULL
                                              ? ZR_CAST_RAW_OBJECT_AS_SUPER(argumentObject)
                                              : ZR_NULL,
                                      *argumentPinned);
        return ZR_NULL;
    }
    return argumentObject;
}

static SZrObject *generic_type_object_build_argument(
        SZrState *state,
        const SZrReflectionGenericTypeArgument *argument,
        TZrUInt32 depth) {
    SZrObject *argumentObject;
    SZrObject *nestedObject;
    SZrObject *childrenArray;
    TZrBool success = ZR_TRUE;
    TZrBool argumentPinned = ZR_FALSE;
    TZrBool childrenPinned = ZR_FALSE;
    TZrUInt32 index;

    if (state == ZR_NULL || argument == ZR_NULL ||
        depth >= ZR_REFLECTION_GENERIC_TYPE_OBJECT_MAX_RECURSION_DEPTH ||
        generic_type_object_argument_kind_name(argument->kind) == ZR_NULL) {
        return ZR_NULL;
    }
    argumentObject = generic_type_object_begin_argument(state, argument->kind, &argumentPinned);
    if (argumentObject == ZR_NULL) {
        return ZR_NULL;
    }

    switch (argument->kind) {
        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_PRIMITIVE:
            if (!generic_type_object_set_int(state,
                                             argumentObject,
                                             "primitiveValueType",
                                             argument->primitiveValueType)) {
                success = ZR_FALSE;
            }
            break;

        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TYPE_TOKEN:
            if (!generic_type_object_set_int(state, argumentObject, "typeToken", argument->typeToken)) {
                success = ZR_FALSE;
            }
            break;

        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_ARRAY:
        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_OWNERSHIP:
        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_NULLABLE:
            nestedObject = generic_type_object_build_argument(state, argument->elementType, depth + 1u);
            if (nestedObject == ZR_NULL ||
                !generic_type_object_set_object(state,
                                                argumentObject,
                                                "elementType",
                                                nestedObject,
                                                ZR_VALUE_TYPE_OBJECT) ||
                (argument->kind == ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_ARRAY &&
                 !generic_type_object_set_int(state, argumentObject, "arrayRank", argument->arrayRank)) ||
                (argument->kind == ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_OWNERSHIP &&
                 !generic_type_object_set_int(state,
                                              argumentObject,
                                              "ownershipQualifier",
                                              argument->ownershipQualifier))) {
                success = ZR_FALSE;
            }
            break;

        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TUPLE:
        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_UNION:
            childrenArray = generic_type_object_new_array(state);
            if (childrenArray == ZR_NULL ||
                !generic_type_object_pin_raw(state,
                                             ZR_CAST_RAW_OBJECT_AS_SUPER(childrenArray),
                                             &childrenPinned)) {
                success = ZR_FALSE;
                break;
            }
            for (index = 0u; index < argument->childCount; ++index) {
                nestedObject = generic_type_object_build_argument(
                        state, &argument->childTypes[index], depth + 1u);
                if (nestedObject == ZR_NULL ||
                    !generic_type_object_array_push(state, childrenArray, nestedObject)) {
                    success = ZR_FALSE;
                    break;
                }
            }
            if (success &&
                (!generic_type_object_set_int(state, argumentObject, "childCount", argument->childCount) ||
                 (argument->kind == ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_UNION &&
                  (!generic_type_object_set_int(state,
                                                argumentObject,
                                                "unionValueType",
                                                argument->unionValueType) ||
                   !generic_type_object_set_int(state,
                                                argumentObject,
                                                "unionNameStringOffset",
                                                argument->unionNameStringOffset))) ||
                 !generic_type_object_set_object(state,
                                                 argumentObject,
                                                 "children",
                                                 childrenArray,
                                                 ZR_VALUE_TYPE_ARRAY))) {
                success = ZR_FALSE;
            }
            generic_type_object_unpin_raw(state->global,
                                          ZR_CAST_RAW_OBJECT_AS_SUPER(childrenArray),
                                          childrenPinned);
            break;

        case ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_NONE:
        default:
            success = ZR_FALSE;
            break;
    }

    generic_type_object_unpin_raw(state->global,
                                  ZR_CAST_RAW_OBJECT_AS_SUPER(argumentObject),
                                  argumentPinned);
    return success ? argumentObject : ZR_NULL;
}

static SZrObject *generic_type_object_build_metadata_node(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        const SZrZrpMetadataPoolSliceView *blob,
        const SZrMetadataRuntimeSignatureTypeNodeView *node,
        TZrUInt32 depth) {
    const SZrMetadataTokenRecord *record;
    SZrMetadataRuntimeSignatureTypeNodeView childNode;
    SZrReflectionGenericTypeArgument directArgument = {0};
    SZrObject *argumentObject;
    SZrObject *childObject;
    SZrObject *childrenArray = ZR_NULL;
    EZrReflectionGenericTypeArgumentKind kind;
    TZrBool success = ZR_TRUE;
    TZrBool argumentPinned = ZR_FALSE;
    TZrBool childrenPinned = ZR_FALSE;
    TZrUInt32 childOffset;
    TZrUInt32 index;

    if (state == ZR_NULL || runtime == ZR_NULL || blob == ZR_NULL || node == ZR_NULL ||
        depth >= ZR_REFLECTION_GENERIC_TYPE_OBJECT_MAX_RECURSION_DEPTH) {
        return ZR_NULL;
    }

    if (node->node == ZR_METADATA_SIGNATURE_NODE_PRIMITIVE) {
        if (node->payload0 <= (TZrUInt32)ZR_VALUE_TYPE_NULL ||
            node->payload0 >= (TZrUInt32)ZR_VALUE_TYPE_UNKNOWN) {
            return ZR_NULL;
        }
        directArgument.kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_PRIMITIVE;
        directArgument.primitiveValueType = node->payload0;
        return generic_type_object_build_argument(state, &directArgument, depth);
    }
    if (node->node == ZR_METADATA_SIGNATURE_NODE_TYPE_DEF ||
        node->node == ZR_METADATA_SIGNATURE_NODE_TYPE_REF ||
        node->node == ZR_METADATA_SIGNATURE_NODE_GENERIC_INST) {
        record = ZrCore_MetadataRuntime_ResolveSignatureTypeNodeRecord(runtime, blob, node);
        if (record == ZR_NULL) {
            return ZR_NULL;
        }
        directArgument.kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TYPE_TOKEN;
        directArgument.typeToken = record->token;
        return generic_type_object_build_argument(state, &directArgument, depth);
    }

    switch (node->node) {
        case ZR_METADATA_SIGNATURE_NODE_ARRAY:
            if (node->payload0 == 0u) {
                return ZR_NULL;
            }
            kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_ARRAY;
            break;
        case ZR_METADATA_SIGNATURE_NODE_TUPLE:
            if (node->childCount == 0u) {
                return ZR_NULL;
            }
            kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_TUPLE;
            break;
        case ZR_METADATA_SIGNATURE_NODE_OWNERSHIP:
            if (node->payload0 <= (TZrUInt32)ZR_REFLECTION_OWNERSHIP_QUALIFIER_NONE ||
                node->payload0 > (TZrUInt32)ZR_REFLECTION_OWNERSHIP_QUALIFIER_LOANED) {
                return ZR_NULL;
            }
            kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_OWNERSHIP;
            break;
        case ZR_METADATA_SIGNATURE_NODE_NULLABLE:
            kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_NULLABLE;
            break;
        case ZR_METADATA_SIGNATURE_NODE_UNION:
            if (node->payload0 <= (TZrUInt32)ZR_VALUE_TYPE_NULL ||
                node->payload0 >= (TZrUInt32)ZR_VALUE_TYPE_UNKNOWN || node->payload1 == 0u) {
                return ZR_NULL;
            }
            kind = ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_UNION;
            break;
        default:
            return ZR_NULL;
    }

    argumentObject = generic_type_object_begin_argument(state, kind, &argumentPinned);
    if (argumentObject == ZR_NULL) {
        return ZR_NULL;
    }

    if (kind == ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_ARRAY ||
        kind == ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_OWNERSHIP ||
        kind == ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_NULLABLE) {
        if (!ZrCore_MetadataRuntime_ReadSignatureTypeNode(blob, node->baseTypeBlobOffset, &childNode)) {
            success = ZR_FALSE;
        } else {
            childObject = generic_type_object_build_metadata_node(
                    state, runtime, blob, &childNode, depth + 1u);
            if (childObject == ZR_NULL || childNode.nextBlobOffset != node->nextBlobOffset ||
                !generic_type_object_set_object(
                        state, argumentObject, "elementType", childObject, ZR_VALUE_TYPE_OBJECT) ||
                (kind == ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_ARRAY &&
                 !generic_type_object_set_int(state, argumentObject, "arrayRank", node->payload0)) ||
                (kind == ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_OWNERSHIP &&
                 !generic_type_object_set_int(
                         state, argumentObject, "ownershipQualifier", node->payload0))) {
                success = ZR_FALSE;
            }
        }
    } else {
        childrenArray = generic_type_object_new_array(state);
        if (childrenArray == ZR_NULL ||
            !generic_type_object_pin_raw(
                    state, ZR_CAST_RAW_OBJECT_AS_SUPER(childrenArray), &childrenPinned)) {
            success = ZR_FALSE;
        } else {
            childOffset = node->childListBlobOffset;
            for (index = 0u; index < node->childCount; ++index) {
                if (!ZrCore_MetadataRuntime_ReadSignatureTypeNode(blob, childOffset, &childNode)) {
                    success = ZR_FALSE;
                    break;
                }
                childObject = generic_type_object_build_metadata_node(
                        state, runtime, blob, &childNode, depth + 1u);
                if (childObject == ZR_NULL ||
                    !generic_type_object_array_push(state, childrenArray, childObject)) {
                    success = ZR_FALSE;
                    break;
                }
                childOffset = childNode.nextBlobOffset;
            }
            if (success && childOffset != node->nextBlobOffset) {
                success = ZR_FALSE;
            }
        }
        if (success &&
            (!generic_type_object_set_int(state, argumentObject, "childCount", node->childCount) ||
             (kind == ZR_REFLECTION_GENERIC_TYPE_ARGUMENT_UNION &&
              (!generic_type_object_set_int(
                       state, argumentObject, "unionValueType", node->payload0) ||
               !generic_type_object_set_int(
                       state, argumentObject, "unionNameStringOffset", node->payload1))) ||
             !generic_type_object_set_object(
                     state, argumentObject, "children", childrenArray, ZR_VALUE_TYPE_ARRAY))) {
            success = ZR_FALSE;
        }
        generic_type_object_unpin_raw(state->global,
                                      childrenArray != ZR_NULL
                                              ? ZR_CAST_RAW_OBJECT_AS_SUPER(childrenArray)
                                              : ZR_NULL,
                                      childrenPinned);
    }

    generic_type_object_unpin_raw(
            state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(argumentObject), argumentPinned);
    return success ? argumentObject : ZR_NULL;
}

static SZrObject *generic_type_object_build_metadata_argument(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        TZrMetadataToken typeSpecToken,
        TZrUInt32 argumentIndex) {
    SZrMetadataRuntimeTypeSpecGenericArgumentView view;

    if (!ZrCore_MetadataRuntime_ReadTypeSpecGenericArgumentView(
                runtime, typeSpecToken, argumentIndex, &view)) {
        return ZR_NULL;
    }
    return generic_type_object_build_metadata_node(state,
                                                   runtime,
                                                   &view.bindingView.signatureView.blob,
                                                   &view.argumentNode,
                                                   0u);
}

static SZrObject *generic_type_object_build_method_parameter(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        const SZrMetadataRuntimeGenericParamView *view) {
    SZrObject *parameterObject;
    SZrObject *result = ZR_NULL;
    TZrBool parameterPinned = ZR_FALSE;

    if (state == ZR_NULL || runtime == ZR_NULL || view == ZR_NULL) {
        return ZR_NULL;
    }
    parameterObject = generic_type_object_build_metadata_literal(
            state,
            runtime,
            view->nameStringOffset,
            "genericMethodParameter");
    if (parameterObject == ZR_NULL ||
        !generic_type_object_pin_raw(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(parameterObject), &parameterPinned)) {
        return ZR_NULL;
    }

    if (generic_type_object_set_string(
                state, parameterObject, "kind", "genericMethodParameter") &&
        generic_type_object_set_int(
                state, parameterObject, "genericMethodToken", view->ownerToken) &&
        generic_type_object_set_int(
                state, parameterObject, "genericParameterIndex", view->parameterIndex) &&
        generic_type_object_set_int(
                state, parameterObject, "genericParameterMetadataIndex", view->genericParamIndex) &&
        generic_type_object_set_int(
                state, parameterObject, "nameStringOffset", view->nameStringOffset) &&
        generic_type_object_set_int(
                state, parameterObject, "firstConstraintIndex", view->firstConstraintIndex) &&
        generic_type_object_set_int(
                state, parameterObject, "constraintCount", view->constraintCount) &&
        generic_type_object_set_int(
                state, parameterObject, "metadataFlags", view->flags) &&
        generic_type_object_set_native_pointer(
                state, parameterObject, "metadataRuntime", runtime)) {
        result = parameterObject;
    }

    generic_type_object_unpin_raw(
            state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(parameterObject), parameterPinned);
    return result;
}

SZrObject *ZrCore_Reflection_BuildGenericMethodDefinitionObject(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        TZrMetadataToken genericMethodToken) {
    SZrMetadataRuntimeGenericOwnerView ownerView;
    SZrMetadataRuntimeGenericParamView parameterView;
    const SZrZrpMetadataMethodDefRow *methodDefRow;
    SZrObject *methodObject;
    SZrObject *parametersArray = ZR_NULL;
    SZrObject *parameterObject;
    SZrObject *result = ZR_NULL;
    TZrBool methodPinned = ZR_FALSE;
    TZrBool parametersPinned = ZR_FALSE;
    TZrUInt32 index;

    if (state == ZR_NULL || runtime == ZR_NULL ||
        !ZrCore_MetadataRuntime_ReadGenericOwnerView(
                runtime, genericMethodToken, &ownerView) ||
        ownerView.methodDefRow == ZR_NULL ||
        ownerView.genericParamCount == 0u) {
        return ZR_NULL;
    }
    methodDefRow = ownerView.methodDefRow;
    methodObject = generic_type_object_build_metadata_literal(
            state,
            runtime,
            methodDefRow->nameStringOffset,
            "genericMethodDefinition");
    if (methodObject == ZR_NULL ||
        !generic_type_object_pin_raw(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(methodObject), &methodPinned)) {
        return ZR_NULL;
    }
    parametersArray = generic_type_object_new_array(state);
    if (parametersArray == ZR_NULL ||
        !generic_type_object_pin_raw(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(parametersArray), &parametersPinned)) {
        goto cleanup;
    }

    for (index = 0u; index < ownerView.genericParamCount; ++index) {
        if (index > ~(TZrUInt32)0u - ownerView.firstGenericParamIndex ||
            !ZrCore_MetadataRuntime_ReadGenericParamView(
                    runtime, genericMethodToken, index, &parameterView) ||
            parameterView.ownerRecord != ownerView.ownerRecord ||
            parameterView.genericParamIndex != ownerView.firstGenericParamIndex + index ||
            parameterView.parameterIndex != index) {
            goto cleanup;
        }
        parameterObject = generic_type_object_build_method_parameter(
                state, runtime, &parameterView);
        if (parameterObject == ZR_NULL ||
            !generic_type_object_array_push(state, parametersArray, parameterObject)) {
            goto cleanup;
        }
    }

    if (!generic_type_object_set_string(
                state, methodObject, "kind", "genericMethodDefinition") ||
        !generic_type_object_set_bool(state, methodObject, "isGenericMethod", ZR_TRUE) ||
        !generic_type_object_set_bool(
                state, methodObject, "isGenericMethodDefinition", ZR_TRUE) ||
        !generic_type_object_set_bool(
                state, methodObject, "isConstructedGenericMethod", ZR_FALSE) ||
        !generic_type_object_set_int(
                state, methodObject, "metadataToken", genericMethodToken) ||
        !generic_type_object_set_int(
                state, methodObject, "genericMethodToken", genericMethodToken) ||
        !generic_type_object_set_int(
                state, methodObject, "declaringTypeToken", methodDefRow->ownerTypeToken) ||
        !generic_type_object_set_int(
                state, methodObject, "nameStringOffset", methodDefRow->nameStringOffset) ||
        !generic_type_object_set_int(
                state, methodObject, "signatureBlobOffset", methodDefRow->signatureBlobOffset) ||
        !generic_type_object_set_int(
                state, methodObject, "signatureBlobLength", methodDefRow->signatureBlobLength) ||
        !generic_type_object_set_int(
                state, methodObject, "metadataFlags", methodDefRow->flags) ||
        !generic_type_object_set_int(
                state, methodObject, "genericParameterCount", ownerView.genericParamCount) ||
        !generic_type_object_set_int(
                state, methodObject, "genericArgumentCount", ownerView.genericParamCount) ||
        !generic_type_object_set_native_pointer(
                state, methodObject, "metadataRuntime", runtime) ||
        !generic_type_object_set_object(
                state,
                methodObject,
                "genericParameters",
                parametersArray,
                ZR_VALUE_TYPE_ARRAY)) {
        goto cleanup;
    }
    result = methodObject;

cleanup:
    generic_type_object_unpin_raw(
            state->global,
            parametersArray != ZR_NULL
                    ? ZR_CAST_RAW_OBJECT_AS_SUPER(parametersArray)
                    : ZR_NULL,
            parametersPinned);
    generic_type_object_unpin_raw(
            state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(methodObject), methodPinned);
    return result;
}

SZrObject *ZrCore_Reflection_BuildMethodSpecGenericContextObject(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        TZrMetadataToken methodSpecToken) {
    SZrMetadataRuntimeMethodSpecSignatureView signatureView;
    SZrMetadataRuntimeMethodSpecGenericArgumentView argumentView;
    SZrObject *contextObject;
    SZrObject *argumentsArray = ZR_NULL;
    SZrObject *argumentObject;
    SZrObject *result = ZR_NULL;
    TZrBool contextPinned = ZR_FALSE;
    TZrBool argumentsPinned = ZR_FALSE;
    TZrUInt32 index;

    if (state == ZR_NULL || runtime == ZR_NULL ||
        !ZrCore_MetadataRuntime_ReadMethodSpecSignatureView(
                runtime, methodSpecToken, &signatureView) ||
        signatureView.argumentCount == 0u) {
        return ZR_NULL;
    }

    contextObject = generic_type_object_build_literal(state, "constructedGenericMethod");
    if (contextObject == ZR_NULL ||
        !generic_type_object_pin_raw(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(contextObject), &contextPinned)) {
        return ZR_NULL;
    }
    argumentsArray = generic_type_object_new_array(state);
    if (argumentsArray == ZR_NULL ||
        !generic_type_object_pin_raw(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(argumentsArray), &argumentsPinned)) {
        goto cleanup;
    }

    for (index = 0u; index < signatureView.argumentCount; ++index) {
        if (!ZrCore_MetadataRuntime_ReadMethodSpecGenericArgumentView(
                    runtime, methodSpecToken, index, &argumentView)) {
            goto cleanup;
        }
        argumentObject = generic_type_object_build_metadata_node(
                state,
                runtime,
                &argumentView.signatureView.blob,
                &argumentView.argumentNode,
                0u);
        if (argumentObject == ZR_NULL ||
            !generic_type_object_array_push(state, argumentsArray, argumentObject)) {
            goto cleanup;
        }
    }

    if (!generic_type_object_set_string(
                state, contextObject, "kind", "genericMethodContext") ||
        !generic_type_object_set_bool(state, contextObject, "isGenericMethod", ZR_TRUE) ||
        !generic_type_object_set_bool(
                state, contextObject, "isConstructedGenericMethod", ZR_TRUE) ||
        !generic_type_object_set_int(
                state, contextObject, "metadataToken", methodSpecToken) ||
        !generic_type_object_set_int(
                state, contextObject, "genericMethodToken", signatureView.methodToken) ||
        !generic_type_object_set_uint(
                state, contextObject, "genericSignatureHash", signatureView.signatureHash) ||
        !generic_type_object_set_int(
                state,
                contextObject,
                "genericArgumentCount",
                signatureView.argumentCount) ||
        !generic_type_object_set_native_pointer(
                state, contextObject, "metadataRuntime", runtime) ||
        !generic_type_object_set_object(
                state,
                contextObject,
                "genericArguments",
                argumentsArray,
                ZR_VALUE_TYPE_ARRAY)) {
        goto cleanup;
    }
    result = contextObject;

cleanup:
    generic_type_object_unpin_raw(
            state->global,
            argumentsArray != ZR_NULL
                    ? ZR_CAST_RAW_OBJECT_AS_SUPER(argumentsArray)
                    : ZR_NULL,
            argumentsPinned);
    generic_type_object_unpin_raw(
            state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(contextObject), contextPinned);
    return result;
}

SZrObject *ZrCore_Reflection_BuildDynamicGenericTypeInstanceObject(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        const SZrReflectionDynamicGenericTypeInstance *instance) {
    SZrReflectionDynamicGenericTypeInstance resolved;
    SZrObject *typeObject;
    SZrObject *argumentsArray;
    SZrObject *argumentObject;
    SZrObject *result = ZR_NULL;
    TZrBool typeObjectPinned = ZR_FALSE;
    TZrBool argumentsPinned = ZR_FALSE;
    const TZrChar *routeName;
    TZrUInt32 index;

    if (state == ZR_NULL || runtime == ZR_NULL || instance == ZR_NULL ||
        instance->genericArgumentCount == 0u ||
        !ZrCore_Reflection_RevalidateDynamicGenericTypeInstance(runtime, instance, &resolved)) {
        return ZR_NULL;
    }

    routeName = resolved.route == ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_AOT
                        ? "aot"
                        : "interpreter-deopt";
    typeObject = generic_type_object_build_literal(state, "constructedGeneric");
    if (typeObject == ZR_NULL ||
        !generic_type_object_pin_raw(state,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(typeObject),
                                     &typeObjectPinned)) {
        return ZR_NULL;
    }
    argumentsArray = generic_type_object_new_array(state);
    if (argumentsArray == ZR_NULL ||
        !generic_type_object_pin_raw(state,
                                     ZR_CAST_RAW_OBJECT_AS_SUPER(argumentsArray),
                                     &argumentsPinned)) {
        generic_type_object_unpin_raw(state != ZR_NULL ? state->global : ZR_NULL,
                                      argumentsArray != ZR_NULL
                                              ? ZR_CAST_RAW_OBJECT_AS_SUPER(argumentsArray)
                                              : ZR_NULL,
                                      argumentsPinned);
        generic_type_object_unpin_raw(state->global,
                                      ZR_CAST_RAW_OBJECT_AS_SUPER(typeObject),
                                      typeObjectPinned);
        return ZR_NULL;
    }

    for (index = 0u; index < resolved.genericArgumentCount; ++index) {
        argumentObject = resolved.requestedArguments != ZR_NULL
                                 ? generic_type_object_build_argument(
                                           state, &resolved.requestedArguments[index], 0u)
                                 : generic_type_object_build_metadata_argument(
                                           state, runtime, resolved.typeSpecToken, index);
        if (argumentObject == ZR_NULL ||
            !generic_type_object_array_push(state, argumentsArray, argumentObject)) {
            result = ZR_NULL;
            break;
        }
    }

    if (index == resolved.genericArgumentCount) {
        result = typeObject;
    }
    if (result != ZR_NULL &&
        (!generic_type_object_set_bool(state, typeObject, "isGenericType", ZR_TRUE) ||
         !generic_type_object_set_bool(state, typeObject, "isConstructedGenericType", ZR_TRUE) ||
         !generic_type_object_set_bool(state,
                                       typeObject,
                                       "isAotCollected",
                                       resolved.route == ZR_REFLECTION_GENERIC_INSTANCE_ROUTE_AOT) ||
         !generic_type_object_set_string(state, typeObject, "genericInstanceRoute", routeName) ||
         !generic_type_object_set_int(state, typeObject, "metadataToken", resolved.typeSpecToken) ||
         !generic_type_object_set_int(state, typeObject, "genericBaseToken", resolved.genericBaseToken) ||
         !generic_type_object_set_int(state,
                                      typeObject,
                                      "genericArgumentCount",
                                      resolved.genericArgumentCount) ||
         !generic_type_object_set_int(
                 state,
                 typeObject,
                 "typeLayoutId",
                 resolved.typeLayoutId == ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE
                         ? -1
                         : (TZrInt64)resolved.typeLayoutId) ||
         !generic_type_object_set_native_pointer(state, typeObject, "metadataRuntime", runtime) ||
         !generic_type_object_set_object(state,
                                         typeObject,
                                         "genericArguments",
                                         argumentsArray,
                                         ZR_VALUE_TYPE_ARRAY))) {
        result = ZR_NULL;
    }

    generic_type_object_unpin_raw(state->global,
                                  ZR_CAST_RAW_OBJECT_AS_SUPER(argumentsArray),
                                  argumentsPinned);
    generic_type_object_unpin_raw(state->global,
                                  ZR_CAST_RAW_OBJECT_AS_SUPER(typeObject),
                                  typeObjectPinned);
    return result;
}

SZrObject *ZrCore_Reflection_MakeGenericTypeObject(
        SZrState *state,
        SZrMetadataRuntime *runtime,
        TZrMetadataToken genericBaseToken,
        const SZrReflectionGenericTypeArgument *arguments,
        TZrUInt32 argumentCount) {
    SZrReflectionDynamicGenericTypeInstance instance;

    if (state == ZR_NULL || runtime == ZR_NULL ||
        !ZrCore_Reflection_ResolveConstructedGenericType(runtime,
                                                        genericBaseToken,
                                                        arguments,
                                                        argumentCount,
                                                        &instance)) {
        return ZR_NULL;
    }
    return ZrCore_Reflection_BuildDynamicGenericTypeInstanceObject(state, runtime, &instance);
}
