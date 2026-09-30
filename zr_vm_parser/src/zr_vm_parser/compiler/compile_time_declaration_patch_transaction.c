#include "compile_time_declaration_patch_transaction.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

#include <stdint.h>

/*
 * 声明补丁先写入 detached arrays 与语义上下文副本；生成成员、接口列表、decorator
 * 列表和属性 metadata 全部准备成功且 observer 放行后，才在函数尾部发布到目标。
 * 事务根持有已纳入准备状态的 GC 值；借用的原生文本仍须由调用方保证跨分配有效。
 * 失败清理释放未转移数组并结束根；
 * 此边界保护目标声明状态，不回滚 observer 自身的外部副作用。
 */

/* 同步兼容包装器使用的回调适配器；实例只在包装器栈帧内有效。 */
typedef struct SZrGeneratedCommitObserverAdapter {
    FZrParserDeclarationPatchCommitObserver observer;
    TZrPtr userData;
} SZrGeneratedCommitObserverAdapter;

/* 校验源数组与容量算术后复制到独立数组，预留本次追加空间且不触碰 source。 */
static TZrBool patch_transaction_clone_array(
        SZrCompilerState *cs,
        const SZrArray *source,
        TZrSize additionalCount,
        SZrArray *detached) {
    TZrSize requiredCapacity;
    TZrSize byteCapacity;

    if (cs == ZR_NULL || cs->state == ZR_NULL || source == ZR_NULL ||
        detached == ZR_NULL || !source->isValid || source->head == ZR_NULL ||
        source->elementSize == 0U || source->length > source->capacity ||
        additionalCount > SIZE_MAX - source->length) {
        return ZR_FALSE;
    }
    ZrCore_Array_Construct(detached);
    requiredCapacity = source->length + additionalCount;
    if (requiredCapacity == 0U) {
        requiredCapacity = 1U;
    }
    if (requiredCapacity > SIZE_MAX / source->elementSize) {
        return ZR_FALSE;
    }
    byteCapacity = requiredCapacity * source->elementSize;
    detached->head = (TZrBytePtr)ZrCore_Memory_RawMallocWithType(
            cs->state->global,
            byteCapacity,
            ZR_MEMORY_NATIVE_TYPE_ARRAY);
    if (detached->head == ZR_NULL) {
        ZrCore_Array_Construct(detached);
        return ZR_FALSE;
    }
    if (source->length > 0U) {
        ZrCore_Memory_RawCopy(
                detached->head,
                source->head,
                source->length * source->elementSize);
    }
    detached->elementSize = source->elementSize;
    detached->length = source->length;
    detached->capacity = requiredCapacity;
    detached->isValid = ZR_TRUE;
    return ZR_TRUE;
}

/* 提交尾段转移 detached 所有权并释放旧数组；调用前所有可失败准备必须完成。 */
static void patch_transaction_publish_array(
        SZrState *state,
        SZrArray *target,
        SZrArray *detached) {
    SZrArray previous = *target;

    *target = *detached;
    ZrCore_Array_Construct(detached);
    ZrCore_Array_Free(state, &previous);
}

/* 写 metadata 字段时临时根住新 key，并用读回确认对象接受了该键值。 */
static TZrBool patch_transaction_set_object_field(
        SZrCompilerState *cs,
        SZrObject *object,
        const TZrChar *name,
        const SZrTypeValue *value) {
    ZrExternCompilerTempRoot keyRoot = {0};
    SZrString *keyString;
    SZrTypeValue key;
    TZrBool result = ZR_FALSE;

    if (cs == ZR_NULL || object == ZR_NULL || name == ZR_NULL ||
        value == ZR_NULL ||
        !extern_compiler_temp_root_begin(cs, &keyRoot)) {
        return ZR_FALSE;
    }
    keyString = ZrCore_String_CreateFromNative(
            cs->state, (TZrNativeString)name);
    if (keyString == ZR_NULL) {
        goto cleanup;
    }
    ZrCore_Value_InitAsRawObject(
            cs->state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    key.type = ZR_VALUE_TYPE_STRING;
    if (!extern_compiler_temp_root_set_value(&keyRoot, &key)) {
        goto cleanup;
    }
    ZrCore_Object_SetValue(cs->state, object, &key, value);
    result = ZrCore_Object_GetValue(cs->state, object, &key) != ZR_NULL;

cleanup:
    extern_compiler_temp_root_end(&keyRoot);
    return result;
}

/* 将 GC 值追加为 roots 的整数索引项，并确认索引可表达且写入已可读。 */
static TZrBool patch_transaction_push_root_value(
        SZrCompilerState *cs,
        SZrObject *roots,
        const SZrTypeValue *value) {
    const TZrSize index = roots != ZR_NULL
                                  ? ZrCore_Object_SuperArrayLength(roots)
                                  : 0U;
    SZrTypeValue key;

    if (cs == ZR_NULL || roots == ZR_NULL || value == ZR_NULL ||
        index > (TZrSize)INT64_MAX || index == SIZE_MAX ||
        !extern_compiler_push_array_value(cs, roots, value)) {
        return ZR_FALSE;
    }
    ZrCore_Value_InitAsInt(cs->state, &key, (TZrInt64)index);
    return ZrCore_Object_SuperArrayLength(roots) == index + 1U &&
           ZrCore_Object_GetValue(cs->state, roots, &key) != ZR_NULL;
}

/* 在把借用字符串加入事务持久根数组期间，用栈根保护它免于 GC 回收。 */
static TZrBool patch_transaction_hold_string(
        SZrCompilerState *cs,
        SZrObject *roots,
        SZrString *string) {
    ZrExternCompilerTempRoot stringRoot = {0};
    SZrTypeValue value;
    TZrBool result = ZR_FALSE;

    if (cs == ZR_NULL || roots == ZR_NULL || string == ZR_NULL ||
        !extern_compiler_temp_root_begin(cs, &stringRoot)) {
        return ZR_FALSE;
    }
    ZrCore_Value_InitAsRawObject(
            cs->state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(string));
    value.type = ZR_VALUE_TYPE_STRING;
    if (extern_compiler_temp_root_set_value(&stringRoot, &value)) {
        result = patch_transaction_push_root_value(cs, roots, &value);
    }
    extern_compiler_temp_root_end(&stringRoot);
    return result;
}

/* text 是借用的原生 C 字符串，调用方须保证其字节跨可触发 GC 的创建操作仍有效。 */
/* 只有临时根到事务根的交接成功才写 result。 */
static TZrBool patch_transaction_create_rooted_string(
        SZrCompilerState *cs,
        SZrObject *roots,
        const TZrChar *text,
        SZrString **result) {
    ZrExternCompilerTempRoot candidateRoot = {0};
    SZrString *string = ZR_NULL;
    SZrTypeValue value;
    TZrBool success = ZR_FALSE;

    if (cs == ZR_NULL || roots == ZR_NULL || text == ZR_NULL ||
        result == ZR_NULL ||
        !extern_compiler_temp_root_begin(cs, &candidateRoot)) {
        return ZR_FALSE;
    }
    string = ZrCore_String_CreateFromNative(
            cs->state, (TZrNativeString)text);
    if (string == ZR_NULL) {
        goto cleanup;
    }
    ZrCore_Value_InitAsRawObject(
            cs->state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(string));
    value.type = ZR_VALUE_TYPE_STRING;
    if (!extern_compiler_temp_root_set_value(&candidateRoot, &value) ||
        !patch_transaction_push_root_value(cs, roots, &value)) {
        goto cleanup;
    }
    *result = string;
    success = ZR_TRUE;

cleanup:
    extern_compiler_temp_root_end(&candidateRoot);
    return success;
}

/* primitive 字段按 canonical value kind 取本机大小，其他情况采用通用 TypeValue 大小。 */
static TZrUInt32 patch_transaction_generated_field_size(
        SZrCompilerState *cs,
        TZrTypeId typeId) {
    const SZrCanonicalTypeNode *type =
            ZrParser_CanonicalType_Find(cs->semanticContext, typeId);

    if (type == ZR_NULL || type->kind != ZR_CANONICAL_TYPE_PRIMITIVE) {
        return sizeof(SZrTypeValue);
    }
    switch (type->data.primitive.valueType) {
        case ZR_VALUE_TYPE_INT8: return sizeof(TZrInt8);
        case ZR_VALUE_TYPE_INT16: return sizeof(TZrInt16);
        case ZR_VALUE_TYPE_INT32: return sizeof(TZrInt32);
        case ZR_VALUE_TYPE_INT64: return sizeof(TZrInt64);
        case ZR_VALUE_TYPE_UINT8: return sizeof(TZrUInt8);
        case ZR_VALUE_TYPE_UINT16: return sizeof(TZrUInt16);
        case ZR_VALUE_TYPE_UINT32: return sizeof(TZrUInt32);
        case ZR_VALUE_TYPE_UINT64: return sizeof(TZrUInt64);
        case ZR_VALUE_TYPE_FLOAT: return sizeof(TZrFloat32);
        case ZR_VALUE_TYPE_DOUBLE: return sizeof(TZrDouble);
        case ZR_VALUE_TYPE_BOOL: return sizeof(TZrBool);
        default: return sizeof(SZrTypeValue);
    }
}

/* 构造尚未发布的生成字段记录，并把成员名称及来源 metadata 纳入事务根。 */
static TZrBool patch_transaction_prepare_generated_field(
        SZrCompilerState *cs,
        const SZrTypePrototypeInfo *targetInfo,
        const SZrParserGeneratedDeclaration *addition,
        SZrString *canonicalTypeName,
        TZrSize declarationOrder,
        TZrSymbolId originTargetSymbolId,
        SZrFileRange location,
        SZrObject *roots,
        SZrTypeMemberInfo *member) {
    ZrExternCompilerTempRoot metadataRoot = {0};
    SZrObject *metadataObject;
    SZrTypeValue metadataValue;
    TZrBool success = ZR_FALSE;

    if (cs == ZR_NULL || targetInfo == ZR_NULL || addition == ZR_NULL ||
        canonicalTypeName == ZR_NULL || roots == ZR_NULL || member == ZR_NULL ||
        addition->kind != ZR_PARSER_GENERATED_DECLARATION_FIELD ||
        addition->name == ZR_NULL ||
        !patch_transaction_hold_string(cs, roots, canonicalTypeName)) {
        return ZR_FALSE;
    }
    /* TODO: 此 helper 对非 struct 一律生成 class-field 记录，而 executor 也会给 union/enum
     * 执行 type transform；核对 declaration.Patch 是否应限制类型种类，并追踪 union/enum 的
     * members 消费路径，确认可接受种类后再加约束或按目标种类构造成员。 */
    /* TODO: 核对 generated declaration 的 initializer/attributes 是否属于此提交接口的
     * 支持面；当前 executor 解码器只填基本字段，先审实际 API 调用方与 member 消费者。 */
    ZrCore_Memory_RawSet(member, 0, sizeof(*member));
    member->memberType = targetInfo->type == ZR_OBJECT_PROTOTYPE_TYPE_STRUCT
                                 ? ZR_AST_STRUCT_FIELD
                                 : ZR_AST_CLASS_FIELD;
     /* BUG: declarationTransform AST runner 把 Patch 结果只写入原生 patchValues（不被 GC 扫描），并在返回前释放 CompileTool frame；
      * decoder 将 GeneratedField.name 借为 native bytes；该 Patch/name 未由 VM stack/call-frame、AOT、global/cache/string-table/ignored/domain root 保活。
      * prepare helper 只把 canonicalTypeName 放入 roots；合法 ASCII 标识符没有长度上限，可超过 ZR_VM_SHORT_STRING_MAX。
      * CreateFromNative 前 candidateRoot slot 仍为 null，新字符串返回后才写根，故输入 text bytes 尚未受保护。
      * 只有长串对象首次 RawMalloc 失败并触发 GcAndMalloc，且 gcMode 为默认 INCREMENTAL、起始
      * isImmediateGcFlag=false、stopGcFlag=false、STW 成功时，FullGC 才进入该收集路径；每个 full_inc
      * target 还必须未被 stop 标志或迭代上限短路，完整 atomic/sweep 才会回收无根 Patch/name。
      * 随后对象重试和 longString 缓冲分配均成功，string.c 会从已释放的 longString 源复制字节，可能产生错误字段名。
      * CompileTool frame/patchValues 均是 native 临时存储，且此 AST runner 不走普通 comptime-call cache。
      * 这是静态可达链的条件结论，未运行复现。 */
    if (!patch_transaction_create_rooted_string(
                cs, roots, addition->name, &member->name)) {
        return ZR_FALSE;
    }
    member->accessModifier =
            addition->visibility == ZR_PARSER_GENERATED_VISIBILITY_PUBLIC
                    ? ZR_ACCESS_PUBLIC
                    : addition->visibility ==
                                      ZR_PARSER_GENERATED_VISIBILITY_PROTECTED
                              ? ZR_ACCESS_PROTECTED
                              : ZR_ACCESS_PRIVATE;
    member->isConst = addition->mutability ==
                      ZR_PARSER_GENERATED_MUTABILITY_LET;
    member->ownershipQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    member->gcBridgeKind = ZR_GC_BRIDGE_NONE;
    member->receiverQualifier = ZR_OWNERSHIP_QUALIFIER_NONE;
    member->receiverEffect = ZR_CANONICAL_RECEIVER_NONE;
    member->declarationOrder = (TZrUInt32)declarationOrder;
    member->fieldTypeName = canonicalTypeName;
    member->fieldSize =
            patch_transaction_generated_field_size(cs, addition->typeId);
    member->virtualSlotIndex = UINT32_MAX;
    member->interfaceContractSlot = UINT32_MAX;
    member->propertyIdentity = UINT32_MAX;
    member->propertySymbolId = ZR_SEMANTIC_ID_INVALID;
    member->getterAccessorSymbolId = ZR_SEMANTIC_ID_INVALID;
    member->setterAccessorSymbolId = ZR_SEMANTIC_ID_INVALID;
    member->initAccessorSymbolId = ZR_SEMANTIC_ID_INVALID;
    ZrCore_Array_Construct(&member->parameterTypes);
    ZrCore_Array_Construct(&member->parameterNames);
    ZrCore_Array_Construct(&member->parameterHasDefaultValues);
    ZrCore_Array_Construct(&member->parameterDefaultValues);
    ZrCore_Array_Construct(&member->genericParameters);
    ZrCore_Array_Construct(&member->parameterPassingModes);
    ZrCore_Array_Construct(&member->decorators);
    ZrCore_Value_ResetAsNull(&member->decoratorMetadataValue);

    if (!extern_compiler_temp_root_begin(cs, &metadataRoot)) {
        return ZR_FALSE;
    }
    metadataObject = extern_compiler_new_object_constant(cs);
    if (metadataObject == ZR_NULL ||
        !extern_compiler_temp_root_set_object(
                &metadataRoot, metadataObject, ZR_VALUE_TYPE_OBJECT)) {
        goto cleanup;
    }
    ZrCore_Value_InitAsInt(cs->state, &metadataValue, 1);
    if (!patch_transaction_set_object_field(
                cs, metadataObject, "generated", &metadataValue)) {
        goto cleanup;
    }
    ZrCore_Value_InitAsUInt(
            cs->state, &metadataValue, originTargetSymbolId);
    if (!patch_transaction_set_object_field(
                cs,
                metadataObject,
                "originTargetSymbolId",
                &metadataValue)) {
        goto cleanup;
    }
    ZrCore_Value_InitAsInt(
            cs->state, &metadataValue, location.start.line);
    if (!patch_transaction_set_object_field(
                cs, metadataObject, "sourceLineStart", &metadataValue)) {
        goto cleanup;
    }
    ZrCore_Value_InitAsInt(
            cs->state, &metadataValue, location.end.line);
    if (!patch_transaction_set_object_field(
                cs, metadataObject, "sourceLineEnd", &metadataValue)) {
        goto cleanup;
    }
    ZrCore_Value_InitAsRawObject(
            cs->state,
            &member->decoratorMetadataValue,
            ZR_CAST_RAW_OBJECT_AS_SUPER(metadataObject));
    member->decoratorMetadataValue.type = ZR_VALUE_TYPE_OBJECT;
    member->hasDecoratorMetadata = ZR_TRUE;
    if (!patch_transaction_push_root_value(
                cs, roots, &member->decoratorMetadataValue)) {
        goto cleanup;
    }
    success = ZR_TRUE;

cleanup:
    extern_compiler_temp_root_end(&metadataRoot);
    return success;
}

/* 兼容接口只观察 generated 阶段；其他阶段与缺省 callback 均继续事务。 */
static TZrBool patch_transaction_generated_observer_adapter(
        EZrParserDeclarationPatchCommitStage stage,
        TZrSize committedCount,
        TZrPtr userData) {
    SZrGeneratedCommitObserverAdapter *adapter =
            (SZrGeneratedCommitObserverAdapter *)userData;

    if (stage != ZR_PARSER_DECLARATION_PATCH_COMMIT_GENERATED ||
        adapter == ZR_NULL || adapter->observer == ZR_NULL) {
        return ZR_TRUE;
    }
    return adapter->observer(committedCount, adapter->userData);
}

/* 旧式 generated-only 入口：用空接口/属性批次调用统一事务，并适配单阶段 observer。 */
TZrBool ZrParser_CompileTime_CommitGeneratedFieldsAtomic(
        SZrCompilerState *cs,
        SZrTypePrototypeInfo *targetInfo,
        const SZrParserGeneratedDeclaration *additions,
        SZrString *const *canonicalTypeNames,
        TZrSize additionCount,
        TZrSymbolId originTargetSymbolId,
        SZrFileRange location,
        FZrParserDeclarationPatchCommitObserver observer,
        TZrPtr observerUserData) {
    SZrParserCompileTimePatchInterfaceAdds interfaceAdds = {0};
    SZrParserCompileTimePatchAttributeAdds attributeAdds = {0};
    SZrGeneratedCommitObserverAdapter adapter;

    adapter.observer = observer;
    adapter.userData = observerUserData;
    return ZrParser_CompileTime_CommitDeclarationPatchAtomic(
            cs,
            targetInfo,
            additions,
            canonicalTypeNames,
            additionCount,
            &interfaceAdds,
            &attributeAdds,
            ZR_NULL,
            originTargetSymbolId,
            location,
            observer != ZR_NULL
                    ? patch_transaction_generated_observer_adapter
                    : ZR_NULL,
            &adapter);
}

TZrBool ZrParser_CompileTime_CommitDeclarationPatchAtomic(
        SZrCompilerState *cs,
        SZrTypePrototypeInfo *targetInfo,
        const SZrParserGeneratedDeclaration *additions,
        SZrString *const *canonicalTypeNames,
        TZrSize additionCount,
        const SZrParserCompileTimePatchInterfaceAdds *interfaceAdds,
        const SZrParserCompileTimePatchAttributeAdds *attributeAdds,
        SZrString *transformDecoratorName,
        TZrSymbolId originTargetSymbolId,
        SZrFileRange location,
        FZrParserDeclarationPatchTransactionObserver observer,
        TZrPtr observerUserData) {
    const TZrBool hasGenerated = additionCount > 0U;
    const TZrBool hasInterfaces =
            interfaceAdds != ZR_NULL && interfaceAdds->count > 0U;
    const TZrBool hasAttributes =
            attributeAdds != ZR_NULL && attributeAdds->count > 0U;
    const TZrBool hasDecorators =
            hasAttributes || transformDecoratorName != ZR_NULL;
    const TZrSize decoratorAdditionCount =
            (hasAttributes ? attributeAdds->count : 0U) +
            (transformDecoratorName != ZR_NULL ? 1U : 0U);
    SZrArray detachedMembers;
    SZrArray detachedSymbols;
    SZrArray detachedInherits;
    SZrArray detachedImplements;
    SZrArray detachedDecorators;
    SZrSemanticContext detachedContext = {0};
    ZrExternCompilerTempRoot rootsRoot = {0};
    ZrExternCompilerTempRoot metadataRoot = {0};
    SZrObject *roots = ZR_NULL;
    SZrTypeValue preparedDecoratorMetadata;
    TZrBool result = ZR_FALSE;

    /* 初始化 detached 状态后，检查参数、预算、目标数组与符号 ID 边界；空补丁不触碰目标即成功。 */
    ZrCore_Array_Construct(&detachedMembers);
    ZrCore_Array_Construct(&detachedSymbols);
    ZrCore_Array_Construct(&detachedInherits);
    ZrCore_Array_Construct(&detachedImplements);
    ZrCore_Array_Construct(&detachedDecorators);
    ZrCore_Value_ResetAsNull(&preparedDecoratorMetadata);
    if (cs == ZR_NULL || cs->state == ZR_NULL ||
        cs->semanticContext == ZR_NULL || targetInfo == ZR_NULL ||
        interfaceAdds == ZR_NULL || attributeAdds == ZR_NULL ||
        (hasGenerated &&
         (additions == ZR_NULL || canonicalTypeNames == ZR_NULL)) ||
        (hasInterfaces && interfaceAdds->typeNames == ZR_NULL) ||
        (hasAttributes && attributeAdds->entries == ZR_NULL) ||
        additionCount > ZR_PARSER_DECLARATION_TRANSFORM_MAX_ADDITIONS ||
        interfaceAdds->count > ZR_PARSER_DECLARATION_TRANSFORM_MAX_ADDITIONS ||
        attributeAdds->count > ZR_PARSER_DECLARATION_TRANSFORM_MAX_ADDITIONS ||
        (hasAttributes && attributeAdds->count == SIZE_MAX) ||
        targetInfo->members.length > UINT32_MAX ||
        additionCount > (TZrSize)UINT32_MAX - targetInfo->members.length ||
        additionCount > UINT32_MAX - cs->semanticContext->nextSymbolId) {
        return ZR_FALSE;
    }
    for (TZrSize index = 0; index < interfaceAdds->count; index++) {
        if (interfaceAdds->typeNames[index] == ZR_NULL) {
            return ZR_FALSE;
        }
    }
    for (TZrSize index = 0; index < attributeAdds->count; index++) {
        if (attributeAdds->entries[index].schema == ZR_NULL ||
            attributeAdds->entries[index].schema->name == ZR_NULL) {
            return ZR_FALSE;
        }
    }
    if (!hasGenerated && !hasInterfaces && !hasDecorators) {
        return ZR_TRUE;
    }

    /* 每类目标数组都先克隆，随后把 borrowed GC 字符串纳入 roots，避免准备时改写目标。 */
    if ((hasGenerated &&
         (!patch_transaction_clone_array(
                  cs, &targetInfo->members, additionCount, &detachedMembers) ||
          !patch_transaction_clone_array(
                  cs,
                  &cs->semanticContext->symbols,
                  additionCount,
                  &detachedSymbols))) ||
        (hasInterfaces &&
         (!patch_transaction_clone_array(
                  cs,
                  &targetInfo->inherits,
                  interfaceAdds->count,
                  &detachedInherits) ||
          !patch_transaction_clone_array(
                  cs,
                  &targetInfo->implements,
                  interfaceAdds->count,
                  &detachedImplements))) ||
        (hasDecorators &&
         !patch_transaction_clone_array(
                 cs,
                 &targetInfo->decorators,
                 decoratorAdditionCount,
                 &detachedDecorators))) {
        goto cleanup;
    }
    if (!extern_compiler_temp_root_begin(cs, &rootsRoot)) {
        goto cleanup;
    }
    roots = extern_compiler_new_array_constant(cs);
    if (roots == ZR_NULL ||
        !extern_compiler_temp_root_set_object(
                &rootsRoot, roots, ZR_VALUE_TYPE_ARRAY)) {
        goto cleanup;
    }
    if (transformDecoratorName != ZR_NULL &&
        !patch_transaction_hold_string(cs, roots, transformDecoratorName)) {
        goto cleanup;
    }
    for (TZrSize index = 0; index < interfaceAdds->count; index++) {
        if (!patch_transaction_hold_string(
                    cs, roots, interfaceAdds->typeNames[index])) {
            goto cleanup;
        }
    }
    for (TZrSize index = 0; index < attributeAdds->count; index++) {
        if (!patch_transaction_hold_string(
                    cs, roots, attributeAdds->entries[index].schema->name)) {
            goto cleanup;
        }
    }

    /* 属性 overlay 单独构造并先接入临时根；target metadata 保持原值直到发布尾段。 */
    if (hasAttributes) {
        if (!extern_compiler_temp_root_begin(cs, &metadataRoot) ||
            !ZrParser_CompileTime_BuildPatchAttributeMetadata(
                    cs,
                    targetInfo,
                    attributeAdds,
                    &preparedDecoratorMetadata) ||
            !extern_compiler_temp_root_set_value(
                    &metadataRoot, &preparedDecoratorMetadata)) {
            goto cleanup;
        }
    }
    if (hasGenerated) {
        /* 语义上下文副本只替换 symbols 数组，逐项登记 symbol/member 到 detached 状态。 */
        detachedContext = *cs->semanticContext;
        detachedContext.symbols = detachedSymbols;
        for (TZrSize index = 0; index < additionCount; index++) {
            SZrTypeMemberInfo member;
            TZrSymbolId symbolId;

            if (!patch_transaction_prepare_generated_field(
                        cs,
                        targetInfo,
                        &additions[index],
                        canonicalTypeNames[index],
                        targetInfo->members.length + index,
                        originTargetSymbolId,
                        location,
                        roots,
                        &member)) {
                goto cleanup;
            }
            symbolId = ZrParser_Semantic_ReserveSymbolId(&detachedContext);
            if (symbolId == ZR_SEMANTIC_ID_INVALID ||
                ZrParser_Semantic_RegisterSymbolWithId(
                        &detachedContext,
                        symbolId,
                        member.name,
                        ZR_SEMANTIC_SYMBOL_KIND_FIELD,
                        additions[index].typeId,
                        ZR_SEMANTIC_ID_INVALID,
                        ZR_NULL,
                        location) == ZR_SEMANTIC_ID_INVALID) {
                goto cleanup;
            }
            member.symbolId = symbolId;
            ZrCore_Array_Push(cs->state, &detachedMembers, &member);
        }
        detachedSymbols = detachedContext.symbols;
        if (observer != ZR_NULL &&
            !observer(
                    ZR_PARSER_DECLARATION_PATCH_COMMIT_GENERATED,
                    additionCount,
                    observerUserData)) {
            goto cleanup;
        }
    }
    if (hasInterfaces) {
        /* 两份接口列表成对追加；observer 检查时尚未对目标发布。 */
        for (TZrSize index = 0; index < interfaceAdds->count; index++) {
            SZrString *name = interfaceAdds->typeNames[index];

            ZrCore_Array_Push(cs->state, &detachedInherits, &name);
            ZrCore_Array_Push(cs->state, &detachedImplements, &name);
        }
        if (observer != ZR_NULL &&
            !observer(
                    ZR_PARSER_DECLARATION_PATCH_COMMIT_INTERFACES,
                    interfaceAdds->count,
                    observerUserData)) {
            goto cleanup;
        }
    }
    if (hasAttributes) {
        /* Attribute 名进入 detached decorator 列表；metadata overlay 仍由单独字段待发布。 */
        for (TZrSize index = 0; index < attributeAdds->count; index++) {
            SZrTypeDecoratorInfo decoratorInfo;

            decoratorInfo.name = attributeAdds->entries[index].schema->name;
            ZrCore_Array_Push(
                    cs->state, &detachedDecorators, &decoratorInfo);
        }
        if (observer != ZR_NULL &&
            !observer(
                    ZR_PARSER_DECLARATION_PATCH_COMMIT_ATTRIBUTES,
                    attributeAdds->count,
                    observerUserData)) {
            goto cleanup;
        }
    }
    if (transformDecoratorName != ZR_NULL) {
        SZrTypeDecoratorInfo decoratorInfo;

        decoratorInfo.name = transformDecoratorName;
        ZrCore_Array_Push(cs->state, &detachedDecorators, &decoratorInfo);
    }

    /* 所有准备与 observer 检查均已通过；以下发布操作不返回失败，之后只清理已转移资源。 */
    if (hasGenerated) {
        patch_transaction_publish_array(
                cs->state, &targetInfo->members, &detachedMembers);
        patch_transaction_publish_array(
                cs->state,
                &cs->semanticContext->symbols,
                &detachedSymbols);
        cs->semanticContext->nextSymbolId = detachedContext.nextSymbolId;
    }
    if (hasInterfaces) {
        patch_transaction_publish_array(
                cs->state, &targetInfo->inherits, &detachedInherits);
        patch_transaction_publish_array(
                cs->state, &targetInfo->implements, &detachedImplements);
    }
    if (hasDecorators) {
        patch_transaction_publish_array(
                cs->state, &targetInfo->decorators, &detachedDecorators);
    }
    if (hasAttributes) {
        targetInfo->decoratorMetadataValue = preparedDecoratorMetadata;
        targetInfo->hasDecoratorMetadata = ZR_TRUE;
    }
    result = ZR_TRUE;

cleanup:
    /* observer veto 或准备失败保留目标原状态；成功发布的数组已从 detached 中移走。 */
    extern_compiler_temp_root_end(&metadataRoot);
    extern_compiler_temp_root_end(&rootsRoot);
    ZrCore_Array_Free(cs->state, &detachedDecorators);
    ZrCore_Array_Free(cs->state, &detachedImplements);
    ZrCore_Array_Free(cs->state, &detachedInherits);
    ZrCore_Array_Free(cs->state, &detachedSymbols);
    ZrCore_Array_Free(cs->state, &detachedMembers);
    return result;
}
