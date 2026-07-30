#include <stdlib.h>
#include <string.h>

#include "unity.h"

#include "container_test_common.h"
#include "runtime_support.h"
#include "backend_aot_canonical_artifact.h"
#include "zr_vm_common/zr_contract_conf.h"
#include "zr_vm_common/zr_meta_conf.h"
#include "zr_vm_core/artifact_schema.h"
#include "zr_vm_core/call_info.h"
#include "zr_vm_core/canonical_consumer.h"
#include "zr_vm_core/closure.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/stack.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_lib_container/module.h"
#include "zr_vm_lib_ffi/module.h"
#include "zr_vm_library/native_binding.h"
#include "zr_vm_parser/compiler.h"

#define REF_LIKE_ABI_TYPE_DEF_TOKEN \
    ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_DEF, 91u)
#define REF_LIKE_ABI_TYPE_REF_TOKEN \
    ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_REF, 91u)
#define REF_LIKE_ABI_TYPE_SPEC_TOKEN \
    ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_TYPE_SPEC, 91u)
#define REF_LIKE_ABI_SIGNATURE_TOKEN \
    ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_SIGNATURE, 91u)
#define REF_LIKE_ABI_MEMBER_TOKEN \
    ZR_METADATA_TOKEN_MAKE(ZR_METADATA_TABLE_MEMBER_DEF, 91u)

#define REF_LIKE_ABI_LAYOUT_HASH ((TZrUInt64)0x51a7c0de12345678ULL)
#define REF_LIKE_ABI_CONTRACT_HASH ((TZrUInt64)0x91ab1c0de7654321ULL)
#define REF_LIKE_ABI_MODULE_HASH ((TZrUInt64)0x91f00d1e5eed1234ULL)

static TZrSize write_ref_like_abi_artifact(
        EZrArtifactAbiLoweringKind loweringKind,
        TZrByte *buffer,
        TZrSize bufferCapacity,
        SZrArtifactPublicIdentity *outIdentity) {
    static const TZrByte signature[] = {
            ZR_ARTIFACT_SIGNATURE_NODE_PRIMITIVE,
            (TZrByte)ZR_VALUE_TYPE_INT64,
            0u,
            0u,
            0u};
    static const TZrByte code[] = {0x01u};
    const TZrUInt32 typeFlags = ZR_ARTIFACT_TYPE_FLAG_VALUE |
                                ZR_ARTIFACT_TYPE_FLAG_READONLY |
                                ZR_ARTIFACT_TYPE_FLAG_REF_LIKE;
    SZrArtifactTypeDefRow typeDef;
    SZrArtifactTypeIdentityRow typeRef;
    SZrArtifactTypeIdentityRow typeSpec;
    SZrArtifactContractRow contract;
    SZrArtifactLayoutRow layout;
    SZrArtifactSectionInput sections[7];
    SZrArtifactDocument document;
    SZrArtifactDiagnostic diagnostic;
    TZrSize writtenSize = 0u;

    memset(outIdentity, 0, sizeof(*outIdentity));
    outIdentity->canonicalTypeId = 91u;
    outIdentity->typeRefToken = REF_LIKE_ABI_TYPE_REF_TOKEN;
    outIdentity->typeSpecToken = REF_LIKE_ABI_TYPE_SPEC_TOKEN;
    outIdentity->signatureToken = REF_LIKE_ABI_SIGNATURE_TOKEN;
    outIdentity->typeRefHash = 0x91a1000000000001ULL;
    outIdentity->typeSpecHash = 0x91a1000000000002ULL;
    outIdentity->signatureHash = ZrCore_Artifact_HashBytes(
            signature, sizeof(signature));
    outIdentity->layoutVersion = 1u;
    outIdentity->layoutHash = REF_LIKE_ABI_LAYOUT_HASH;
    outIdentity->callableContractHash = REF_LIKE_ABI_CONTRACT_HASH;
    outIdentity->moduleHash = REF_LIKE_ABI_MODULE_HASH;

    memset(&typeDef, 0, sizeof(typeDef));
    typeDef.token = REF_LIKE_ABI_TYPE_DEF_TOKEN;
    typeDef.flags = typeFlags;
    typeDef.canonicalTypeId = outIdentity->canonicalTypeId;
    typeDef.typeSignatureHash = outIdentity->signatureHash;

    memset(&typeRef, 0, sizeof(typeRef));
    typeRef.token = REF_LIKE_ABI_TYPE_REF_TOKEN;
    typeRef.signatureToken = REF_LIKE_ABI_SIGNATURE_TOKEN;
    typeRef.canonicalTypeId = outIdentity->canonicalTypeId;
    typeRef.flags = typeFlags;
    typeRef.signatureLength = (TZrUInt32)sizeof(signature);
    typeRef.signatureHash = outIdentity->typeRefHash;
    typeRef.layoutVersion = outIdentity->layoutVersion;
    typeRef.layoutHash = outIdentity->layoutHash;
    typeSpec = typeRef;
    typeSpec.token = REF_LIKE_ABI_TYPE_SPEC_TOKEN;
    typeSpec.signatureHash = outIdentity->typeSpecHash;

    memset(&contract, 0, sizeof(contract));
    contract.memberToken = REF_LIKE_ABI_MEMBER_TOKEN;
    contract.signatureToken = REF_LIKE_ABI_SIGNATURE_TOKEN;
    contract.parameterCount = 1u;
    contract.flags = ZR_ARTIFACT_CONTRACT_FLAG_SCOPED;
    contract.escapeFlags = ZR_ARTIFACT_CALLABLE_ESCAPE_FLAG_SCOPED_INPUT;
    contract.abiLoweringKind = loweringKind;
    contract.contractHash = outIdentity->callableContractHash;

    memset(&layout, 0, sizeof(layout));
    layout.typeToken = REF_LIKE_ABI_TYPE_DEF_TOKEN;
    layout.version = outIdentity->layoutVersion;
    layout.byteSize = (TZrUInt32)(sizeof(void *) * 2u);
    layout.byteAlignment = (TZrUInt32)sizeof(void *);
    layout.gcScanKind = ZR_ARTIFACT_GC_SCAN_FREE;
    layout.layoutHash = outIdentity->layoutHash;

    sections[0] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_TYPE_DEF_TABLE,
            ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
            1u,
            &typeDef};
    sections[1] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_TYPE_REF_TABLE,
            ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
            1u,
            &typeRef};
    sections[2] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_TYPE_SPEC_TABLE,
            ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
            1u,
            &typeSpec};
    sections[3] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_SIGNATURE_HEAP,
            ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
            (TZrUInt32)sizeof(signature),
            signature};
    sections[4] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_CONTRACT_TABLE,
            ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
            1u,
            &contract};
    sections[5] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_LAYOUT_TABLE,
            ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
            1u,
            &layout};
    sections[6] = (SZrArtifactSectionInput){
            ZR_ARTIFACT_SECTION_CODE_TABLE,
            ZR_ARTIFACT_SECTION_FLAG_MANDATORY,
            (TZrUInt32)sizeof(code),
            code};

    memset(&document, 0, sizeof(document));
    document.kind = ZR_ARTIFACT_KIND_ZRO;
    document.identity = *outIdentity;
    document.sectionCount = 7u;
    document.sections = sections;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_Artifact_Write(
                    &document,
                    buffer,
                    bufferCapacity,
                    &writtenSize,
                    &diagnostic));
    return writtenSize;
}

static const ZrLibTypeDescriptor *find_type_descriptor(
        const ZrLibModuleDescriptor *module,
        const char *name) {
    if (module == NULL || name == NULL) {
        return NULL;
    }
    for (TZrSize index = 0u; index < module->typeCount; index++) {
        if (module->types[index].name != NULL &&
            strcmp(module->types[index].name, name) == 0) {
            return &module->types[index];
        }
    }
    return NULL;
}

static const ZrLibFieldDescriptor *find_field_descriptor(
        const ZrLibTypeDescriptor *type,
        TZrUInt32 role) {
    if (type == NULL) {
        return NULL;
    }
    for (TZrSize index = 0u; index < type->fieldCount; index++) {
        if (type->fields[index].contractRole == role) {
            return &type->fields[index];
        }
    }
    return NULL;
}

static const ZrLibMethodDescriptor *find_method_descriptor(
        const ZrLibTypeDescriptor *type,
        TZrUInt32 role) {
    if (type == NULL) {
        return NULL;
    }
    for (TZrSize index = 0u; index < type->methodCount; index++) {
        if (type->methods[index].contractRole == role) {
            return &type->methods[index];
        }
    }
    return NULL;
}

static const ZrLibMetaMethodDescriptor *find_meta_descriptor(
        const ZrLibTypeDescriptor *type,
        EZrMetaType metaType) {
    if (type == NULL) {
        return NULL;
    }
    for (TZrSize index = 0u; index < type->metaMethodCount; index++) {
        if (type->metaMethods[index].metaType == metaType) {
            return &type->metaMethods[index];
        }
    }
    return NULL;
}

static SZrFunction *compile_source(
        SZrState *state,
        const char *path,
        const char *source) {
    SZrString *sourceName;

    if (state == NULL || path == NULL || source == NULL) {
        return NULL;
    }
    sourceName = ZrCore_String_Create(
            state, (TZrNativeString)path, strlen(path));
    if (sourceName == NULL) {
        return NULL;
    }
    return ZrParser_Source_Compile(
            state, source, strlen(source), sourceName);
}

static TZrInt64 force_pool_ffi_gc_native(SZrState *state) {
    SZrCallInfo *nativeCallInfo;
    TZrStackValuePointer resultSlot;

    if (state == NULL || state->callInfoList == NULL) {
        return 0;
    }
    nativeCallInfo = state->callInfoList;
    resultSlot = nativeCallInfo->functionBase.valuePointer;
    if (resultSlot == NULL) {
        return 0;
    }
    ZrCore_GarbageCollector_GcFull(state, ZR_TRUE);
    ZrCore_Value_InitAsInt(state, ZrCore_Stack_GetValue(resultSlot), 1);
    state->stackTop.valuePointer = resultSlot + 1;
    return 1;
}

static void install_pool_ffi_gc_probe(SZrState *state) {
    SZrObject *globalObject;
    SZrClosureNative *closure;
    SZrString *nameString;
    SZrTypeValue key;
    SZrTypeValue value;

    TEST_ASSERT_NOT_NULL(state);
    TEST_ASSERT_NOT_NULL(state->global);
    globalObject = ZR_CAST_OBJECT(
            state, state->global->zrObject.value.object);
    TEST_ASSERT_NOT_NULL(globalObject);

    closure = ZrCore_ClosureNative_New(state, 0u);
    TEST_ASSERT_NOT_NULL(closure);
    closure->nativeFunction = force_pool_ffi_gc_native;
    ZrCore_RawObject_MarkAsPermanent(
            state, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));

    nameString = ZrCore_String_CreateFromNative(
            state, "__forcePoolFfiGc");
    TEST_ASSERT_NOT_NULL(nameString);
    ZrCore_Value_InitAsRawObject(
            state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(nameString));
    key.type = ZR_VALUE_TYPE_STRING;
    ZrCore_Value_InitAsRawObject(
            state, &value, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    value.type = ZR_VALUE_TYPE_CLOSURE;
    value.isNative = ZR_TRUE;
    ZrCore_Object_SetValue(state, globalObject, &key, &value);
}

static void test_public_ref_like_abi_contract_is_validated_by_vm_and_aot(void) {
    const TZrUInt32 typeFlags = ZR_ARTIFACT_TYPE_FLAG_VALUE |
                                ZR_ARTIFACT_TYPE_FLAG_READONLY |
                                ZR_ARTIFACT_TYPE_FLAG_REF_LIKE;
    TZrByte artifact[4096];
    SZrArtifactPublicIdentity identity;
    SZrCanonicalConsumerProjection vmProjection;
    SZrCanonicalConsumerProjection aotProjection;
    SZrCanonicalPublicRefLikeAbiExpectation expected;
    SZrArtifactDiagnostic diagnostic;
    TZrSize artifactLength;

    artifactLength = write_ref_like_abi_artifact(
            ZR_ARTIFACT_ABI_LOWERING_ZR_VALUE_FRAME,
            artifact,
            sizeof(artifact),
            &identity);
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_CanonicalConsumer_Open(
                    artifact,
                    artifactLength,
                    &identity,
                    &vmProjection,
                    &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            backend_aot_open_canonical_artifact(
                    artifact,
                    artifactLength,
                    &identity,
                    &aotProjection,
                    &diagnostic));

    memset(&expected, 0, sizeof(expected));
    expected.typeToken = REF_LIKE_ABI_TYPE_REF_TOKEN;
    expected.callableSignatureToken = REF_LIKE_ABI_SIGNATURE_TOKEN;
    expected.typeRefHash = identity.typeRefHash;
    expected.typeFlags = typeFlags;
    expected.layoutVersion = 1u;
    expected.layoutHash = REF_LIKE_ABI_LAYOUT_HASH;
    expected.callableEscapeFlags =
            ZR_ARTIFACT_CALLABLE_ESCAPE_FLAG_SCOPED_INPUT;
    expected.abiLoweringKind = ZR_ARTIFACT_ABI_LOWERING_ZR_VALUE_FRAME;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_CanonicalConsumer_ValidatePublicRefLikeAbi(
                    &vmProjection, &expected, &diagnostic));
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_CanonicalConsumer_ValidatePublicRefLikeAbi(
                    &aotProjection, &expected, &diagnostic));

    expected.typeRefHash++;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_TYPE_REF_HASH_MISMATCH,
            ZrCore_CanonicalConsumer_ValidatePublicRefLikeAbi(
                    &vmProjection, &expected, &diagnostic));
    expected.typeRefHash = identity.typeRefHash;
    expected.typeFlags ^= ZR_ARTIFACT_TYPE_FLAG_READONLY;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_INVALID_SIGNATURE,
            ZrCore_CanonicalConsumer_ValidatePublicRefLikeAbi(
                    &vmProjection, &expected, &diagnostic));
    expected.typeFlags = typeFlags;
    expected.layoutVersion++;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_LAYOUT_VERSION_MISMATCH,
            ZrCore_CanonicalConsumer_ValidatePublicRefLikeAbi(
                    &vmProjection, &expected, &diagnostic));
    expected.layoutVersion = 1u;
    expected.layoutHash++;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_LAYOUT_HASH_MISMATCH,
            ZrCore_CanonicalConsumer_ValidatePublicRefLikeAbi(
                    &vmProjection, &expected, &diagnostic));
    expected.layoutHash = REF_LIKE_ABI_LAYOUT_HASH;
    expected.callableEscapeFlags = 0u;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_INVALID_SIGNATURE,
            ZrCore_CanonicalConsumer_ValidatePublicRefLikeAbi(
                    &vmProjection, &expected, &diagnostic));

    artifactLength = write_ref_like_abi_artifact(
            ZR_ARTIFACT_ABI_LOWERING_NATIVE_DIRECT,
            artifact,
            sizeof(artifact),
            &identity);
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_CanonicalConsumer_Open(
                    artifact,
                    artifactLength,
                    &identity,
                    &vmProjection,
                    &diagnostic));
    expected.callableEscapeFlags =
            ZR_ARTIFACT_CALLABLE_ESCAPE_FLAG_SCOPED_INPUT;
    expected.abiLoweringKind = ZR_ARTIFACT_ABI_LOWERING_NATIVE_DIRECT;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_INVALID_SIGNATURE,
            ZrCore_CanonicalConsumer_ValidatePublicRefLikeAbi(
                    &vmProjection, &expected, &diagnostic));

    artifactLength = write_ref_like_abi_artifact(
            ZR_ARTIFACT_ABI_LOWERING_NATIVE_MARSHALLED,
            artifact,
            sizeof(artifact),
            &identity);
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            backend_aot_open_canonical_artifact(
                    artifact,
                    artifactLength,
                    &identity,
                    &aotProjection,
                    &diagnostic));
    expected.abiLoweringKind = ZR_ARTIFACT_ABI_LOWERING_NATIVE_MARSHALLED;
    TEST_ASSERT_EQUAL_INT(
            ZR_ARTIFACT_STATUS_OK,
            ZrCore_CanonicalConsumer_ValidatePublicRefLikeAbi(
                    &aotProjection, &expected, &diagnostic));
}

static void test_pooling_and_pinned_pointer_descriptors_publish_structured_contracts(void) {
    const ZrLibModuleDescriptor *pooling =
            ZrVmLibContainer_GetPoolingModuleDescriptor();
    const ZrLibModuleDescriptor *ffi = ZrVmLibFfi_GetModuleDescriptor();
    const ZrLibTypeDescriptor *pool;
    const ZrLibTypeDescriptor *lease;
    const ZrLibTypeDescriptor *pointer;

    TEST_ASSERT_NOT_NULL(pooling);
    TEST_ASSERT_EQUAL_STRING("zr.pooling", pooling->moduleName);
    pool = find_type_descriptor(pooling, "BufferPool");
    lease = find_type_descriptor(pooling, "PoolLease");
    TEST_ASSERT_NOT_NULL(pool);
    TEST_ASSERT_NOT_NULL(lease);
    TEST_ASSERT_EQUAL_UINT64(1u, lease->genericParameterCount);
    TEST_ASSERT_BITS_HIGH(
            ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_CONTIGUOUS_SOURCE_OWNER),
            lease->protocolMask);
    TEST_ASSERT_NOT_NULL(find_field_descriptor(
            lease, ZR_MEMBER_CONTRACT_ROLE_INDEX_LENGTH));
    TEST_ASSERT_NOT_NULL(find_method_descriptor(
            lease, ZR_MEMBER_CONTRACT_ROLE_CONTIGUOUS_VIEW_CREATE));
    TEST_ASSERT_NOT_NULL(find_meta_descriptor(lease, ZR_META_GET_ITEM));
    TEST_ASSERT_NOT_NULL(find_meta_descriptor(lease, ZR_META_SET_ITEM));
    TEST_ASSERT_NOT_NULL(find_meta_descriptor(lease, ZR_META_CLOSE));

    TEST_ASSERT_NOT_NULL(ffi);
    pointer = find_type_descriptor(ffi, "Ptr");
    TEST_ASSERT_NOT_NULL(pointer);
    TEST_ASSERT_BITS_HIGH(
            ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_CONTIGUOUS_SOURCE_NATIVE_PINNED),
            pointer->protocolMask);
    TEST_ASSERT_NOT_NULL(find_field_descriptor(
            pointer, ZR_MEMBER_CONTRACT_ROLE_INDEX_LENGTH));
    TEST_ASSERT_NOT_NULL(find_method_descriptor(
            pointer, ZR_MEMBER_CONTRACT_ROLE_CONTIGUOUS_VIEW_CREATE));
}

static void test_pool_lease_returns_once_and_reuses_backing_after_view_nll(void) {
    static const char kSource[] =
            "var {Span} = import(\"zr.container\");\n"
            "var {BufferPool} = import(\"zr.pooling\");\n"
            "var pool = new BufferPool();\n"
            "var lease = pool.rent<int>(2);\n"
            "var firstGeneration = lease.generation;\n"
            "var view: Span<int> = lease.span();\n"
            "view[0] = 41;\n"
            "var beforeReturn = view[0];\n"
            "lease.close();\n"
            "lease.close();\n"
            "var next = pool.rent<int>(2);\n"
            "var nextView: Span<int> = next.span();\n"
            "nextView[0] = 42;\n"
            "if (beforeReturn == 41 && nextView[0] == 42 &&\n"
            "    next.generation > firstGeneration && pool.returnCount == 1 &&\n"
            "    pool.reuseCount == 1) { return 1; }\n"
            "return 0;\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, "pool_lease_reuse.zr", kSource);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(state, function);
    ZrContainerTests_DestroyState(state);
}

static void test_pool_lease_close_is_rejected_while_view_remains_live(void) {
    static const char kSource[] =
            "var {Span} = import(\"zr.container\");\n"
            "var {BufferPool} = import(\"zr.pooling\");\n"
            "var pool = new BufferPool();\n"
            "var lease = pool.rent<int>(2);\n"
            "var view: Span<int> = lease.span();\n"
            "lease.close();\n"
            "return view[0];\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(
            state, "pool_lease_close_with_live_view.zr", kSource);
    TEST_ASSERT_NULL(function);
    ZrContainerTests_DestroyState(state);
}

static void test_pool_lease_reuse_survives_full_gc_stress(void) {
    static const char kSource[] =
            "var {Span} = import(\"zr.container\");\n"
            "var {BufferPool} = import(\"zr.pooling\");\n"
            "var pool = new BufferPool();\n"
            "var sum = 0;\n"
            "for (var i = 0; i < 32; i = i + 1) {\n"
            "    var lease = pool.rent<int>(4);\n"
            "    var view: Span<int> = lease.span();\n"
            "    view[0] = i;\n"
            "    zr.__forcePoolFfiGc();\n"
            "    sum = sum + view[0];\n"
            "    lease.close();\n"
            "}\n"
            "if (sum == 496 && pool.returnCount == 32 &&\n"
            "    pool.reuseCount == 31) { return 1; }\n"
            "return 0;\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    install_pool_ffi_gc_probe(state);
    function = compile_source(state, "pool_lease_gc_stress.zr", kSource);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            state, function, &result));
    TEST_ASSERT_EQUAL_INT64(1, result);

    ZrCore_Function_Free(state, function);
    ZrContainerTests_DestroyState(state);
}

static void test_pool_lease_using_cleanup_returns_backing_on_throw(void) {
    static const char kSource[] =
            "var {BufferPool} = import(\"zr.pooling\");\n"
            "var pool = new BufferPool();\n"
            "var lease = pool.rent<int>(3);\n"
            "try {\n"
            "    using (lease) {\n"
            "        throw \"boom\";\n"
            "    }\n"
            "} catch (error) { }\n"
            "var next = pool.rent<int>(3);\n"
            "return pool.returnCount * 100 + pool.reuseCount * 10 +\n"
            "       next.generation;\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(state, "pool_lease_throw_cleanup.zr", kSource);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            state, function, &result));
    TEST_ASSERT_EQUAL_INT64(112, result);

    ZrCore_Function_Free(state, function);
    ZrContainerTests_DestroyState(state);
}

static void test_explicit_pinned_pointer_span_stays_valid_across_gc_and_owner_close(void) {
    static const char kSource[] =
            "var {Span} = import(\"zr.container\");\n"
            "var ffi = import(\"zr.ffi\");\n"
            "var buffer = ffi.BufferHandle.allocate(4);\n"
            "buffer.write(0, [11, 12, 13, 14]);\n"
            "var pin = buffer.pin();\n"
            "var view: Span<u8> = pin.span();\n"
            "buffer.close();\n"
            "var gcMarker = zr.__forcePoolFfiGc();\n"
            "view[1] = 42;\n"
            "var value = view[1];\n"
            "pin.close();\n"
            "pin.close();\n"
            "return value + gcMarker;\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrFunction *function;
    TZrInt64 result = 0;

    TEST_ASSERT_NOT_NULL(state);
    install_pool_ffi_gc_probe(state);
    function = compile_source(state, "ffi_pinned_span_gc.zr", kSource);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_TRUE(ZrTests_Runtime_Function_ExecuteExpectInt64(
            state, function, &result));
    TEST_ASSERT_EQUAL_INT64(43, result);

    ZrCore_Function_Free(state, function);
    ZrContainerTests_DestroyState(state);
}

static void test_pinned_pointer_close_is_rejected_while_view_remains_live(void) {
    static const char kSource[] =
            "var {Span} = import(\"zr.container\");\n"
            "var ffi = import(\"zr.ffi\");\n"
            "var buffer = ffi.BufferHandle.allocate(2);\n"
            "var pin = buffer.pin();\n"
            "var view: Span<u8> = pin.span();\n"
            "pin.close();\n"
            "return view[0];\n";
    SZrState *state = ZrContainerTests_CreateState();
    SZrFunction *function;

    TEST_ASSERT_NOT_NULL(state);
    function = compile_source(
            state, "ffi_pin_close_with_live_view.zr", kSource);
    TEST_ASSERT_NULL(function);
    ZrContainerTests_DestroyState(state);
}

void setUp(void) {}

void tearDown(void) {}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_public_ref_like_abi_contract_is_validated_by_vm_and_aot);
    RUN_TEST(test_pooling_and_pinned_pointer_descriptors_publish_structured_contracts);
    RUN_TEST(test_pool_lease_returns_once_and_reuses_backing_after_view_nll);
    RUN_TEST(test_pool_lease_close_is_rejected_while_view_remains_live);
    RUN_TEST(test_pool_lease_reuse_survives_full_gc_stress);
    RUN_TEST(test_pool_lease_using_cleanup_returns_backing_on_throw);
    RUN_TEST(test_explicit_pinned_pointer_span_stays_valid_across_gc_and_owner_close);
    RUN_TEST(test_pinned_pointer_close_is_rejected_while_view_remains_live);
    return UNITY_END();
}
