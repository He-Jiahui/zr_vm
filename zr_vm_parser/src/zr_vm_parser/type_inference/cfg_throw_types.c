#include "cfg_internal.h"

#include <string.h>

TZrUInt32 cfg_throw_kind_mask(EZrParserCfgThrowKind kind) {
    if (kind == ZR_PARSER_CFG_THROW_KIND_UNKNOWN) {
        return 0u;
    }
    return (TZrUInt32)(1u << (TZrUInt32)kind);
}

TZrBool cfg_throw_kind_mask_has_single_kind(TZrUInt32 mask,
                                            EZrParserCfgThrowKind *outKind) {
    EZrParserCfgThrowKind kind;

    if (outKind != ZR_NULL) {
        *outKind = ZR_PARSER_CFG_THROW_KIND_UNKNOWN;
    }
    if (mask == 0u || (mask & (mask - 1u)) != 0u || outKind == ZR_NULL) {
        return ZR_FALSE;
    }

    for (kind = ZR_PARSER_CFG_THROW_KIND_BOOL;
         kind <= ZR_PARSER_CFG_THROW_KIND_FLOAT;
         kind = (EZrParserCfgThrowKind)((TZrUInt32)kind + 1u)) {
        if (mask == cfg_throw_kind_mask(kind)) {
            *outKind = kind;
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}

static TZrBool cfg_string_native_view(SZrString *value,
                                      TZrNativeString *outNative,
                                      TZrSize *outLength) {
    if (outNative != ZR_NULL) {
        *outNative = ZR_NULL;
    }
    if (outLength != ZR_NULL) {
        *outLength = 0;
    }
    if (value == ZR_NULL || outNative == ZR_NULL || outLength == ZR_NULL) {
        return ZR_FALSE;
    }

    if (value->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        *outNative = ZrCore_String_GetNativeStringShort(value);
        *outLength = value->shortStringLength;
    } else {
        *outNative = ZrCore_String_GetNativeString(value);
        *outLength = *outNative != ZR_NULL ? value->longStringLength : 0;
    }

    return (TZrBool)(*outNative != ZR_NULL);
}

TZrBool cfg_string_equals(SZrString *left, SZrString *right) {
    TZrNativeString leftNative;
    TZrNativeString rightNative;
    TZrSize leftLength;
    TZrSize rightLength;

    if (!cfg_string_native_view(left, &leftNative, &leftLength) ||
        !cfg_string_native_view(right, &rightNative, &rightLength)) {
        return ZR_FALSE;
    }

    return (TZrBool)(leftLength == rightLength &&
                     memcmp(leftNative, rightNative, leftLength) == 0);
}

static TZrBool cfg_string_equals_literal(SZrString *value, const TZrChar *literal) {
    TZrNativeString nativeValue;
    TZrSize nativeLength;
    TZrSize literalLength;

    if (value == ZR_NULL || literal == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!cfg_string_native_view(value, &nativeValue, &nativeLength)) {
        return ZR_FALSE;
    }

    literalLength = strlen(literal);
    return (TZrBool)(nativeLength == literalLength &&
                     memcmp(nativeValue, literal, literalLength) == 0);
}

SZrString *cfg_type_info_simple_name(SZrType *typeInfo) {
    if (typeInfo == ZR_NULL) {
        return ZR_NULL;
    }

    while (typeInfo->subType != ZR_NULL) {
        typeInfo = typeInfo->subType;
    }

    if (typeInfo->name != ZR_NULL &&
        typeInfo->name->type == ZR_AST_IDENTIFIER_LITERAL) {
        return typeInfo->name->data.identifier.name;
    }

    return ZR_NULL;
}

TZrBool cfg_throw_kind_matches_type_name(EZrParserCfgThrowKind kind,
                                         SZrString *typeName) {
    switch (kind) {
        case ZR_PARSER_CFG_THROW_KIND_BOOL:
            return (TZrBool)(cfg_string_equals_literal(typeName, "bool") ||
                             cfg_string_equals_literal(typeName, "Bool"));
        case ZR_PARSER_CFG_THROW_KIND_INTEGER:
            return (TZrBool)(cfg_string_equals_literal(typeName, "int") ||
                             cfg_string_equals_literal(typeName, "uint") ||
                             cfg_string_equals_literal(typeName, "i8") ||
                             cfg_string_equals_literal(typeName, "u8") ||
                             cfg_string_equals_literal(typeName, "i16") ||
                             cfg_string_equals_literal(typeName, "u16") ||
                             cfg_string_equals_literal(typeName, "i32") ||
                             cfg_string_equals_literal(typeName, "u32") ||
                             cfg_string_equals_literal(typeName, "i64") ||
                             cfg_string_equals_literal(typeName, "u64") ||
                             cfg_string_equals_literal(typeName, "Integer") ||
                             cfg_string_equals_literal(typeName, "UInt64") ||
                             cfg_string_equals_literal(typeName, "Byte"));
        case ZR_PARSER_CFG_THROW_KIND_STRING:
            return (TZrBool)(cfg_string_equals_literal(typeName, "string") ||
                             cfg_string_equals_literal(typeName, "String"));
        case ZR_PARSER_CFG_THROW_KIND_CHAR:
            return (TZrBool)(cfg_string_equals_literal(typeName, "char") ||
                             cfg_string_equals_literal(typeName, "Char"));
        case ZR_PARSER_CFG_THROW_KIND_FLOAT:
            return (TZrBool)(cfg_string_equals_literal(typeName, "float") ||
                             cfg_string_equals_literal(typeName, "double") ||
                             cfg_string_equals_literal(typeName, "Float") ||
                             cfg_string_equals_literal(typeName, "Double"));
        default:
            return ZR_FALSE;
    }
}

TZrBool cfg_type_name_throw_kind(SZrString *typeName,
                                 EZrParserCfgThrowKind *outKind) {
    EZrParserCfgThrowKind kind;

    if (outKind != ZR_NULL) {
        *outKind = ZR_PARSER_CFG_THROW_KIND_UNKNOWN;
    }
    if (typeName == ZR_NULL || outKind == ZR_NULL) {
        return ZR_FALSE;
    }

    for (kind = ZR_PARSER_CFG_THROW_KIND_BOOL;
         kind <= ZR_PARSER_CFG_THROW_KIND_FLOAT;
         kind = (EZrParserCfgThrowKind)((TZrUInt32)kind + 1u)) {
        if (cfg_throw_kind_matches_type_name(kind, typeName)) {
            *outKind = kind;
            return ZR_TRUE;
        }
    }

    return ZR_FALSE;
}
