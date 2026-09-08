#include "zr_vm_core/metadata_runtime.h"

#include <string.h>

#include "zr_vm_core/artifact_schema.h"
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

static TZrBool metadata_runtime_call_binding_tables_ready(const SZrMetadataRuntime *runtime) {
    const SZrAotCodeRegistration *registration;

    if (runtime == ZR_NULL || runtime->codeRegistration == ZR_NULL) {
        return ZR_FALSE;
    }
    registration = runtime->codeRegistration;
    if (registration->callBindingRowSize != ZR_ARTIFACT_CALL_BINDING_ROW_ENCODED_SIZE ||
        registration->callBindingRowCount > ZR_ARTIFACT_MAX_ROW_COUNT ||
        registration->functionCount != runtime->functionCount ||
        registration->methodInfoCount != runtime->methodInfoCount) {
        return ZR_FALSE;
    }
    if (registration->callBindingRowCount == 0u) {
        return registration->callBindingRows == ZR_NULL &&
               registration->callBindingTargetFunctionIndices == ZR_NULL;
    }
    return registration->callBindingRows != ZR_NULL &&
           registration->callBindingTargetFunctionIndices != ZR_NULL;
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

ZR_CORE_API TZrBool ZrCore_MetadataRuntime_ReadCallBindingView(
        SZrMetadataRuntime *runtime,
        TZrUInt32 rowIndex,
        SZrMetadataRuntimeCallBindingView *outView) {
    const SZrAotCodeRegistration *registration;
    SZrArtifactSectionView section = {0};
    SZrArtifactCallBindingRow row;
    SZrMetadataRuntimeCallBindingView view = {0};
    TZrUInt32 targetFunctionIndex;

    if (outView != ZR_NULL) {
        ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
        outView->targetFunctionIndex = UINT32_MAX;
    }
    if (runtime == ZR_NULL || outView == ZR_NULL ||
        !metadata_runtime_call_binding_tables_ready(runtime)) {
        return ZR_FALSE;
    }
    registration = runtime->codeRegistration;
    if (rowIndex >= registration->callBindingRowCount) {
        return ZR_FALSE;
    }
    section.kind = ZR_ARTIFACT_SECTION_CALL_BINDING_TABLE;
    section.data = registration->callBindingRows;
    section.elementSize = registration->callBindingRowSize;
    section.elementCount = registration->callBindingRowCount;
    section.byteLength = (TZrUInt64)section.elementCount * section.elementSize;
    if (ZrCore_Artifact_ReadCallBindingRow(&section, rowIndex, &row, ZR_NULL) != ZR_ARTIFACT_STATUS_OK ||
        row.functionIndex >= registration->functionCount) {
        return ZR_FALSE;
    }

    view.functionIndex = row.functionIndex;
    view.cacheIndex = row.cacheIndex;
    view.instructionIndex = row.instructionIndex;
    view.contract = row.contract;
    view.location = row.location;
    targetFunctionIndex = registration->callBindingTargetFunctionIndices[rowIndex];
    view.targetFunctionIndex = targetFunctionIndex;
    if (view.location.kind == ZR_CALL_BINDING_RELOCATION_MODULE ||
        view.location.kind == ZR_CALL_BINDING_RELOCATION_VM_MODULE ||
        (view.contract.bindingKind == ZR_CALL_BINDING_TYPED_FUNCTION &&
         view.location.kind == ZR_CALL_BINDING_RELOCATION_NONE)) {
        if (targetFunctionIndex != UINT32_MAX) return ZR_FALSE;
        *outView = view;
        return ZR_TRUE;
    }
    if (targetFunctionIndex >= registration->functionCount ||
        registration->functionPointers == ZR_NULL ||
        registration->functionPointers[targetFunctionIndex] == ZR_NULL ||
        registration->methodInfos == ZR_NULL ||
        targetFunctionIndex >= registration->methodInfoCount) {
        return ZR_FALSE;
    }
    view.functionPointer = registration->functionPointers[targetFunctionIndex];
    view.methodInfo = registration->methodInfos[targetFunctionIndex];
    if (view.methodInfo == ZR_NULL || view.methodInfo->functionIndex != targetFunctionIndex ||
        view.methodInfo->invoker == ZR_NULL) {
        return ZR_FALSE;
    }
    view.invoker = view.methodInfo->invoker;
    *outView = view;
    return ZR_TRUE;
}

static TZrBool metadata_runtime_call_binding_fail(
        SZrState *state, EZrCallBindingStatus status,
        const SZrMetadataRuntimeCallBindingView *view, TZrUInt32 rowIndex,
        TZrUInt64 expected, TZrUInt64 actual) {
    SZrCallBindingDiagnostic *diagnostic = &state->lastCallBindingError;
    diagnostic->status = status;
    diagnostic->targetMetadataToken = view != ZR_NULL ? view->contract.targetMetadataToken : 0u;
    diagnostic->instructionIndex = view != ZR_NULL ? view->instructionIndex : 0u;
    diagnostic->candidateIndex = rowIndex;
    diagnostic->expected = expected;
    diagnostic->actual = actual;
    return ZR_FALSE;
}

static void metadata_runtime_invalidate_call_bindings(SZrState *state, SZrMetadataRuntime *runtime) {
    for (TZrUInt32 index = 0u; index < runtime->functionCount; ++index) {
        SZrFunction *function = ZrCore_Function_ResolveGraphFunctionByFlatIndex(
                state, runtime->metadataFunction, index);
        if (function == ZR_NULL) continue;
        for (TZrUInt32 cacheIndex = 0u; cacheIndex < function->callSiteCacheLength; ++cacheIndex) {
            ZrCore_CallBinding_Invalidate(&function->callSiteCaches[cacheIndex].binding);
        }
    }
}

static TZrBool metadata_runtime_validate_call_binding_rows(SZrState *state, SZrMetadataRuntime *runtime) {
    const SZrAotCodeRegistration *registration = runtime->codeRegistration;
    TZrUInt32 previousFunctionIndex = 0u;
    TZrUInt32 previousCacheIndex = 0u;
    TZrUInt32 expectedCount = 0u;

    for (TZrUInt32 index = 0u; index < runtime->functionCount; ++index) {
        SZrFunction *function = ZrCore_Function_ResolveGraphFunctionByFlatIndex(
                state, runtime->metadataFunction, index);
        if (function == ZR_NULL) {
            return metadata_runtime_call_binding_fail(state, ZR_CALL_BINDING_INVALID_RELOCATION,
                    ZR_NULL, 0u, runtime->functionCount, index);
        }
        if (registration->functionPointers[index] == ZR_NULL) continue;
        for (TZrUInt32 cacheIndex = 0u; cacheIndex < function->callSiteCacheLength; ++cacheIndex) {
            if (function->callSiteCaches[cacheIndex].binding.contract.bindingKind != ZR_CALL_BINDING_NONE) {
                ++expectedCount;
            }
        }
    }
    if (expectedCount != registration->callBindingRowCount) {
        return metadata_runtime_call_binding_fail(state, ZR_CALL_BINDING_INVALID_RELOCATION,
                ZR_NULL, 0u, expectedCount, registration->callBindingRowCount);
    }
    for (TZrUInt32 rowIndex = 0u; rowIndex < registration->callBindingRowCount; ++rowIndex) {
        SZrMetadataRuntimeCallBindingView view;
        SZrFunction *function;
        SZrFunctionCallSiteCacheEntry *entry;
        SZrFunction *target;
        if (!ZrCore_MetadataRuntime_ReadCallBindingView(runtime, rowIndex, &view)) {
            return metadata_runtime_call_binding_fail(state, ZR_CALL_BINDING_INVALID_RELOCATION,
                    ZR_NULL, rowIndex, 0u, 0u);
        }
        if (rowIndex != 0u && (view.functionIndex < previousFunctionIndex ||
            (view.functionIndex == previousFunctionIndex && view.cacheIndex <= previousCacheIndex))) {
            return metadata_runtime_call_binding_fail(state, ZR_CALL_BINDING_AMBIGUOUS_TARGET,
                    &view, rowIndex, 0u, 0u);
        }
        previousFunctionIndex = view.functionIndex;
        previousCacheIndex = view.cacheIndex;
        function = ZrCore_Function_ResolveGraphFunctionByFlatIndex(
                state, runtime->metadataFunction, view.functionIndex);
        if (function == ZR_NULL || function->callSiteCaches == ZR_NULL ||
            view.cacheIndex >= function->callSiteCacheLength ||
            view.instructionIndex >= function->instructionsLength ||
            registration->functionPointers[view.functionIndex] == ZR_NULL) {
            return metadata_runtime_call_binding_fail(state, ZR_CALL_BINDING_INVALID_RELOCATION,
                    &view, rowIndex, 0u, 0u);
        }
        entry = &function->callSiteCaches[view.cacheIndex];
        if (ZrCore_CallBinding_CompareContracts(&entry->binding.contract, &view.contract,
                &state->lastCallBindingError) != ZR_CALL_BINDING_OK) {
            state->lastCallBindingError.instructionIndex = entry->instructionIndex;
            state->lastCallBindingError.candidateIndex = rowIndex;
            return ZR_FALSE;
        }
        if (entry->instructionIndex != view.instructionIndex ||
            memcmp(&entry->bindingLocation, &view.location, sizeof(view.location)) != 0) {
            return metadata_runtime_call_binding_fail(state, ZR_CALL_BINDING_INVALID_RELOCATION,
                    &view, rowIndex, entry->instructionIndex, view.instructionIndex);
        }
        if (view.contract.bindingKind == ZR_CALL_BINDING_TYPED_FUNCTION) {
            if (view.location.kind != ZR_CALL_BINDING_RELOCATION_NONE ||
                entry->binding.target.targetKind != ZR_CALL_BINDING_TARGET_NONE ||
                entry->binding.generation != function->callBindingGeneration) {
                return metadata_runtime_call_binding_fail(state, ZR_CALL_BINDING_INVALID_RELOCATION,
                        &view, rowIndex, 0u, 0u);
            }
            continue;
        }
        if (view.location.kind == ZR_CALL_BINDING_RELOCATION_VM_MODULE) {
            if (entry->binding.target.targetKind != ZR_CALL_BINDING_TARGET_NONE ||
                entry->binding.generation != function->callBindingGeneration)
                return metadata_runtime_call_binding_fail(state, ZR_CALL_BINDING_INVALID_RELOCATION,
                        &view, rowIndex, 0u, 0u);
            continue;
        }
        if (view.location.kind == ZR_CALL_BINDING_RELOCATION_MODULE) {
            if ((view.contract.bindingKind != ZR_CALL_BINDING_VIRTUAL &&
                 view.contract.bindingKind != ZR_CALL_BINDING_INTERFACE) ||
                entry->binding.target.targetKind != ZR_CALL_BINDING_TARGET_NONE ||
                entry->binding.generation != function->callBindingGeneration) {
                return metadata_runtime_call_binding_fail(state, ZR_CALL_BINDING_TARGET_KIND_MISMATCH,
                        &view, rowIndex, ZR_CALL_BINDING_TARGET_NONE, entry->binding.target.targetKind);
            }
            continue;
        }
        target = ZrCore_Function_ResolveGraphFunctionByFlatIndex(
                state, runtime->metadataFunction, view.targetFunctionIndex);
        if (target == ZR_NULL || entry->binding.target.targetKind != ZR_CALL_BINDING_TARGET_VM ||
            entry->binding.target.vm.function != target || entry->binding.target.callableObject == ZR_NULL) {
            return metadata_runtime_call_binding_fail(state, ZR_CALL_BINDING_TARGET_NOT_FOUND,
                    &view, rowIndex, 0u, view.targetFunctionIndex);
        }
    }
    return ZR_TRUE;
}

ZR_CORE_API TZrBool ZrCore_MetadataRuntime_LinkCallBindings(
        struct SZrState *state,
        SZrMetadataRuntime *runtime) {
    if (state == ZR_NULL || runtime == ZR_NULL) return ZR_FALSE;
    memset(&state->lastCallBindingError, 0, sizeof(state->lastCallBindingError));
    if (runtime->codeRegistration == ZR_NULL) return ZR_TRUE;
    if (runtime->metadataFunction == ZR_NULL ||
        !metadata_runtime_call_binding_tables_ready(runtime) ||
        (runtime->functionCount != 0u && runtime->codeRegistration->functionPointers == ZR_NULL)) {
        metadata_runtime_invalidate_call_bindings(state, runtime);
        return metadata_runtime_call_binding_fail(state, ZR_CALL_BINDING_INVALID_RELOCATION,
                ZR_NULL, 0u, 0u, 0u);
    }

    /* Rebuild from loaded metadata before trusting any registration row or index. */
    if (!ZrCore_CallBinding_LinkFunction(state, runtime->metadataFunction, &state->lastCallBindingError) ||
        !metadata_runtime_validate_call_binding_rows(state, runtime)) {
        metadata_runtime_invalidate_call_bindings(state, runtime);
        return ZR_FALSE;
    }
    for (TZrUInt32 rowIndex = 0u; rowIndex < runtime->codeRegistration->callBindingRowCount; ++rowIndex) {
        SZrMetadataRuntimeCallBindingView view;
        SZrFunction *function;
        SZrFunctionCallSiteCacheEntry *entry;
        SZrCallBindingCandidate candidate;
        if (!ZrCore_MetadataRuntime_ReadCallBindingView(runtime, rowIndex, &view)) {
            metadata_runtime_invalidate_call_bindings(state, runtime);
            return metadata_runtime_call_binding_fail(state, ZR_CALL_BINDING_INVALID_RELOCATION,
                    ZR_NULL, rowIndex, 0u, 0u);
        }
        function = ZrCore_Function_ResolveGraphFunctionByFlatIndex(
                state, runtime->metadataFunction, view.functionIndex);
        entry = &function->callSiteCaches[view.cacheIndex];
        /* Slot contracts remain deferred until a receiver chooses its implementation. */
        if (view.location.kind == ZR_CALL_BINDING_RELOCATION_MODULE ||
            view.location.kind == ZR_CALL_BINDING_RELOCATION_VM_MODULE ||
            view.contract.bindingKind == ZR_CALL_BINDING_TYPED_FUNCTION) continue;
        candidate.contract = entry->binding.contract;
        candidate.generation = entry->binding.generation;
        candidate.target = entry->binding.target;
        candidate.target.targetKind = ZR_CALL_BINDING_TARGET_AOT;
        candidate.target.aot.thunk = view.functionPointer;
        candidate.target.aot.methodInfo = view.methodInfo;
        candidate.target.aot.invoker = view.invoker;
        if (ZrCore_CallBinding_Resolve(&entry->binding.contract,
                                       &candidate,
                                       1u,
                                       candidate.generation,
                                       &entry->binding,
                                       &state->lastCallBindingError) != ZR_CALL_BINDING_OK) {
            metadata_runtime_invalidate_call_bindings(state, runtime);
            return ZR_FALSE;
        }
    }
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
