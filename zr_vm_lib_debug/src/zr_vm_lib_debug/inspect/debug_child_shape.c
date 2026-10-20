#include "debug_internal.h"

static const SZrTypeValue *zr_debug_child_object_get_field(SZrState *state,
                                                           SZrObject *object,
                                                           const TZrChar *fieldName) {
    SZrTypeValue key;
    SZrString *fieldString;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    fieldString = ZrCore_String_CreateFromNative(state, (TZrNativeString)fieldName);
    if (fieldString == ZR_NULL) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(state, object, &key);
}

/** @brief 统计调试器可展开字段，隐藏运行时内部的 __zr_ 键。 */
TZrSize zr_debug_count_visible_object_entries(const SZrHashSet *set) {
    TZrSize bucketIndex;
    TZrSize count = 0;

    if (set == ZR_NULL || !set->isValid || set->buckets == ZR_NULL || set->capacity == 0) {
        return 0;
    }

    for (bucketIndex = 0; bucketIndex < set->capacity; bucketIndex++) {
        SZrHashKeyValuePair *pair = set->buckets[bucketIndex];
        while (pair != ZR_NULL) {
            const TZrChar *keyText = pair->key.type == ZR_VALUE_TYPE_STRING && pair->key.value.object != ZR_NULL
                                             ? ZrCore_String_GetNativeString(ZR_CAST_STRING(ZR_NULL, pair->key.value.object))
                                             : ZR_NULL;
            if (keyText == ZR_NULL || strncmp(keyText, "__zr_", 5) != 0) {
                count++;
            }
            pair = pair->next;
        }
    }

    return count;
}

/** @brief 将运行时值映射成协议中的 named/indexed 子节点数量。 */
void zr_debug_value_child_shape(SZrState *state,
                                const SZrTypeValue *value,
                                TZrSize *outNamedVariables,
                                TZrSize *outIndexedVariables) {
    SZrObject *object;
    SZrObject *indexedStorage = ZR_NULL;
    SZrObjectPrototype *prototype;
    const TZrChar *indexedSyntheticName = ZR_NULL;
    TZrSize indexedCount = 0;
    TZrSize namedVariables = 0;
    TZrSize indexedVariables = 0;

    if (outNamedVariables != ZR_NULL) {
        *outNamedVariables = 0;
    }
    if (outIndexedVariables != ZR_NULL) {
        *outIndexedVariables = 0;
    }

    if (state == ZR_NULL || value == ZR_NULL ||
        !(value->type == ZR_VALUE_TYPE_OBJECT || value->type == ZR_VALUE_TYPE_ARRAY) ||
        value->value.object == ZR_NULL) {
        return;
    }

    object = ZR_CAST_OBJECT(state, value->value.object);
    if (object == ZR_NULL) {
        return;
    }

    /* union 的内部元字段只公开 variant 与 payload，数量须和展开路径一致。 */
    if (zr_debug_value_is_union_carrier(state, value)) {
        namedVariables = 1u + zr_debug_count_union_carrier_payload_fields(state, value);
        if (outNamedVariables != ZR_NULL) {
            *outNamedVariables = namedVariables;
        }
        if (outIndexedVariables != ZR_NULL) {
            *outIndexedVariables = 0;
        }
        return;
    }

    /* 普通数组直接公开索引；库容器将隐藏数组合成为一个命名节点。 */
    if (zr_debug_try_resolve_indexed_storage(state, value, &indexedStorage, &indexedSyntheticName, &indexedCount) &&
        object->internalType == ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        indexedVariables = indexedCount;
    } else {
        namedVariables = zr_debug_count_visible_object_entries(&object->nodeMap);
        if (indexedStorage != ZR_NULL && indexedSyntheticName != ZR_NULL) {
            namedVariables++;
        }

        prototype = zr_debug_resolve_value_prototype(state, value);
        if (prototype != ZR_NULL) {
            namedVariables += ZR_DEBUG_INSTANCE_SYNTHETIC_FIELD_COUNT;
        }
    }

    if (outNamedVariables != ZR_NULL) {
        *outNamedVariables = namedVariables;
    }
    if (outIndexedVariables != ZR_NULL) {
        *outIndexedVariables = indexedVariables;
    }
}

static const TZrChar *zr_debug_semantic_type_name(SZrState *state, const SZrTypeValue *value) {
    SZrObjectPrototype *prototype;

    if (state == ZR_NULL || value == ZR_NULL) {
        return "value";
    }

    prototype = zr_debug_resolve_value_prototype(state, value);
    if ((value->type == ZR_VALUE_TYPE_OBJECT || value->type == ZR_VALUE_TYPE_ARRAY) &&
        prototype != ZR_NULL &&
        prototype->name != ZR_NULL) {
        return zr_debug_string_native(prototype->name);
    }

    return zr_debug_value_type_name(value->type);
}

static const TZrChar *zr_debug_semantic_ownership_name(EZrOwnershipValueKind kind) {
    switch (kind) {
        case ZR_OWNERSHIP_VALUE_KIND_UNIQUE:
            return "unique";
        case ZR_OWNERSHIP_VALUE_KIND_SHARED:
            return "shared";
        case ZR_OWNERSHIP_VALUE_KIND_WEAK:
            return "weak";
        case ZR_OWNERSHIP_VALUE_KIND_BORROWED:
            return "borrowed";
        case ZR_OWNERSHIP_VALUE_KIND_LOANED:
            return "loaned";
        case ZR_OWNERSHIP_VALUE_KIND_NONE:
        default:
            return "";
    }
}

static void zr_debug_semantic_append_ownership(const SZrTypeValue *value, TZrChar *buffer, TZrSize bufferSize) {
    const TZrChar *ownershipName;
    TZrSize length;

    if (value == ZR_NULL || buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    ownershipName = zr_debug_semantic_ownership_name(value->ownershipKind);
    if (ownershipName[0] == '\0') {
        return;
    }

    length = strlen(buffer);
    if (length + 1u >= bufferSize) {
        return;
    }

    snprintf(buffer + length, bufferSize - length, ", ownership %s", ownershipName);
    buffer[bufferSize - 1u] = '\0';
}

/** @brief 为预览及求值结果描述值类型、子节点形状与所有权状态。 */
void zr_debug_value_semantic_summary(SZrState *state,
                                     const SZrTypeValue *value,
                                     TZrSize namedVariables,
                                     TZrSize indexedVariables,
                                     TZrChar *buffer,
                                     TZrSize bufferSize) {
    const TZrChar *typeName;

    if (buffer == ZR_NULL || bufferSize == 0) {
        return;
    }

    buffer[0] = '\0';
    if (value == ZR_NULL) {
        return;
    }

    switch (value->type) {
        case ZR_VALUE_TYPE_NULL:
            zr_debug_copy_text(buffer, bufferSize, "null value");
            break;
        case ZR_VALUE_TYPE_BOOL:
            snprintf(buffer,
                     bufferSize,
                     "logical %s",
                     value->value.nativeObject.nativeBool ? "true" : "false");
            buffer[bufferSize - 1u] = '\0';
            break;
        case ZR_VALUE_TYPE_INT8:
        case ZR_VALUE_TYPE_INT16:
        case ZR_VALUE_TYPE_INT32:
        case ZR_VALUE_TYPE_INT64:
            snprintf(buffer,
                     bufferSize,
                     "integer %lld",
                     (long long)value->value.nativeObject.nativeInt64);
            buffer[bufferSize - 1u] = '\0';
            break;
        case ZR_VALUE_TYPE_UINT8:
        case ZR_VALUE_TYPE_UINT16:
        case ZR_VALUE_TYPE_UINT32:
        case ZR_VALUE_TYPE_UINT64:
            snprintf(buffer,
                     bufferSize,
                     "integer %llu",
                     (unsigned long long)value->value.nativeObject.nativeUInt64);
            buffer[bufferSize - 1u] = '\0';
            break;
        case ZR_VALUE_TYPE_FLOAT:
        case ZR_VALUE_TYPE_DOUBLE:
            snprintf(buffer, bufferSize, "number %.15g", value->value.nativeObject.nativeDouble);
            buffer[bufferSize - 1u] = '\0';
            break;
        case ZR_VALUE_TYPE_OBJECT:
        case ZR_VALUE_TYPE_ARRAY:
            if (zr_debug_value_is_union_carrier(state, value)) {
                SZrObject *unionObject = ZR_CAST_OBJECT(state, value->value.object);
                SZrString *typeName = ZR_NULL;
                SZrString *variantName = ZR_NULL;
                const SZrTypeValue *typeValue;
                const SZrTypeValue *variantValue;

                typeValue = unionObject != ZR_NULL
                                    ? zr_debug_child_object_get_field(state, unionObject, "__zr_unionType")
                                    : ZR_NULL;
                variantValue = unionObject != ZR_NULL
                                       ? zr_debug_child_object_get_field(state, unionObject, "__zr_unionVariant")
                                       : ZR_NULL;
                typeName = typeValue != ZR_NULL && typeValue->type == ZR_VALUE_TYPE_STRING &&
                                   typeValue->value.object != ZR_NULL
                           ? ZR_CAST_STRING(state, typeValue->value.object)
                           : ZR_NULL;
                variantName = variantValue != ZR_NULL && variantValue->type == ZR_VALUE_TYPE_STRING &&
                                      variantValue->value.object != ZR_NULL
                              ? ZR_CAST_STRING(state, variantValue->value.object)
                              : ZR_NULL;
                snprintf(buffer,
                         bufferSize,
                         "union %s.%s, named %llu",
                         typeName != ZR_NULL ? zr_debug_string_native(typeName) : "value",
                         variantName != ZR_NULL ? zr_debug_string_native(variantName) : "variant",
                         (unsigned long long)namedVariables);
                buffer[bufferSize - 1u] = '\0';
                break;
            }
            typeName = zr_debug_semantic_type_name(state, value);
            if (namedVariables > 0 || indexedVariables > 0) {
                snprintf(buffer,
                         bufferSize,
                         "expandable %s, named %llu, indexed %llu",
                         typeName,
                         (unsigned long long)namedVariables,
                         (unsigned long long)indexedVariables);
            } else {
                snprintf(buffer, bufferSize, "%s value", typeName);
            }
            buffer[bufferSize - 1u] = '\0';
            break;
        case ZR_VALUE_TYPE_FUNCTION:
        case ZR_VALUE_TYPE_CLOSURE:
            snprintf(buffer, bufferSize, "callable %s", zr_debug_semantic_type_name(state, value));
            buffer[bufferSize - 1u] = '\0';
            break;
        case ZR_VALUE_TYPE_STRING:
            zr_debug_copy_text(buffer, bufferSize, "string value");
            break;
        default:
            snprintf(buffer, bufferSize, "%s value", zr_debug_semantic_type_name(state, value));
            buffer[bufferSize - 1u] = '\0';
            break;
    }

    zr_debug_semantic_append_ownership(value, buffer, bufferSize);
}

/**
 * @fn static const SZrTypeValue *zr_debug_child_object_get_field(SZrState *state, SZrObject *object, const TZrChar *fieldName)
 *
 * ## Summary
 * 为 boxed union 摘要读取名字字段；把原生字段名转成运行时字符串键，再沿 Core 普通对象查找路径取得值。
 *
 * ## Parameters
 * @param[in,out] state 用于字符串驻留/分配及对象读取的借用状态；非空时须已建立可用 global/stringTable。
 * @param[in] object 仍由运行时持有的对象或数组布局，不转移所有权。
 * @param[in] fieldName 调用期间有效的 NUL 结尾字段名；不接收内嵌 NUL 长度。
 *
 * ## Returns
 * @return 查到的借用值槽，可能来自对象自身或原型链；输入为空、普通字符串创建失败或字段缺席时为 NULL。
 *
 * ## Constraints and lifecycle
 * 不调用用户 getter，也不复制返回槽或建立持久 GC root；调用者须在对象/原型仍存活且未改变查找结构时使用，不能释放该槽。
 * 字符串创建可能更新驻留表/触发分配，并可能沿 ZrCore_Exception_Throw 非局部退出；本函数没有恢复点，NULL 检查不表示所有 OOM 都普通返回，也不保证回滚运行时状态。
 * 本文件仅用固定的 __zr_unionType 和 __zr_unionVariant 查询；该入口不是任意表达式求值器。
 *
 * ## References
 * - ZrCore_String_CreateFromNative (zr_vm_core/include/zr_vm_core/string.h:195)。
 * - string_create_short (zr_vm_core/src/zr_vm_core/string/string.c:635)。
 * - ZrCore_Object_GetValue (zr_vm_core/src/zr_vm_core/object/object.c:1830)。
 * - object_get_prototype_value_unchecked (zr_vm_core/src/zr_vm_core/object/object.c:845)。
 * - ZrCore_Exception_Throw (zr_vm_core/src/zr_vm_core/exception/exception.c:99)。
 */

/**
 * @fn TZrSize zr_debug_count_visible_object_entries(const SZrHashSet *set)
 *
 * ## Summary
 * 为变量预览与实际展开共用一套计数口径：遍历 nodeMap，隐藏字符串键以 __zr_ 开头的内部项。
 *
 * ## Parameters
 * @param[in] set 有效哈希表或 NULL；有效表的桶链和键在遍历期间须保持可读且稳定。
 *
 * ## Returns
 * @return 未被内部字符串前缀过滤的条目数；NULL、未初始化表、空桶地址或零容量返回 0。
 *
 * ## Constraints and lifecycle
 * 这里只统计本表，不统计原型继承值，也不添加 $prototype、data、entries 或 union variant 等合成节点；非字符串键及无法取得文本的键仍计入。
 * 不分配、不取得锁、不固定 GC 对象；字符串键字节是运行时借用视图，调用方须阻止并发修改/释放或在遍历间移动该对象图。
 * 0 同时可表示无可见条目和输入不可用，不能由结果区分错误。
 *
 * ## References
 * - zr_debug_expand_value_handle (zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1683)。
 * - ZrCore_String_GetNativeString (zr_vm_core/include/zr_vm_core/string.h:218)。
 * - ZR_CAST_OBJECT (zr_vm_core/include/zr_vm_core/conversion.h:66)。
 */

/**
 * @fn void zr_debug_value_child_shape(SZrState *state, const SZrTypeValue *value, TZrSize *outNamedVariables, TZrSize *outIndexedVariables)
 *
 * ## Summary
 * 给变量预览与求值结果填充 named/indexed 子节点数量，使摘要计数跟随当前展开策略，而不在这里创建变量句柄。
 *
 * ## Parameters
 * @param[in,out] state 借用 VM 状态；完整对象检查所经字符串键查找可能更新驻留表/分配。
 * @param[in] value 调用期间保持有效且类型标签与对象布局一致的值槽，可为 NULL。
 * @param[out] outNamedVariables 可选命名子节点输出，非空时在检查输入之前先写 0。
 * @param[out] outIndexedVariables 可选索引子节点输出，非空时在检查输入之前先写 0。
 *
 * ## Constraints and lifecycle
 * 无有效 state/value 或非对象/数组、空对象时保留已初始化的零输出。两个结果若都需要保留，应使用独立输出位置；函数不检测输出与输入对象的别名。
 * union_shape：识别到 boxed carrier 后 named 为 1 个 variant 加匹配内部 payload 键数，indexed 为 0，并提前结束；这里不构建或验证完整 union 编译元数据。
 * indexed_storage：原生数组用其数组长度填 indexed；普通对象/库容器使用可见 nodeMap 条目，再按解析出的 data/entries 存储加一个合成命名节点，有可用原型时再加 ZR_DEBUG_INSTANCE_SYNTHETIC_FIELD_COUNT。必须与实际展开路径共同维护。
 * 调用者维持暂停帧、值和对象图的借用有效期；本函数不加锁、不建立 root/pin。union/容器识别中创建字符串键，普通失败可能回退为其他形状，Throw 可非局部离开；不提供完整事务回滚。
 *
 * ## References
 * - zr_debug_fill_value_preview (zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:745)。
 * - zr_debug_evaluate_with_policy (zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2476)。
 * - zr_debug_expand_value_handle (zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:1683)。
 * - zr_debug_count_union_carrier_payload_fields (zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_union.c:429)。
 * - zr_debug_value_is_union_carrier (zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_union.c:464)。
 * - zr_debug_try_resolve_indexed_storage (zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:834)。
 * - zr_debug_resolve_value_prototype (zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:346)。
 */

/**
 * @fn static const TZrChar *zr_debug_semantic_type_name(SZrState *state, const SZrTypeValue *value)
 *
 * ## Summary
 * 为语义摘要挑选展示类型名：对象/数组优先使用已解析原型的名字，其余使用调试器的值类型词。
 *
 * ## Parameters
 * @param[in] state 只读借用状态，供解析当前值原型，可为 NULL。
 * @param[in] value 只读借用值槽；非空对象须符合类型标签与布局不变量。
 *
 * ## Returns
 * @return 静态类型词或运行时原型名字的借用 NUL 字节视图；state/value 为 NULL 时返回静态 "value"。
 *
 * ## Constraints and lifecycle
 * 不复制或取得字符串所有权；原型名不能跨对象释放/移动 GC 或名字替换后继续持有，调用者应在本次展示期间立即消费。
 * 只对对象/数组选择原型名；函数、闭包等仍按值标签回退，不读取任意用户属性或执行用户转换。
 *
 * ## References
 * - zr_debug_resolve_value_prototype (zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:346)。
 * - zr_debug_value_type_name (zr_vm_lib_debug/src/zr_vm_lib_debug/debug.c:188)。
 * - ZrCore_String_GetNativeString (zr_vm_core/include/zr_vm_core/string.h:218)。
 */

/**
 * @fn static const TZrChar *zr_debug_semantic_ownership_name(EZrOwnershipValueKind kind)
 *
 * ## Summary
 * 把值槽上的所有权枚举转成可选的展示词，供语义摘要尾缀使用，不执行所有权转换。
 *
 * ## Parameters
 * @param[in] kind 待显示的 ownershipKind 枚举。
 *
 * ## Returns
 * @return 进程静态的 unique/shared/weak/borrowed/loaned 词；NONE 或未识别枚举返回静态空串。
 *
 * ## Constraints and lifecycle
 * 返回指针不需释放；枚举词只表达当前标签，不证明对应对象已保活、当前借用仍合法或可以跨线程共享。
 *
 * ## References
 * - zr_debug_semantic_append_ownership (zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_child_shape.c:150)。
 */

/**
 * @fn static void zr_debug_semantic_append_ownership(const SZrTypeValue *value, TZrChar *buffer, TZrSize bufferSize)
 *
 * ## Summary
 * 在已有值描述之后尽力追加所有权词；缺少标签或连追加起始都放不下时保留原文，可写尾缀允许截断。
 *
 * ## Parameters
 * @param[in] value 仅借用其 ownershipKind；NULL 时不修改缓冲区。
 * @param[in,out] buffer 含现有 NUL 结尾文本的可写缓冲区；有效文本必须在 bufferSize 范围内终止。
 * @param[in] bufferSize 缓冲区总字节容量，包含结尾 NUL。
 *
 * ## Constraints and lifecycle
 * NULL/零容量、空所有权词或已有文本后连一个字符与 NUL 都放不下时不写；能进入 snprintf 时尾缀可能被截断，并非保证完整尾缀或全不追加。
 * 按字节容量处理并强制最后一字节为 NUL；不保证截断后的 UTF-8 完整，也不返回截断状态。
 * 不分配、不改变值槽、不加 root/pin 或锁；调用者负责有效且不被并发修改的输入与缓冲区。
 *
 * ## References
 * - zr_debug_semantic_ownership_name (zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_child_shape.c:132)。
 * - zr_debug_value_semantic_summary (zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_child_shape.c:173)。
 */

/**
 * @fn void zr_debug_value_semantic_summary(SZrState *state, const SZrTypeValue *value, TZrSize namedVariables, TZrSize indexedVariables, TZrChar *buffer, TZrSize bufferSize)
 *
 * ## Summary
 * 把当前值、已有子节点计数和所有权标签组合为短文本，供变量预览与求值响应展示；不在这里执行表达式或重新计算子节点数量。
 *
 * ## Parameters
 * @param[in,out] state 借用 VM 状态；union 字段识别/读取中可能创建字符串键并分配。
 * @param[in] value 当前有效值槽；NULL 时把有效输出留为空串，不转移值或对象所有权。
 * @param[in] namedVariables 调用方按同一展示路径取得的命名节点数，仅用于格式化。
 * @param[in] indexedVariables 调用方取得的索引节点数，仅用于格式化。
 * @param[out] buffer 可选可写输出，不能与仍需读取的值/名字存储重叠；非空且容量非零时先清空。
 * @param[in] bufferSize 输出字节容量，包含 NUL。
 *
 * ## Constraints and lifecycle
 * 按值标签格式化 null/bool/整数/浮点/string/callable 或对象描述；boxed union 使用 __zr_unionType/__zr_unionVariant 字符串字段，缺少名字时用 value/variant 词；对象计数不核实是否与真实展开一致。
 * 末尾尽力追加 ownershipKind；格式化正常返回时有界并终止，但文本/所有权尾缀可能按字节截断，没有成功/截断状态返回，不保证合法 UTF-8 边界。
 * 读取到的对象、原型及字符串均借用，调用方维持本次停点与 GC 有效期；本函数不持有锁、root/pin，不返回这些内部指针。
 * union 识别和字段读取可分配并 Throw；非局部离开时缓冲区可能已清空或只写入部分内容，不保证原缓冲区/运行时状态回滚，也不把普通 NULL 回退当作所有 OOM 的处理。
 *
 * ## References
 * - zr_debug_fill_value_preview (zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:745)。
 * - zr_debug_evaluate_with_policy (zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_snapshot.c:2476)。
 * - zr_debug_value_is_union_carrier (zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_union.c:464)。
 * - zr_debug_child_object_get_field (zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_child_shape.c:3)。
 * - zr_debug_semantic_type_name (zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_child_shape.c:115)。
 * - zr_debug_semantic_append_ownership (zr_vm_lib_debug/src/zr_vm_lib_debug/inspect/debug_child_shape.c:150)。
 * - ZrCore_Exception_Throw (zr_vm_core/src/zr_vm_core/exception/exception.c:99)。
 */
