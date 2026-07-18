#include "zr_vm_core/metadata_runtime.h"

#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"

static void metadata_runtime_clear_method_binding_view(SZrMetadataRuntimeMethodBindingView *outView) {
    if (outView != ZR_NULL) {
        ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
    }
}

static void metadata_runtime_clear_interpreter_method_binding_view(
        SZrMetadataRuntimeInterpreterMethodBindingView *outView) {
    if (outView != ZR_NULL) {
        ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
    }
}

static const SZrZrpMetadataMethodDefRow *metadata_runtime_find_method_def_row(
        SZrMetadataRuntime *runtime,
        TZrMetadataToken methodToken) {
    SZrZrpMetadataSectionView view;
    const SZrZrpMetadataMethodDefRow *rows;
    const SZrZrpMetadataMethodDefRow *matchedRow = ZR_NULL;

    if (!ZrCore_MetadataRuntime_GetZrpSectionView(
                runtime, ZR_ZRP_METADATA_SECTION_METHOD_DEFS, &view) ||
        view.data == ZR_NULL ||
        view.elementSize != (TZrUInt32)sizeof(SZrZrpMetadataMethodDefRow)) {
        return ZR_NULL;
    }
    rows = (const SZrZrpMetadataMethodDefRow *)(const void *)view.data;
    for (TZrUInt32 index = 0u; index < view.count; ++index) {
        if (rows[index].token != methodToken) {
            continue;
        }
        if (matchedRow != ZR_NULL) {
            return ZR_NULL;
        }
        matchedRow = &rows[index];
    }
    return matchedRow;
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

TZrBool ZrCore_MetadataRuntime_ReadInterpreterMethodBindingView(
        struct SZrState *state,
        SZrMetadataRuntime *runtime,
        TZrMetadataToken methodToken,
        SZrMetadataRuntimeInterpreterMethodBindingView *outView) {
    const SZrMetadataTokenRecord *methodRecord;
    const SZrZrpMetadataMethodDefRow *methodDefRow;
    SZrFunction *function;

    metadata_runtime_clear_interpreter_method_binding_view(outView);
    if (state == ZR_NULL || runtime == ZR_NULL || outView == ZR_NULL ||
        !metadata_runtime_is_local_method_token(methodToken) ||
        runtime->metadataFunction == ZR_NULL) {
        return ZR_FALSE;
    }
    methodRecord = ZrCore_MetadataRuntime_ResolveMethodRecord(runtime, methodToken);
    methodDefRow = metadata_runtime_find_method_def_row(runtime, methodToken);
    if (methodRecord == ZR_NULL || methodDefRow == ZR_NULL) {
        return ZR_FALSE;
    }
    function = ZrCore_Function_ResolveGraphFunctionByFlatIndex(
            state, runtime->metadataFunction, methodDefRow->functionIndex);
    if (function == ZR_NULL || function->super.type != ZR_RAW_OBJECT_TYPE_FUNCTION ||
        function->super.isNative || function->instructionsList == ZR_NULL) {
        return ZR_FALSE;
    }

    outView->methodToken = methodToken;
    outView->methodRecord = methodRecord;
    outView->methodDefRow = methodDefRow;
    outView->functionIndex = methodDefRow->functionIndex;
    outView->function = function;
    return ZR_TRUE;
}
