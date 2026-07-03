#include "zr_vm_core/metadata_runtime.h"

#include "zr_vm_core/memory.h"

static void metadata_runtime_clear_method_binding_view(SZrMetadataRuntimeMethodBindingView *outView) {
    if (outView != ZR_NULL) {
        ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
    }
}

static TZrBool metadata_runtime_is_local_method_token(TZrMetadataToken methodToken) {
    return methodToken != 0u && ZR_METADATA_TOKEN_TABLE(methodToken) == ZR_METADATA_TABLE_MEMBER_DEF;
}

static TZrBool metadata_runtime_method_binding_tables_ready(const SZrMetadataRuntime *runtime) {
    const SZrAotCodeRegistration *registration;

    if (runtime == ZR_NULL || runtime->codeRegistration == ZR_NULL) {
        return ZR_FALSE;
    }

    registration = runtime->codeRegistration;
    return registration->methodTokens != ZR_NULL &&
           registration->methodInfos != ZR_NULL &&
           registration->functionPointers != ZR_NULL &&
           runtime->methodTokenCount != 0u &&
           runtime->methodInfoCount != 0u &&
           runtime->functionCount != 0u &&
           registration->methodTokenCount == runtime->methodTokenCount &&
           registration->methodInfoCount == runtime->methodInfoCount &&
           registration->functionCount == runtime->functionCount &&
           runtime->methodTokenCount == runtime->methodInfoCount &&
           runtime->methodTokenCount <= runtime->functionCount;
}

ZR_CORE_API TZrBool ZrCore_MetadataRuntime_ReadMethodBindingView(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken methodToken,
        SZrMetadataRuntimeMethodBindingView *outView) {
    const SZrAotCodeRegistration *registration;
    const SZrAotMethodInfo *methodInfo;
    FZrAotEntryThunk functionPointer;
    TZrUInt32 functionIndex = 0u;
    TZrUInt32 index;
    TZrBool hasMatch = ZR_FALSE;

    metadata_runtime_clear_method_binding_view(outView);
    if (outView == ZR_NULL ||
        !metadata_runtime_is_local_method_token(methodToken) ||
        !metadata_runtime_method_binding_tables_ready(runtime)) {
        return ZR_FALSE;
    }

    registration = runtime->codeRegistration;
    for (index = 0u; index < runtime->methodTokenCount; ++index) {
        if (registration->methodTokens[index] != methodToken) {
            continue;
        }
        if (hasMatch) {
            return ZR_FALSE;
        }
        hasMatch = ZR_TRUE;
        functionIndex = index;
    }

    if (!hasMatch ||
        functionIndex >= runtime->methodInfoCount ||
        functionIndex >= runtime->functionCount) {
        return ZR_FALSE;
    }

    methodInfo = registration->methodInfos[functionIndex];
    functionPointer = registration->functionPointers[functionIndex];
    if (methodInfo == ZR_NULL ||
        methodInfo->functionIndex != functionIndex ||
        methodInfo->invoker == ZR_NULL ||
        functionPointer == ZR_NULL) {
        return ZR_FALSE;
    }

    outView->methodToken = methodToken;
    outView->functionIndex = functionIndex;
    outView->methodInfo = methodInfo;
    outView->functionPointer = functionPointer;
    outView->invoker = methodInfo->invoker;
    return ZR_TRUE;
}
