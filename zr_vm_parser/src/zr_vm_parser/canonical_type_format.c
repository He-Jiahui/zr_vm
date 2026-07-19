#include "zr_vm_parser/canonical_type.h"
#include "zr_vm_parser/semantic.h"

#include "zr_vm_core/array.h"

#include <stdio.h>
#include <string.h>

#define ZR_CANONICAL_TYPE_FORMAT_MAX_DEPTH ((TZrSize)256U)

typedef struct SZrCanonicalTypeFormatState {
    const SZrSemanticContext *context;
    TZrChar *buffer;
    TZrSize bufferSize;
    TZrSize offset;
} SZrCanonicalTypeFormatState;

static TZrBool canonical_type_format_append(
        SZrCanonicalTypeFormatState *state,
        const TZrChar *text) {
    TZrSize length;

    if (state == ZR_NULL || text == ZR_NULL) {
        return ZR_FALSE;
    }
    length = strlen(text);
    if (state->offset + length >= state->bufferSize) {
        return ZR_FALSE;
    }
    memcpy(state->buffer + state->offset, text, length);
    state->offset += length;
    state->buffer[state->offset] = '\0';
    return ZR_TRUE;
}

static TZrBool canonical_type_format_append_string(
        SZrCanonicalTypeFormatState *state,
        const SZrString *text) {
    TZrNativeString nativeText;
    TZrSize length;

    if (state == ZR_NULL || text == ZR_NULL) {
        return ZR_FALSE;
    }
    nativeText = ZrCore_String_GetNativeString(text);
    length = ZrCore_String_GetByteLength(text);
    if (nativeText == ZR_NULL || state->offset + length >= state->bufferSize) {
        return ZR_FALSE;
    }
    memcpy(state->buffer + state->offset, nativeText, length);
    state->offset += length;
    state->buffer[state->offset] = '\0';
    return ZR_TRUE;
}

static const TZrChar *canonical_type_format_primitive_name(EZrValueType valueType) {
    switch (valueType) {
        case ZR_VALUE_TYPE_NULL: return "null";
        case ZR_VALUE_TYPE_BOOL: return "bool";
        case ZR_VALUE_TYPE_INT8: return "i8";
        case ZR_VALUE_TYPE_INT16: return "i16";
        case ZR_VALUE_TYPE_INT32: return "i32";
        case ZR_VALUE_TYPE_INT64: return "int";
        case ZR_VALUE_TYPE_UINT8: return "u8";
        case ZR_VALUE_TYPE_UINT16: return "u16";
        case ZR_VALUE_TYPE_UINT32: return "u32";
        case ZR_VALUE_TYPE_UINT64: return "uint";
        case ZR_VALUE_TYPE_FLOAT: return "float";
        case ZR_VALUE_TYPE_DOUBLE: return "double";
        case ZR_VALUE_TYPE_STRING: return "string";
        case ZR_VALUE_TYPE_BUFFER: return "buffer";
        case ZR_VALUE_TYPE_ARRAY: return "array";
        case ZR_VALUE_TYPE_FUNCTION: return "function";
        case ZR_VALUE_TYPE_CLOSURE_VALUE: return "closure-value";
        case ZR_VALUE_TYPE_CLOSURE: return "closure";
        case ZR_VALUE_TYPE_OBJECT: return "object";
        case ZR_VALUE_TYPE_THREAD: return "thread";
        case ZR_VALUE_TYPE_NATIVE_POINTER: return "native-pointer";
        case ZR_VALUE_TYPE_NATIVE_DATA: return "native-data";
        case ZR_VALUE_TYPE_VM_MEMORY: return "vm-memory";
        case ZR_VALUE_TYPE_UNKNOWN: return "unknown";
        default: return ZR_NULL;
    }
}

static TZrBool canonical_type_format_node(
        SZrCanonicalTypeFormatState *state,
        TZrTypeId typeId,
        TZrSize depth);

static TZrBool canonical_type_format_id_array(
        SZrCanonicalTypeFormatState *state,
        const SZrArray *typeIds,
        const TZrChar *separator,
        TZrSize depth) {
    TZrSize index;

    if (state == ZR_NULL || typeIds == ZR_NULL || separator == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = 0; index < typeIds->length; index++) {
        const TZrTypeId *typeId = (const TZrTypeId *)ZrCore_Array_Get((SZrArray *)typeIds, index);
        if ((index > 0 && !canonical_type_format_append(state, separator)) ||
            typeId == ZR_NULL ||
            !canonical_type_format_node(state, *typeId, depth + 1U)) {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool canonical_type_format_generic_arguments(
        SZrCanonicalTypeFormatState *state,
        const SZrArray *arguments,
        TZrSize depth) {
    TZrSize index;

    if (state == ZR_NULL || arguments == ZR_NULL) {
        return ZR_FALSE;
    }
    for (index = 0; index < arguments->length; index++) {
        const SZrCanonicalGenericArgument *argument =
                (const SZrCanonicalGenericArgument *)ZrCore_Array_Get(
                        (SZrArray *)arguments,
                        index);
        TZrChar constBuffer[32];

        if ((index > 0U && !canonical_type_format_append(state, ", ")) ||
            argument == ZR_NULL) {
            return ZR_FALSE;
        }
        if (argument->kind == ZR_CANONICAL_GENERIC_ARGUMENT_TYPE) {
            if (!canonical_type_format_node(state, argument->data.typeId, depth + 1U)) {
                return ZR_FALSE;
            }
        } else if (argument->kind == ZR_CANONICAL_GENERIC_ARGUMENT_CONST_INT) {
            if (snprintf(
                        constBuffer,
                        sizeof(constBuffer),
                        "%lld",
                        (long long)argument->data.constIntValue) < 0 ||
                !canonical_type_format_append(state, constBuffer)) {
                return ZR_FALSE;
            }
        } else if (argument->kind == ZR_CANONICAL_GENERIC_ARGUMENT_CONST_PARAMETER) {
            if (argument->data.constParameter.displayName != ZR_NULL) {
                if (!canonical_type_format_append(
                            state,
                            ZrCore_String_GetNativeString(
                                    argument->data.constParameter.displayName))) {
                    return ZR_FALSE;
                }
            } else if (snprintf(
                               constBuffer,
                               sizeof(constBuffer),
                               "$const(%u,%u)",
                               (unsigned int)argument->data.constParameter.ownerSymbolId,
                               (unsigned int)argument->data.constParameter.ordinal) < 0 ||
                       !canonical_type_format_append(state, constBuffer)) {
                return ZR_FALSE;
            }
        } else {
            return ZR_FALSE;
        }
    }
    return ZR_TRUE;
}

static TZrBool canonical_type_format_nominal(
        SZrCanonicalTypeFormatState *state,
        const SZrCanonicalNominalType *nominal) {
    if (state == ZR_NULL || nominal == ZR_NULL || nominal->name == ZR_NULL) {
        return ZR_FALSE;
    }
    if (nominal->moduleIdentity != ZR_NULL &&
        ZrCore_String_GetByteLength(nominal->moduleIdentity) > 0 &&
        (!canonical_type_format_append_string(state, nominal->moduleIdentity) ||
         !canonical_type_format_append(state, "."))) {
        return ZR_FALSE;
    }
    return canonical_type_format_append_string(state, nominal->name);
}

static TZrBool canonical_type_format_generic_parameter(
        SZrCanonicalTypeFormatState *state,
        const SZrCanonicalGenericParameterType *parameter) {
    TZrChar text[64];
    TZrInt32 written;

    if (state == ZR_NULL || parameter == ZR_NULL) {
        return ZR_FALSE;
    }
    written = snprintf(
            text,
            sizeof(text),
            "!%u:%u",
            (unsigned int)parameter->ownerSymbolId,
            (unsigned int)parameter->ordinal);
    return written > 0 &&
           (TZrSize)written < sizeof(text) &&
           canonical_type_format_append(state, text);
}

static TZrBool canonical_type_format_array(
        SZrCanonicalTypeFormatState *state,
        const SZrCanonicalArrayType *arrayType,
        TZrSize depth) {
    TZrUInt32 rank;

    if (state == ZR_NULL || arrayType == ZR_NULL) {
        return ZR_FALSE;
    }
    if (arrayType->storageKind == ZR_CANONICAL_ARRAY_STORAGE_INLINE &&
        !canonical_type_format_append(state, "inline ")) {
        return ZR_FALSE;
    }
    if (arrayType->storageKind == ZR_CANONICAL_ARRAY_STORAGE_NATIVE &&
        !canonical_type_format_append(state, "native ")) {
        return ZR_FALSE;
    }
    if (!canonical_type_format_node(state, arrayType->elementTypeId, depth + 1U)) {
        return ZR_FALSE;
    }
    if (arrayType->rank == 1U) {
        return canonical_type_format_append(state, "[]");
    }
    if (!canonical_type_format_append(state, "[")) {
        return ZR_FALSE;
    }
    for (rank = 1U; rank < arrayType->rank; rank++) {
        if (!canonical_type_format_append(state, ",")) {
            return ZR_FALSE;
        }
    }
    return canonical_type_format_append(state, "]");
}

static TZrBool canonical_type_format_ref(
        SZrCanonicalTypeFormatState *state,
        const SZrCanonicalRefType *refType,
        TZrSize depth) {
    return canonical_type_format_append(
                   state,
                   refType->access == ZR_CANONICAL_REF_READONLY ? "ref readonly " : "ref ") &&
           canonical_type_format_node(state, refType->pointeeTypeId, depth + 1U);
}

static TZrBool canonical_type_format_owner(
        SZrCanonicalTypeFormatState *state,
        const SZrCanonicalOwnerType *owner,
        TZrSize depth) {
    const TZrChar *name;

    switch (owner->ownerKind) {
        case ZR_CANONICAL_OWNER_UNIQUE: name = "Unique<"; break;
        case ZR_CANONICAL_OWNER_SHARED: name = "Shared<"; break;
        case ZR_CANONICAL_OWNER_WEAK: name = "Weak<"; break;
        case ZR_CANONICAL_OWNER_ATOMIC_SHARED: name = "AtomicShared<"; break;
        default: return ZR_FALSE;
    }
    return canonical_type_format_append(state, name) &&
           canonical_type_format_node(state, owner->targetTypeId, depth + 1U) &&
           canonical_type_format_append(state, ">");
}

static TZrBool canonical_type_format_parameter(
        SZrCanonicalTypeFormatState *state,
        const SZrCanonicalParameterContract *parameter,
        TZrSize depth) {
    const SZrCanonicalTypeNode *typeNode;
    TZrTypeId displayTypeId;
    const TZrChar *prefix = "";

    if (state == ZR_NULL || parameter == ZR_NULL) {
        return ZR_FALSE;
    }
    typeNode = ZrParser_CanonicalType_Find(state->context, parameter->typeId);
    displayTypeId = parameter->typeId;
    if (typeNode != ZR_NULL && typeNode->kind == ZR_CANONICAL_TYPE_REF) {
        displayTypeId = typeNode->data.refType.pointeeTypeId;
    }

    switch (parameter->passingForm) {
        case ZR_CANONICAL_PASSING_VALUE:
            displayTypeId = parameter->typeId;
            break;
        case ZR_CANONICAL_PASSING_IN:
            prefix = "in ";
            break;
        case ZR_CANONICAL_PASSING_REF:
            prefix = parameter->escapeUpperBound == ZR_CANONICAL_ESCAPE_FUNCTION
                             ? "scoped ref "
                             : "ref ";
            break;
        case ZR_CANONICAL_PASSING_REF_READONLY:
            prefix = parameter->escapeUpperBound == ZR_CANONICAL_ESCAPE_FUNCTION
                             ? "scoped ref readonly "
                             : "ref readonly ";
            break;
        case ZR_CANONICAL_PASSING_OUT:
            prefix = "out ";
            break;
        default:
            return ZR_FALSE;
    }
    return canonical_type_format_append(state, prefix) &&
           canonical_type_format_node(state, displayTypeId, depth + 1U);
}

static TZrBool canonical_type_format_function(
        SZrCanonicalTypeFormatState *state,
        const SZrCanonicalFunctionType *function,
        TZrSize depth) {
    TZrSize index;

    if ((function->effectFlags & ZR_CANONICAL_CALLABLE_EFFECT_ASYNC) != 0U &&
        !canonical_type_format_append(state, "async ")) {
        return ZR_FALSE;
    }
    if ((function->effectFlags & ZR_CANONICAL_CALLABLE_EFFECT_GENERATOR) != 0U &&
        !canonical_type_format_append(state, "generator ")) {
        return ZR_FALSE;
    }
    if (function->receiverEffect == ZR_CANONICAL_RECEIVER_READONLY &&
        !canonical_type_format_append(state, "const ")) {
        return ZR_FALSE;
    }
    if (!canonical_type_format_append(state, "fn(")) {
        return ZR_FALSE;
    }
    for (index = 0; index < function->parameterContracts.length; index++) {
        const SZrCanonicalParameterContract *parameter =
                (const SZrCanonicalParameterContract *)ZrCore_Array_Get(
                        (SZrArray *)&function->parameterContracts,
                        index);
        if ((index > 0 && !canonical_type_format_append(state, ", ")) ||
            !canonical_type_format_parameter(state, parameter, depth + 1U)) {
            return ZR_FALSE;
        }
    }
    if (!canonical_type_format_append(state, ") -> ") ||
        !canonical_type_format_node(state, function->returnTypeId, depth + 1U)) {
        return ZR_FALSE;
    }
    if ((function->effectFlags & ZR_CANONICAL_CALLABLE_EFFECT_THROWS) != 0U &&
        !canonical_type_format_append(state, " throws")) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool canonical_type_format_node(
        SZrCanonicalTypeFormatState *state,
        TZrTypeId typeId,
        TZrSize depth) {
    const SZrCanonicalTypeNode *node;

    if (state == ZR_NULL || depth > ZR_CANONICAL_TYPE_FORMAT_MAX_DEPTH) {
        return ZR_FALSE;
    }
    node = ZrParser_CanonicalType_Find(state->context, typeId);
    if (node == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (node->kind) {
        case ZR_CANONICAL_TYPE_PRIMITIVE:
            return canonical_type_format_append(
                    state,
                    canonical_type_format_primitive_name(node->data.primitive.valueType));
        case ZR_CANONICAL_TYPE_NOMINAL:
            return canonical_type_format_nominal(state, &node->data.nominal);
        case ZR_CANONICAL_TYPE_GENERIC_PARAMETER:
            return canonical_type_format_generic_parameter(state, &node->data.genericParameter);
        case ZR_CANONICAL_TYPE_GENERIC_INSTANCE:
            return canonical_type_format_node(state, node->data.genericInstance.definitionTypeId, depth + 1U) &&
                   canonical_type_format_append(state, "<") &&
                   canonical_type_format_generic_arguments(
                           state,
                           &node->data.genericInstance.arguments,
                           depth + 1U) &&
                   canonical_type_format_append(state, ">");
        case ZR_CANONICAL_TYPE_ARRAY:
            return canonical_type_format_array(state, &node->data.array, depth);
        case ZR_CANONICAL_TYPE_TUPLE:
            return canonical_type_format_append(state, "(") &&
                   canonical_type_format_id_array(
                           state,
                           &node->data.typeList.elementTypeIds,
                           ", ",
                           depth + 1U) &&
                   canonical_type_format_append(state, ")");
        case ZR_CANONICAL_TYPE_UNION:
            return canonical_type_format_node(state, node->data.unionType.definitionTypeId, depth + 1U) &&
                   canonical_type_format_append(state, "{") &&
                   canonical_type_format_id_array(
                           state,
                           &node->data.unionType.variantTypeIds,
                           " | ",
                           depth + 1U) &&
                   canonical_type_format_append(state, "}");
        case ZR_CANONICAL_TYPE_ERROR:
            return canonical_type_format_append(state, "<error>");
        case ZR_CANONICAL_TYPE_NEVER:
            return canonical_type_format_append(state, "never");
        case ZR_CANONICAL_TYPE_REF:
            return canonical_type_format_ref(state, &node->data.refType, depth);
        case ZR_CANONICAL_TYPE_OWNER:
            return canonical_type_format_owner(state, &node->data.owner, depth);
        case ZR_CANONICAL_TYPE_READONLY_VIEW:
            return canonical_type_format_append(state, "readonly ") &&
                   canonical_type_format_node(state, node->data.target.targetTypeId, depth + 1U);
        case ZR_CANONICAL_TYPE_NULLABLE:
            return canonical_type_format_node(state, node->data.target.targetTypeId, depth + 1U) &&
                   canonical_type_format_append(state, "?");
        case ZR_CANONICAL_TYPE_FUNCTION:
            return canonical_type_format_function(state, &node->data.function, depth);
        default:
            return ZR_FALSE;
    }
}

TZrBool ZrParser_CanonicalType_Format(
        const SZrSemanticContext *context,
        TZrTypeId typeId,
        TZrChar *buffer,
        TZrSize bufferSize) {
    SZrCanonicalTypeFormatState state;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }
    buffer[0] = '\0';
    if (context == ZR_NULL || typeId == ZR_SEMANTIC_ID_INVALID) {
        return ZR_FALSE;
    }

    state.context = context;
    state.buffer = buffer;
    state.bufferSize = bufferSize;
    state.offset = 0;
    if (!canonical_type_format_node(&state, typeId, 0U)) {
        buffer[0] = '\0';
        return ZR_FALSE;
    }
    return ZR_TRUE;
}
