#include "unity.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/metadata_runtime.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/type_layout.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

void setUp(void) {}

void tearDown(void) {}

#define ARRAY_COUNT(array_) (sizeof(array_) / sizeof((array_)[0]))

static char *read_text_file_owned(const char *path) {
    FILE *file;
    long fileSize;
    char *buffer;

    if (path == ZR_NULL) {
        return ZR_NULL;
    }

    file = fopen(path, "rb");
    if (file == ZR_NULL) {
        return ZR_NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return ZR_NULL;
    }
    fileSize = ftell(file);
    if (fileSize < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return ZR_NULL;
    }

    buffer = (char *)malloc((size_t)fileSize + 1u);
    if (buffer == ZR_NULL) {
        fclose(file);
        return ZR_NULL;
    }

    if (fileSize > 0 && fread(buffer, 1u, (size_t)fileSize, file) != (size_t)fileSize) {
        free(buffer);
        fclose(file);
        return ZR_NULL;
    }
    buffer[fileSize] = '\0';
    fclose(file);
    return buffer;
}

static char *read_repo_text_file_owned(const char *relativePath) {
    const char *sourceFile = __FILE__;
    const char *marker;
    char path[1024];
    size_t rootLength;
    size_t relativeLength;

    if (relativePath == ZR_NULL) {
        return ZR_NULL;
    }

    marker = strstr(sourceFile, "tests/module/test_metadata_runtime_type_layout.c");
    if (marker == ZR_NULL) {
        marker = strstr(sourceFile, "tests\\module\\test_metadata_runtime_type_layout.c");
    }
    if (marker == ZR_NULL) {
        return read_text_file_owned(relativePath);
    }

    rootLength = (size_t)(marker - sourceFile);
    relativeLength = strlen(relativePath);
    if (rootLength + relativeLength + 1u >= sizeof(path)) {
        return ZR_NULL;
    }

    memcpy(path, sourceFile, rootLength);
    memcpy(path + rootLength, relativePath, relativeLength + 1u);
    return read_text_file_owned(path);
}

static void assert_text_contains_all(const char *text, const char *const *needles, size_t needleCount) {
    for (size_t index = 0u; index < needleCount; index++) {
        if (strstr(text, needles[index]) == ZR_NULL) {
            printf("Missing source contract text: %s\n", needles[index]);
            TEST_FAIL_MESSAGE("missing required source contract text");
        }
    }
}

static void test_metadata_runtime_attaches_type_layout_registry_count(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    const SZrTypeLayout *typeLayouts[4] = {0};
    SZrMetadataRuntime *runtime;

    registration.typeLayouts = typeLayouts;
    registration.typeLayoutCount = 4u;

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);

    TEST_ASSERT_NOT_NULL(runtime);
    TEST_ASSERT_EQUAL_PTR(typeLayouts, runtime->codeRegistration->typeLayouts);
    TEST_ASSERT_EQUAL_UINT32(4u, runtime->typeLayoutCount);
}

static void test_metadata_runtime_resolves_type_layout_from_code_registration_registry(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrTypeLayout matchingLayout = {0};
    SZrTypeLayout mismatchedLayout = {0};
    SZrTypeLayout stalePrototypeLayouts[44] = {0};
    const SZrTypeLayout *registeredLayouts[44] = {0};
    const SZrTypeLayout *resolvedLayout;
    SZrMetadataRuntime *runtime;

    stalePrototypeLayouts[42].cTypeId = 42u;
    stalePrototypeLayouts[42].byteSize = 96u;
    metadataFunction.prototypeFrameTypeLayouts = stalePrototypeLayouts;
    metadataFunction.prototypeFrameTypeLayoutLength = 44u;

    matchingLayout.cTypeId = 42u;
    matchingLayout.byteSize = 24u;
    mismatchedLayout.cTypeId = 99u;
    mismatchedLayout.byteSize = 32u;
    registeredLayouts[42] = &matchingLayout;
    registeredLayouts[43] = &mismatchedLayout;
    registration.typeLayouts = registeredLayouts;
    registration.typeLayoutCount = 44u;

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);

    TEST_ASSERT_NULL(ZrCore_MetadataRuntime_ResolveTypeLayout(ZR_NULL, 42u));
    TEST_ASSERT_NULL(ZrCore_MetadataRuntime_ResolveTypeLayout(runtime, ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE));
    TEST_ASSERT_NULL(ZrCore_MetadataRuntime_ResolveTypeLayout(runtime, 44u));
    TEST_ASSERT_NULL(ZrCore_MetadataRuntime_ResolveTypeLayout(runtime, 41u));
    TEST_ASSERT_NULL(ZrCore_MetadataRuntime_ResolveTypeLayout(runtime, 43u));

    resolvedLayout = ZrCore_MetadataRuntime_ResolveTypeLayout(runtime, 42u);
    TEST_ASSERT_EQUAL_PTR(&matchingLayout, resolvedLayout);
    TEST_ASSERT_EQUAL_UINT32(24u, resolvedLayout->byteSize);
}

static void test_metadata_runtime_resolve_type_layout_does_not_fallback_to_prototype_cache(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrTypeLayout stalePrototypeLayouts[8] = {0};
    SZrMetadataRuntime *runtime;

    stalePrototypeLayouts[7].cTypeId = 7u;
    stalePrototypeLayouts[7].byteSize = 64u;
    metadataFunction.prototypeFrameTypeLayouts = stalePrototypeLayouts;
    metadataFunction.prototypeFrameTypeLayoutLength = 8u;

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);

    TEST_ASSERT_NOT_NULL(runtime);
    TEST_ASSERT_NULL(ZrCore_MetadataRuntime_ResolveTypeLayout(runtime, 7u));
}

static void test_metadata_runtime_resolves_gc_descriptor_from_code_registration_registry(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrTypeLayout matchingLayout = {0};
    SZrTypeLayout sparseLayout = {0};
    SZrTypeLayout mismatchedLayout = {0};
    const SZrTypeLayout *registeredLayouts[44] = {0};
    TZrUInt32 gcOffsets[1] = {8u};
    SZrAotGcDescriptor matchingDescriptor = {0};
    SZrAotGcDescriptor descriptorWithoutLayout = {0};
    SZrAotGcDescriptor mismatchedDescriptor = {0};
    const SZrAotGcDescriptor *registeredDescriptors[44] = {0};
    const SZrAotGcDescriptor *resolvedDescriptor;
    SZrMetadataRuntime *runtime;

    matchingLayout.cTypeId = 42u;
    matchingLayout.byteSize = 24u;
    sparseLayout.cTypeId = 41u;
    sparseLayout.byteSize = 16u;
    mismatchedLayout.cTypeId = 43u;
    mismatchedLayout.byteSize = 32u;
    registeredLayouts[41] = &sparseLayout;
    registeredLayouts[42] = &matchingLayout;
    registeredLayouts[43] = &mismatchedLayout;

    matchingDescriptor.typeLayoutId = 42u;
    matchingDescriptor.gcFieldCount = 1u;
    matchingDescriptor.gcFieldOffsets = gcOffsets;
    descriptorWithoutLayout.typeLayoutId = 40u;
    descriptorWithoutLayout.gcFieldCount = 1u;
    descriptorWithoutLayout.gcFieldOffsets = gcOffsets;
    mismatchedDescriptor.typeLayoutId = 99u;
    mismatchedDescriptor.gcFieldCount = 1u;
    mismatchedDescriptor.gcFieldOffsets = gcOffsets;
    registeredDescriptors[40] = &descriptorWithoutLayout;
    registeredDescriptors[42] = &matchingDescriptor;
    registeredDescriptors[43] = &mismatchedDescriptor;

    registration.typeLayouts = registeredLayouts;
    registration.typeLayoutCount = 44u;
    registration.gcDescriptors = registeredDescriptors;
    registration.gcDescriptorCount = 44u;

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);

    TEST_ASSERT_NULL(ZrCore_MetadataRuntime_ResolveGcDescriptor(ZR_NULL, 42u));
    TEST_ASSERT_NULL(ZrCore_MetadataRuntime_ResolveGcDescriptor(runtime,
                                                               ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE));
    TEST_ASSERT_NULL(ZrCore_MetadataRuntime_ResolveGcDescriptor(runtime, 44u));
    TEST_ASSERT_NULL(ZrCore_MetadataRuntime_ResolveGcDescriptor(runtime, 41u));
    TEST_ASSERT_NULL(ZrCore_MetadataRuntime_ResolveGcDescriptor(runtime, 40u));
    TEST_ASSERT_NULL(ZrCore_MetadataRuntime_ResolveGcDescriptor(runtime, 43u));

    resolvedDescriptor = ZrCore_MetadataRuntime_ResolveGcDescriptor(runtime, 42u);
    TEST_ASSERT_EQUAL_PTR(&matchingDescriptor, resolvedDescriptor);
    TEST_ASSERT_EQUAL_UINT32(1u, resolvedDescriptor->gcFieldCount);
    TEST_ASSERT_EQUAL_PTR(gcOffsets, resolvedDescriptor->gcFieldOffsets);
}

static void test_metadata_runtime_gc_descriptor_does_not_fallback_to_prototype_cache(void) {
    SZrObjectModule module = {0};
    SZrFunction metadataFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrTypeLayout stalePrototypeLayouts[8] = {0};
    SZrAotGcDescriptor staleDescriptor = {0};
    const SZrAotGcDescriptor *registeredDescriptors[8] = {0};
    SZrMetadataRuntime *runtime;

    stalePrototypeLayouts[7].cTypeId = 7u;
    stalePrototypeLayouts[7].byteSize = 64u;
    metadataFunction.prototypeFrameTypeLayouts = stalePrototypeLayouts;
    metadataFunction.prototypeFrameTypeLayoutLength = 8u;

    staleDescriptor.typeLayoutId = 7u;
    staleDescriptor.gcFieldCount = 1u;
    registeredDescriptors[7] = &staleDescriptor;
    registration.gcDescriptors = registeredDescriptors;
    registration.gcDescriptorCount = 8u;

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &metadataFunction, &registration);

    TEST_ASSERT_NOT_NULL(runtime);
    TEST_ASSERT_NULL(ZrCore_MetadataRuntime_ResolveGcDescriptor(runtime, 7u));
}

static void test_metadata_runtime_function_layout_resolver_uses_attached_registry_context(void) {
    SZrObjectModule module = {0};
    SZrFunction entryFunction = {0};
    SZrFunction childFunction = {0};
    SZrAotCodeRegistration registration = {0};
    SZrTypeLayout registryLayout = {0};
    SZrTypeLayout stalePrototypeLayouts[44] = {0};
    const SZrTypeLayout *registeredLayouts[44] = {0};
    const SZrTypeLayout *resolvedLayout;
    SZrMetadataRuntime *runtime;

    stalePrototypeLayouts[42].cTypeId = 42u;
    stalePrototypeLayouts[42].byteSize = 96u;
    entryFunction.prototypeFrameTypeLayouts = stalePrototypeLayouts;
    entryFunction.prototypeFrameTypeLayoutLength = 44u;
    childFunction.prototypeContextFunction = &entryFunction;

    registryLayout.cTypeId = 42u;
    registryLayout.byteSize = 24u;
    registeredLayouts[42] = &registryLayout;
    registration.typeLayouts = registeredLayouts;
    registration.typeLayoutCount = 44u;

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &entryFunction, &registration);
    ZrCore_MetadataRuntime_AttachFunction(runtime, &entryFunction);

    resolvedLayout = ZrCore_MetadataRuntime_ResolveFunctionTypeLayout(&childFunction, 42u);
    TEST_ASSERT_EQUAL_PTR(&registryLayout, resolvedLayout);
    TEST_ASSERT_EQUAL_UINT32(24u, resolvedLayout->byteSize);
}

static void test_metadata_runtime_function_layout_resolver_does_not_fallback_to_prototype_cache(void) {
    SZrFunction entryFunction = {0};
    SZrFunction childFunction = {0};
    SZrTypeLayout stalePrototypeLayouts[8] = {0};

    stalePrototypeLayouts[7].cTypeId = 7u;
    stalePrototypeLayouts[7].byteSize = 64u;
    entryFunction.prototypeFrameTypeLayouts = stalePrototypeLayouts;
    entryFunction.prototypeFrameTypeLayoutLength = 8u;
    childFunction.prototypeContextFunction = &entryFunction;

    TEST_ASSERT_NULL(ZrCore_MetadataRuntime_ResolveFunctionTypeLayout(&entryFunction, 7u));
    TEST_ASSERT_NULL(ZrCore_MetadataRuntime_ResolveFunctionTypeLayout(&childFunction, 7u));
}

static void test_metadata_runtime_resolves_prototype_layout_from_attached_registry_context(void) {
    SZrObjectModule module = {0};
    SZrFunction entryFunction = {0};
    SZrFunction childFunction = {0};
    SZrObjectPrototype prototype = {0};
    SZrObjectPrototype *prototypeInstances[44] = {0};
    SZrAotCodeRegistration registration = {0};
    SZrTypeLayout registryLayout = {0};
    SZrTypeLayout stalePrototypeLayouts[44] = {0};
    const SZrTypeLayout *registeredLayouts[44] = {0};
    const SZrTypeLayout *resolvedLayout;
    TZrUInt32 typeLayoutId = ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE;
    SZrMetadataRuntime *runtime;

    prototypeInstances[42] = &prototype;
    entryFunction.prototypeInstances = prototypeInstances;
    entryFunction.prototypeInstancesLength = 44u;
    entryFunction.prototypeCount = 44u;
    childFunction.prototypeContextFunction = &entryFunction;

    stalePrototypeLayouts[42].cTypeId = 42u;
    stalePrototypeLayouts[42].byteSize = 96u;
    entryFunction.prototypeFrameTypeLayouts = stalePrototypeLayouts;
    entryFunction.prototypeFrameTypeLayoutLength = 44u;

    registryLayout.cTypeId = 42u;
    registryLayout.byteSize = 24u;
    registeredLayouts[42] = &registryLayout;
    registration.typeLayouts = registeredLayouts;
    registration.typeLayoutCount = 44u;

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &entryFunction, &registration);
    ZrCore_MetadataRuntime_AttachFunction(runtime, &entryFunction);

    resolvedLayout = ZrCore_MetadataRuntime_ResolveFunctionPrototypeTypeLayout(&childFunction,
                                                                              &prototype,
                                                                              &typeLayoutId);

    TEST_ASSERT_EQUAL_PTR(&registryLayout, resolvedLayout);
    TEST_ASSERT_EQUAL_UINT32(42u, typeLayoutId);
    TEST_ASSERT_EQUAL_UINT32(24u, resolvedLayout->byteSize);
}

static void test_metadata_runtime_prototype_layout_resolver_does_not_fallback_to_prototype_cache(void) {
    SZrObjectModule module = {0};
    SZrFunction entryFunction = {0};
    SZrObjectPrototype prototype = {0};
    SZrObjectPrototype *prototypeInstances[8] = {0};
    SZrAotCodeRegistration registration = {0};
    SZrTypeLayout stalePrototypeLayouts[8] = {0};
    TZrUInt32 typeLayoutId = 7u;
    SZrMetadataRuntime *runtime;

    prototypeInstances[7] = &prototype;
    entryFunction.prototypeInstances = prototypeInstances;
    entryFunction.prototypeInstancesLength = 8u;
    entryFunction.prototypeCount = 8u;
    stalePrototypeLayouts[7].cTypeId = 7u;
    stalePrototypeLayouts[7].byteSize = 64u;
    entryFunction.prototypeFrameTypeLayouts = stalePrototypeLayouts;
    entryFunction.prototypeFrameTypeLayoutLength = 8u;

    runtime = ZrCore_Module_AttachMetadataRuntime(&module, &entryFunction, &registration);
    ZrCore_MetadataRuntime_AttachFunction(runtime, &entryFunction);

    TEST_ASSERT_NULL(ZrCore_MetadataRuntime_ResolveFunctionPrototypeTypeLayout(&entryFunction,
                                                                              &prototype,
                                                                              &typeLayoutId));
    TEST_ASSERT_EQUAL_UINT32(ZR_FUNCTION_FRAME_TYPE_LAYOUT_ID_NONE, typeLayoutId);
}

static void test_reflection_layout_source_consumes_metadata_runtime_registry(void) {
    static const char *needles[] = {
            "#include \"zr_vm_core/metadata_runtime.h\"",
            "ZrCore_MetadataRuntime_ResolveFunctionPrototypeTypeLayout(entryFunction, prototype, &typeLayoutId)",
            "reflection_find_registry_field_by_instance_index(typeLayout, scriptFieldCount)",
            "reflection_apply_type_layout_to_layout_object(state, layoutObject, typeLayout)",
            "reflection_apply_field_layout_to_member(state, memberReflection, registryField)"};
    char *reflectionText = read_repo_text_file_owned("zr_vm_core/src/zr_vm_core/reflection.c");

    TEST_ASSERT_NOT_NULL(reflectionText);
    assert_text_contains_all(reflectionText, needles, ARRAY_COUNT(needles));
    free(reflectionText);
}

int main(void) {
    UNITY_BEGIN();
    RUN_TEST(test_metadata_runtime_attaches_type_layout_registry_count);
    RUN_TEST(test_metadata_runtime_resolves_type_layout_from_code_registration_registry);
    RUN_TEST(test_metadata_runtime_resolve_type_layout_does_not_fallback_to_prototype_cache);
    RUN_TEST(test_metadata_runtime_resolves_gc_descriptor_from_code_registration_registry);
    RUN_TEST(test_metadata_runtime_gc_descriptor_does_not_fallback_to_prototype_cache);
    RUN_TEST(test_metadata_runtime_function_layout_resolver_uses_attached_registry_context);
    RUN_TEST(test_metadata_runtime_function_layout_resolver_does_not_fallback_to_prototype_cache);
    RUN_TEST(test_metadata_runtime_resolves_prototype_layout_from_attached_registry_context);
    RUN_TEST(test_metadata_runtime_prototype_layout_resolver_does_not_fallback_to_prototype_cache);
    RUN_TEST(test_reflection_layout_source_consumes_metadata_runtime_registry);
    return UNITY_END();
}
