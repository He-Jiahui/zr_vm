#include "ffi_runtime/ffi_runtime_internal.h"

void zr_ffi_destroy_type(ZrFfiTypeLayout *type) {
    TZrSize index;

    if (type == ZR_NULL) {
        return;
    }

    free(type->name);
    switch (type->kind) {
        case ZR_FFI_TYPE_POINTER:
            zr_ffi_destroy_type(type->as.pointer.pointee);
            break;
        case ZR_FFI_TYPE_STRUCT:
        case ZR_FFI_TYPE_UNION:
            for (index = 0; index < type->as.aggregate.fieldCount; index++) {
                free(type->as.aggregate.fields[index].name);
                zr_ffi_destroy_type(type->as.aggregate.fields[index].type);
            }
            free(type->as.aggregate.fields);
            break;
        case ZR_FFI_TYPE_ENUM:
            zr_ffi_destroy_type(type->as.enumType.underlying);
            break;
        case ZR_FFI_TYPE_FUNCTION:
            zr_ffi_destroy_signature(type->as.functionType.signature);
            break;
        default:
            break;
    }
#if ZR_VM_HAS_LIBFFI
    free(type->ffiElements);
#endif
    free(type);
}

void zr_ffi_destroy_signature(ZrFfiSignature *signature) {
    TZrSize index;

    if (signature == ZR_NULL) {
        return;
    }

    for (index = 0; index < signature->parameterCount; index++) {
        zr_ffi_destroy_type(signature->parameters[index].type);
    }
    free(signature->parameters);
    zr_ffi_destroy_type(signature->returnType);
#if ZR_VM_HAS_LIBFFI
    free(signature->ffiParameterTypes);
#endif
    free(signature);
}

ZrFfiTypeLayout *zr_ffi_new_type(ZrFfiTypeKind kind) {
    ZrFfiTypeLayout *type = (ZrFfiTypeLayout *) calloc(1, sizeof(ZrFfiTypeLayout));
    if (type != ZR_NULL) {
        type->kind = kind;
    }
    return type;
}

ZrFfiTypeLayout *zr_ffi_clone_type(const ZrFfiTypeLayout *type) {
    TZrSize index;
    ZrFfiTypeLayout *copy;

    if (type == ZR_NULL) {
        return ZR_NULL;
    }

    copy = zr_ffi_new_type(type->kind);
    if (copy == ZR_NULL) {
        return ZR_NULL;
    }

    copy->size = type->size;
    copy->align = type->align;
    copy->name = zr_ffi_strdup(type->name);
#if ZR_VM_HAS_LIBFFI
    copy->ffiType = type->ffiType;
    copy->ffiAggregateType = type->ffiAggregateType;
#endif

    switch (type->kind) {
        case ZR_FFI_TYPE_POINTER:
            copy->as.pointer.direction = type->as.pointer.direction;
            copy->as.pointer.pointee = zr_ffi_clone_type(type->as.pointer.pointee);
            break;
        case ZR_FFI_TYPE_STRING:
            copy->as.stringType.encoding = type->as.stringType.encoding;
            break;
        case ZR_FFI_TYPE_STRUCT:
        case ZR_FFI_TYPE_UNION:
            copy->as.aggregate.fieldCount = type->as.aggregate.fieldCount;
            if (copy->as.aggregate.fieldCount > 0) {
                copy->as.aggregate.fields =
                        (ZrFfiFieldLayout *) calloc(copy->as.aggregate.fieldCount, sizeof(ZrFfiFieldLayout));
            }
            if (copy->as.aggregate.fieldCount > 0 && copy->as.aggregate.fields == ZR_NULL) {
                zr_ffi_destroy_type(copy);
                return ZR_NULL;
            }
            for (index = 0; index < copy->as.aggregate.fieldCount; index++) {
                copy->as.aggregate.fields[index].name = zr_ffi_strdup(type->as.aggregate.fields[index].name);
                copy->as.aggregate.fields[index].offset = type->as.aggregate.fields[index].offset;
                copy->as.aggregate.fields[index].type = zr_ffi_clone_type(type->as.aggregate.fields[index].type);
            }
#if ZR_VM_HAS_LIBFFI
            if (copy->as.aggregate.fieldCount > 0) {
                copy->ffiElements = (ffi_type **) calloc(copy->as.aggregate.fieldCount + 1, sizeof(ffi_type *));
                if (copy->ffiElements == ZR_NULL) {
                    zr_ffi_destroy_type(copy);
                    return ZR_NULL;
                }
                for (index = 0; index < copy->as.aggregate.fieldCount; index++) {
                    copy->ffiElements[index] = copy->as.aggregate.fields[index].type->ffiType;
                }
                copy->ffiAggregateType.elements = copy->ffiElements;
                copy->ffiType = &copy->ffiAggregateType;
            }
#endif
            break;
        case ZR_FFI_TYPE_ENUM:
            copy->as.enumType.underlying = zr_ffi_clone_type(type->as.enumType.underlying);
            break;
        default:
            break;
    }

    return copy;
}

void zr_ffi_init_primitive_type(ZrFfiTypeLayout *type, const char *name, TZrSize size, TZrSize align
#if ZR_VM_HAS_LIBFFI
                                       ,
                                       ffi_type *ffiType
#endif
) {
    if (type == ZR_NULL) {
        return;
    }

    type->name = zr_ffi_strdup(name);
    type->size = size;
    type->align = align;
#if ZR_VM_HAS_LIBFFI
    type->ffiType = ffiType;
#endif
}

ZrFfiTypeLayout *zr_ffi_make_primitive_type(const char *name) {
    ZrFfiTypeLayout *type = ZR_NULL;

    if (name == ZR_NULL) {
        return ZR_NULL;
    }

    if (strcmp(name, "void") == 0) {
        type = zr_ffi_new_type(ZR_FFI_TYPE_VOID);
        zr_ffi_init_primitive_type(type, "void", 0, 1
#if ZR_VM_HAS_LIBFFI
                                   ,
                                   &ffi_type_void
#endif
        );
    } else if (strcmp(name, "bool") == 0) {
        type = zr_ffi_new_type(ZR_FFI_TYPE_BOOL);
        zr_ffi_init_primitive_type(type, "bool", sizeof(unsigned char), sizeof(unsigned char)
#if ZR_VM_HAS_LIBFFI
                                                                                ,
                                   &ffi_type_uint8
#endif
        );
    } else if (strcmp(name, "i8") == 0) {
        type = zr_ffi_new_type(ZR_FFI_TYPE_I8);
        zr_ffi_init_primitive_type(type, "i8", sizeof(int8_t), sizeof(int8_t)
#if ZR_VM_HAS_LIBFFI
                                                                       ,
                                   &ffi_type_sint8
#endif
        );
    } else if (strcmp(name, "u8") == 0) {
        type = zr_ffi_new_type(ZR_FFI_TYPE_U8);
        zr_ffi_init_primitive_type(type, "u8", sizeof(uint8_t), sizeof(uint8_t)
#if ZR_VM_HAS_LIBFFI
                                                                        ,
                                   &ffi_type_uint8
#endif
        );
    } else if (strcmp(name, "i16") == 0) {
        type = zr_ffi_new_type(ZR_FFI_TYPE_I16);
        zr_ffi_init_primitive_type(type, "i16", sizeof(int16_t), sizeof(int16_t)
#if ZR_VM_HAS_LIBFFI
                                                                         ,
                                   &ffi_type_sint16
#endif
        );
    } else if (strcmp(name, "u16") == 0) {
        type = zr_ffi_new_type(ZR_FFI_TYPE_U16);
        zr_ffi_init_primitive_type(type, "u16", sizeof(uint16_t), sizeof(uint16_t)
#if ZR_VM_HAS_LIBFFI
                                                                          ,
                                   &ffi_type_uint16
#endif
        );
    } else if (strcmp(name, "i32") == 0) {
        type = zr_ffi_new_type(ZR_FFI_TYPE_I32);
        zr_ffi_init_primitive_type(type, "i32", sizeof(int32_t), sizeof(int32_t)
#if ZR_VM_HAS_LIBFFI
                                                                         ,
                                   &ffi_type_sint32
#endif
        );
    } else if (strcmp(name, "u32") == 0) {
        type = zr_ffi_new_type(ZR_FFI_TYPE_U32);
        zr_ffi_init_primitive_type(type, "u32", sizeof(uint32_t), sizeof(uint32_t)
#if ZR_VM_HAS_LIBFFI
                                                                          ,
                                   &ffi_type_uint32
#endif
        );
    } else if (strcmp(name, "i64") == 0) {
        type = zr_ffi_new_type(ZR_FFI_TYPE_I64);
        zr_ffi_init_primitive_type(type, "i64", sizeof(int64_t), sizeof(int64_t)
#if ZR_VM_HAS_LIBFFI
                                                                         ,
                                   &ffi_type_sint64
#endif
        );
    } else if (strcmp(name, "u64") == 0) {
        type = zr_ffi_new_type(ZR_FFI_TYPE_U64);
        zr_ffi_init_primitive_type(type, "u64", sizeof(uint64_t), sizeof(uint64_t)
#if ZR_VM_HAS_LIBFFI
                                                                          ,
                                   &ffi_type_uint64
#endif
        );
    } else if (strcmp(name, "f32") == 0) {
        type = zr_ffi_new_type(ZR_FFI_TYPE_F32);
        zr_ffi_init_primitive_type(type, "f32", sizeof(float), sizeof(float)
#if ZR_VM_HAS_LIBFFI
                                                                       ,
                                   &ffi_type_float
#endif
        );
    } else if (strcmp(name, "f64") == 0) {
        type = zr_ffi_new_type(ZR_FFI_TYPE_F64);
        zr_ffi_init_primitive_type(type, "f64", sizeof(double), sizeof(double)
#if ZR_VM_HAS_LIBFFI
                                                                        ,
                                   &ffi_type_double
#endif
        );
    }

    return type;
}

ffi_abi zr_ffi_parse_abi(const char *abiText, char *errorBuffer, TZrSize errorBufferSize) {
    if (abiText == ZR_NULL || abiText[0] == '\0' || strcmp(abiText, "default") == 0 || strcmp(abiText, "system") == 0 ||
        strcmp(abiText, "sysv") == 0 || strcmp(abiText, "win64") == 0) {
#if ZR_VM_HAS_LIBFFI
        return FFI_DEFAULT_ABI;
#else
        return 0;
#endif
    }

    if (strcmp(abiText, "cdecl") == 0) {
#if ZR_VM_HAS_LIBFFI && defined(_WIN32) && !defined(_WIN64)
        return FFI_MS_CDECL;
#elif ZR_VM_HAS_LIBFFI
        return FFI_DEFAULT_ABI;
#else
        return 0;
#endif
    }

    if (strcmp(abiText, "stdcall") == 0) {
#if ZR_VM_HAS_LIBFFI && defined(_WIN32) && !defined(_WIN64)
        return FFI_STDCALL;
#elif ZR_VM_HAS_LIBFFI && defined(_WIN64)
        return FFI_DEFAULT_ABI;
#else
        snprintf(errorBuffer, errorBufferSize, "stdcall is not supported on this build");
        return 0;
#endif
    }

    snprintf(errorBuffer, errorBufferSize, "unsupported calling convention '%s'", abiText);
    return 0;
}

ZrFfiTypeLayout *zr_ffi_pointer_type_from_target(const ZrFfiTypeLayout *target) {
    ZrFfiTypeLayout *pointerType = zr_ffi_new_type(ZR_FFI_TYPE_POINTER);
    if (pointerType == ZR_NULL) {
        return ZR_NULL;
    }

    pointerType->name = zr_ffi_strdup("pointer");
    pointerType->size = sizeof(void *);
    pointerType->align = sizeof(void *);
    pointerType->as.pointer.direction = ZR_FFI_DIRECTION_IN;
    pointerType->as.pointer.pointee = zr_ffi_clone_type(target);
#if ZR_VM_HAS_LIBFFI
    pointerType->ffiType = &ffi_type_pointer;
#endif
    if (pointerType->as.pointer.pointee == ZR_NULL) {
        zr_ffi_destroy_type(pointerType);
        return ZR_NULL;
    }
    return pointerType;
}

ZrFfiTypeLayout *zr_ffi_parse_type_descriptor(SZrState *state, const SZrTypeValue *descriptorValue,
                                                     char *errorBuffer, TZrSize errorBufferSize) {
    ZrFfiTypeLayout *type;
    SZrObject *descriptorObject;
    const char *kindText;

    if (descriptorValue == ZR_NULL) {
        snprintf(errorBuffer, errorBufferSize, "missing type descriptor");
        return ZR_NULL;
    }

    if (descriptorValue->type == ZR_VALUE_TYPE_STRING) {
        const char *primitiveName = ZR_NULL;
        zr_ffi_read_string_value(state, descriptorValue, &primitiveName);
        type = zr_ffi_make_primitive_type(primitiveName);
        if (type == ZR_NULL) {
            snprintf(errorBuffer, errorBufferSize, "unknown primitive type '%s'",
                     primitiveName != ZR_NULL ? primitiveName : "<null>");
        }
        return type;
    }

    if (!zr_ffi_value_is_object(descriptorValue, &descriptorObject)) {
        snprintf(errorBuffer, errorBufferSize, "type descriptor must be a string or object");
        return ZR_NULL;
    }

    if (!zr_ffi_read_object_string_field(state, descriptorObject, "kind", &kindText)) {
        snprintf(errorBuffer, errorBufferSize, "type descriptor object is missing 'kind'");
        return ZR_NULL;
    }

    if (strcmp(kindText, "string") == 0) {
        const char *encodingText = "utf8";
        type = zr_ffi_new_type(ZR_FFI_TYPE_STRING);
        if (type == ZR_NULL) {
            return ZR_NULL;
        }
        type->name = zr_ffi_strdup("string");
        type->size = sizeof(void *);
        type->align = sizeof(void *);
        zr_ffi_read_object_string_field(state, descriptorObject, "encoding", &encodingText);
        if (strcmp(encodingText, "utf16") == 0) {
            type->as.stringType.encoding = ZR_FFI_STRING_UTF16;
        } else if (strcmp(encodingText, "ansi") == 0) {
            type->as.stringType.encoding = ZR_FFI_STRING_ANSI;
        } else {
            type->as.stringType.encoding = ZR_FFI_STRING_UTF8;
        }
#if ZR_VM_HAS_LIBFFI
        type->ffiType = &ffi_type_pointer;
#endif
        return type;
    }

    if (strcmp(kindText, "pointer") == 0) {
        const SZrTypeValue *pointeeValue = zr_ffi_find_field_raw(state, descriptorObject, "to");
        const char *directionText = "in";
        type = zr_ffi_new_type(ZR_FFI_TYPE_POINTER);
        if (type == ZR_NULL) {
            return ZR_NULL;
        }
        type->name = zr_ffi_strdup("pointer");
        type->size = sizeof(void *);
        type->align = sizeof(void *);
        zr_ffi_read_object_string_field(state, descriptorObject, "direction", &directionText);
        if (strcmp(directionText, "out") == 0) {
            type->as.pointer.direction = ZR_FFI_DIRECTION_OUT;
        } else if (strcmp(directionText, "inout") == 0) {
            type->as.pointer.direction = ZR_FFI_DIRECTION_INOUT;
        } else {
            type->as.pointer.direction = ZR_FFI_DIRECTION_IN;
        }
        type->as.pointer.pointee = zr_ffi_parse_type_descriptor(state, pointeeValue, errorBuffer, errorBufferSize);
        if (type->as.pointer.pointee == ZR_NULL) {
            zr_ffi_destroy_type(type);
            return ZR_NULL;
        }
#if ZR_VM_HAS_LIBFFI
        type->ffiType = &ffi_type_pointer;
#endif
        return type;
    }

    if (strcmp(kindText, "struct") == 0 || strcmp(kindText, "union") == 0) {
        TZrBool isUnion = strcmp(kindText, "union") == 0 ? ZR_TRUE : ZR_FALSE;
        SZrObject *fieldsArray = ZR_NULL;
        TZrSize fieldCount;
        TZrSize index;
        TZrSize currentSize = 0;
        TZrSize maxAlign = 1;
        TZrInt64 packValue = 0;
        TZrInt64 explicitAlignValue = 0;
        TZrSize effectivePack = 0;

        if (!zr_ffi_read_object_field_object(state, descriptorObject, "fields", &fieldsArray) ||
            fieldsArray->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
            snprintf(errorBuffer, errorBufferSize, "%s descriptor is missing a 'fields' array", kindText);
            return ZR_NULL;
        }

        fieldCount = zr_ffi_array_length(state, fieldsArray);
        type = zr_ffi_new_type(isUnion ? ZR_FFI_TYPE_UNION : ZR_FFI_TYPE_STRUCT);
        if (type == ZR_NULL) {
            return ZR_NULL;
        }

        zr_ffi_read_object_string_field(state, descriptorObject, "name", &kindText);
        type->name = zr_ffi_strdup(kindText != ZR_NULL ? kindText : (isUnion ? "union" : "struct"));
        zr_ffi_read_object_int_field(state, descriptorObject, "pack", &packValue);
        zr_ffi_read_object_int_field(state, descriptorObject, "align", &explicitAlignValue);
        if (packValue > 0) {
            effectivePack = (TZrSize)packValue;
        }
        type->as.aggregate.fieldCount = fieldCount;
        if (fieldCount > 0) {
            type->as.aggregate.fields = (ZrFfiFieldLayout *) calloc(fieldCount, sizeof(ZrFfiFieldLayout));
        }
        if (fieldCount > 0 && type->as.aggregate.fields == ZR_NULL) {
            zr_ffi_destroy_type(type);
            return ZR_NULL;
        }

        for (index = 0; index < fieldCount; index++) {
            const SZrTypeValue *fieldValue = zr_ffi_array_get(state, fieldsArray, index);
            SZrObject *fieldObject;
            const char *fieldName = ZR_NULL;
            const SZrTypeValue *fieldTypeValue;
            ZrFfiTypeLayout *fieldType;
            TZrInt64 explicitOffsetValue = -1;
            TZrSize fieldAlign;
            TZrSize fieldOffset;

            if (!zr_ffi_value_is_object(fieldValue, &fieldObject) ||
                !zr_ffi_read_object_string_field(state, fieldObject, "name", &fieldName)) {
                snprintf(errorBuffer, errorBufferSize, "aggregate field %llu is invalid", (unsigned long long) index);
                zr_ffi_destroy_type(type);
                return ZR_NULL;
            }

            fieldTypeValue = zr_ffi_find_field_raw(state, fieldObject, "type");
            fieldType = zr_ffi_parse_type_descriptor(state, fieldTypeValue, errorBuffer, errorBufferSize);
            if (fieldType == ZR_NULL) {
                zr_ffi_destroy_type(type);
                return ZR_NULL;
            }

            type->as.aggregate.fields[index].name = zr_ffi_strdup(fieldName);
            type->as.aggregate.fields[index].type = fieldType;
            zr_ffi_read_object_int_field(state, fieldObject, "offset", &explicitOffsetValue);
            fieldAlign = fieldType->align;
            if (effectivePack > 0 && fieldAlign > effectivePack) {
                fieldAlign = effectivePack;
            }

            if (explicitOffsetValue >= 0) {
                fieldOffset = (TZrSize)explicitOffsetValue;
            } else if (isUnion) {
                fieldOffset = 0;
            } else {
                fieldOffset = zr_ffi_align_up(currentSize, fieldAlign);
            }

            type->as.aggregate.fields[index].offset = fieldOffset;
            if (fieldOffset + fieldType->size > currentSize) {
                currentSize = fieldOffset + fieldType->size;
            }
            if (fieldAlign > maxAlign) {
                maxAlign = fieldAlign;
            }
        }

        if (effectivePack > 0 && maxAlign > effectivePack) {
            maxAlign = effectivePack;
        }
        if (explicitAlignValue > 0) {
            maxAlign = (TZrSize)explicitAlignValue;
        }
        type->align = maxAlign;
        type->size = zr_ffi_align_up(currentSize, maxAlign);
#if ZR_VM_HAS_LIBFFI
        type->ffiElements = (ffi_type **) calloc(fieldCount + 1, sizeof(ffi_type *));
        if (type->ffiElements == ZR_NULL) {
            zr_ffi_destroy_type(type);
            return ZR_NULL;
        }
        for (index = 0; index < fieldCount; index++) {
            type->ffiElements[index] = type->as.aggregate.fields[index].type->ffiType;
        }
        memset(&type->ffiAggregateType, 0, sizeof(type->ffiAggregateType));
        type->ffiAggregateType.size = type->size;
        type->ffiAggregateType.alignment = (unsigned short)type->align;
        type->ffiAggregateType.type = FFI_TYPE_STRUCT;
        type->ffiAggregateType.elements = type->ffiElements;
        type->ffiType = &type->ffiAggregateType;
#endif
        return type;
    }

    if (strcmp(kindText, "enum") == 0) {
        const SZrTypeValue *underlyingValue = zr_ffi_find_field_raw(state, descriptorObject, "underlyingType");
        if (underlyingValue == ZR_NULL) {
            underlyingValue = zr_ffi_find_field_raw(state, descriptorObject, "valueType");
        }
        type = zr_ffi_new_type(ZR_FFI_TYPE_ENUM);
        if (type == ZR_NULL) {
            return ZR_NULL;
        }
        type->name = zr_ffi_strdup("enum");
        type->as.enumType.underlying =
                zr_ffi_parse_type_descriptor(state, underlyingValue, errorBuffer, errorBufferSize);
        if (type->as.enumType.underlying == ZR_NULL) {
            zr_ffi_destroy_type(type);
            return ZR_NULL;
        }
        type->size = type->as.enumType.underlying->size;
        type->align = type->as.enumType.underlying->align;
#if ZR_VM_HAS_LIBFFI
        type->ffiType = type->as.enumType.underlying->ffiType;
#endif
        return type;
    }

    if (strcmp(kindText, "function") == 0) {
        type = zr_ffi_new_type(ZR_FFI_TYPE_FUNCTION);
        if (type == ZR_NULL) {
            return ZR_NULL;
        }
        type->name = zr_ffi_strdup("function");
        type->size = sizeof(void *);
        type->align = sizeof(void *);
        type->as.functionType.signature = zr_ffi_parse_signature(state, descriptorObject, errorBuffer, errorBufferSize);
        if (type->as.functionType.signature == ZR_NULL) {
            zr_ffi_destroy_type(type);
            return ZR_NULL;
        }
#if ZR_VM_HAS_LIBFFI
        type->ffiType = &ffi_type_pointer;
#endif
        return type;
    }

    snprintf(errorBuffer, errorBufferSize, "unsupported type kind '%s'", kindText);
    return ZR_NULL;
}

ZrFfiSignature *zr_ffi_parse_signature(SZrState *state, SZrObject *signatureObject, char *errorBuffer,
                                              TZrSize errorBufferSize) {
    ZrFfiSignature *signature;
    const SZrTypeValue *returnTypeValue;
    SZrObject *parametersArray = ZR_NULL;
    TZrSize parameterCount = 0;
    TZrSize index;
    const char *abiText = ZR_NULL;

    signature = (ZrFfiSignature *) calloc(1, sizeof(ZrFfiSignature));
    if (signature == ZR_NULL) {
        snprintf(errorBuffer, errorBufferSize, "out of memory while allocating signature");
        return ZR_NULL;
    }

    returnTypeValue = zr_ffi_find_field_raw(state, signatureObject, "returnType");
    signature->returnType = zr_ffi_parse_type_descriptor(state, returnTypeValue, errorBuffer, errorBufferSize);
    if (signature->returnType == ZR_NULL) {
        zr_ffi_destroy_signature(signature);
        return ZR_NULL;
    }

    zr_ffi_read_object_field_object(state, signatureObject, "parameters", &parametersArray);
    if (parametersArray != ZR_NULL) {
        parameterCount = zr_ffi_array_length(state, parametersArray);
    }
    signature->parameterCount = parameterCount;
    if (parameterCount > 0) {
        signature->parameters = (ZrFfiParameter *) calloc(parameterCount, sizeof(ZrFfiParameter));
    }
    if (parameterCount > 0 && signature->parameters == ZR_NULL) {
        snprintf(errorBuffer, errorBufferSize, "out of memory while allocating signature parameters");
        zr_ffi_destroy_signature(signature);
        return ZR_NULL;
    }

    for (index = 0; index < parameterCount; index++) {
        const SZrTypeValue *parameterValue = zr_ffi_array_get(state, parametersArray, index);
        const SZrTypeValue *parameterTypeValue = parameterValue;
        SZrObject *parameterObject = ZR_NULL;

        if (zr_ffi_value_is_object(parameterValue, &parameterObject)) {
            const SZrTypeValue *embeddedTypeValue = zr_ffi_find_field_raw(state, parameterObject, "type");
            if (embeddedTypeValue != ZR_NULL) {
                parameterTypeValue = embeddedTypeValue;
            }
        }

        signature->parameters[index].type =
                zr_ffi_parse_type_descriptor(state, parameterTypeValue, errorBuffer, errorBufferSize);
        if (signature->parameters[index].type == ZR_NULL) {
            zr_ffi_destroy_signature(signature);
            return ZR_NULL;
        }
    }

    zr_ffi_read_object_string_field(state, signatureObject, "abi", &abiText);
    if (abiText == ZR_NULL) {
        zr_ffi_read_object_string_field(state, signatureObject, "callingConvention", &abiText);
    }
    signature->abi = zr_ffi_parse_abi(abiText, errorBuffer, errorBufferSize);
    if (errorBuffer[0] != '\0') {
        zr_ffi_destroy_signature(signature);
        return ZR_NULL;
    }
    zr_ffi_read_object_bool_field(state, signatureObject, "varargs", &signature->isVarargs);

#if ZR_VM_HAS_LIBFFI
    if (parameterCount > 0) {
        signature->ffiParameterTypes = (ffi_type **) calloc(parameterCount, sizeof(ffi_type *));
        if (signature->ffiParameterTypes == ZR_NULL) {
            snprintf(errorBuffer, errorBufferSize, "out of memory while allocating ffi parameter type list");
            zr_ffi_destroy_signature(signature);
            return ZR_NULL;
        }
        for (index = 0; index < parameterCount; index++) {
            signature->ffiParameterTypes[index] = signature->parameters[index].type->ffiType;
        }
    }

    if (signature->isVarargs) {
        if (ffi_prep_cif_var(&signature->cif, signature->abi, (unsigned int) parameterCount,
                             (unsigned int) parameterCount, signature->returnType->ffiType,
                             signature->ffiParameterTypes) == FFI_OK) {
            signature->cifPrepared = ZR_TRUE;
        } else {
            snprintf(errorBuffer, errorBufferSize, "ffi_prep_cif_var failed");
            zr_ffi_destroy_signature(signature);
            return ZR_NULL;
        }
    } else {
        if (ffi_prep_cif(&signature->cif, signature->abi, (unsigned int) parameterCount, signature->returnType->ffiType,
                         signature->ffiParameterTypes) == FFI_OK) {
            signature->cifPrepared = ZR_TRUE;
        } else {
            snprintf(errorBuffer, errorBufferSize, "ffi_prep_cif failed");
            zr_ffi_destroy_signature(signature);
            return ZR_NULL;
        }
    }
#endif

    return signature;
}

