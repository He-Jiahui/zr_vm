#include "compiler_internal.h"
#include "compiler_type_prototype_lookup.h"

static TZrBool compiler_type_name_is_inline_value_slot(const TZrChar *typeNameStr) {
    if (typeNameStr == ZR_NULL) {
        return ZR_FALSE;
    }

    return (TZrBool)(strcmp(typeNameStr, "string") == 0 ||
                     strcmp(typeNameStr, "str") == 0 ||
                     strcmp(typeNameStr, "String") == 0 ||
                     strcmp(typeNameStr, "zr.builtin.String") == 0 ||
                     strcmp(typeNameStr, "object") == 0 ||
                     strcmp(typeNameStr, "obj") == 0 ||
                     strcmp(typeNameStr, "Object") == 0 ||
                     strcmp(typeNameStr, "zr.builtin.Object") == 0 ||
                     strcmp(typeNameStr, "array") == 0 ||
                     strcmp(typeNameStr, "Array") == 0 ||
                     strcmp(typeNameStr, "function") == 0 ||
                     strcmp(typeNameStr, "Function") == 0 ||
                     strcmp(typeNameStr, "closure") == 0 ||
                     strcmp(typeNameStr, "Closure") == 0 ||
                     strcmp(typeNameStr, "buffer") == 0 ||
                     strcmp(typeNameStr, "Buffer") == 0 ||
                     strcmp(typeNameStr, "thread") == 0 ||
                     strcmp(typeNameStr, "Thread") == 0 ||
                     strcmp(typeNameStr, "Module") == 0 ||
                     strcmp(typeNameStr, "zr.builtin.Module") == 0 ||
                     strcmp(typeNameStr, "TypeInfo") == 0 ||
                     strcmp(typeNameStr, "zr.builtin.TypeInfo") == 0);
}

static TZrUInt32 compiler_type_name_storage_alignment(const TZrChar *typeNameStr) {
    if (typeNameStr == ZR_NULL) {
        return 0u;
    }

    if (strcmp(typeNameStr, "int") == 0 || strcmp(typeNameStr, "i64") == 0) {
        return (TZrUInt32)_Alignof(TZrInt64);
    }
    if (strcmp(typeNameStr, "i8") == 0) {
        return (TZrUInt32)_Alignof(TZrInt8);
    }
    if (strcmp(typeNameStr, "i16") == 0) {
        return (TZrUInt32)_Alignof(TZrInt16);
    }
    if (strcmp(typeNameStr, "i32") == 0) {
        return (TZrUInt32)_Alignof(TZrInt32);
    }
    if (strcmp(typeNameStr, "uint") == 0 || strcmp(typeNameStr, "u64") == 0) {
        return (TZrUInt32)_Alignof(TZrUInt64);
    }
    if (strcmp(typeNameStr, "u8") == 0) {
        return (TZrUInt32)_Alignof(TZrUInt8);
    }
    if (strcmp(typeNameStr, "u16") == 0) {
        return (TZrUInt32)_Alignof(TZrUInt16);
    }
    if (strcmp(typeNameStr, "u32") == 0) {
        return (TZrUInt32)_Alignof(TZrUInt32);
    }
    if (strcmp(typeNameStr, "f32") == 0) {
        return (TZrUInt32)_Alignof(TZrFloat32);
    }
    if (strcmp(typeNameStr, "float") == 0 || strcmp(typeNameStr, "double") == 0 || strcmp(typeNameStr, "f64") == 0 ||
        strcmp(typeNameStr, "f") == 0) {
        return (TZrUInt32)_Alignof(TZrDouble);
    }
    if (strcmp(typeNameStr, "bool") == 0) {
        return (TZrUInt32)_Alignof(TZrBool);
    }
    if (compiler_type_name_is_inline_value_slot(typeNameStr)) {
        return (TZrUInt32)_Alignof(SZrTypeValue);
    }
    return 0u;
}

TZrUInt32 calculate_type_size(SZrCompilerState *cs, SZrType *type) {
    EZrOwnershipQualifier ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    const SZrType *ownershipInnerType = ZR_NULL;

    if (cs == ZR_NULL || type == ZR_NULL || type->name == ZR_NULL) {
        return 0;
    }

    if (type->referenceAccess != ZR_REFERENCE_ACCESS_NONE) {
        return sizeof(SZrTypeValue);
    }

    if (ZrParser_AstType_TryUnwrapOwnershipGeneric(type, &ownershipQualifier, &ownershipInnerType)) {
        ZR_UNUSED_PARAMETER(ownershipQualifier);
        ZR_UNUSED_PARAMETER(ownershipInnerType);
        return sizeof(SZrTypeValue);
    }
    
    // 处理数组类型
    if (type->dimensions > 0) {
        // 数组本身是指针类型，固定8字节
        // 注意：数组元素的实际大小在运行时确定，这里只返回指针大小
        return sizeof(TZrPtr); // 8 bytes
    }
    
    // 处理基本类型（通过类型名称字符串匹配）
    if (type->name->type == ZR_AST_IDENTIFIER_LITERAL) {
        SZrString *typeName = type->name->data.identifier.name;
        if (typeName == ZR_NULL) {
            return 0;
        }
        
        TZrNativeString typeNameStr = ZrCore_String_GetNativeStringShort(typeName);
        if (typeNameStr == ZR_NULL) {
            return 0;
        }
        
        // 匹配基本类型名称
        if (strcmp(typeNameStr, "int") == 0 || strcmp(typeNameStr, "i64") == 0) {
            return sizeof(TZrInt64); // 8 bytes
        }
        if (strcmp(typeNameStr, "i8") == 0) {
            return sizeof(TZrInt8); // 1 byte
        }
        if (strcmp(typeNameStr, "i16") == 0) {
            return sizeof(TZrInt16); // 2 bytes
        }
        if (strcmp(typeNameStr, "i32") == 0) {
            return sizeof(TZrInt32); // 4 bytes
        }
        if (strcmp(typeNameStr, "uint") == 0 || strcmp(typeNameStr, "u64") == 0) {
            return sizeof(TZrUInt64); // 8 bytes
        }
        if (strcmp(typeNameStr, "u8") == 0) {
            return sizeof(TZrUInt8); // 1 byte
        }
        if (strcmp(typeNameStr, "u16") == 0) {
            return sizeof(TZrUInt16); // 2 bytes
        }
        if (strcmp(typeNameStr, "u32") == 0) {
            return sizeof(TZrUInt32); // 4 bytes
        }
        if (strcmp(typeNameStr, "f32") == 0) {
            return sizeof(TZrFloat32); // 4 bytes
        }
        if (strcmp(typeNameStr, "float") == 0 || strcmp(typeNameStr, "double") == 0 || strcmp(typeNameStr, "f64") == 0 || strcmp(typeNameStr, "f") == 0) {
            return sizeof(TZrDouble); // 8 bytes
        }
        if (strcmp(typeNameStr, "bool") == 0) {
            return sizeof(TZrBool); // 1 byte
        }
        if (compiler_type_name_is_inline_value_slot(typeNameStr)) {
            return sizeof(SZrTypeValue);
        }

        {
            SZrTypePrototypeInfo *prototype = find_compiler_type_prototype(cs, typeName);
            if (prototype != ZR_NULL && prototype->layoutByteSize > 0) {
                return prototype->layoutByteSize;
            }
        }
        
        // 如果是自定义类型（struct/class），大小未知，需要在运行时确定
        // 返回0表示需要运行时计算
        return 0;
    }
    
    return 0;
}

// 辅助函数：应用对齐规则计算偏移量
TZrUInt32 align_offset(TZrUInt32 offset, TZrUInt32 align) {
    // 对齐到align字节边界
    return ((offset + align - 1) / align) * align;
}

// 辅助函数：确定类型的对齐要求（字节）
TZrUInt32 get_type_alignment(SZrCompilerState *cs, SZrType *type) {
    EZrOwnershipQualifier ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    const SZrType *ownershipInnerType = ZR_NULL;

    if (cs == ZR_NULL || type == ZR_NULL) {
        return (TZrUInt32)_Alignof(SZrTypeValue);
    }

    if (type->referenceAccess != ZR_REFERENCE_ACCESS_NONE) {
        return (TZrUInt32)_Alignof(SZrTypeValue);
    }
    if (ZrParser_AstType_TryUnwrapOwnershipGeneric(type, &ownershipQualifier, &ownershipInnerType)) {
        ZR_UNUSED_PARAMETER(ownershipQualifier);
        ZR_UNUSED_PARAMETER(ownershipInnerType);
        return (TZrUInt32)_Alignof(SZrTypeValue);
    }
    if (type->dimensions > 0) {
        return (TZrUInt32)_Alignof(TZrPtr);
    }

    if (type->name != ZR_NULL && type->name->type == ZR_AST_IDENTIFIER_LITERAL) {
        SZrString *typeName = type->name->data.identifier.name;
        SZrTypePrototypeInfo *prototype =
                typeName != ZR_NULL ? find_compiler_type_prototype(cs, typeName) : ZR_NULL;
        TZrUInt32 alignment;
        if (prototype != ZR_NULL && prototype->layoutByteAlign > 0) {
            return prototype->layoutByteAlign;
        }
        alignment = compiler_type_name_storage_alignment(
                typeName != ZR_NULL ? ZrCore_String_GetNativeStringShort(typeName) : ZR_NULL);
        if (alignment > 0u) {
            return alignment;
        }
    }
    return (TZrUInt32)_Alignof(SZrTypeValue);
}

