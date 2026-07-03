//
// Primitive raw POD FieldInfo value marshaling helpers.
//

#include "reflection_field_value_primitive.h"

#include "zr_vm_core/state.h"
#include "zr_vm_core/type_layout.h"

#include <float.h>
#include <stdint.h>
#include <string.h>

static TZrBool reflection_field_value_primitive_byte_size(EZrValueType valueType, TZrUInt32 *outByteSize) {
    TZrUInt32 byteSize;

    if (outByteSize == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (valueType) {
        case ZR_VALUE_TYPE_BOOL:
            byteSize = (TZrUInt32)sizeof(TZrBool);
            break;
        case ZR_VALUE_TYPE_INT8:
            byteSize = (TZrUInt32)sizeof(TZrInt8);
            break;
        case ZR_VALUE_TYPE_INT16:
            byteSize = (TZrUInt32)sizeof(TZrInt16);
            break;
        case ZR_VALUE_TYPE_INT32:
            byteSize = (TZrUInt32)sizeof(TZrInt32);
            break;
        case ZR_VALUE_TYPE_INT64:
            byteSize = (TZrUInt32)sizeof(TZrInt64);
            break;
        case ZR_VALUE_TYPE_UINT8:
            byteSize = (TZrUInt32)sizeof(TZrUInt8);
            break;
        case ZR_VALUE_TYPE_UINT16:
            byteSize = (TZrUInt32)sizeof(TZrUInt16);
            break;
        case ZR_VALUE_TYPE_UINT32:
            byteSize = (TZrUInt32)sizeof(TZrUInt32);
            break;
        case ZR_VALUE_TYPE_UINT64:
            byteSize = (TZrUInt32)sizeof(TZrUInt64);
            break;
        case ZR_VALUE_TYPE_FLOAT:
            byteSize = (TZrUInt32)sizeof(TZrFloat32);
            break;
        case ZR_VALUE_TYPE_DOUBLE:
            byteSize = (TZrUInt32)sizeof(TZrDouble);
            break;
        default:
            return ZR_FALSE;
    }

    *outByteSize = byteSize;
    return ZR_TRUE;
}

static TZrBool reflection_field_value_load_signed_int(const TZrByte *address,
                                                      TZrUInt32 byteSize,
                                                      TZrInt64 *outValue) {
    if (address == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (byteSize) {
        case sizeof(TZrInt8): {
            TZrInt8 value;
            memcpy(&value, address, sizeof(value));
            *outValue = value;
            return ZR_TRUE;
        }
        case sizeof(TZrInt16): {
            TZrInt16 value;
            memcpy(&value, address, sizeof(value));
            *outValue = value;
            return ZR_TRUE;
        }
        case sizeof(TZrInt32): {
            TZrInt32 value;
            memcpy(&value, address, sizeof(value));
            *outValue = value;
            return ZR_TRUE;
        }
        case sizeof(TZrInt64): {
            TZrInt64 value;
            memcpy(&value, address, sizeof(value));
            *outValue = value;
            return ZR_TRUE;
        }
        default:
            return ZR_FALSE;
    }
}

static TZrBool reflection_field_value_load_unsigned_int(const TZrByte *address,
                                                        TZrUInt32 byteSize,
                                                        TZrUInt64 *outValue) {
    if (address == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (byteSize) {
        case sizeof(TZrUInt8): {
            TZrUInt8 value;
            memcpy(&value, address, sizeof(value));
            *outValue = value;
            return ZR_TRUE;
        }
        case sizeof(TZrUInt16): {
            TZrUInt16 value;
            memcpy(&value, address, sizeof(value));
            *outValue = value;
            return ZR_TRUE;
        }
        case sizeof(TZrUInt32): {
            TZrUInt32 value;
            memcpy(&value, address, sizeof(value));
            *outValue = value;
            return ZR_TRUE;
        }
        case sizeof(TZrUInt64): {
            TZrUInt64 value;
            memcpy(&value, address, sizeof(value));
            *outValue = value;
            return ZR_TRUE;
        }
        default:
            return ZR_FALSE;
    }
}

static TZrBool reflection_field_value_signed_range_for_byte_size(TZrUInt32 byteSize,
                                                                 TZrInt64 *outMin,
                                                                 TZrInt64 *outMax) {
    TZrInt64 minValue;
    TZrInt64 maxValue;

    if (outMin == ZR_NULL || outMax == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (byteSize) {
        case sizeof(TZrInt8):
            minValue = ZR_TYPE_RANGE_INT8_MIN;
            maxValue = ZR_TYPE_RANGE_INT8_MAX;
            break;
        case sizeof(TZrInt16):
            minValue = ZR_TYPE_RANGE_INT16_MIN;
            maxValue = ZR_TYPE_RANGE_INT16_MAX;
            break;
        case sizeof(TZrInt32):
            minValue = ZR_TYPE_RANGE_INT32_MIN;
            maxValue = ZR_TYPE_RANGE_INT32_MAX;
            break;
        case sizeof(TZrInt64):
            minValue = ZR_TYPE_RANGE_INT64_MIN;
            maxValue = ZR_TYPE_RANGE_INT64_MAX;
            break;
        default:
            return ZR_FALSE;
    }

    *outMin = minValue;
    *outMax = maxValue;
    return ZR_TRUE;
}

static TZrBool reflection_field_value_unsigned_max_for_byte_size(TZrUInt32 byteSize, TZrUInt64 *outMax) {
    TZrUInt64 maxValue;

    if (outMax == ZR_NULL) {
        return ZR_FALSE;
    }

    switch (byteSize) {
        case sizeof(TZrUInt8):
            maxValue = (TZrUInt64)UINT8_MAX;
            break;
        case sizeof(TZrUInt16):
            maxValue = (TZrUInt64)UINT16_MAX;
            break;
        case sizeof(TZrUInt32):
            maxValue = (TZrUInt64)UINT32_MAX;
            break;
        case sizeof(TZrUInt64):
            maxValue = (TZrUInt64)UINT64_MAX;
            break;
        default:
            return ZR_FALSE;
    }

    *outMax = maxValue;
    return ZR_TRUE;
}

static TZrBool reflection_field_value_signed_range_contains_unsigned(TZrUInt32 byteSize, TZrUInt64 value) {
    TZrInt64 minValue;
    TZrInt64 maxValue;

    return reflection_field_value_signed_range_for_byte_size(byteSize, &minValue, &maxValue) &&
           minValue <= 0 &&
           value <= (TZrUInt64)maxValue;
}

static TZrBool reflection_field_value_float32_can_store_losslessly(TZrDouble value) {
    TZrFloat32 storedValue;

    if (value != value) {
        return ZR_FALSE;
    }
    if (value > (TZrDouble)FLT_MAX || value < -((TZrDouble)FLT_MAX)) {
        return ZR_FALSE;
    }
    storedValue = (TZrFloat32)value;
    return (TZrDouble)storedValue == value;
}

static TZrBool reflection_field_value_store_signed_int(TZrByte *address,
                                                       TZrUInt32 byteSize,
                                                       TZrInt64 value) {
    TZrInt64 minValue;
    TZrInt64 maxValue;

    if (address == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!reflection_field_value_signed_range_for_byte_size(byteSize, &minValue, &maxValue) ||
        value < minValue ||
        value > maxValue) {
        return ZR_FALSE;
    }

    switch (byteSize) {
        case sizeof(TZrInt8): {
            TZrInt8 storedValue = (TZrInt8)value;
            memcpy(address, &storedValue, sizeof(storedValue));
            return ZR_TRUE;
        }
        case sizeof(TZrInt16): {
            TZrInt16 storedValue = (TZrInt16)value;
            memcpy(address, &storedValue, sizeof(storedValue));
            return ZR_TRUE;
        }
        case sizeof(TZrInt32): {
            TZrInt32 storedValue = (TZrInt32)value;
            memcpy(address, &storedValue, sizeof(storedValue));
            return ZR_TRUE;
        }
        case sizeof(TZrInt64): {
            TZrInt64 storedValue = value;
            memcpy(address, &storedValue, sizeof(storedValue));
            return ZR_TRUE;
        }
        default:
            return ZR_FALSE;
    }
}

static TZrBool reflection_field_value_store_unsigned_int(TZrByte *address,
                                                         TZrUInt32 byteSize,
                                                         TZrUInt64 value) {
    TZrUInt64 maxValue;

    if (address == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!reflection_field_value_unsigned_max_for_byte_size(byteSize, &maxValue) ||
        value > maxValue) {
        return ZR_FALSE;
    }

    switch (byteSize) {
        case sizeof(TZrUInt8): {
            TZrUInt8 storedValue = (TZrUInt8)value;
            memcpy(address, &storedValue, sizeof(storedValue));
            return ZR_TRUE;
        }
        case sizeof(TZrUInt16): {
            TZrUInt16 storedValue = (TZrUInt16)value;
            memcpy(address, &storedValue, sizeof(storedValue));
            return ZR_TRUE;
        }
        case sizeof(TZrUInt32): {
            TZrUInt32 storedValue = (TZrUInt32)value;
            memcpy(address, &storedValue, sizeof(storedValue));
            return ZR_TRUE;
        }
        case sizeof(TZrUInt64): {
            TZrUInt64 storedValue = value;
            memcpy(address, &storedValue, sizeof(storedValue));
            return ZR_TRUE;
        }
        default:
            return ZR_FALSE;
    }
}

static TZrBool reflection_field_value_accepts_raw_primitive(const SZrTypeLayoutField *fieldLayout,
                                                            EZrValueType valueType) {
    TZrUInt32 expectedByteSize;

    return fieldLayout != ZR_NULL &&
           (fieldLayout->flags & (ZR_TYPE_LAYOUT_FIELD_FLAG_VALUE_SLOT |
                                  ZR_TYPE_LAYOUT_FIELD_FLAG_GC_VALUE |
                                  ZR_TYPE_LAYOUT_FIELD_FLAG_OWNERSHIP_VALUE)) == 0u &&
           reflection_field_value_primitive_byte_size(valueType, &expectedByteSize) &&
           fieldLayout->byteSize == expectedByteSize;
}

TZrBool ZrCore_ReflectionFieldValue_LoadPrimitive(
        SZrState *state,
        const SZrTypeLayoutField *fieldLayout,
        EZrValueType valueType,
        const TZrByte *fieldAddress,
        SZrTypeValue *result) {
    if (state == ZR_NULL ||
        fieldAddress == ZR_NULL ||
        result == ZR_NULL ||
        !reflection_field_value_accepts_raw_primitive(fieldLayout, valueType)) {
        return ZR_FALSE;
    }

    switch (valueType) {
        ZR_VALUE_CASES_SIGNED_INT {
            TZrInt64 value;
            if (!reflection_field_value_load_signed_int(fieldAddress, fieldLayout->byteSize, &value)) {
                return ZR_FALSE;
            }
            ZR_VALUE_FAST_SET(result, nativeInt64, value, ZR_VALUE_TYPE_INT64);
            return ZR_TRUE;
        }
        ZR_VALUE_CASES_UNSIGNED_INT {
            TZrUInt64 value;
            if (!reflection_field_value_load_unsigned_int(fieldAddress, fieldLayout->byteSize, &value)) {
                return ZR_FALSE;
            }
            ZR_VALUE_FAST_SET(result, nativeUInt64, value, ZR_VALUE_TYPE_UINT64);
            return ZR_TRUE;
        }
        case ZR_VALUE_TYPE_BOOL: {
            TZrBool value;
            memcpy(&value, fieldAddress, sizeof(value));
            ZrCore_Value_InitAsBool(state, result, value);
            return ZR_TRUE;
        }
        case ZR_VALUE_TYPE_FLOAT: {
            TZrFloat32 value;
            memcpy(&value, fieldAddress, sizeof(value));
            ZR_VALUE_FAST_SET(result, nativeDouble, (TZrDouble)value, ZR_VALUE_TYPE_DOUBLE);
            return ZR_TRUE;
        }
        case ZR_VALUE_TYPE_DOUBLE: {
            TZrDouble value;
            memcpy(&value, fieldAddress, sizeof(value));
            ZR_VALUE_FAST_SET(result, nativeDouble, value, ZR_VALUE_TYPE_DOUBLE);
            return ZR_TRUE;
        }
        default:
            return ZR_FALSE;
    }
}

TZrBool ZrCore_ReflectionFieldValue_StorePrimitive(
        const SZrTypeLayoutField *fieldLayout,
        EZrValueType valueType,
        TZrByte *fieldAddress,
        const SZrTypeValue *value) {
    if (fieldAddress == ZR_NULL ||
        value == ZR_NULL ||
        !reflection_field_value_accepts_raw_primitive(fieldLayout, valueType)) {
        return ZR_FALSE;
    }

    switch (valueType) {
        ZR_VALUE_CASES_SIGNED_INT
            if (!ZR_VALUE_IS_TYPE_INT(value->type)) {
                return ZR_FALSE;
            }
            if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
                TZrUInt64 unsignedValue = value->value.nativeObject.nativeUInt64;
                if (!reflection_field_value_signed_range_contains_unsigned(fieldLayout->byteSize, unsignedValue)) {
                    return ZR_FALSE;
                }
                return reflection_field_value_store_signed_int(fieldAddress,
                                                               fieldLayout->byteSize,
                                                               (TZrInt64)unsignedValue);
            }
            return reflection_field_value_store_signed_int(fieldAddress,
                                                           fieldLayout->byteSize,
                                                           value->value.nativeObject.nativeInt64);
        ZR_VALUE_CASES_UNSIGNED_INT
            if (!ZR_VALUE_IS_TYPE_INT(value->type)) {
                return ZR_FALSE;
            }
            if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
                TZrInt64 signedValue = value->value.nativeObject.nativeInt64;
                if (signedValue < 0) {
                    return ZR_FALSE;
                }
                return reflection_field_value_store_unsigned_int(fieldAddress,
                                                                 fieldLayout->byteSize,
                                                                 (TZrUInt64)signedValue);
            }
            return reflection_field_value_store_unsigned_int(fieldAddress,
                                                             fieldLayout->byteSize,
                                                             value->value.nativeObject.nativeUInt64);
        case ZR_VALUE_TYPE_BOOL:
            if (value->type != ZR_VALUE_TYPE_BOOL) {
                return ZR_FALSE;
            }
            {
                TZrBool storedValue = value->value.nativeObject.nativeBool != 0u;
                memcpy(fieldAddress, &storedValue, sizeof(storedValue));
            }
            return ZR_TRUE;
        case ZR_VALUE_TYPE_FLOAT: {
            TZrFloat32 storedValue;
            TZrDouble sourceValue;
            if (!ZR_VALUE_IS_TYPE_FLOAT(value->type)) {
                return ZR_FALSE;
            }
            sourceValue = value->value.nativeObject.nativeDouble;
            if (!reflection_field_value_float32_can_store_losslessly(sourceValue)) {
                return ZR_FALSE;
            }
            storedValue = (TZrFloat32)sourceValue;
            memcpy(fieldAddress, &storedValue, sizeof(storedValue));
            return ZR_TRUE;
        }
        case ZR_VALUE_TYPE_DOUBLE:
            if (!ZR_VALUE_IS_TYPE_FLOAT(value->type)) {
                return ZR_FALSE;
            }
            memcpy(fieldAddress, &value->value.nativeObject.nativeDouble, sizeof(TZrDouble));
            return ZR_TRUE;
        default:
            return ZR_FALSE;
    }
}
