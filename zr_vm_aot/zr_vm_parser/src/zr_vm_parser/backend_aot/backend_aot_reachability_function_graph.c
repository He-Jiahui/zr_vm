#include "backend_aot_reachability_function_graph.h"

#include "backend_aot_internal.h"
#include "zr_vm_core/metadata_token.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

static TZrBool backend_aot_static_reachability_has_entry(const SZrAotFunctionTable *table, TZrUInt32 flatIndex) {
    for (TZrUInt32 index = 0u; index < table->count; index++) {
        if (table->entries[index].flatIndex == flatIndex) {
            return ZR_TRUE;
        }
    }
    return ZR_FALSE;
}

static const SZrFunction *backend_aot_static_reachability_find_entry_function(const SZrAotFunctionTable *table,
                                                                              TZrUInt32 flatIndex) {
    if (table == ZR_NULL || table->entries == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrUInt32 index = 0u; index < table->count; index++) {
        if (table->entries[index].flatIndex == flatIndex) {
            return table->entries[index].function;
        }
    }

    return ZR_NULL;
}

static TZrBool backend_aot_static_reachability_append_root(TZrUInt32 *roots,
                                                           EZrAotReachabilityReason *rootReasons,
                                                           TZrUInt32 rootCapacity,
                                                           TZrUInt32 *rootCount,
                                                           TZrUInt32 root,
                                                           EZrAotReachabilityReason reason,
                                                           TZrUInt32 markCount) {
    if (roots == ZR_NULL || rootReasons == ZR_NULL || rootCount == ZR_NULL || root >= markCount) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < *rootCount; index++) {
        if (roots[index] == root) {
            return ZR_TRUE;
        }
    }

    if (*rootCount >= rootCapacity) {
        return ZR_FALSE;
    }

    roots[*rootCount] = root;
    rootReasons[*rootCount] = reason;
    (*rootCount)++;
    return ZR_TRUE;
}

static TZrBool backend_aot_static_reachability_collect_export_roots(const SZrAotFunctionTable *table,
                                                                    TZrUInt32 *roots,
                                                                    EZrAotReachabilityReason *rootReasons,
                                                                    TZrUInt32 rootCapacity,
                                                                    TZrUInt32 *rootCount,
                                                                    TZrUInt32 markCount) {
    const SZrFunction *entryFunction =
            backend_aot_static_reachability_find_entry_function(table, ZR_AOT_FUNCTION_TREE_ROOT_INDEX);

    if (entryFunction == ZR_NULL) {
        return ZR_FALSE;
    }
    if (entryFunction->topLevelCallableBindingLength > 0u &&
        entryFunction->topLevelCallableBindings == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 bindingIndex = 0u;
         bindingIndex < entryFunction->topLevelCallableBindingLength;
         bindingIndex++) {
        const SZrFunctionTopLevelCallableBinding *binding =
                &entryFunction->topLevelCallableBindings[bindingIndex];
        const SZrFunction *childFunction;
        TZrUInt32 targetIndex;

        if (binding->exportKind != ZR_MODULE_EXPORT_KIND_FUNCTION ||
            binding->callableChildIndex == ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE) {
            continue;
        }
        if (entryFunction->childFunctionList == ZR_NULL ||
            binding->callableChildIndex >= entryFunction->childFunctionLength) {
            return ZR_FALSE;
        }

        childFunction = &entryFunction->childFunctionList[binding->callableChildIndex];
        targetIndex = backend_aot_find_function_table_index(table, childFunction);
        if (targetIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
            return ZR_FALSE;
        }
        if (!backend_aot_static_reachability_append_root(roots,
                                                         rootReasons,
                                                         rootCapacity,
                                                         rootCount,
                                                         targetIndex,
                                                         ZR_AOT_REACHABILITY_REASON_ROOT_EXPORT,
                                                         markCount)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool backend_aot_static_reachability_collect_manifest_roots(const SZrAotFunctionTable *table,
                                                                      const TZrUInt32 *manifestRoots,
                                                                      TZrUInt32 manifestRootCount,
                                                                      TZrUInt32 *roots,
                                                                      EZrAotReachabilityReason *rootReasons,
                                                                      TZrUInt32 rootCapacity,
                                                                      TZrUInt32 *rootCount,
                                                                      TZrUInt32 markCount) {
    if (manifestRootCount == 0u) {
        return ZR_TRUE;
    }
    if (manifestRoots == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 manifestIndex = 0u; manifestIndex < manifestRootCount; manifestIndex++) {
        TZrUInt32 root = manifestRoots[manifestIndex];
        if (root >= markCount || !backend_aot_static_reachability_has_entry(table, root)) {
            return ZR_FALSE;
        }
        if (!backend_aot_static_reachability_append_root(roots,
                                                         rootReasons,
                                                         rootCapacity,
                                                         rootCount,
                                                         root,
                                                         ZR_AOT_REACHABILITY_REASON_MANIFEST,
                                                         markCount)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static const SZrTypeValue *backend_aot_function_metadata_field(SZrState *state,
                                                               const SZrFunction *function,
                                                               const TZrChar *fieldName) {
    SZrObject *metadataObject;
    SZrString *fieldString;
    SZrTypeValue key;

    if (state == ZR_NULL || function == ZR_NULL || fieldName == ZR_NULL ||
        !function->hasDecoratorMetadata ||
        function->decoratorMetadataValue.type != ZR_VALUE_TYPE_OBJECT ||
        function->decoratorMetadataValue.value.object == ZR_NULL) {
        return ZR_NULL;
    }

    metadataObject = ZR_CAST_OBJECT(state, function->decoratorMetadataValue.value.object);
    if (metadataObject == ZR_NULL) {
        return ZR_NULL;
    }

    fieldString = ZrCore_String_CreateFromNative(state, (TZrNativeString)fieldName);
    if (fieldString == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    return ZrCore_Object_GetValue(state, metadataObject, &key);
}

static TZrBool backend_aot_function_metadata_bool_field_is_true(SZrState *state,
                                                                const SZrFunction *function,
                                                                const TZrChar *fieldName) {
    const SZrTypeValue *value = backend_aot_function_metadata_field(state, function, fieldName);

    return (TZrBool)(value != ZR_NULL &&
                     value->type == ZR_VALUE_TYPE_BOOL &&
                     value->value.nativeObject.nativeBool);
}

static TZrBool backend_aot_function_metadata_uint64_field(SZrState *state,
                                                          const SZrFunction *function,
                                                          const TZrChar *fieldName,
                                                          TZrUInt64 *outValue);

static TZrBool backend_aot_function_metadata_uint32_field(SZrState *state,
                                                          const SZrFunction *function,
                                                          const TZrChar *fieldName,
                                                          TZrUInt32 *outValue) {
    TZrUInt64 rawValue = 0u;

    if (outValue != ZR_NULL) {
        *outValue = ZR_AOT_INVALID_FUNCTION_INDEX;
    }
    if (!backend_aot_function_metadata_uint64_field(state, function, fieldName, &rawValue) ||
        rawValue > (TZrUInt64)0xFFFFFFFFu) {
        if (outValue != ZR_NULL) {
            *outValue = ZR_AOT_INVALID_FUNCTION_INDEX;
        }
        return ZR_FALSE;
    }
    if (outValue != ZR_NULL) {
        *outValue = (TZrUInt32)rawValue;
    }
    return ZR_TRUE;
}

static TZrBool backend_aot_function_metadata_uint64_field(SZrState *state,
                                                          const SZrFunction *function,
                                                          const TZrChar *fieldName,
                                                          TZrUInt64 *outValue) {
    const SZrTypeValue *value = backend_aot_function_metadata_field(state, function, fieldName);

    if (outValue != ZR_NULL) {
        *outValue = 0u;
    }
    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_UINT64) {
        return ZR_FALSE;
    }
    if (outValue != ZR_NULL) {
        *outValue = value->value.nativeObject.nativeUInt64;
    }
    return ZR_TRUE;
}

static SZrString *backend_aot_function_metadata_string_field(SZrState *state,
                                                             const SZrFunction *function,
                                                             const TZrChar *fieldName) {
    const SZrTypeValue *value = backend_aot_function_metadata_field(state, function, fieldName);

    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_STRING || value->value.object == ZR_NULL) {
        return ZR_NULL;
    }
    return ZR_CAST_STRING(state, value->value.object);
}

static TZrBool backend_aot_function_is_reflection_annotation_root(SZrState *state,
                                                                  const SZrFunction *function) {
    return backend_aot_function_metadata_bool_field_is_true(state, function, "reflectable");
}

static TZrBool backend_aot_function_dynamic_dependency_function_index(SZrState *state,
                                                                      const SZrFunction *function,
                                                                      TZrUInt32 *outTargetIndex) {
    return backend_aot_function_metadata_uint32_field(state,
                                                     function,
                                                     "dynamicDependencyFunctionIndex",
                                                     outTargetIndex);
}

static TZrBool backend_aot_function_dynamic_dependency_method_token(SZrState *state,
                                                                    const SZrFunction *function,
                                                                    TZrMetadataToken *outMethodToken) {
    TZrUInt32 rawToken = 0u;

    if (outMethodToken != ZR_NULL) {
        *outMethodToken = 0u;
    }
    if (!backend_aot_function_metadata_uint32_field(state,
                                                    function,
                                                    "dynamicDependencyMethodToken",
                                                    &rawToken) ||
        rawToken == 0u ||
        ZR_METADATA_TOKEN_TABLE(rawToken) != ZR_METADATA_TABLE_MEMBER_DEF) {
        return ZR_FALSE;
    }
    if (outMethodToken != ZR_NULL) {
        *outMethodToken = rawToken;
    }
    return ZR_TRUE;
}

static SZrString *backend_aot_function_dynamic_dependency_method_name(SZrState *state,
                                                                      const SZrFunction *function) {
    return backend_aot_function_metadata_string_field(state, function, "dynamicDependencyMethodName");
}

static TZrBool backend_aot_function_dynamic_dependency_method_signature_hash(SZrState *state,
                                                                             const SZrFunction *function,
                                                                             TZrUInt64 *outSignatureHash) {
    return backend_aot_function_metadata_uint64_field(state,
                                                     function,
                                                     "dynamicDependencyMethodSignatureHash",
                                                     outSignatureHash);
}

static TZrBool backend_aot_typed_function_symbol_matches_method_token(const SZrFunctionTypedExportSymbol *symbol,
                                                                      TZrMetadataToken methodToken) {
    if (symbol == ZR_NULL ||
        symbol->symbolKind != ZR_FUNCTION_TYPED_SYMBOL_FUNCTION ||
        symbol->metadataToken != methodToken ||
        ZR_METADATA_TOKEN_TABLE(symbol->metadataToken) != ZR_METADATA_TABLE_MEMBER_DEF) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool backend_aot_resolve_dynamic_dependency_method_token(const SZrAotFunctionTable *table,
                                                                   TZrMetadataToken methodToken,
                                                                   TZrUInt32 *outTargetIndex) {
    const SZrFunction *rootFunction;
    TZrUInt32 matchedMethodTokenSymbolCount = 0u;
    TZrUInt32 matchedTargetIndex = ZR_AOT_INVALID_FUNCTION_INDEX;

    if (outTargetIndex != ZR_NULL) {
        *outTargetIndex = ZR_AOT_INVALID_FUNCTION_INDEX;
    }
    if (table == ZR_NULL ||
        methodToken == 0u ||
        ZR_METADATA_TOKEN_TABLE(methodToken) != ZR_METADATA_TABLE_MEMBER_DEF ||
        outTargetIndex == ZR_NULL) {
        return ZR_FALSE;
    }

    rootFunction = backend_aot_static_reachability_find_entry_function(table, ZR_AOT_FUNCTION_TREE_ROOT_INDEX);
    if (rootFunction == ZR_NULL ||
        rootFunction->childFunctionList == ZR_NULL ||
        rootFunction->typedExportedSymbols == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 symbolIndex = 0u; symbolIndex < rootFunction->typedExportedSymbolLength; symbolIndex++) {
        const SZrFunctionTypedExportSymbol *symbol = &rootFunction->typedExportedSymbols[symbolIndex];
        const SZrFunction *childFunction;
        TZrUInt32 targetIndex;

        if (!backend_aot_typed_function_symbol_matches_method_token(symbol, methodToken) ||
            symbol->callableChildIndex == ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE ||
            symbol->callableChildIndex >= rootFunction->childFunctionLength) {
            continue;
        }

        childFunction = &rootFunction->childFunctionList[symbol->callableChildIndex];
        targetIndex = backend_aot_find_function_table_index(table, childFunction);
        if (targetIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
            return ZR_FALSE;
        }

        matchedMethodTokenSymbolCount++;
        matchedTargetIndex = targetIndex;
    }

    if (matchedMethodTokenSymbolCount != 1u) {
        return ZR_FALSE;
    }

    *outTargetIndex = matchedTargetIndex;
    return ZR_TRUE;
}

static TZrBool backend_aot_typed_export_symbol_matches_name(const SZrFunctionTypedExportSymbol *symbol,
                                                            const SZrString *methodName,
                                                            TZrBool hasMethodSignatureHash,
                                                            TZrUInt64 methodSignatureHash) {
    if (symbol == ZR_NULL ||
        symbol->name == ZR_NULL ||
        methodName == ZR_NULL ||
        !(symbol->name == methodName || ZrCore_String_Equal(symbol->name, (SZrString *)methodName))) {
        return ZR_FALSE;
    }
    if (hasMethodSignatureHash && symbol->signatureHash != methodSignatureHash) {
        return ZR_FALSE;
    }
    return ZR_TRUE;
}

static TZrBool backend_aot_resolve_dynamic_dependency_method_name(const SZrAotFunctionTable *table,
                                                                  const SZrString *methodName,
                                                                  TZrBool hasMethodSignatureHash,
                                                                  TZrUInt64 methodSignatureHash,
                                                                  TZrUInt32 *outTargetIndex) {
    const SZrFunction *rootFunction;
    TZrUInt32 matchedSymbolCount = 0u;
    TZrUInt32 matchedTargetIndex = ZR_AOT_INVALID_FUNCTION_INDEX;

    if (outTargetIndex != ZR_NULL) {
        *outTargetIndex = ZR_AOT_INVALID_FUNCTION_INDEX;
    }
    if (table == ZR_NULL || methodName == ZR_NULL || outTargetIndex == ZR_NULL) {
        return ZR_FALSE;
    }

    rootFunction = backend_aot_static_reachability_find_entry_function(table, ZR_AOT_FUNCTION_TREE_ROOT_INDEX);
    if (rootFunction == ZR_NULL ||
        rootFunction->childFunctionList == ZR_NULL ||
        rootFunction->typedExportedSymbols == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 symbolIndex = 0u; symbolIndex < rootFunction->typedExportedSymbolLength; symbolIndex++) {
        const SZrFunctionTypedExportSymbol *symbol = &rootFunction->typedExportedSymbols[symbolIndex];
        const SZrFunction *childFunction;
        TZrUInt32 targetIndex;

        if (symbol->symbolKind != ZR_FUNCTION_TYPED_SYMBOL_FUNCTION ||
            symbol->exportKind != ZR_MODULE_EXPORT_KIND_FUNCTION ||
            !backend_aot_typed_export_symbol_matches_name(symbol,
                                                          methodName,
                                                          hasMethodSignatureHash,
                                                          methodSignatureHash) ||
            symbol->callableChildIndex == ZR_FUNCTION_CALLABLE_CHILD_INDEX_NONE ||
            symbol->callableChildIndex >= rootFunction->childFunctionLength) {
            continue;
        }

        childFunction = &rootFunction->childFunctionList[symbol->callableChildIndex];
        targetIndex = backend_aot_find_function_table_index(table, childFunction);
        if (targetIndex == ZR_AOT_INVALID_FUNCTION_INDEX) {
            return ZR_FALSE;
        }

        matchedSymbolCount++;
        matchedTargetIndex = targetIndex;
    }

    if (matchedSymbolCount != 1u) {
        return ZR_FALSE;
    }

    *outTargetIndex = matchedTargetIndex;
    return ZR_TRUE;
}

static TZrBool backend_aot_collect_reflection_annotation_root(const SZrAotFunctionTable *table,
                                                              TZrUInt32 *annotationRoots,
                                                              TZrUInt32 annotationRootCapacity,
                                                              TZrUInt32 *rootCount,
                                                              TZrUInt32 root) {
    if (table == ZR_NULL || annotationRoots == ZR_NULL || rootCount == ZR_NULL ||
        root == ZR_AOT_INVALID_FUNCTION_INDEX ||
        !backend_aot_static_reachability_has_entry(table, root)) {
        return ZR_FALSE;
    }

    for (TZrUInt32 index = 0u; index < *rootCount; index++) {
        if (annotationRoots[index] == root) {
            return ZR_TRUE;
        }
    }
    if (*rootCount >= annotationRootCapacity) {
        return ZR_FALSE;
    }

    annotationRoots[*rootCount] = root;
    (*rootCount)++;
    return ZR_TRUE;
}

TZrBool backend_aot_collect_reflection_annotation_roots(SZrState *state,
                                                        const SZrAotFunctionTable *table,
                                                        TZrUInt32 *annotationRoots,
                                                        TZrUInt32 annotationRootCapacity,
                                                        TZrUInt32 *outAnnotationRootCount) {
    TZrUInt32 rootCount = 0u;

    if (outAnnotationRootCount != ZR_NULL) {
        *outAnnotationRootCount = 0u;
    }
    if (state == ZR_NULL || table == ZR_NULL || table->entries == ZR_NULL ||
        table->count > table->capacity ||
        (annotationRootCapacity > 0u && annotationRoots == ZR_NULL)) {
        return ZR_FALSE;
    }

    for (TZrUInt32 entryIndex = 0u; entryIndex < table->count; entryIndex++) {
        const SZrAotFunctionEntry *entry = &table->entries[entryIndex];
        TZrBool hasDynamicDependencyFunctionIndex = ZR_FALSE;
        TZrUInt32 dynamicDependencyFunctionIndex = ZR_AOT_INVALID_FUNCTION_INDEX;
        TZrMetadataToken dynamicDependencyMethodToken = 0u;
        TZrUInt32 dynamicDependencyMethodIndex = ZR_AOT_INVALID_FUNCTION_INDEX;
        const SZrString *dynamicDependencyMethodName = ZR_NULL;
        TZrBool hasDynamicDependencyMethodSignatureHash = ZR_FALSE;
        TZrUInt64 dynamicDependencyMethodSignatureHash = 0u;
        TZrUInt32 dynamicDependencyMethodNameIndex = ZR_AOT_INVALID_FUNCTION_INDEX;

        if (entry->function == ZR_NULL) {
            return ZR_FALSE;
        }
        if (backend_aot_function_is_reflection_annotation_root(state, entry->function) &&
            !backend_aot_collect_reflection_annotation_root(table,
                                                           annotationRoots,
                                                           annotationRootCapacity,
                                                           &rootCount,
                                                           entry->flatIndex)) {
            return ZR_FALSE;
        }
        hasDynamicDependencyFunctionIndex = backend_aot_function_dynamic_dependency_function_index(state,
                                                                                                  entry->function,
                                                                                                  &dynamicDependencyFunctionIndex);
        if (hasDynamicDependencyFunctionIndex &&
            !backend_aot_collect_reflection_annotation_root(table,
                                                           annotationRoots,
                                                           annotationRootCapacity,
                                                           &rootCount,
                                                           dynamicDependencyFunctionIndex)) {
            return ZR_FALSE;
        }
        if (backend_aot_function_dynamic_dependency_method_token(state,
                                                                entry->function,
                                                                &dynamicDependencyMethodToken) &&
            (!backend_aot_resolve_dynamic_dependency_method_token(table,
                                                                  dynamicDependencyMethodToken,
                                                                  &dynamicDependencyMethodIndex) ||
             !backend_aot_collect_reflection_annotation_root(table,
                                                             annotationRoots,
                                                             annotationRootCapacity,
                                                             &rootCount,
                                                             dynamicDependencyMethodIndex))) {
            return ZR_FALSE;
        }
        dynamicDependencyMethodName = backend_aot_function_dynamic_dependency_method_name(state, entry->function);
        hasDynamicDependencyMethodSignatureHash =
                backend_aot_function_dynamic_dependency_method_signature_hash(state,
                                                                             entry->function,
                                                                             &dynamicDependencyMethodSignatureHash);
        if (dynamicDependencyMethodName != ZR_NULL &&
            (!backend_aot_resolve_dynamic_dependency_method_name(table,
                                                                 dynamicDependencyMethodName,
                                                                 hasDynamicDependencyMethodSignatureHash,
                                                                 dynamicDependencyMethodSignatureHash,
                                                                 &dynamicDependencyMethodNameIndex) ||
             !backend_aot_collect_reflection_annotation_root(table,
                                                             annotationRoots,
                                                             annotationRootCapacity,
                                                             &rootCount,
                                                             dynamicDependencyMethodNameIndex))) {
            return ZR_FALSE;
        }
    }

    if (outAnnotationRootCount != ZR_NULL) {
        *outAnnotationRootCount = rootCount;
    }
    return ZR_TRUE;
}

static TZrBool backend_aot_static_reachability_collect_annotation_roots(const SZrAotFunctionTable *table,
                                                                        const TZrUInt32 *annotationRoots,
                                                                        TZrUInt32 annotationRootCount,
                                                                        TZrUInt32 *roots,
                                                                        EZrAotReachabilityReason *rootReasons,
                                                                        TZrUInt32 rootCapacity,
                                                                        TZrUInt32 *rootCount,
                                                                        TZrUInt32 markCount) {
    if (annotationRootCount == 0u) {
        return ZR_TRUE;
    }
    if (annotationRoots == ZR_NULL) {
        return ZR_FALSE;
    }

    for (TZrUInt32 annotationIndex = 0u; annotationIndex < annotationRootCount; annotationIndex++) {
        TZrUInt32 root = annotationRoots[annotationIndex];
        if (root >= markCount || !backend_aot_static_reachability_has_entry(table, root)) {
            return ZR_FALSE;
        }
        if (!backend_aot_static_reachability_append_root(roots,
                                                         rootReasons,
                                                         rootCapacity,
                                                         rootCount,
                                                         root,
                                                         ZR_AOT_REACHABILITY_REASON_REFLECTION_ANNOTATION,
                                                         markCount)) {
            return ZR_FALSE;
        }
    }

    return ZR_TRUE;
}

static TZrBool backend_aot_static_reachability_append_edge(SZrAotReachabilityEdge *edges,
                                                           TZrUInt32 edgeCapacity,
                                                           TZrUInt32 *edgeCount,
                                                           TZrUInt32 source,
                                                           TZrUInt32 target,
                                                           TZrUInt32 markCount) {
    if (edgeCount == ZR_NULL || source >= markCount || target >= markCount) {
        return ZR_FALSE;
    }
    if (*edgeCount >= edgeCapacity || edges == ZR_NULL) {
        return ZR_FALSE;
    }

    edges[*edgeCount].source = source;
    edges[*edgeCount].target = target;
    edges[*edgeCount].reason = ZR_AOT_REACHABILITY_REASON_DIRECT_CALL;
    (*edgeCount)++;
    return ZR_TRUE;
}

static TZrBool backend_aot_static_reachability_scan_instruction(SZrState *state,
                                                                const SZrAotFunctionTable *table,
                                                                const SZrFunction *function,
                                                                TZrUInt32 sourceIndex,
                                                                const TZrInstruction *instruction,
                                                                SZrAotReachabilityEdge *edges,
                                                                TZrUInt32 edgeCapacity,
                                                                TZrUInt32 *edgeCount,
                                                                TZrUInt32 markCount) {
    TZrUInt32 targetIndex = ZR_AOT_INVALID_FUNCTION_INDEX;

    switch (instruction->instruction.operationCode) {
        case ZR_INSTRUCTION_ENUM(GET_CONSTANT):
            if (backend_aot_resolve_callable_constant_function_index(table,
                                                                     state,
                                                                     function,
                                                                     instruction->instruction.operand.operand2[0],
                                                                     &targetIndex)) {
                return backend_aot_static_reachability_append_edge(edges,
                                                                   edgeCapacity,
                                                                   edgeCount,
                                                                   sourceIndex,
                                                                   targetIndex,
                                                                   markCount);
            }
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(CREATE_CLOSURE):
            if (backend_aot_resolve_callable_constant_function_index(
                        table,
                        state,
                        function,
                        (TZrInt32)instruction->instruction.operand.operand1[0],
                        &targetIndex)) {
                return backend_aot_static_reachability_append_edge(edges,
                                                                   edgeCapacity,
                                                                   edgeCount,
                                                                   sourceIndex,
                                                                   targetIndex,
                                                                   markCount);
            }
            return ZR_TRUE;
        case ZR_INSTRUCTION_ENUM(GET_SUB_FUNCTION):
            if (function->childFunctionList != ZR_NULL &&
                instruction->instruction.operand.operand1[0] < function->childFunctionLength) {
                const SZrFunction *childFunction =
                        &function->childFunctionList[instruction->instruction.operand.operand1[0]];
                targetIndex = backend_aot_find_function_table_index(table, childFunction);
                if (targetIndex != ZR_AOT_INVALID_FUNCTION_INDEX) {
                    return backend_aot_static_reachability_append_edge(edges,
                                                                       edgeCapacity,
                                                                       edgeCount,
                                                                       sourceIndex,
                                                                       targetIndex,
                                                                       markCount);
                }
            }
            return ZR_TRUE;
        default:
            return ZR_TRUE;
    }
}

static TZrBool backend_aot_static_reachability_collect_edges(SZrState *state,
                                                             const SZrAotFunctionTable *table,
                                                             SZrAotReachabilityEdge *edges,
                                                             TZrUInt32 edgeCapacity,
                                                             TZrUInt32 *outEdgeCount,
                                                             TZrUInt32 markCount) {
    TZrUInt32 edgeCount = 0u;

    for (TZrUInt32 entryIndex = 0u; entryIndex < table->count; entryIndex++) {
        const SZrAotFunctionEntry *entry = &table->entries[entryIndex];
        const SZrFunction *function = entry->function;

        if (function == ZR_NULL || entry->flatIndex >= markCount) {
            return ZR_FALSE;
        }
        if (function->instructionsLength > 0u && function->instructionsList == ZR_NULL) {
            return ZR_FALSE;
        }

        for (TZrUInt32 instructionIndex = 0u; instructionIndex < function->instructionsLength; instructionIndex++) {
            if (!backend_aot_static_reachability_scan_instruction(state,
                                                                  table,
                                                                  function,
                                                                  entry->flatIndex,
                                                                  &function->instructionsList[instructionIndex],
                                                                  edges,
                                                                  edgeCapacity,
                                                                  &edgeCount,
                                                                  markCount)) {
                return ZR_FALSE;
            }
        }
    }

    if (outEdgeCount != ZR_NULL) {
        *outEdgeCount = edgeCount;
    }
    return ZR_TRUE;
}

TZrBool backend_aot_compute_static_callable_reachability(SZrState *state,
                                                         const SZrAotFunctionTable *table,
                                                         const TZrUInt32 *annotationRoots,
                                                         TZrUInt32 annotationRootCount,
                                                         const TZrUInt32 *manifestRoots,
                                                         TZrUInt32 manifestRootCount,
                                                         TZrUInt32 *roots,
                                                         EZrAotReachabilityReason *rootReasons,
                                                         TZrUInt32 rootCapacity,
                                                         SZrAotReachabilityMark *marks,
                                                         TZrUInt32 markCount,
                                                         SZrAotReachabilityEdge *edges,
                                                         TZrUInt32 edgeCapacity,
                                                         TZrUInt32 *queue,
                                                         TZrUInt32 queueCapacity,
                                                         TZrUInt32 *outMarkedCount,
                                                         TZrUInt32 *outEdgeCount) {
    TZrUInt32 rootCount = 0u;
    TZrUInt32 edgeCount = 0u;
    TZrUInt32 indexSpace;

    if (outMarkedCount != ZR_NULL) {
        *outMarkedCount = 0u;
    }
    if (outEdgeCount != ZR_NULL) {
        *outEdgeCount = 0u;
    }
    if (table == ZR_NULL || table->entries == ZR_NULL || table->count == 0u ||
        table->count > table->capacity || roots == ZR_NULL || rootReasons == ZR_NULL ||
        rootCapacity == 0u || marks == ZR_NULL || queue == ZR_NULL) {
        return ZR_FALSE;
    }

    indexSpace = backend_aot_function_table_index_space(table);
    if (indexSpace == ZR_AOT_COUNT_NONE || markCount < indexSpace ||
        !backend_aot_static_reachability_has_entry(table, ZR_AOT_FUNCTION_TREE_ROOT_INDEX)) {
        return ZR_FALSE;
    }

    if (!backend_aot_static_reachability_append_root(roots,
                                                     rootReasons,
                                                     rootCapacity,
                                                     &rootCount,
                                                     ZR_AOT_FUNCTION_TREE_ROOT_INDEX,
                                                     ZR_AOT_REACHABILITY_REASON_ROOT_ENTRY,
                                                     markCount)) {
        return ZR_FALSE;
    }
    if (!backend_aot_static_reachability_collect_export_roots(table,
                                                              roots,
                                                              rootReasons,
                                                              rootCapacity,
                                                              &rootCount,
                                                              markCount)) {
        return ZR_FALSE;
    }
    if (!backend_aot_static_reachability_collect_annotation_roots(table,
                                                                  annotationRoots,
                                                                  annotationRootCount,
                                                                  roots,
                                                                  rootReasons,
                                                                  rootCapacity,
                                                                  &rootCount,
                                                                  markCount)) {
        return ZR_FALSE;
    }
    if (!backend_aot_static_reachability_collect_manifest_roots(table,
                                                                manifestRoots,
                                                                manifestRootCount,
                                                                roots,
                                                                rootReasons,
                                                                rootCapacity,
                                                                &rootCount,
                                                                markCount)) {
        return ZR_FALSE;
    }

    if (!backend_aot_static_reachability_collect_edges(state,
                                                       table,
                                                       edges,
                                                       edgeCapacity,
                                                       &edgeCount,
                                                       markCount)) {
        return ZR_FALSE;
    }

    if (outEdgeCount != ZR_NULL) {
        *outEdgeCount = edgeCount;
    }
    return backend_aot_reachability_compute(marks,
                                            markCount,
                                            roots,
                                            rootReasons,
                                            rootCount,
                                            edges,
                                            edgeCount,
                                            queue,
                                            queueCapacity,
                                            outMarkedCount);
}
