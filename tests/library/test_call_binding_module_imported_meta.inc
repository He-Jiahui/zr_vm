#include "zr_vm_core/gc.h"
#include "zr_vm_core/module_call_binding.h"

#define IMPORTED_META_REQUIRE(condition, message) \
    do { \
        if (!(condition)) { failure = (message); goto cleanup; } \
    } while (0)

/* Inspect the real import contract before linking; every check releases test pins. */
static void test_source_module_meta_call_is_token_bound(void) {
    SZrGcNativeCallPin pins[6] = {0};
    const char *failure = ZR_NULL;
    SZrFunction *consumer = ZR_NULL;
    SZrString *path = ZR_NULL;
    SZrObjectModule *module = ZR_NULL;
    SZrFunction *metadata = ZR_NULL;
    SZrFunctionCallSiteCacheEntry *entry = ZR_NULL;
    const SZrMetadataTokenRecord *definition = ZR_NULL;
    const SZrMetadataTokenRecord *owner = ZR_NULL;
    const SZrTypeValue *constant = ZR_NULL;
    SZrFunction *candidate = ZR_NULL;
    SZrObjectPrototype *prototype = ZR_NULL;
    const SZrMeta *meta = ZR_NULL;
    SZrCallBindingDiagnostic diagnostic = {0};
    TZrUInt32 matches = 0u;
    TZrUInt32 methodMatches = 0u;
    TZrUInt32 metaMatches = 0u;
    TZrUInt64 layoutHash = 0u;
    TZrInt64 result = 0;
    TZrBool linked = ZR_FALSE;

    provider.source = "pub class Callable { pub @call(value: int): int { return value + 1; } }";
    consumer = compile_consumer("var provider = import(\"call_binding_provider\"); "
            "var value = new provider.Callable(); return value(41);");
    IMPORTED_META_REQUIRE(consumer != ZR_NULL, "Imported @call consumer must compile");
    IMPORTED_META_REQUIRE(ZrCore_Gc_NativeCallPinObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(consumer), &pins[0]),
            "Consumer must remain fixed during provider loading");
    path = ZrCore_String_CreateFromNative(state, (TZrNativeString)provider.path);
    IMPORTED_META_REQUIRE(path != ZR_NULL, "Provider path must exist");
    IMPORTED_META_REQUIRE(ZrCore_Gc_NativeCallPinObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(path), &pins[1]),
            "Provider path must remain fixed during loading");
    module = ZrCore_Module_ImportByPath(state, path);
    IMPORTED_META_REQUIRE(module != ZR_NULL && ZrCore_Module_GetFromCache(state, path) == module,
            "Provider must be the actual cached runtime module");
    IMPORTED_META_REQUIRE(ZrCore_Gc_NativeCallPinObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(module), &pins[2]),
            "Provider module must remain fixed during metadata lookup");
    metadata = ZrCore_CallBinding_GetModuleFunction(state, module);
    IMPORTED_META_REQUIRE(metadata != ZR_NULL && metadata->super.type == ZR_RAW_OBJECT_TYPE_FUNCTION &&
            !metadata->super.isNative, "Provider must expose a VM metadata function");
    IMPORTED_META_REQUIRE(ZrCore_Gc_NativeCallPinObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(metadata), &pins[3]),
            "Provider metadata function must remain fixed");

    for (TZrUInt32 row = 0u; row < consumer->callSiteCacheLength; ++row) {
        SZrFunctionCallSiteCacheEntry *item = &consumer->callSiteCaches[row];
        if (item->bindingLocation.kind != ZR_CALL_BINDING_RELOCATION_VM_MODULE ||
            item->binding.contract.bindingKind != ZR_CALL_BINDING_DIRECT ||
            item->binding.contract.operation != ZR_CALL_BINDING_OPERATION_META) continue;
        entry = item;
        ++matches;
    }
    IMPORTED_META_REQUIRE(matches == 1u, "Consumer must contain exactly one imported direct META binding");
    IMPORTED_META_REQUIRE(metadata->moduleSignatureHash != 0u &&
            entry->binding.contract.moduleSignatureHash == metadata->moduleSignatureHash,
            "Consumer must select this actual provider by module signature");
    IMPORTED_META_REQUIRE(entry->binding.target.targetKind == ZR_CALL_BINDING_TARGET_NONE,
            "Imported META target must still require real linking");
    IMPORTED_META_REQUIRE(metadata->metadataTokenRecords != ZR_NULL, "Provider token records must exist");
    matches = 0u;
    for (TZrUInt32 row = 0u; row < metadata->metadataTokenRecordLength; ++row) {
        const SZrMetadataTokenRecord *record = &metadata->metadataTokenRecords[row];
        if (record->token != entry->binding.contract.targetMetadataToken) continue;
        definition = record;
        ++matches;
    }
    IMPORTED_META_REQUIRE(matches == 1u &&
            ZR_METADATA_TOKEN_TABLE(definition->token) == ZR_METADATA_TABLE_MEMBER_DEF &&
            definition->reserved0 == ZR_METADATA_TOKEN_RECORD_CALLABLE_CONSTANT,
            "META target must name one real provider callable constant");
    IMPORTED_META_REQUIRE(metadata->constantValueList != ZR_NULL &&
            definition->ownerIndex < metadata->constantValueLength, "Callable constant index must be valid");
    constant = &metadata->constantValueList[definition->ownerIndex];
    IMPORTED_META_REQUIRE(constant->type == ZR_VALUE_TYPE_FUNCTION && constant->isGarbageCollectable &&
            !constant->isNative && constant->value.object != ZR_NULL &&
            constant->value.object->type == ZR_RAW_OBJECT_TYPE_FUNCTION && !constant->value.object->isNative,
            "Provider callable constant must contain a real VM function");
    candidate = (SZrFunction *)constant->value.object;
    IMPORTED_META_REQUIRE(ZrCore_Gc_NativeCallPinObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(candidate), &pins[4]),
            "Provider META function must remain fixed during linking and execution");
    IMPORTED_META_REQUIRE(definition->signatureHash != 0u &&
            definition->signatureHash == ZrCore_CallBinding_FunctionSignatureHash(candidate) &&
            entry->binding.contract.signatureHash == definition->signatureHash &&
            entry->binding.contract.signatureToken == definition->relatedToken,
            "Consumer, definition and actual META function signatures must agree");
    IMPORTED_META_REQUIRE(definition->ownerToken != 0u &&
            entry->binding.contract.ownerTypeToken == definition->ownerToken,
            "Consumer must retain the actual provider owner token");
    matches = 0u;
    for (TZrUInt32 row = 0u; row < metadata->metadataTokenRecordLength; ++row) {
        const SZrMetadataTokenRecord *record = &metadata->metadataTokenRecords[row];
        if (record->token != definition->ownerToken) continue;
        owner = record;
        ++matches;
    }
    IMPORTED_META_REQUIRE(matches == 1u && ZR_METADATA_TOKEN_TABLE(owner->token) == ZR_METADATA_TABLE_TYPE_DEF &&
            owner->reserved0 == ZR_METADATA_TOKEN_RECORD_CALLABLE_OWNER,
            "META function must have one real callable owner definition");
    layoutHash = ZrCore_CallBinding_PrototypeLayoutHash(metadata, owner->ownerIndex);
    IMPORTED_META_REQUIRE(layoutHash != 0u && owner->signatureHash == layoutHash &&
            definition->layoutHash == layoutHash && entry->binding.contract.layoutHash == layoutHash &&
            entry->binding.contract.layoutVersion == definition->layoutVersion,
            "Consumer, callable and serialized owner layout must agree");
    IMPORTED_META_REQUIRE(metadata->prototypeInstances != ZR_NULL &&
            owner->ownerIndex < metadata->prototypeInstancesLength, "Owner prototype index must be valid");
    prototype = metadata->prototypeInstances[owner->ownerIndex];
    IMPORTED_META_REQUIRE(prototype != ZR_NULL && prototype->super.super.type == ZR_RAW_OBJECT_TYPE_OBJECT &&
            !prototype->super.super.isNative &&
            prototype->super.internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE,
            "Owner definition must refer to a real VM prototype");
    IMPORTED_META_REQUIRE(ZrCore_Gc_NativeCallPinObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(prototype), &pins[5]),
            "Owner prototype must remain fixed during linking and execution");
    meta = prototype->metaTable.metas[ZR_META_CALL];
    IMPORTED_META_REQUIRE(meta != ZR_NULL && meta->metaType == ZR_META_CALL && meta->function == candidate,
            "Actual CALL meta slot must contain the provider callable constant");
    for (TZrUInt32 slot = 0u; slot < ZR_META_ENUM_MAX; ++slot) {
        const SZrMeta *item = prototype->metaTable.metas[slot];
        if (item != ZR_NULL && item->function == candidate) ++metaMatches;
    }
    IMPORTED_META_REQUIRE(metaMatches == 1u, "META callable must occur in exactly one actual meta slot");
    IMPORTED_META_REQUIRE(prototype->memberDescriptorCount <= prototype->memberDescriptorCapacity &&
            (prototype->memberDescriptorCount == 0u || prototype->memberDescriptors != ZR_NULL),
            "Owner descriptor array must be valid");
    for (TZrUInt32 row = 0u; row < prototype->memberDescriptorCount; ++row) {
        if (prototype->memberDescriptors[row].methodFunction == candidate) ++methodMatches;
    }
    IMPORTED_META_REQUIRE(methodMatches == 0u, "META fixture must use its meta table without a method descriptor");

    linked = ZrCore_CallBinding_LinkImportedModule(state, consumer, module, &diagnostic);
    if (!linked) {
        fprintf(stderr, "[zr-test-imported-meta] linked=%u status=%u token=0x%08x instruction=%u "
                        "candidate=%u expected=%llu actual=%llu\n",
                (unsigned)linked, (unsigned)diagnostic.status, (unsigned)diagnostic.targetMetadataToken,
                (unsigned)diagnostic.instructionIndex, (unsigned)diagnostic.candidateIndex,
                (unsigned long long)diagnostic.expected, (unsigned long long)diagnostic.actual);
        fprintf(stderr, "[zr-test-imported-meta] contractToken=0x%08x instruction=%u moduleSig=%llu "
                        "providerSig=%llu signature=0x%08x hash=%llu owner=0x%08x ownerIndex=%u "
                        "layoutVersion=%u layoutHash=%llu methodMatches=%u metaMatches=%u\n",
                (unsigned)entry->binding.contract.targetMetadataToken, (unsigned)entry->instructionIndex,
                (unsigned long long)entry->binding.contract.moduleSignatureHash,
                (unsigned long long)metadata->moduleSignatureHash,
                (unsigned)entry->binding.contract.signatureToken, (unsigned long long)definition->signatureHash,
                (unsigned)owner->token, (unsigned)owner->ownerIndex,
                (unsigned)entry->binding.contract.layoutVersion, (unsigned long long)layoutHash,
                (unsigned)methodMatches, (unsigned)metaMatches);
    }
    IMPORTED_META_REQUIRE(linked, "Imported @call must link through its real provider meta table");
    IMPORTED_META_REQUIRE(entry->binding.target.targetKind == ZR_CALL_BINDING_TARGET_VM &&
            entry->binding.target.vm.function == candidate && entry->picSlotCount == 1u &&
            entry->picSlots[0].cachedOwnerPrototype == prototype &&
            entry->picSlots[0].cachedFunction == candidate &&
            entry->binding.target.ownerLayoutGeneration == prototype->layoutGeneration,
            "Linked META target and owner must match the observed provider");
    IMPORTED_META_REQUIRE(ZrTests_Runtime_Function_ExecuteExpectInt64(state, consumer, &result),
            "Linked imported @call must execute");
    IMPORTED_META_REQUIRE(result == 42 && entry->runtimeHitCount > 0u &&
            entry->binding.target.targetKind == ZR_CALL_BINDING_TARGET_VM &&
            entry->binding.target.vm.function == candidate && entry->picSlots[0].cachedOwnerPrototype == prototype,
            "Imported @call must return 42 through the exact bound target");

cleanup:
    for (TZrUInt32 pin = (TZrUInt32)(sizeof(pins) / sizeof(pins[0])); pin > 0u; --pin)
        ZrCore_Gc_NativeCallUnpin(state->global, &pins[pin - 1u]);
    TEST_ASSERT_TRUE_MESSAGE(failure == ZR_NULL, failure);
}

#undef IMPORTED_META_REQUIRE

#include "zr_vm_core/constant_reference.h"

typedef struct SZrImportedMetaNegativeFixture {
    SZrGcNativeCallPin pins[8];
    SZrFunction *consumer, *metadata, *candidate;
    SZrObjectModule *module;
    SZrFunctionCallSiteCacheEntry *entry;
    const SZrMetadataTokenRecord *definition, *owner;
    SZrObjectPrototype *prototype;
    SZrMeta *meta;
    SZrCompiledMemberInfo member;
    TZrUInt32 cacheIndex;
} SZrImportedMetaNegativeFixture;

#define META_NEG_CHECK(condition, message) do { if (!(condition)) return (message); } while (0)
#define META_NEG_REQUIRE(condition, message) \
    do { if (!(condition)) { failure = (message); goto cleanup; } } while (0)

static void imported_meta_negative_close(SZrImportedMetaNegativeFixture *fixture) {
    for (TZrUInt32 pin = (TZrUInt32)(sizeof(fixture->pins) / sizeof(fixture->pins[0])); pin > 0u; --pin)
        ZrCore_Gc_NativeCallUnpin(state->global, &fixture->pins[pin - 1u]);
}

/* Every borrowed witness belongs to a pinned real provider or consumer. */
static const char *imported_meta_negative_open(SZrImportedMetaNegativeFixture *fixture, const char *source) {
    SZrString *path;
    const SZrTypeValue *constant;
    TZrUInt32 matches = 0u, count, memberMatches = 0u;
    TZrSize offset = sizeof(count);
    TZrUInt64 layout;
    provider.source = source;
    fixture->consumer = compile_consumer("var provider = import(\"call_binding_provider\"); "
            "var value = new provider.Callable(); return value(41);");
    META_NEG_CHECK(fixture->consumer != ZR_NULL, "Real imported META negative consumer must compile");
    META_NEG_CHECK(ZrCore_Gc_NativeCallPinObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(fixture->consumer),
            &fixture->pins[0]), "Negative consumer pin must succeed");
    path = ZrCore_String_CreateFromNative(state, (TZrNativeString)provider.path);
    META_NEG_CHECK(path != ZR_NULL && ZrCore_Gc_NativeCallPinObject(state,
            ZR_CAST_RAW_OBJECT_AS_SUPER(path), &fixture->pins[1]), "Negative provider path must remain fixed");
    fixture->module = ZrCore_Module_ImportByPath(state, path);
    META_NEG_CHECK(fixture->module != ZR_NULL && ZrCore_Module_GetFromCache(state, path) == fixture->module &&
            ZrCore_Gc_NativeCallPinObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(fixture->module),
                    &fixture->pins[2]), "Negative provider must be the pinned cached module");
    fixture->metadata = ZrCore_CallBinding_GetModuleFunction(state, fixture->module);
    META_NEG_CHECK(fixture->metadata != ZR_NULL && fixture->metadata->super.type == ZR_RAW_OBJECT_TYPE_FUNCTION &&
            !fixture->metadata->super.isNative && ZrCore_Gc_NativeCallPinObject(state,
                    ZR_CAST_RAW_OBJECT_AS_SUPER(fixture->metadata), &fixture->pins[3]), "Negative provider metadata must be VM");
    for (TZrUInt32 row = 0u; row < fixture->consumer->callSiteCacheLength; ++row) {
        SZrFunctionCallSiteCacheEntry *entry = &fixture->consumer->callSiteCaches[row];
        if (entry->bindingLocation.kind != ZR_CALL_BINDING_RELOCATION_VM_MODULE ||
            entry->binding.contract.bindingKind != ZR_CALL_BINDING_DIRECT ||
            entry->binding.contract.operation != ZR_CALL_BINDING_OPERATION_META) continue;
        fixture->entry = entry;
        fixture->cacheIndex = row;
        ++matches;
    }
    META_NEG_CHECK(matches == 1u && fixture->metadata->moduleSignatureHash != 0u &&
            fixture->entry->binding.contract.moduleSignatureHash == fixture->metadata->moduleSignatureHash &&
            fixture->entry->binding.target.targetKind == ZR_CALL_BINDING_TARGET_NONE,
            "Negative fixture must contain one authentic unlinked imported META contract");
    META_NEG_CHECK(fixture->metadata->metadataTokenRecords != ZR_NULL, "Negative provider records must exist");
    matches = 0u;
    for (TZrUInt32 row = 0u; row < fixture->metadata->metadataTokenRecordLength; ++row) {
        const SZrMetadataTokenRecord *record = &fixture->metadata->metadataTokenRecords[row];
        if (record->token == fixture->entry->binding.contract.targetMetadataToken) {
            fixture->definition = record;
            ++matches;
        }
    }
    META_NEG_CHECK(matches == 1u && ZR_METADATA_TOKEN_TABLE(fixture->definition->token) == ZR_METADATA_TABLE_MEMBER_DEF &&
            fixture->definition->reserved0 == ZR_METADATA_TOKEN_RECORD_CALLABLE_CONSTANT &&
            fixture->metadata->constantValueList != ZR_NULL &&
            fixture->definition->ownerIndex < fixture->metadata->constantValueLength, "Negative callable definition must be unique");
    constant = &fixture->metadata->constantValueList[fixture->definition->ownerIndex];
    META_NEG_CHECK(constant->type == ZR_VALUE_TYPE_FUNCTION && constant->isGarbageCollectable && !constant->isNative &&
            constant->value.object != ZR_NULL && constant->value.object->type == ZR_RAW_OBJECT_TYPE_FUNCTION &&
            !constant->value.object->isNative, "Negative callable constant must be a VM function");
    fixture->candidate = (SZrFunction *)constant->value.object;
    META_NEG_CHECK(ZrCore_Gc_NativeCallPinObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(fixture->candidate),
            &fixture->pins[4]), "Negative candidate pin must succeed");
    META_NEG_CHECK(fixture->definition->signatureHash != 0u &&
            ZrCore_CallBinding_FunctionSignatureHash(fixture->candidate) == fixture->definition->signatureHash &&
            fixture->entry->binding.contract.signatureHash == fixture->definition->signatureHash &&
            fixture->entry->binding.contract.signatureToken == fixture->definition->relatedToken &&
            fixture->definition->ownerToken != 0u &&
            fixture->entry->binding.contract.ownerTypeToken == fixture->definition->ownerToken,
            "Negative actual callable and consumer identities must agree");
    matches = 0u;
    for (TZrUInt32 row = 0u; row < fixture->metadata->metadataTokenRecordLength; ++row) {
        const SZrMetadataTokenRecord *record = &fixture->metadata->metadataTokenRecords[row];
        if (record->token == fixture->definition->ownerToken) { fixture->owner = record; ++matches; }
    }
    META_NEG_CHECK(matches == 1u && ZR_METADATA_TOKEN_TABLE(fixture->owner->token) == ZR_METADATA_TABLE_TYPE_DEF &&
            fixture->owner->reserved0 == ZR_METADATA_TOKEN_RECORD_CALLABLE_OWNER, "Negative owner must be unique");
    layout = ZrCore_CallBinding_PrototypeLayoutHash(fixture->metadata, fixture->owner->ownerIndex);
    META_NEG_CHECK(layout != 0u && layout == fixture->owner->signatureHash && layout == fixture->definition->layoutHash &&
            layout == fixture->entry->binding.contract.layoutHash &&
            fixture->entry->binding.contract.layoutVersion == fixture->definition->layoutVersion &&
            fixture->metadata->prototypeInstances != ZR_NULL &&
            fixture->owner->ownerIndex < fixture->metadata->prototypeInstancesLength, "Negative owner layout must agree");
    fixture->prototype = fixture->metadata->prototypeInstances[fixture->owner->ownerIndex];
    META_NEG_CHECK(fixture->prototype != ZR_NULL && fixture->prototype->super.super.type == ZR_RAW_OBJECT_TYPE_OBJECT &&
            !fixture->prototype->super.super.isNative &&
            fixture->prototype->super.internalType == ZR_OBJECT_INTERNAL_TYPE_OBJECT_PROTOTYPE &&
            ZrCore_Gc_NativeCallPinObject(state, ZR_CAST_RAW_OBJECT_AS_SUPER(fixture->prototype),
                    &fixture->pins[5]), "Negative owner must be a pinned VM prototype");
    fixture->meta = fixture->prototype->metaTable.metas[ZR_META_CALL];
    META_NEG_CHECK(fixture->meta != ZR_NULL && fixture->meta->metaType == ZR_META_CALL &&
            fixture->meta->function == fixture->candidate, "Negative real CALL slot must match its constant");
    matches = 0u;
    for (TZrUInt32 slot = 0u; slot < ZR_META_ENUM_MAX; ++slot)
        if (fixture->prototype->metaTable.metas[slot] != ZR_NULL &&
            fixture->prototype->metaTable.metas[slot]->function == fixture->candidate) ++matches;
    META_NEG_CHECK(matches == 1u && fixture->prototype->memberDescriptorCount <= fixture->prototype->memberDescriptorCapacity &&
            (fixture->prototype->memberDescriptorCount == 0u || fixture->prototype->memberDescriptors != ZR_NULL),
            "Negative callable must have one META identity and valid descriptor storage");
    for (TZrUInt32 row = 0u; row < fixture->prototype->memberDescriptorCount; ++row)
        META_NEG_CHECK(fixture->prototype->memberDescriptors[row].methodFunction != fixture->candidate,
                "Negative META callable must not have an ordinary method descriptor");
    META_NEG_CHECK(fixture->metadata->prototypeData != ZR_NULL &&
            fixture->metadata->prototypeDataLength >= sizeof(count), "Negative serialized prototypes must exist");
    memcpy(&count, fixture->metadata->prototypeData, sizeof(count));
    META_NEG_CHECK(count == fixture->metadata->prototypeCount && fixture->owner->ownerIndex < count,
            "Negative serialized prototype count must agree");
    for (TZrUInt32 index = 0u; index <= fixture->owner->ownerIndex; ++index) {
        SZrCompiledPrototypeInfo info;
        TZrUInt64 prefix, bytes;
        META_NEG_CHECK(offset <= fixture->metadata->prototypeDataLength &&
                fixture->metadata->prototypeDataLength - offset >= sizeof(info), "Negative prototype header must be bounded");
        memcpy(&info, fixture->metadata->prototypeData + offset, sizeof(info));
        prefix = sizeof(info) + ((TZrUInt64)info.inheritsCount + info.decoratorsCount) * sizeof(TZrUInt32);
        bytes = prefix + (TZrUInt64)info.membersCount * sizeof(fixture->member);
        META_NEG_CHECK(bytes <= fixture->metadata->prototypeDataLength - offset, "Negative prototype members must be bounded");
        if (index == fixture->owner->ownerIndex) {
            for (TZrUInt32 row = 0u; row < info.membersCount; ++row) {
                SZrCompiledMemberInfo member;
                memcpy(&member, fixture->metadata->prototypeData + offset + (TZrSize)prefix +
                        (TZrSize)row * sizeof(member), sizeof(member));
                if (member.functionConstantIndex != fixture->definition->ownerIndex ||
                    (member.memberType != ZR_AST_CONSTANT_CLASS_META_FUNCTION &&
                     member.memberType != ZR_AST_CONSTANT_STRUCT_META_FUNCTION)) continue;
                fixture->member = member;
                ++memberMatches;
            }
        }
        offset += (TZrSize)bytes;
    }
    META_NEG_CHECK(memberMatches == 1u && fixture->member.isMetaMethod == ZR_TRUE &&
            fixture->member.metaType == ZR_META_CALL && fixture->member.isStatic <= ZR_TRUE,
            "Negative serialized owning CALL member must be unique");
    fprintf(stderr, "[zr-test-imported-meta-negative] fixture static=%u token=0x%08x instruction=%u owner=0x%08x\n",
            (unsigned)fixture->member.isStatic, (unsigned)fixture->definition->token,
            (unsigned)fixture->entry->instructionIndex, (unsigned)fixture->owner->token);
    return ZR_NULL;
}

static TZrBool imported_meta_negative_link(SZrImportedMetaNegativeFixture *fixture,
        const char *label, EZrCallBindingStatus expected) {
    SZrCallBindingDiagnostic diagnostic = {0};
    TZrBool linked;
    ZrCore_CallBinding_Invalidate(&fixture->entry->binding);
    linked = ZrCore_CallBinding_LinkImportedModule(state, fixture->consumer, fixture->module, &diagnostic);
    fprintf(stderr, "[zr-test-imported-meta-negative] %s linked=%u status=%u token=0x%08x instruction=%u contractInstruction=%u\n",
            label, (unsigned)linked, (unsigned)diagnostic.status,
            (unsigned)diagnostic.targetMetadataToken, (unsigned)diagnostic.instructionIndex,
            (unsigned)fixture->entry->instructionIndex);
    if (expected == ZR_CALL_BINDING_OK)
        return linked && fixture->entry->binding.target.targetKind == ZR_CALL_BINDING_TARGET_VM &&
                fixture->entry->binding.target.vm.function == fixture->candidate &&
                fixture->entry->picSlots[0].cachedOwnerPrototype == fixture->prototype;
    return !linked && diagnostic.status == expected &&
            diagnostic.targetMetadataToken == fixture->entry->binding.contract.targetMetadataToken &&
            fixture->entry->binding.target.targetKind == ZR_CALL_BINDING_TARGET_NONE;
}

static TZrBool imported_meta_negative_prepare(SZrImportedMetaNegativeFixture *fixture,
        const SZrTypeValue *receiver, const char *label, EZrCallBindingStatus expected) {
    SZrCallBindingDiagnostic diagnostic = {0};
    SZrTypeValue callable;
    TZrBool prepared = ZrCore_CallBinding_PrepareMember(state, fixture->consumer, fixture->cacheIndex,
            receiver, &callable, &diagnostic);
    fprintf(stderr, "[zr-test-imported-meta-negative] %s prepared=%u status=%u token=0x%08x instruction=%u\n",
            label, (unsigned)prepared, (unsigned)diagnostic.status,
            (unsigned)diagnostic.targetMetadataToken, (unsigned)diagnostic.instructionIndex);
    if (expected == ZR_CALL_BINDING_OK)
        return prepared && callable.isGarbageCollectable && !callable.isNative &&
                callable.value.object == ZR_CAST_RAW_OBJECT_AS_SUPER(fixture->candidate);
    return !prepared && diagnostic.status == expected &&
            diagnostic.targetMetadataToken == fixture->entry->binding.contract.targetMetadataToken &&
            diagnostic.instructionIndex == fixture->entry->instructionIndex &&
            fixture->entry->binding.target.targetKind == ZR_CALL_BINDING_TARGET_NONE;
}

static void test_imported_meta_rejects_missing_or_wrong_call_slot(void) {
    SZrImportedMetaNegativeFixture fixture = {0};
    const char *failure = imported_meta_negative_open(&fixture,
            "pub class Callable { pub @call(value: int): int { return value + 1; } }");
    TZrBool matched;
    EZrMetaType savedType;
    SZrFunction *savedFunction;
    if (failure != ZR_NULL) goto cleanup;
    fixture.prototype->metaTable.metas[ZR_META_CALL] = ZR_NULL;
    matched = imported_meta_negative_link(&fixture, "missing-call", ZR_CALL_BINDING_TARGET_NOT_FOUND);
    fixture.prototype->metaTable.metas[ZR_META_CALL] = fixture.meta;
    META_NEG_REQUIRE(matched, "Missing CALL slot must leave an unlinked target");
    savedType = fixture.meta->metaType;
    fixture.meta->metaType = ZR_META_CONSTRUCTOR;
    matched = imported_meta_negative_link(&fixture, "wrong-meta-type", ZR_CALL_BINDING_INVALID_RELOCATION);
    fixture.meta->metaType = savedType;
    META_NEG_REQUIRE(matched, "Inconsistent CALL meta type must be rejected");
    savedFunction = fixture.meta->function;
    fixture.meta->function = ZR_NULL;
    matched = imported_meta_negative_link(&fixture, "missing-call-function", ZR_CALL_BINDING_TARGET_KIND_MISMATCH);
    fixture.meta->function = savedFunction;
    META_NEG_REQUIRE(matched, "Missing META VM function must be rejected");
cleanup:
    imported_meta_negative_close(&fixture);
    TEST_ASSERT_TRUE_MESSAGE(failure == ZR_NULL, failure);
}

static void test_imported_meta_rejects_ambiguous_meta_function(void) {
    SZrImportedMetaNegativeFixture fixture = {0};
    const char *failure = imported_meta_negative_open(&fixture,
            "pub class Callable { pub @constructor() {} pub @call(value: int): int { return value + 1; } }");
    SZrMeta *constructor;
    SZrFunction *savedFunction;
    TZrBool matched;
    if (failure != ZR_NULL) goto cleanup;
    constructor = fixture.prototype->metaTable.metas[ZR_META_CONSTRUCTOR];
    META_NEG_REQUIRE(constructor != ZR_NULL && constructor->metaType == ZR_META_CONSTRUCTOR &&
            constructor->function != ZR_NULL && constructor->function != fixture.candidate,
            "Ambiguity fixture must have a distinct real constructor META function");
    savedFunction = constructor->function;
    constructor->function = fixture.candidate;
    matched = imported_meta_negative_link(&fixture, "duplicate-meta-function", ZR_CALL_BINDING_AMBIGUOUS_TARGET);
    constructor->function = savedFunction;
    META_NEG_REQUIRE(matched, "One callable constant in two consistent META slots must be ambiguous");
cleanup:
    imported_meta_negative_close(&fixture);
    TEST_ASSERT_TRUE_MESSAGE(failure == ZR_NULL, failure);
}

static void test_imported_meta_rejects_static_call_receiver_abi(void) {
    SZrImportedMetaNegativeFixture fixture = {0};
    const char *failure = imported_meta_negative_open(&fixture,
            "pub class Callable { pub static @call(value: int): int { return value + 1; } }");
    if (failure != ZR_NULL) goto cleanup;
    META_NEG_REQUIRE(fixture.member.isStatic == ZR_TRUE, "Static source must produce authentic static CALL metadata");
    META_NEG_REQUIRE(imported_meta_negative_link(&fixture, "static-call", ZR_CALL_BINDING_INVALID_RELOCATION),
            "Instance META call must reject the static function receiver ABI");
cleanup:
    imported_meta_negative_close(&fixture);
    TEST_ASSERT_TRUE_MESSAGE(failure == ZR_NULL, failure);
}

static void test_imported_meta_keeps_contract_layout_and_receiver_guards(void) {
    SZrImportedMetaNegativeFixture fixture = {0};
    const char *failure = imported_meta_negative_open(&fixture,
            "pub class Callable { pub @call(value: int): int { return value + 1; } }");
    TZrUInt64 saved;
    TZrBool matched;
    SZrObject *receiver, *unrelated;
    SZrTypeValue value;
    if (failure != ZR_NULL) goto cleanup;
    saved = fixture.entry->binding.contract.signatureHash;
    fixture.entry->binding.contract.signatureHash = saved == 1u ? 2u : saved ^ 1u;
    matched = imported_meta_negative_link(&fixture, "wrong-signature", ZR_CALL_BINDING_SIGNATURE_MISMATCH);
    fixture.entry->binding.contract.signatureHash = saved;
    META_NEG_REQUIRE(matched, "META branch must preserve exact consumer signature checks");
    saved = fixture.entry->binding.contract.layoutHash;
    fixture.entry->binding.contract.layoutHash = saved == 1u ? 2u : saved ^ 1u;
    matched = imported_meta_negative_link(&fixture, "wrong-layout", ZR_CALL_BINDING_LAYOUT_MISMATCH);
    fixture.entry->binding.contract.layoutHash = saved;
    META_NEG_REQUIRE(matched, "META branch must preserve exact consumer layout checks");
    receiver = ZrCore_Object_New(state, fixture.prototype);
    META_NEG_REQUIRE(receiver != ZR_NULL && ZrCore_Gc_NativeCallPinObject(state,
            ZR_CAST_RAW_OBJECT_AS_SUPER(receiver), &fixture.pins[6]), "Actual owner instance must remain fixed");
    unrelated = ZrCore_Object_New(state, fixture.prototype->superPrototype);
    META_NEG_REQUIRE(unrelated != ZR_NULL && ZrCore_Gc_NativeCallPinObject(state,
            ZR_CAST_RAW_OBJECT_AS_SUPER(unrelated), &fixture.pins[7]), "Unrelated real instance must remain fixed");
    META_NEG_REQUIRE(imported_meta_negative_link(&fixture, "link-before-primitive-receiver", ZR_CALL_BINDING_OK),
            "META target must link before receiver checks");
    ZrCore_Value_InitAsInt(state, &value, 41);
    META_NEG_REQUIRE(imported_meta_negative_prepare(&fixture, &value, "primitive-receiver", ZR_CALL_BINDING_LAYOUT_MISMATCH),
            "Bound META call must reject a primitive receiver");
    META_NEG_REQUIRE(imported_meta_negative_link(&fixture, "link-before-unrelated-receiver", ZR_CALL_BINDING_OK),
            "Receiver rejection must permit exact relinking");
    ZrCore_Value_InitAsRawObject(state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(unrelated));
    META_NEG_REQUIRE(imported_meta_negative_prepare(&fixture, &value, "unrelated-receiver", ZR_CALL_BINDING_LAYOUT_MISMATCH),
            "Bound META call must reject a receiver outside the owner ancestry");
    META_NEG_REQUIRE(imported_meta_negative_link(&fixture, "link-before-generation-change", ZR_CALL_BINDING_OK),
            "META target must relink before owner mutation");
    saved = fixture.prototype->layoutGeneration;
    fixture.prototype->layoutGeneration = saved ^ 1u;
    ZrCore_Value_InitAsRawObject(state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(receiver));
    matched = imported_meta_negative_prepare(&fixture, &value, "changed-owner-generation", ZR_CALL_BINDING_LAYOUT_MISMATCH);
    fixture.prototype->layoutGeneration = saved;
    META_NEG_REQUIRE(matched, "Bound META call must reject an owner layout generation change");
    META_NEG_REQUIRE(imported_meta_negative_link(&fixture, "link-after-restore", ZR_CALL_BINDING_OK) &&
            imported_meta_negative_prepare(&fixture, &value, "restored-owner-receiver", ZR_CALL_BINDING_OK),
            "Restored owner and actual instance must prepare the exact META function");
cleanup:
    imported_meta_negative_close(&fixture);
    TEST_ASSERT_TRUE_MESSAGE(failure == ZR_NULL, failure);
}

#undef META_NEG_REQUIRE
#undef META_NEG_CHECK
