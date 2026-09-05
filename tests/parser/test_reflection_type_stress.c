#include "unity.h"

#include <stdio.h>
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

#include "harness/runtime_support.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/gc.h"
#include "zr_vm_core/hash_set.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/reflection.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser/compiler.h"
#include "zr_vm_parser/parser.h"

static SZrState *g_state;

void setUp(void) {
    g_state = ZrTests_Runtime_State_Create(ZR_NULL);
    TEST_ASSERT_NOT_NULL(g_state);
}

void tearDown(void) {
    if (g_state != ZR_NULL) {
        ZrTests_Runtime_State_Destroy(g_state);
        g_state = ZR_NULL;
    }
}

static TZrBool append_text(
        TZrChar *buffer,
        TZrSize capacity,
        TZrSize *length,
        const TZrChar *format,
        ...) {
    int written;
    va_list arguments;

    if (buffer == ZR_NULL || length == ZR_NULL || *length >= capacity) {
        return ZR_FALSE;
    }
    va_start(arguments, format);
    written = vsnprintf(
            buffer + *length,
            capacity - *length,
            format,
            arguments);
    va_end(arguments);
    if (written < 0 || (TZrSize)written >= capacity - *length) {
        return ZR_FALSE;
    }
    *length += (TZrSize)written;
    return ZR_TRUE;
}

static TZrChar *build_deep_type_source(TZrUInt32 depth) {
    TZrSize capacity = (TZrSize)depth * 96u + 256u;
    TZrChar *source = (TZrChar *)malloc(capacity);
    TZrSize length;

    if (source == ZR_NULL || depth == 0u) {
        free(source);
        return ZR_NULL;
    }
    length = (TZrSize)snprintf(
            source,
            capacity,
            "module reflection_deep_stress;\n"
            "pub class C0000 { pub var f0000: int; }\n");
    for (TZrUInt32 index = 1u; index < depth; index++) {
        if (!append_text(
                    source,
                    capacity,
                    &length,
                    "pub class C%04u: C%04u { pub var f%04u: int; }\n",
                    index,
                    index - 1u,
                    index)) {
            free(source);
            return ZR_NULL;
        }
    }
    return source;
}

static SZrObjectModule *create_module(
        const TZrChar *moduleName,
        const TZrChar *fileName,
        const TZrChar *source,
        SZrFunction **outFunction) {
    SZrString *sourceName = ZrCore_String_CreateFromNative(
            g_state, (TZrNativeString)fileName);
    SZrFunction *function = ZrParser_Source_Compile(
            g_state, source, strlen(source), sourceName);
    SZrObjectModule *module;

    if (outFunction != ZR_NULL) {
        *outFunction = function;
    }
    if (function == ZR_NULL) {
        return ZR_NULL;
    }
    module = ZrCore_Module_Create(g_state);
    if (module == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Module_SetInfo(
            g_state,
            module,
            ZrCore_String_CreateFromNative(
                    g_state, (TZrNativeString)moduleName),
            ZrCore_Module_CalculatePathHash(g_state, sourceName),
            sourceName);
    if (ZrCore_Module_CreatePrototypesFromData(
                g_state, module, function) == 0u) {
        return ZR_NULL;
    }
    return module;
}

static SZrObjectPrototype *module_prototype(
        SZrObjectModule *module,
        const TZrChar *name) {
    const SZrTypeValue *value = ZrCore_Module_GetPubExport(
            g_state,
            module,
            ZrCore_String_CreateFromNative(
                    g_state, (TZrNativeString)name));

    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_OBJECT ||
        value->value.object == ZR_NULL) {
        return ZR_NULL;
    }
    return (SZrObjectPrototype *)ZR_CAST_OBJECT(
            g_state, value->value.object);
}

static SZrObject *type_descriptor(SZrObjectPrototype *prototype) {
    SZrTypeValue prototypeValue;
    SZrTypeValue descriptorValue;

    if (prototype == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Value_InitAsRawObject(
            g_state,
            &prototypeValue,
            ZR_CAST_RAW_OBJECT_AS_SUPER(prototype));
    prototypeValue.type = ZR_VALUE_TYPE_OBJECT;
    ZrCore_Value_ResetAsNull(&descriptorValue);
    if (!ZrCore_Reflection_TypeOfValue(
                g_state, &prototypeValue, &descriptorValue) ||
        descriptorValue.type != ZR_VALUE_TYPE_OBJECT ||
        descriptorValue.value.object == ZR_NULL) {
        return ZR_NULL;
    }
    return ZR_CAST_OBJECT(g_state, descriptorValue.value.object);
}

static SZrObject *member_at(SZrObject *members, TZrUInt32 index) {
    SZrTypeValue key;
    const SZrTypeValue *entryValue;

    ZrCore_Value_InitAsInt(g_state, &key, index);
    entryValue = ZrCore_Object_GetValue(g_state, members, &key);
    if (entryValue == ZR_NULL || entryValue->type != ZR_VALUE_TYPE_OBJECT ||
        entryValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }
    return ZR_CAST_OBJECT(g_state, entryValue->value.object);
}

static SZrObject *object_field(
        SZrObject *object,
        const TZrChar *name,
        EZrValueType expectedType) {
    SZrString *keyString = ZrCore_String_CreateFromNative(
            g_state, (TZrNativeString)name);
    SZrTypeValue key;
    const SZrTypeValue *value;

    if (object == ZR_NULL || keyString == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Value_InitAsRawObject(
            g_state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    key.type = ZR_VALUE_TYPE_STRING;
    value = ZrCore_Object_GetValue(g_state, object, &key);
    if (value == ZR_NULL || value->type != expectedType ||
        value->value.object == ZR_NULL) {
        return ZR_NULL;
    }
    return ZR_CAST_OBJECT(g_state, value->value.object);
}

static const SZrTypeValue *value_field(SZrObject *object, const TZrChar *name) {
    SZrString *keyString = ZrCore_String_CreateFromNative(g_state, (TZrNativeString) name);
    SZrTypeValue key;

    if (object == ZR_NULL || keyString == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Value_InitAsRawObject(g_state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(keyString));
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(g_state, object, &key);
}

static void unpin_if_added(SZrRawObject *object, TZrBool added) {
    if (added) {
        ZrCore_GarbageCollector_UnignoreObject(g_state->global, object);
    }
}

static TZrBool append_synthetic_member_entries(
        SZrObject *descriptor,
        TZrUInt32 targetCount) {
    SZrObject *members = object_field(
            descriptor, "members", ZR_VALUE_TYPE_OBJECT);
    SZrObject *bucket = object_field(
            members, "seed", ZR_VALUE_TYPE_ARRAY);
    TZrBool descriptorPinned = ZR_FALSE;
    TZrBool bucketPinned = ZR_FALSE;
    TZrBool seedPinned = ZR_FALSE;
    TZrBool success = ZR_FALSE;
    SZrTypeValue firstKey;
    SZrTypeValue memberValue;
    const SZrTypeValue *firstMember;

    ZrCore_Value_ResetAsNull(&memberValue);
    if (descriptor == ZR_NULL || bucket == ZR_NULL ||
        bucket->nodeMap.elementCount != 1u || targetCount < 1u ||
        !ZrCore_GarbageCollector_IgnoreObjectIfNeededFast(
                g_state->global,
                g_state,
                ZR_CAST_RAW_OBJECT_AS_SUPER(descriptor),
                &descriptorPinned) ||
        !ZrCore_GarbageCollector_IgnoreObjectIfNeededFast(
                g_state->global,
                g_state,
                ZR_CAST_RAW_OBJECT_AS_SUPER(bucket),
                &bucketPinned)) {
        goto cleanup;
    }
    if (!ZrCore_HashSet_EnsureDenseSequentialIntKeyCapacityAndPairPoolExact(
                g_state,
                &bucket->nodeMap,
                ZrCore_HashSet_MinDenseSequentialIntKeyCapacity(targetCount))) {
        goto cleanup;
    }
    ZrCore_Value_InitAsInt(g_state, &firstKey, 0);
    firstMember = ZrCore_Object_GetValue(g_state, bucket, &firstKey);
    if (firstMember == ZR_NULL ||
        firstMember->type != ZR_VALUE_TYPE_OBJECT ||
        firstMember->value.object == ZR_NULL) {
        goto cleanup;
    }
    ZrCore_Value_InitAsRawObject(
            g_state,
            &memberValue,
            firstMember->value.object);
    memberValue.type = ZR_VALUE_TYPE_OBJECT;
    if (!ZrCore_GarbageCollector_IgnoreObjectIfNeededFast(
                g_state->global,
                g_state,
                firstMember->value.object,
                &seedPinned)) {
        goto cleanup;
    }
    /* Capacity/cache pressure is independent from the distinct-object GC graph. */
    for (TZrUInt32 index = 1u; index < targetCount; index++) {
        SZrTypeValue arrayKey;

        ZrCore_Value_InitAsInt(
                g_state, &arrayKey, (TZrInt64)bucket->nodeMap.elementCount);
        ZrCore_Object_SetValue(g_state, bucket, &arrayKey, &memberValue);
        if (g_state->threadStatus != ZR_THREAD_STATUS_FINE) {
            goto cleanup;
        }
    }
    success = bucket->nodeMap.elementCount == targetCount;

cleanup:
    if (memberValue.value.object != ZR_NULL) {
        unpin_if_added(memberValue.value.object, seedPinned);
    }
    if (bucket != ZR_NULL) {
        unpin_if_added(ZR_CAST_RAW_OBJECT_AS_SUPER(bucket), bucketPinned);
    }
    if (descriptor != ZR_NULL) {
        unpin_if_added(
                ZR_CAST_RAW_OBJECT_AS_SUPER(descriptor), descriptorPinned);
    }
    return success;
}

static void test_reflection_query_caches_one_hundred_thousand_members(void) {
    const TZrUInt32 memberCount = 100000u;
    static const TZrChar *source =
            "module reflection_wide_stress;\n"
            "pub class Wide { pub var seed: int; }\n";
    SZrFunction *function = ZR_NULL;
    SZrObjectModule *module;
    SZrObject *descriptor;
    SZrObject *firstResult = ZR_NULL;
    SZrObject *secondResult = ZR_NULL;
    SZrReflectionMemberQuery query;
    EZrReflectionQueryStatus status;
    SZrReflectionMemberCacheStats stats;
    SZrGarbageCollector *collector;
    TZrMemoryOffset savedDebt;
    TZrBool appendSucceeded;
    TZrBool firstQuerySucceeded;
    TZrBool secondQuerySucceeded;
    TZrBool modulePinned = ZR_FALSE;
    TZrBool descriptorPinned = ZR_FALSE;
    TZrBool resultPinned = ZR_FALSE;
    TZrBool cacheKeyPinned = ZR_FALSE;
    SZrString *cacheKeyString;
    SZrTypeValue cacheKey;
    const SZrTypeValue *cacheValue;

    module = create_module(
            "reflection_wide_stress",
            "reflection_wide_stress.zr",
            source,
            &function);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(module);
    descriptor = type_descriptor(module_prototype(module, "Wide"));
    TEST_ASSERT_NOT_NULL(descriptor);
    collector = g_state->global->garbageCollector;
    TEST_ASSERT_NOT_NULL(collector);
    savedDebt = collector->gcDebtSize;
    collector->gcDebtSize = -((TZrMemoryOffset)1024 * 1024 * 1024);
    appendSucceeded = append_synthetic_member_entries(
            descriptor, memberCount);
    ZrCore_Reflection_MemberQueryInitDefault(&query);
    query.scope = ZR_REFLECTION_MEMBER_SCOPE_DECLARED;
    query.access = ZR_REFLECTION_MEMBER_ACCESS_ALL;
    query.includeCompilerGenerated = ZR_TRUE;
    query.includeMetaMethods = ZR_TRUE;
    query.hasNonPublicAccessCapability = ZR_TRUE;
    ZrCore_Reflection_DebugResetMemberCacheStats();
    firstQuerySucceeded = appendSucceeded && ZrCore_Reflection_QueryMembers(
            g_state,
            descriptor,
            ZR_REFLECTION_MEMBER_KIND_ANY,
            &query,
            &firstResult,
            &status);
    secondQuerySucceeded = firstQuerySucceeded &&
            ZrCore_Reflection_QueryMembers(
                    g_state,
                    descriptor,
                    ZR_REFLECTION_MEMBER_KIND_ANY,
                    &query,
                    &secondResult,
                    &status);
    TEST_ASSERT_TRUE(appendSucceeded);
    TEST_ASSERT_TRUE(firstQuerySucceeded);
    TEST_ASSERT_TRUE(secondQuerySucceeded);
    TEST_ASSERT_EQUAL_UINT64(
            memberCount, firstResult->nodeMap.elementCount);
    TEST_ASSERT_NOT_NULL(member_at(firstResult, 0u));
    TEST_ASSERT_NOT_NULL(member_at(firstResult, memberCount - 1u));
    TEST_ASSERT_EQUAL_PTR(
            member_at(firstResult, 0u),
            member_at(firstResult, memberCount - 1u));
    TEST_ASSERT_EQUAL_PTR(firstResult, secondResult);
    cacheKeyString = ZrCore_String_CreateFromNative(
            g_state,
            "__zr_reflection_query_4_0_3_2_1_1_1");
    TEST_ASSERT_NOT_NULL(cacheKeyString);
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObjectIfNeededFast(
            g_state->global,
            g_state,
            ZR_CAST_RAW_OBJECT_AS_SUPER(cacheKeyString),
            &cacheKeyPinned));
    ZrCore_Value_InitAsRawObject(
            g_state,
            &cacheKey,
            ZR_CAST_RAW_OBJECT_AS_SUPER(cacheKeyString));
    cacheKey.type = ZR_VALUE_TYPE_STRING;
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObjectIfNeededFast(
            g_state->global,
            g_state,
            ZR_CAST_RAW_OBJECT_AS_SUPER(module),
            &modulePinned));
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObjectIfNeededFast(
            g_state->global,
            g_state,
            ZR_CAST_RAW_OBJECT_AS_SUPER(descriptor),
            &descriptorPinned));
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObjectIfNeededFast(
            g_state->global,
            g_state,
            ZR_CAST_RAW_OBJECT_AS_SUPER(firstResult),
            &resultPinned));
    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
    cacheValue = ZrCore_Object_GetValue(g_state, descriptor, &cacheKey);
    TEST_ASSERT_NOT_NULL(cacheValue);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_ARRAY, cacheValue->type);
    TEST_ASSERT_EQUAL_PTR(firstResult, cacheValue->value.object);
    TEST_ASSERT_EQUAL_UINT64(
            memberCount,
            ZR_CAST_OBJECT(g_state, cacheValue->value.object)
                    ->nodeMap.elementCount);
    stats = ZrCore_Reflection_DebugGetMemberCacheStats();
    TEST_ASSERT_EQUAL_UINT64(1u, stats.missCount);
    TEST_ASSERT_EQUAL_UINT64(1u, stats.hitCount);
    collector->gcDebtSize = savedDebt;
    unpin_if_added(
            ZR_CAST_RAW_OBJECT_AS_SUPER(cacheKeyString), cacheKeyPinned);
    unpin_if_added(
            ZR_CAST_RAW_OBJECT_AS_SUPER(firstResult), resultPinned);
    unpin_if_added(
            ZR_CAST_RAW_OBJECT_AS_SUPER(descriptor), descriptorPinned);
    unpin_if_added(
            ZR_CAST_RAW_OBJECT_AS_SUPER(module), modulePinned);
    ZrCore_Function_Free(g_state, function);
}

static void test_reflection_deep_inheritance_cache_survives_compacting_gc(void) {
    const TZrUInt32 depth = 512u;
    TZrChar typeName[16];
    TZrChar *source = build_deep_type_source(depth);
    SZrFunction *function = ZR_NULL;
    SZrObjectModule *module;
    SZrObject *descriptor;
    SZrObject *members = ZR_NULL;
    SZrReflectionMemberQuery query;
    EZrReflectionQueryStatus status;
    SZrReflectionMemberCacheStats stats;

    TEST_ASSERT_NOT_NULL(source);
    module = create_module(
            "reflection_deep_stress",
            "reflection_deep_stress.zr",
            source,
            &function);
    free(source);
    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(module);
    snprintf(typeName, sizeof(typeName), "C%04u", depth - 1u);
    descriptor = type_descriptor(module_prototype(module, typeName));
    TEST_ASSERT_NOT_NULL(descriptor);
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObject(
            g_state, ZR_CAST_RAW_OBJECT_AS_SUPER(module)));
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObject(
            g_state, ZR_CAST_RAW_OBJECT_AS_SUPER(descriptor)));
    ZrCore_Reflection_MemberQueryInitDefault(&query);
    ZrCore_Reflection_DebugResetMemberCacheStats();
    TEST_ASSERT_TRUE(ZrCore_Reflection_QueryMembers(
            g_state,
            descriptor,
            ZR_REFLECTION_MEMBER_KIND_FIELD,
            &query,
            &members,
            &status));
    TEST_ASSERT_EQUAL_UINT64(depth, members->nodeMap.elementCount);
    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
    for (TZrUInt32 iteration = 0u; iteration < 10000u; iteration++) {
        SZrObject *cached = ZR_NULL;

        TEST_ASSERT_TRUE(ZrCore_Reflection_QueryMembers(
                g_state,
                descriptor,
                ZR_REFLECTION_MEMBER_KIND_FIELD,
                &query,
                &cached,
                &status));
        TEST_ASSERT_EQUAL_PTR(members, cached);
    }
    stats = ZrCore_Reflection_DebugGetMemberCacheStats();
    TEST_ASSERT_EQUAL_UINT64(1u, stats.missCount);
    TEST_ASSERT_EQUAL_UINT64(10000u, stats.hitCount);
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_UnignoreObject(
            g_state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(descriptor)));
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_UnignoreObject(
            g_state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(module)));
    ZrCore_Function_Free(g_state, function);
}

static void test_reflection_constructor_throw_reports_boundary_and_clears_result(void) {
    static const TZrChar *source =
            "module reflection_throw_stress;\n"
            "pub class Payload {}\n"
            "pub class Throwing {\n"
            "  pub var value: object;\n"
            "  pub @constructor() {\n"
            "    try {\n"
            "      this.value = new Payload();\n"
            "      throw \"reflection boom\";\n"
            "    } finally {\n"
            "      this.value = new Payload();\n"
            "    }\n"
            "  }\n"
            "}\n";
    SZrFunction *function = ZR_NULL;
    SZrObjectModule *module = create_module(
            "reflection_throw_stress",
            "reflection_throw_stress.zr",
            source,
            &function);
    SZrObject *descriptor;
    SZrTypeValue result;
    EZrReflectionConstructionStatus status;
    TZrUInt32 savedHandlerDepth;
    TZrUInt32 savedRootDepth;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(module);
    descriptor = type_descriptor(module_prototype(module, "Throwing"));
    TEST_ASSERT_NOT_NULL(descriptor);
    savedHandlerDepth = g_state->exceptionHandlerStackLength;
    savedRootDepth = g_state->aotGcRootFrameDepth;
    ZrCore_Value_InitAsInt(g_state, &result, 99);
    TEST_ASSERT_FALSE(ZrCore_Reflection_CreateInstance(
            g_state,
            descriptor,
            ZR_NULL,
            0u,
            &result,
            &status));
    TEST_ASSERT_EQUAL_INT(
            ZR_REFLECTION_CONSTRUCTION_STATUS_CONSTRUCTOR_THREW,
            status);
    TEST_ASSERT_EQUAL_INT(ZR_VALUE_TYPE_NULL, result.type);
    TEST_ASSERT_EQUAL_UINT32(savedHandlerDepth, g_state->exceptionHandlerStackLength);
    TEST_ASSERT_EQUAL_UINT32(savedRootDepth, g_state->aotGcRootFrameDepth);
    ZrCore_Function_Free(g_state, function);
}

static void test_reflection_constructor_cache_and_result_survive_compacting_gc(void) {
    static const TZrChar *source = "module reflection_constructor_gc_stress;\n"
                                   "pub class Box {\n"
                                   "  pub var value: int;\n"
                                   "  pub @constructor(value: int) { this.value = value; }\n"
                                   "}\n";
    SZrFunction *function = ZR_NULL;
    SZrObjectModule *module =
            create_module("reflection_constructor_gc_stress", "reflection_constructor_gc_stress.zr", source, &function);
    SZrObject *descriptor;
    SZrTypeValue argument;
    SZrTypeValue result;
    SZrObject *firstInstance;
    const SZrTypeValue *firstValue;
    EZrReflectionConstructionStatus status;
    SZrReflectionConstructionCacheStats stats;

    TEST_ASSERT_NOT_NULL(function);
    TEST_ASSERT_NOT_NULL(module);
    descriptor = type_descriptor(module_prototype(module, "Box"));
    TEST_ASSERT_NOT_NULL(descriptor);
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObject(g_state, ZR_CAST_RAW_OBJECT_AS_SUPER(module)));
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObject(g_state, ZR_CAST_RAW_OBJECT_AS_SUPER(descriptor)));

    ZrCore_Value_InitAsInt(g_state, &argument, 41);
    ZrCore_Reflection_DebugResetConstructionCacheStats();
    TEST_ASSERT_TRUE(ZrCore_Reflection_CreateInstance(g_state, descriptor, &argument, 1u, &result, &status));
    firstInstance = ZR_CAST_OBJECT(g_state, result.value.object);
    TEST_ASSERT_NOT_NULL(firstInstance);
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_IgnoreObject(g_state, ZR_CAST_RAW_OBJECT_AS_SUPER(firstInstance)));

    ZrCore_GarbageCollector_GcFull(g_state, ZR_TRUE);
    firstValue = value_field(firstInstance, "value");
    TEST_ASSERT_NOT_NULL(firstValue);
    TEST_ASSERT_EQUAL_INT64(41, firstValue->value.nativeObject.nativeInt64);
    for (TZrUInt32 iteration = 0u; iteration < 10000u; iteration++) {
        const SZrTypeValue *value;

        ZrCore_Value_InitAsInt(g_state, &argument, (TZrInt64) iteration);
        TEST_ASSERT_TRUE(ZrCore_Reflection_CreateInstance(g_state, descriptor, &argument, 1u, &result, &status));
        value = value_field(ZR_CAST_OBJECT(g_state, result.value.object), "value");
        TEST_ASSERT_NOT_NULL(value);
        TEST_ASSERT_EQUAL_INT64(iteration, value->value.nativeObject.nativeInt64);
    }
    stats = ZrCore_Reflection_DebugGetConstructionCacheStats();
    TEST_ASSERT_EQUAL_UINT64(1u, stats.missCount);
    TEST_ASSERT_EQUAL_UINT64(10000u, stats.hitCount);

    TEST_ASSERT_TRUE(
            ZrCore_GarbageCollector_UnignoreObject(g_state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(firstInstance)));
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_UnignoreObject(g_state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(descriptor)));
    TEST_ASSERT_TRUE(ZrCore_GarbageCollector_UnignoreObject(g_state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(module)));
    ZrCore_Function_Free(g_state, function);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_reflection_query_caches_one_hundred_thousand_members);
    RUN_TEST(test_reflection_deep_inheritance_cache_survives_compacting_gc);
    RUN_TEST(test_reflection_constructor_throw_reports_boundary_and_clears_result);
    RUN_TEST(test_reflection_constructor_cache_and_result_survive_compacting_gc);
    return UNITY_END();
}
