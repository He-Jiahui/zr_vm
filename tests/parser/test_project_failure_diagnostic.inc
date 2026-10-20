#include "zr_vm_core/closure.h"

static const TZrChar *project_failure_diagnostic_string_bytes(const SZrString *string, size_t *length) {
    *length = 0;
    if (string == ZR_NULL || string->super.type != ZR_RAW_OBJECT_TYPE_STRING) return ZR_NULL;
    const TZrChar *bytes = ZrCore_String_GetNativeString(string);
    if (bytes != ZR_NULL) *length = (size_t)ZrCore_String_GetByteLength(string);
    return bytes;
}

static void project_failure_diagnostic_write_string(const SZrString *string) {
    size_t length;
    const TZrChar *bytes = project_failure_diagnostic_string_bytes(string, &length);
    if (bytes != ZR_NULL) fwrite(bytes, 1, length, stderr);
    else fputs("<none>", stderr);
}

static const SZrTypeValue *project_failure_diagnostic_own_field(
        const SZrHashSet *fields, const TZrChar *name, size_t length) {
    if (name == ZR_NULL || !fields->isValid || fields->buckets == ZR_NULL) return ZR_NULL;
    TZrSize remaining = fields->elementCount;
    for (TZrSize bucket = 0; bucket < fields->capacity && remaining > 0; ++bucket) {
        for (const SZrHashKeyValuePair *pair = fields->buckets[bucket];
             pair != ZR_NULL && remaining > 0; pair = pair->next) {
            --remaining;
            const SZrTypeValue *key = &pair->key;
            if (key->type != ZR_VALUE_TYPE_STRING || !key->isGarbageCollectable ||
                key->value.object == ZR_NULL || key->value.object->type != ZR_RAW_OBJECT_TYPE_STRING) continue;
            size_t keyLength;
            const TZrChar *bytes = project_failure_diagnostic_string_bytes(
                    (const SZrString *)key->value.object, &keyLength);
            if (bytes != ZR_NULL && keyLength == length && memcmp(bytes, name, length) == 0) return &pair->value;
        }
    }
    return ZR_NULL;
}

static const SZrFunction *project_failure_diagnostic_value_function(const SZrTypeValue *value) {
    if (value == ZR_NULL || value->isNative || !value->isGarbageCollectable ||
        value->value.object == ZR_NULL || value->value.object->isNative) return ZR_NULL;
    const SZrRawObject *object = value->value.object;
    const SZrFunction *function = ZR_NULL;
    if (value->type == ZR_VALUE_TYPE_FUNCTION && object->type == ZR_RAW_OBJECT_TYPE_FUNCTION)
        function = (const SZrFunction *)object;
    else if (value->type == ZR_VALUE_TYPE_CLOSURE && object->type == ZR_RAW_OBJECT_TYPE_CLOSURE)
        function = ((const SZrClosure *)object)->function;
    return function != ZR_NULL && function->super.type == ZR_RAW_OBJECT_TYPE_FUNCTION && !function->super.isNative
                   ? function : ZR_NULL;
}

/* Report stored owner witnesses without resolving or selecting a call target. */
static void project_failure_diagnostic_owner(const SZrFunction *provider, TZrMetadataToken token,
                                             const SZrFunction *candidate) {
    if (token == 0u) return;
    const SZrMetadataTokenRecord *owner = ZR_NULL;
    TZrUInt32 matches = 0;
    for (TZrUInt32 row = 0; provider->metadataTokenRecords != ZR_NULL &&
                            row < provider->metadataTokenRecordLength; ++row) {
        if (provider->metadataTokenRecords[row].token == token) {
            owner = &provider->metadataTokenRecords[row];
            ++matches;
        }
    }
    fprintf(stderr, "[zr-test-project-provider] ownerToken=0x%08x matches=%u\n", (unsigned)token, (unsigned)matches);
    if (matches != 1u || ZR_METADATA_TOKEN_TABLE(owner->token) != ZR_METADATA_TABLE_TYPE_DEF ||
        owner->reserved0 != ZR_METADATA_TOKEN_RECORD_CALLABLE_OWNER) return;
    fprintf(stderr, "[zr-test-project-provider] ownerIndex=%u hash=%llu version=%u layout=%llu\n",
            (unsigned)owner->ownerIndex, (unsigned long long)owner->signatureHash,
            (unsigned)owner->layoutVersion, (unsigned long long)owner->layoutHash);
    if (provider->prototypeInstances == ZR_NULL || owner->ownerIndex >= provider->prototypeInstancesLength) return;
    const SZrObjectPrototype *prototype = provider->prototypeInstances[owner->ownerIndex];
    if (prototype == ZR_NULL || prototype->super.super.type != ZR_RAW_OBJECT_TYPE_OBJECT ||
        prototype->super.super.isNative || prototype->super.internalType != ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE) return;
    fprintf(stderr, "[zr-test-project-provider] prototype=%p generation=%llu descriptors=%u capacity=%u name=",
            (void *)prototype, (unsigned long long)prototype->layoutGeneration,
            (unsigned)prototype->memberDescriptorCount, (unsigned)prototype->memberDescriptorCapacity);
    project_failure_diagnostic_write_string(prototype->name);
    fputc('\n', stderr);
    TZrUInt32 methodMatches = 0;
    if (prototype->memberDescriptorCount <= prototype->memberDescriptorCapacity &&
        (prototype->memberDescriptorCount == 0u || prototype->memberDescriptors != ZR_NULL)) {
        for (TZrUInt32 row = 0; row < prototype->memberDescriptorCount; ++row) {
            const SZrMemberDescriptor *item = &prototype->memberDescriptors[row];
            const TZrBool methodMatch = candidate != ZR_NULL && item->methodFunction == candidate;
            methodMatches += methodMatch ? 1u : 0u;
            fprintf(stderr, "[zr-test-project-provider] descriptor=%u kind=%u role=%u accessor=%u modifiers=%u "
                            "static=%u slot=%u method=%p getter=%p setter=%p initializer=%p candidateMethod=%u name=",
                    (unsigned)row, (unsigned)item->kind, (unsigned)item->contractRole, (unsigned)item->accessorRole,
                    (unsigned)item->modifierFlags, (unsigned)item->isStatic, (unsigned)item->virtualSlotIndex,
                    (void *)item->methodFunction, (void *)item->getterFunction, (void *)item->setterFunction,
                    (void *)item->initializerFunction, (unsigned)methodMatch);
            project_failure_diagnostic_write_string(item->name);
            fputc('\n', stderr);
        }
    } else fputs("[zr-test-project-provider] descriptorArray=invalid\n", stderr);
    TZrUInt32 metaMatches = 0;
    for (TZrUInt32 slot = 0; slot < ZR_META_ENUM_MAX; ++slot) {
        const SZrMeta *meta = prototype->metaTable.metas[slot];
        if (meta == ZR_NULL) continue;
        const SZrFunction *function = meta->function;
        const TZrBool vmFunction = function != ZR_NULL && function->super.type == ZR_RAW_OBJECT_TYPE_FUNCTION &&
                                   !function->super.isNative;
        const TZrBool metaMatch = candidate != ZR_NULL && vmFunction && function == candidate;
        metaMatches += metaMatch ? 1u : 0u;
        fprintf(stderr, "[zr-test-project-provider] metaSlot=%u metaType=%u consistent=%u function=%p "
                        "rawTag=%d native=%d candidateMeta=%u name=",
                (unsigned)slot, (unsigned)meta->metaType,
                (unsigned)((TZrUInt32)meta->metaType < ZR_META_ENUM_MAX && (TZrUInt32)meta->metaType == slot),
                (void *)function, function != ZR_NULL ? (int)function->super.type : -1,
                function != ZR_NULL ? (int)function->super.isNative : -1, (unsigned)metaMatch);
        project_failure_diagnostic_write_string(vmFunction ? function->functionName : ZR_NULL);
        fputc('\n', stderr);
    }
    fprintf(stderr, "[zr-test-project-provider] candidate=%p methodMatches=%u metaMatches=%u\n",
            (void *)candidate, (unsigned)methodMatches, (unsigned)metaMatches);
}

static void project_failure_diagnostic_provider(const SZrObjectModule *module, TZrMetadataToken token) {
    const SZrFunction *provider;
    if (module->hasMetadataRuntime) provider = module->metadataRuntime.metadataFunction;
    else provider = project_failure_diagnostic_value_function(project_failure_diagnostic_own_field(
            &module->super.nodeMap, "__zr_reflection_entry_function", sizeof("__zr_reflection_entry_function") - 1));
    const TZrBool valid = provider != ZR_NULL && provider->super.type == ZR_RAW_OBJECT_TYPE_FUNCTION &&
                         !provider->super.isNative;
    fprintf(stderr, "[zr-test-project-provider] module=%p pathHash=%llu init=%u generation=%u runtime=%u "
                    "registration=%p provider=%p valid=%u moduleSig=%llu token=0x%08x path=",
            (void *)module, (unsigned long long)module->pathHash, (unsigned)module->initState,
            (unsigned)module->metadataGeneration, (unsigned)module->hasMetadataRuntime,
            module->hasMetadataRuntime ? (void *)module->metadataRuntime.codeRegistration : ZR_NULL,
            (void *)provider, (unsigned)valid, valid ? (unsigned long long)provider->moduleSignatureHash : 0ull,
            (unsigned)token);
    project_failure_diagnostic_write_string(module->fullPath);
    fputs(" moduleName=", stderr);
    project_failure_diagnostic_write_string(module->moduleName);
    fputc('\n', stderr);
    if (!valid) return;
    fprintf(stderr, "[zr-test-project-provider] records=%u recordArray=%p exports=%u exportArray=%p "
                    "constants=%u constantArray=%p prototypes=%u prototypeArray=%p\n",
            (unsigned)provider->metadataTokenRecordLength, (void *)provider->metadataTokenRecords,
            (unsigned)provider->typedExportedSymbolLength, (void *)provider->typedExportedSymbols,
            (unsigned)provider->constantValueLength, (void *)provider->constantValueList,
            (unsigned)provider->prototypeInstancesLength, (void *)provider->prototypeInstances);
    TZrUInt32 recordMatches = 0;
    for (TZrUInt32 row = 0; provider->metadataTokenRecords != ZR_NULL &&
                            row < provider->metadataTokenRecordLength; ++row) {
        const SZrMetadataTokenRecord *record = &provider->metadataTokenRecords[row];
        if (record->token != token) continue;
        ++recordMatches;
        fprintf(stderr, "[zr-test-project-provider] record=%u token=0x%08x table=%u related=0x%08x marker=%u "
                        "owner=0x%08x index=%u hash=%llu version=%u layout=%llu target=0x%08x\n",
                (unsigned)row, (unsigned)record->token, (unsigned)ZR_METADATA_TOKEN_TABLE(record->token),
                (unsigned)record->relatedToken, (unsigned)record->reserved0, (unsigned)record->ownerToken,
                (unsigned)record->ownerIndex, (unsigned long long)record->signatureHash,
                (unsigned)record->layoutVersion, (unsigned long long)record->layoutHash,
                (unsigned)record->targetMetadataToken);
        const SZrTypeValue *value = record->reserved0 == ZR_METADATA_TOKEN_RECORD_CALLABLE_CONSTANT &&
                                           provider->constantValueList != ZR_NULL &&
                                           record->ownerIndex < provider->constantValueLength
                                       ? &provider->constantValueList[record->ownerIndex] : ZR_NULL;
        const SZrRawObject *object = value != ZR_NULL && value->isGarbageCollectable ? value->value.object : ZR_NULL;
        const SZrFunction *candidate = project_failure_diagnostic_value_function(value);
        fprintf(stderr, "[zr-test-project-provider] constantWitness=%p tag=%d native=%d gc=%d object=%p "
                        "rawTag=%d candidateFunction=%p name=",
                (void *)value, value != ZR_NULL ? (int)value->type : -1,
                value != ZR_NULL ? (int)value->isNative : -1,
                value != ZR_NULL ? (int)value->isGarbageCollectable : -1, (void *)object,
                object != ZR_NULL ? (int)object->type : -1, (void *)candidate);
        project_failure_diagnostic_write_string(candidate != ZR_NULL ? candidate->functionName : ZR_NULL);
        fputc('\n', stderr);
        project_failure_diagnostic_owner(provider, record->ownerToken, candidate);
    }
    TZrUInt32 exportMatches = 0;
    TZrUInt32 functionExports = 0;
    for (TZrUInt32 row = 0; provider->typedExportedSymbols != ZR_NULL &&
                            row < provider->typedExportedSymbolLength; ++row) {
        const SZrFunctionTypedExportSymbol *symbol = &provider->typedExportedSymbols[row];
        if (symbol->metadataToken != token) continue;
        ++exportMatches;
        functionExports += symbol->symbolKind == ZR_FUNCTION_TYPED_SYMBOL_FUNCTION ? 1u : 0u;
        size_t length;
        const TZrChar *name = project_failure_diagnostic_string_bytes(symbol->name, &length);
        const SZrTypeValue *value = project_failure_diagnostic_own_field(&module->super.nodeMap, name, length);
        fprintf(stderr, "[zr-test-project-provider] export=%u kind=%u exportKind=%u readiness=%u token=0x%08x "
                        "signature=0x%08x hash=%llu child=%u ownValue=%p candidateFunction=%p name=",
                (unsigned)row, (unsigned)symbol->symbolKind, (unsigned)symbol->exportKind,
                (unsigned)symbol->readiness, (unsigned)symbol->metadataToken, (unsigned)symbol->signatureToken,
                (unsigned long long)symbol->signatureHash, (unsigned)symbol->callableChildIndex,
                (void *)value, (void *)project_failure_diagnostic_value_function(value));
        project_failure_diagnostic_write_string(symbol->name);
        fputc('\n', stderr);
    }
    fprintf(stderr, "[zr-test-project-provider] token=0x%08x recordMatches=%u exportMatches=%u functionExports=%u\n",
            (unsigned)token, (unsigned)recordMatches, (unsigned)exportMatches, (unsigned)functionExports);
}

/* Cached providers are rooted; the failed consumer and its frame are not borrowed here. */
static void project_failure_diagnostic_providers(const SZrState *state, const SZrCallBindingDiagnostic *binding) {
    if (binding->status != ZR_CALL_BINDING_TARGET_NOT_FOUND || state->global == ZR_NULL) return;
    const SZrTypeValue *value = &state->global->loadedModulesRegistry;
    if (value->type != ZR_VALUE_TYPE_OBJECT || value->isNative || !value->isGarbageCollectable || value->value.object == ZR_NULL ||
        value->value.object->type != ZR_RAW_OBJECT_TYPE_OBJECT || value->value.object->isNative) return;
    const SZrHashSet *fields = &((const SZrObject *)value->value.object)->nodeMap;
    if (!fields->isValid || fields->buckets == ZR_NULL) return;
    TZrSize remaining = fields->elementCount;
    for (TZrSize bucket = 0; bucket < fields->capacity && remaining > 0; ++bucket) {
        for (const SZrHashKeyValuePair *pair = fields->buckets[bucket];
             pair != ZR_NULL && remaining > 0; pair = pair->next) {
            --remaining;
            const SZrTypeValue *moduleValue = &pair->value;
            if (moduleValue->type != ZR_VALUE_TYPE_OBJECT || !moduleValue->isGarbageCollectable ||
                moduleValue->isNative || moduleValue->value.object == ZR_NULL ||
                moduleValue->value.object->type != ZR_RAW_OBJECT_TYPE_OBJECT ||
                moduleValue->value.object->isNative) continue;
            const SZrObject *object = (const SZrObject *)moduleValue->value.object;
            if (object->internalType == ZR_OBJECT_INTERNAL_TYPE_MODULE)
                project_failure_diagnostic_provider((const SZrObjectModule *)object, binding->targetMetadataToken);
        }
    }
}

/* Borrow the stored own field without allocating keys or invoking VM lookup. */
static const SZrTypeValue *project_failure_diagnostic_own_message(const SZrTypeValue *exception) {
    const SZrHashSet *fields;
    TZrSize remaining;
    TZrSize bucket;

    if (exception->type != ZR_VALUE_TYPE_OBJECT || !exception->isGarbageCollectable ||
        exception->value.object == ZR_NULL || exception->value.object->type != ZR_RAW_OBJECT_TYPE_OBJECT) {
        return ZR_NULL;
    }
    fields = &((const SZrObject *)exception->value.object)->nodeMap;
    if (!fields->isValid || fields->buckets == ZR_NULL) {
        return ZR_NULL;
    }
    remaining = fields->elementCount;
    for (bucket = 0; bucket < fields->capacity && remaining > 0; ++bucket) {
        const SZrHashKeyValuePair *pair;
        for (pair = fields->buckets[bucket]; pair != ZR_NULL && remaining > 0; pair = pair->next) {
            const SZrString *key;
            const TZrChar *bytes;
            --remaining;
            if (pair->key.type != ZR_VALUE_TYPE_STRING || !pair->key.isGarbageCollectable ||
                pair->key.value.object == ZR_NULL || pair->key.value.object->type != ZR_RAW_OBJECT_TYPE_STRING) {
                continue;
            }
            key = (const SZrString *)pair->key.value.object;
            bytes = ZrCore_String_GetNativeString(key);
            if (bytes != ZR_NULL && ZrCore_String_GetByteLength(key) == sizeof("message") - 1 &&
                memcmp(bytes, "message", sizeof("message") - 1) == 0) {
                return &pair->value;
            }
        }
    }
    return ZR_NULL;
}

/* Observe the failed call before the enclosing TryRun performs its cleanup. */
static void project_failure_diagnostic_log(const SZrState *state, EZrThreadStatus status) {
    const char *enabled = getenv("ZR_VM_TEST_PROJECT_RUN_DIAGNOSTIC");
    if (enabled == ZR_NULL || strcmp(enabled, "1") != 0 || status == ZR_THREAD_STATUS_FINE || state == ZR_NULL) {
        return;
    }

    const TZrBool hasException = state->hasCurrentException;
    const EZrThreadStatus threadStatus = state->threadStatus;
    const int exceptionStatus = hasException ? (int)state->currentExceptionStatus : -1;
    const int exceptionType = hasException ? (int)state->currentException.type : -1;
    const SZrRawObject *exceptionObject =
        hasException && state->currentException.isGarbageCollectable ? state->currentException.value.object : ZR_NULL;
    const SZrCallBindingDiagnostic binding = state->lastCallBindingError;
    const TZrChar *moduleDiagnostic = ZrCore_GlobalState_GetModuleLoadDiagnostic(state->global);
    const SZrTypeValue *message =
        hasException ? project_failure_diagnostic_own_message(&state->currentException) : ZR_NULL;
    const SZrString *messageString =
        message != ZR_NULL && message->type == ZR_VALUE_TYPE_STRING && message->isGarbageCollectable &&
                message->value.object != ZR_NULL && message->value.object->type == ZR_RAW_OBJECT_TYPE_STRING
            ? (const SZrString *)message->value.object : ZR_NULL;
    const TZrChar *messageBytes =
        messageString != ZR_NULL ? ZrCore_String_GetNativeString(messageString) : ZR_NULL;
    const size_t messageLength = (size_t)ZrCore_String_GetByteLength(messageString);

    fprintf(stderr, "[zr-test-project-run] test=%s status=%u thread=%u hasException=%u exceptionStatus=%d "
                    "tag=%d object=%p lastBinding=%u token=%u instruction=%u candidate=%u "
                    "expected=%llu actual=%llu moduleLoad=%s\n",
            Unity.CurrentTestName != ZR_NULL ? Unity.CurrentTestName : "<unknown>",
            (unsigned)status, (unsigned)threadStatus, (unsigned)hasException, exceptionStatus, exceptionType,
            (void *)exceptionObject, (unsigned)binding.status, (unsigned)binding.targetMetadataToken,
            (unsigned)binding.instructionIndex, (unsigned)binding.candidateIndex,
            (unsigned long long)binding.expected, (unsigned long long)binding.actual,
            moduleDiagnostic != ZR_NULL ? moduleDiagnostic : "<none>");
    fprintf(stderr, "[zr-test-project-run] ownMessage=%u messageTag=%d bytes=%zu: ",
            (unsigned)(messageBytes != ZR_NULL), message != ZR_NULL ? (int)message->type : -1, messageLength);
    if (messageBytes != ZR_NULL) {
        fwrite(messageBytes, 1, messageLength, stderr);
    }
    fputc('\n', stderr);
    project_failure_diagnostic_providers(state, &binding);
}
