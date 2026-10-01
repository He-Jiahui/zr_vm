#include "compiler_metadata_signature.h"
#include "type_inference_internal.h"

#include "zr_vm_core/hash.h"

#include <string.h>

/**
 * @brief 固定持久化签名身份使用的 v1 哈希域。
 * @note 更改此前缀会改变已写入元数据的稳定标识，须先审查格式兼容性。
 */
static const TZrByte CZrMetadataSignatureHashV1Prefix[] = {
        'z',
        'r',
        '.',
        'm',
        'd',
        '.',
        's',
        'i',
        'g',
        '.',
        'v',
        '1',
        '\0',
};

/**
 * @brief 对完整签名字节计算稳定身份，供 token、TypeSpec 与 module record 共用。
 * @pre signatureBlob 指向本次签名的完整非空序列化范围。
 * @return 稳定哈希；0 包括空输入和底层分配/更新失败。TODO: 核查 compiler_metadata_token.c:2032 的 target fallback 是否应传播失败，因 module_import_signature.c:1086 会将零哈希视为缺席并跳过验证。
 */
TZrUInt64 metadata_signature_hash_v1(const TZrByte *signatureBlob, TZrSize signatureBlobLength) {
    if (signatureBlob == ZR_NULL || signatureBlobLength == 0) {
        return 0;
    }

    return ZrCore_Hash_CreateStable64WithPrefix(CZrMetadataSignatureHashV1Prefix,
                                                sizeof(CZrMetadataSignatureHashV1Prefix),
                                                signatureBlob,
                                                signatureBlobLength);
}

/** @brief 借用字符串对象中的原生文本，仅在对象仍存活时有效。 */
static TZrNativeString metadata_token_string_text(struct SZrString *stringValue) {
    return stringValue != ZR_NULL ? ZrCore_String_GetNativeString(stringValue) : ZR_NULL;
}

/** @brief 统一 metadata 收集和 effect 过滤中的空字符串判定。 */
TZrSize metadata_token_string_length(SZrString *stringValue) {
    TZrNativeString text = metadata_token_string_text(stringValue);
    return text != ZR_NULL ? strlen(text) : 0;
}

/**
 * @brief 按字符串堆稳定算法派生非零键；堆构建器仍会拒绝异值碰撞。
 * TODO: 此处复制了堆构建器的键算法；核查并同步两处规则，或收敛到共享接口。
 */
static TZrUInt32 metadata_token_string_stable_index(SZrString *value) {
    TZrNativeString text = metadata_token_string_text(value);
    TZrSize length = text != ZR_NULL ? strlen(text) : 0;
    TZrUInt64 hash;

    if (length == 0) {
        return 0;
    }

    hash = ZrCore_Hash_CreateStable64((const TZrByte *)text, length);
    return (TZrUInt32)(hash & 0x7FFFFFFFu) + 1u;
}

/**
 * @brief 在本次写出使用的预建字符串堆快照中查找键。
 * @pre entries 必须与收集阶段生成的快照一致。
 * @return 命中时返回稳定键；未收集、空值或缺席均返回格式中的零引用。
 */
TZrUInt32 metadata_token_string_heap_index(const SZrMetadataStringHeapEntry *entries,
                                           TZrUInt32 entryCount,
                                           SZrString *value) {
    TZrUInt32 expectedIndex;

    if (entries == ZR_NULL || entryCount == 0 || metadata_token_string_length(value) == 0) {
        return 0;
    }

    expectedIndex = metadata_token_string_stable_index(value);
    for (TZrUInt32 index = 0; index < entryCount; index++) {
        if (entries[index].stringIndex == expectedIndex &&
            ((entries[index].value == value) ||
             (entries[index].value != ZR_NULL &&
              value != ZR_NULL &&
              ZrCore_String_Equal(entries[index].value, value)))) {
            return expectedIndex;
        }
    }

    return 0;
}

/** @brief 将解析出的泛型实参名称映射为通用类型签名输入，不复制名称字符串。 */
static void metadata_token_type_ref_from_name(SZrString *typeName, SZrFunctionTypedTypeRef *outTypeRef) {
    TZrNativeString typeNameText;
    TZrSize typeNameLength;
    EZrValueType primitiveType;

    if (outTypeRef == ZR_NULL) {
        return;
    }

    ZrCore_Memory_RawSet(outTypeRef, 0, sizeof(*outTypeRef));
    outTypeRef->baseType = ZR_VALUE_TYPE_OBJECT;
    outTypeRef->elementBaseType = ZR_VALUE_TYPE_OBJECT;
    if (typeName == ZR_NULL) {
        return;
    }

    typeNameText = metadata_token_string_text(typeName);
    typeNameLength = typeNameText != ZR_NULL ? strlen(typeNameText) : 0;
    if (inferred_type_try_map_primitive_name(typeNameText, typeNameLength, &primitiveType)) {
        outTypeRef->baseType = primitiveType;
        return;
    }

    outTypeRef->typeName = typeName;
}

/** @brief 向共享 metadata 输出缓冲区追加单字节字段。 */
void metadata_token_write_u8(TZrByte *buffer, TZrSize *offset, TZrUInt8 value) {
    buffer[*offset] = value;
    *offset += 1;
}

/** @brief 向共享 metadata 输出缓冲区追加小端序 u32 字段。 */
void metadata_token_write_u32(TZrByte *buffer, TZrSize *offset, TZrUInt32 value) {
    buffer[*offset + 0] = (TZrByte)(value & 0xFFu);
    buffer[*offset + 1] = (TZrByte)((value >> 8) & 0xFFu);
    buffer[*offset + 2] = (TZrByte)((value >> 16) & 0xFFu);
    buffer[*offset + 3] = (TZrByte)((value >> 24) & 0xFFu);
    *offset += 4;
}

/**
 * @brief 在当前脚本声明树查找 union；extern block 仅作为透明容器递归展开。
 * @note 不解析外部模块或导入类型，返回 AST 节点仍由脚本树拥有。
 */
static SZrAstNode *metadata_token_find_union_declaration_in_array(SZrAstNodeArray *declarations,
                                                                  SZrString *typeName) {
    if (declarations == ZR_NULL || declarations->nodes == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = 0; index < declarations->count; index++) {
        SZrAstNode *declaration = declarations->nodes[index];

        if (declaration == ZR_NULL) {
            continue;
        }
        if (declaration->type == ZR_AST_UNION_DECLARATION &&
            declaration->data.unionDeclaration.name != ZR_NULL &&
            declaration->data.unionDeclaration.name->name != ZR_NULL &&
            ZrCore_String_Equal(declaration->data.unionDeclaration.name->name, typeName)) {
            return declaration;
        }
        if (declaration->type == ZR_AST_EXTERN_BLOCK) {
            SZrAstNode *match = metadata_token_find_union_declaration_in_array(
                    declaration->data.externBlock.declarations,
                    typeName);
            if (match != ZR_NULL) {
                return match;
            }
        }
    }

    return ZR_NULL;
}

/**
 * @brief 将本地 union 类型名拆为基类型名及按源码顺序排列的泛型实参。
 * @pre cs 属于有效脚本编译；两个输出指针均可写。
 * @return 成功时把实参数组所有权交给调用方，由 cs->state 释放；失败输出不得读取。
 * @note 声明查找递归穿过 extern block，但仅限当前脚本 AST。
 */
TZrBool metadata_token_try_resolve_union_signature_type(SZrCompilerState *cs,
                                                        SZrString *typeName,
                                                        SZrString **outBaseName,
                                                        SZrArray *outArgumentTypeNames) {
    SZrArray argumentTypeNames;
    SZrString *baseName = ZR_NULL;
    SZrString *lookupName;
    TZrBool parsedGeneric;

    if (outBaseName != ZR_NULL) {
        *outBaseName = ZR_NULL;
    }
    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->scriptAst == ZR_NULL ||
        cs->scriptAst->type != ZR_AST_SCRIPT || typeName == ZR_NULL ||
        outBaseName == ZR_NULL || outArgumentTypeNames == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrCore_Array_Construct(&argumentTypeNames);
    parsedGeneric = try_parse_generic_instance_type_name(cs->state, typeName, &baseName, &argumentTypeNames);
    lookupName = parsedGeneric && baseName != ZR_NULL ? baseName : typeName;
    if (metadata_token_find_union_declaration_in_array(cs->scriptAst->data.script.statements, lookupName) == ZR_NULL) {
        ZrCore_Array_Free(cs->state, &argumentTypeNames);
        return ZR_FALSE;
    }

    *outBaseName = lookupName;
    *outArgumentTypeNames = argumentTypeNames;
    return ZR_TRUE;
}

/** @brief 按 writer 相同的类型映射逐项计长，保证泛型实参预留长度与写出对齐。 */
static TZrSize metadata_token_generic_argument_signatures_size(SZrCompilerState *cs, SZrArray *argumentTypeNames) {
    TZrSize size = 0;

    if (argumentTypeNames == ZR_NULL) {
        return 0;
    }

    for (TZrSize index = 0; index < argumentTypeNames->length; index++) {
        SZrString **argumentTypeNamePtr = (SZrString **)ZrCore_Array_Get(argumentTypeNames, index);
        SZrFunctionTypedTypeRef argumentTypeRef;

        metadata_token_type_ref_from_name(argumentTypeNamePtr != ZR_NULL ? *argumentTypeNamePtr : ZR_NULL,
                                          &argumentTypeRef);
        size += metadata_token_type_ref_signature_size(cs, &argumentTypeRef);
    }

    return size;
}

/** @brief 按解析器给出的原始顺序逐项写泛型类型树，与计长 helper 配对。 */
static void metadata_token_write_generic_argument_signatures(TZrByte *buffer,
                                                            TZrSize *offset,
                                                            SZrCompilerState *cs,
                                                            SZrArray *argumentTypeNames,
                                                            const SZrMetadataStringHeapEntry *stringHeapEntries,
                                                            TZrUInt32 stringHeapEntryCount) {
    if (argumentTypeNames == ZR_NULL) {
        return;
    }

    for (TZrSize index = 0; index < argumentTypeNames->length; index++) {
        SZrString **argumentTypeNamePtr = (SZrString **)ZrCore_Array_Get(argumentTypeNames, index);
        SZrFunctionTypedTypeRef argumentTypeRef;

        metadata_token_type_ref_from_name(argumentTypeNamePtr != ZR_NULL ? *argumentTypeNamePtr : ZR_NULL,
                                          &argumentTypeRef);
        metadata_token_write_type_ref_signature(buffer,
                                                offset,
                                                cs,
                                                &argumentTypeRef,
                                                stringHeapEntries,
                                                stringHeapEntryCount);
    }
}

/**
 * @brief 递归计算 token、TypeDef、TypeSpec 共用的类型树编码长度。
 * @pre 编译上下文与类型引用须和随后写出阶段一致；调用方在分配前检查聚合堆上限。
 * TODO: 多层及实参长度累加未检查 TZrSize 溢出；核实所有目标平台的输入上限可排除此情况。
 */
TZrSize metadata_token_type_ref_signature_size(SZrCompilerState *cs, const SZrFunctionTypedTypeRef *typeRef) {
    TZrSize typeNameLength;

    /* 空引用与普通 object 使用同一占位；size 和 writer 必须共同维护此兜底格式。 */
    if (typeRef == ZR_NULL) {
        return 1 + sizeof(TZrUInt32);
    }

    /* nullable、ownership、array 按固定优先级递归退壳，直到唯一叶类型分派。 */
    if (typeRef->isNullable) {
        SZrFunctionTypedTypeRef nested = *typeRef;
        nested.isNullable = ZR_FALSE;
        return 1 + metadata_token_type_ref_signature_size(cs, &nested);
    }

    if (typeRef->ownershipQualifier != ZR_OWNERSHIP_QUALIFIER_NONE) {
        SZrFunctionTypedTypeRef nested = *typeRef;
        nested.ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
        return 1 + sizeof(TZrUInt32) + metadata_token_type_ref_signature_size(cs, &nested);
    }

    if (typeRef->isArray) {
        SZrFunctionTypedTypeRef element;

        ZrCore_Memory_RawSet(&element, 0, sizeof(element));
        element.baseType = typeRef->elementBaseType;
        element.elementBaseType = ZR_VALUE_TYPE_OBJECT;
        element.typeName = typeRef->elementTypeName;
        return 1 + sizeof(TZrUInt32) + metadata_token_type_ref_signature_size(cs, &element);
    }

    typeNameLength = metadata_token_string_length(typeRef->typeName);
    if (typeNameLength > 0) {
        SZrString *unionBaseName = ZR_NULL;
        SZrArray unionArgumentTypeNames;
        SZrString *genericBaseName = ZR_NULL;
        SZrArray genericArgumentTypeNames;

        /* 只有当前脚本声明树中的 union 才走 union frame；否则继续普通类型分派。 */
        if (metadata_token_try_resolve_union_signature_type(cs,
                                                            typeRef->typeName,
                                                            &unionBaseName,
                                                            &unionArgumentTypeNames)) {
            TZrSize size = 1 + sizeof(TZrUInt32) + sizeof(TZrUInt32) + sizeof(TZrUInt32);

            for (TZrSize index = 0; index < unionArgumentTypeNames.length; index++) {
                SZrString **argumentTypeNamePtr =
                        (SZrString **)ZrCore_Array_Get(&unionArgumentTypeNames, index);
                SZrFunctionTypedTypeRef argumentTypeRef;

                metadata_token_type_ref_from_name(
                        argumentTypeNamePtr != ZR_NULL ? *argumentTypeNamePtr : ZR_NULL,
                        &argumentTypeRef);
                size += metadata_token_type_ref_signature_size(cs, &argumentTypeRef);
            }
            ZrCore_Array_Free(cs->state, &unionArgumentTypeNames);
            return size;
        }
        /* 本地 union 优先于一般泛型实例；两种 frame 都分别计入实参子树。 */
        if (cs != ZR_NULL && cs->state != ZR_NULL &&
            try_parse_generic_instance_type_name(cs->state,
                                                 typeRef->typeName,
                                                 &genericBaseName,
                                                 &genericArgumentTypeNames)) {
            TZrSize openTypeLength = 1 + sizeof(TZrUInt32) + sizeof(TZrUInt32);
            TZrSize argumentLength =
                    metadata_token_generic_argument_signatures_size(cs, &genericArgumentTypeNames);

            ZrCore_Array_Free(cs->state, &genericArgumentTypeNames);
            return 1 + openTypeLength + sizeof(TZrUInt32) + argumentLength;
        }

        /* 未识别为本地 union 或泛型实例的命名类型退回普通 TypeRef frame。 */
        return 1 + sizeof(TZrUInt32) + sizeof(TZrUInt32);
    }

    return 1 + sizeof(TZrUInt32);
}

/**
 * @brief 递归写出一个类型树，并按同一编译上下文解析本地 union 与泛型实例。
 * @pre buffer 容量须覆盖配对 size 结果；类型、编译上下文和字符串堆快照与计长阶段一致。
 * @note nullable、ownership、array 外壳按计长阶段相同顺序逐层退壳；输入须保持存活至同步写出完成。
 */
void metadata_token_write_type_ref_signature(TZrByte *buffer,
                                             TZrSize *offset,
                                             SZrCompilerState *cs,
                                             const SZrFunctionTypedTypeRef *typeRef,
                                             const SZrMetadataStringHeapEntry *stringHeapEntries,
                                             TZrUInt32 stringHeapEntryCount) {
    TZrNativeString typeNameText;
    TZrSize typeNameLength;

    /* 空引用与普通 object 使用同一占位；size 和 writer 必须共同维护此兜底格式。 */
    if (typeRef == ZR_NULL) {
        metadata_token_write_u8(buffer, offset, ZR_METADATA_SIGNATURE_NODE_PRIMITIVE);
        metadata_token_write_u32(buffer, offset, (TZrUInt32)ZR_VALUE_TYPE_OBJECT);
        return;
    }

    /* nullable、ownership、array 按固定优先级递归退壳，直到唯一叶类型分派。 */
    if (typeRef->isNullable) {
        SZrFunctionTypedTypeRef nested = *typeRef;
        nested.isNullable = ZR_FALSE;
        metadata_token_write_u8(buffer, offset, ZR_METADATA_SIGNATURE_NODE_NULLABLE);
        metadata_token_write_type_ref_signature(buffer,
                                                offset,
                                                cs,
                                                &nested,
                                                stringHeapEntries,
                                                stringHeapEntryCount);
        return;
    }

    if (typeRef->ownershipQualifier != ZR_OWNERSHIP_QUALIFIER_NONE) {
        SZrFunctionTypedTypeRef nested = *typeRef;
        nested.ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
        metadata_token_write_u8(buffer, offset, ZR_METADATA_SIGNATURE_NODE_OWNERSHIP);
        metadata_token_write_u32(buffer, offset, typeRef->ownershipQualifier);
        metadata_token_write_type_ref_signature(buffer,
                                                offset,
                                                cs,
                                                &nested,
                                                stringHeapEntries,
                                                stringHeapEntryCount);
        return;
    }

    if (typeRef->isArray) {
        SZrFunctionTypedTypeRef element;

        ZrCore_Memory_RawSet(&element, 0, sizeof(element));
        element.baseType = typeRef->elementBaseType;
        element.elementBaseType = ZR_VALUE_TYPE_OBJECT;
        element.typeName = typeRef->elementTypeName;
        metadata_token_write_u8(buffer, offset, ZR_METADATA_SIGNATURE_NODE_ARRAY);
        metadata_token_write_u32(buffer, offset, 1u);
        metadata_token_write_type_ref_signature(buffer,
                                                offset,
                                                cs,
                                                &element,
                                                stringHeapEntries,
                                                stringHeapEntryCount);
        return;
    }

    typeNameText = metadata_token_string_text(typeRef->typeName);
    typeNameLength = typeNameText != ZR_NULL ? strlen(typeNameText) : 0;
    if (typeNameLength > 0) {
        SZrString *unionBaseName = ZR_NULL;
        SZrArray unionArgumentTypeNames;
        SZrString *genericBaseName = ZR_NULL;
        SZrArray genericArgumentTypeNames;

        /* 只有当前脚本声明树中的 union 才走 union frame；否则继续普通类型分派。 */
        if (metadata_token_try_resolve_union_signature_type(cs,
                                                            typeRef->typeName,
                                                            &unionBaseName,
                                                            &unionArgumentTypeNames)) {
            metadata_token_write_u8(buffer, offset, ZR_METADATA_SIGNATURE_NODE_UNION);
            metadata_token_write_u32(buffer, offset, (TZrUInt32)typeRef->baseType);
            metadata_token_write_string_ref(buffer,
                                            offset,
                                            unionBaseName,
                                            stringHeapEntries,
                                            stringHeapEntryCount);
            metadata_token_write_u32(buffer, offset, (TZrUInt32)unionArgumentTypeNames.length);

            for (TZrSize index = 0; index < unionArgumentTypeNames.length; index++) {
                SZrString **argumentTypeNamePtr =
                        (SZrString **)ZrCore_Array_Get(&unionArgumentTypeNames, index);
                SZrFunctionTypedTypeRef argumentTypeRef;

                metadata_token_type_ref_from_name(
                        argumentTypeNamePtr != ZR_NULL ? *argumentTypeNamePtr : ZR_NULL,
                        &argumentTypeRef);
                metadata_token_write_type_ref_signature(buffer,
                                                        offset,
                                                        cs,
                                                        &argumentTypeRef,
                                                        stringHeapEntries,
                                                        stringHeapEntryCount);
            }
            ZrCore_Array_Free(cs->state, &unionArgumentTypeNames);
            return;
        }
        /* 先识别本地 union，再识别普通 generic instance，保持与计长阶段相同的分派次序。 */
        if (cs != ZR_NULL && cs->state != ZR_NULL &&
            try_parse_generic_instance_type_name(cs->state,
                                                 typeRef->typeName,
                                                 &genericBaseName,
                                                 &genericArgumentTypeNames)) {
            metadata_token_write_u8(buffer, offset, ZR_METADATA_SIGNATURE_NODE_GENERIC_INST);
            metadata_token_write_u8(buffer, offset, ZR_METADATA_SIGNATURE_NODE_TYPE_REF);
            metadata_token_write_u32(buffer, offset, (TZrUInt32)typeRef->baseType);
            metadata_token_write_string_ref(buffer,
                                            offset,
                                            genericBaseName,
                                            stringHeapEntries,
                                            stringHeapEntryCount);
            metadata_token_write_u32(buffer, offset, (TZrUInt32)genericArgumentTypeNames.length);
            metadata_token_write_generic_argument_signatures(buffer,
                                                            offset,
                                                            cs,
                                                            &genericArgumentTypeNames,
                                                            stringHeapEntries,
                                                            stringHeapEntryCount);
            ZrCore_Array_Free(cs->state, &genericArgumentTypeNames);
            return;
        }

        /* 未识别为本地 union 或泛型实例的命名类型退回普通 TypeRef frame。 */
        metadata_token_write_u8(buffer, offset, ZR_METADATA_SIGNATURE_NODE_TYPE_REF);
        metadata_token_write_u32(buffer, offset, (TZrUInt32)typeRef->baseType);
        metadata_token_write_string_ref(buffer,
                                        offset,
                                        typeRef->typeName,
                                        stringHeapEntries,
                                        stringHeapEntryCount);
        return;
    }

    metadata_token_write_u8(buffer, offset, ZR_METADATA_SIGNATURE_NODE_PRIMITIVE);
    metadata_token_write_u32(buffer, offset, (TZrUInt32)typeRef->baseType);
}

/**
 * @brief 计算导出方法及导入 effect 共用的方法签名 frame 长度。
 * @pre 参数数量、返回类型和参数数组须与随后 writer 输入相同；参数数组可空时按 object 占位计长。
 * TODO: 参数签名长度以 TZrSize 累加而未检查溢出；核实调用方数量及堆上限足以排除溢出。
 */
TZrSize metadata_token_method_signature_size(SZrCompilerState *cs,
                                             const SZrFunctionTypedTypeRef *returnType,
                                             TZrUInt32 genericParameterCount,
                                             TZrUInt32 parameterCount,
                                             const SZrFunctionTypedTypeRef *parameterTypes) {
    TZrSize size;

    /* writer 的固定头部预留 generic arity 字节；参数缺数组时各项按 object 占位计长。 */
    ZR_UNUSED_PARAMETER(genericParameterCount);
    size = 1 + 1 + 1 + sizeof(TZrUInt32) +
           metadata_token_type_ref_signature_size(cs, returnType) +
           sizeof(TZrUInt32);
    for (TZrUInt32 index = 0; index < parameterCount; index++) {
        const SZrFunctionTypedTypeRef *parameterType =
                parameterTypes != ZR_NULL ? &parameterTypes[index] : ZR_NULL;
        size += 1 + metadata_token_type_ref_signature_size(cs, parameterType);
    }
    return size;
}

/**
 * @brief 写出方法 frame、返回类型及有序参数类型，供导出和导入 effect 共用。
 * @pre 目标区容量覆盖配对 size 结果，参数及堆快照在同步写出期间保持有效。
 * BUG: 合法非零泛型方法 blob 放入合法 ZRP signature pool，并经 ZrCore_MetadataRuntime_AttachZrpMetadata 附加后，ReadSignatureView 将 writer 的 arity 当作 flags，并从后续零 u32 读出元数 0；TODO: 核查 compiler raw signatureBlobHeap 到该 pool 的装载入口，自动桥接尚未证实。
 * TODO: arity 大于 255 时被饱和为 0xff；核实上游限制或定义扩展编码。
 */
void metadata_token_write_method_signature(TZrByte *buffer,
                                           TZrSize *offset,
                                           SZrCompilerState *cs,
                                           const SZrFunctionTypedTypeRef *returnType,
                                           TZrUInt32 genericParameterCount,
                                           TZrUInt32 parameterCount,
                                           const SZrFunctionTypedTypeRef *parameterTypes,
                                           const SZrMetadataStringHeapEntry *stringHeapEntries,
                                           TZrUInt32 stringHeapEntryCount) {
    /* METHOD_SIG 头字段需与 core reader 的 flags/arity 偏移保持一致；当前布局见 BUG。 */
    metadata_token_write_u8(buffer, offset, ZR_METADATA_SIGNATURE_NODE_METHOD_SIG);
    metadata_token_write_u8(buffer, offset, 1u);
    metadata_token_write_u8(buffer,
                            offset,
                            genericParameterCount <= 0xFFu
                                    ? (TZrUInt8)genericParameterCount
                                    : 0xFFu);
    metadata_token_write_u32(buffer, offset, 0u);
    metadata_token_write_type_ref_signature(buffer,
                                            offset,
                                            cs,
                                            returnType,
                                            stringHeapEntries,
                                            stringHeapEntryCount);
    metadata_token_write_u32(buffer, offset, parameterCount);
    for (TZrUInt32 index = 0; index < parameterCount; index++) {
        const SZrFunctionTypedTypeRef *parameterType =
                parameterTypes != ZR_NULL ? &parameterTypes[index] : ZR_NULL;
        metadata_token_write_u8(buffer, offset, 0u);
        metadata_token_write_type_ref_signature(buffer,
                                                offset,
                                                cs,
                                                parameterType,
                                                stringHeapEntries,
                                                stringHeapEntryCount);
    }
}

/** @brief 计算字段/属性 frame 长度；值类型须与随后字段 writer 使用同一快照。 */
TZrSize metadata_token_field_signature_size(SZrCompilerState *cs,
                                            const SZrFunctionTypedTypeRef *valueType) {
    return 1 + 1 + metadata_token_type_ref_signature_size(cs, valueType);
}

/** @brief 写出反射和 metadata reader 共用的字段 frame 及值类型树。 */
void metadata_token_write_field_signature(TZrByte *buffer,
                                          TZrSize *offset,
                                          SZrCompilerState *cs,
                                          const SZrFunctionTypedTypeRef *valueType,
                                          const SZrMetadataStringHeapEntry *stringHeapEntries,
                                          TZrUInt32 stringHeapEntryCount) {
    metadata_token_write_u8(buffer, offset, ZR_METADATA_SIGNATURE_NODE_FIELD_SIG);
    metadata_token_write_u8(buffer, offset, 1u);
    metadata_token_write_type_ref_signature(buffer,
                                            offset,
                                            cs,
                                            valueType,
                                            stringHeapEntries,
                                            stringHeapEntryCount);
}

/** @brief 按导出符号种类选择方法或字段计长路径，与写出 dispatch 成对维护。 */
TZrSize metadata_token_symbol_signature_size(SZrCompilerState *cs, const SZrFunctionTypedExportSymbol *symbol) {
    if (symbol == ZR_NULL) {
        return 0;
    }

    /* 非 function 统一走字段 frame；function 才包含返回值与有序参数表。 */
    if (symbol->symbolKind != ZR_FUNCTION_TYPED_SYMBOL_FUNCTION) {
        return metadata_token_field_signature_size(cs, &symbol->valueType);
    }

    return metadata_token_method_signature_size(cs,
                                                 &symbol->valueType,
                                                 symbol->genericParameterCount,
                                                 symbol->parameterCount,
                                                symbol->parameterTypes);
}

/** @brief 按导出符号种类分派方法/字段 writer，保持 token planner 的布局顺序。 */
void metadata_token_write_symbol_signature(TZrByte *buffer,
                                           TZrSize *offset,
                                           SZrCompilerState *cs,
                                           const SZrFunctionTypedExportSymbol *symbol,
                                           const SZrMetadataStringHeapEntry *stringHeapEntries,
                                           TZrUInt32 stringHeapEntryCount) {
    if (symbol == ZR_NULL) {
        return;
    }

    /* 必须和 size dispatcher 使用相同判别，避免符号 frame 计长/写出错位。 */
    if (symbol->symbolKind != ZR_FUNCTION_TYPED_SYMBOL_FUNCTION) {
        metadata_token_write_field_signature(buffer,
                                             offset,
                                             cs,
                                             &symbol->valueType,
                                             stringHeapEntries,
                                             stringHeapEntryCount);
        return;
    }

    metadata_token_write_method_signature(buffer,
                                          offset,
                                          cs,
                                          &symbol->valueType,
                                          symbol->genericParameterCount,
                                          symbol->parameterCount,
                                          symbol->parameterTypes,
                                          stringHeapEntries,
                                          stringHeapEntryCount);
}

/** @brief 将固定宽度字符串堆键写入调用方已收集的快照，不在写出阶段扩充堆。 */
void metadata_token_write_string_ref(TZrByte *buffer,
                                     TZrSize *offset,
                                     SZrString *value,
                                     const SZrMetadataStringHeapEntry *stringHeapEntries,
                                     TZrUInt32 stringHeapEntryCount) {
    TZrUInt32 stringIndex = metadata_token_string_heap_index(stringHeapEntries, stringHeapEntryCount, value);

    metadata_token_write_u32(buffer, offset, stringIndex);
}
