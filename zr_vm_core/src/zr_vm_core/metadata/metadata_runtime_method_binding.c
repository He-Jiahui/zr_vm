#include "zr_vm_core/metadata_runtime.h"

#include <string.h>

#include "zr_vm_core/artifact_schema.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"

/* 供公开读取入口统一建立失败输出状态；空指针允许调用方直接进入失败路径。 */
static void metadata_runtime_clear_method_binding_view(SZrMetadataRuntimeMethodBindingView *outView) {
    if (outView != ZR_NULL) {
        ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
    }
}

/* 解释器 MethodSpec 解析失败时不留下上一次成功解析的函数图借用指针。 */
static void metadata_runtime_clear_interpreter_method_binding_view(
        SZrMetadataRuntimeInterpreterMethodBindingView *outView) {
    if (outView != ZR_NULL) {
        ZrCore_Memory_RawSet(outView, 0, sizeof(*outView));
    }
}

/* 注册表行是加载器投影；在解码前先核对行大小、计数及空表指针约定。 */
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

/* 解释器入口需从本地 MethodDef 节定位函数图索引；重复 token 视为不可信元数据。 */
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

/* 这两类方法视图只接受当前元数据运行时拥有的 MEMBER_DEF token。 */
static TZrBool metadata_runtime_is_local_method_token(TZrMetadataToken methodToken) {
    return methodToken != 0u && ZR_METADATA_TOKEN_TABLE(methodToken) == ZR_METADATA_TABLE_MEMBER_DEF;
}

/* AOT 方法表按同一函数索引并列；数量不一致时不能安全投影 token 到入口。 */
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

/**
 * @brief 将本地 MethodDef token 解析为已注册的 AOT 入口视图，供反射解析接入生成代码。
 * @pre runtime 中的方法 token、MethodInfo 与 thunk 表按函数索引对齐；outView 可空，失败时非空输出清零。
 * @return 仅在 token 唯一且 MethodInfo 索引、invoker 与函数入口均有效时返回 true。
 * @note 返回指针借用 code registration 的存储；调用方不得跨注册表销毁或替换继续使用。
 */
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

/**
 * @brief 解码一行 AOT call-binding 投影，供加载校验、链接安装及诊断测试读取。
 * @pre rowIndex 必须落在已附加注册表的行范围；模块延迟槽可合法地没有 AOT 目标指针。
 * @return 成功时给出调用点契约、重定位位置及可立即安装的目标视图；失败时清理输出。
 * @note 视图中的方法信息和 thunk 借用注册表，生命周期不超过该注册表。
 */
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

/* 统一保留首个失败行的定位字段，使加载器能把结构校验错误交给上层报告。 */
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

/* 加载/校验任一环节失败时整体撤销图内缓存，避免旧代际目标在后续执行中残留。 */
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

/* 先把注册表投影与刚由元数据重建的调用点逐项对照，再允许安装 AOT 目标。 */
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

/**
 * @brief 在 AOT 元数据挂接后重建并校验调用点，再安装可静态确定的生成代码目标。
 * @pre 函数图已全部 AttachFunction，注册表与函数图来自同一份 AOT 产物；模块槽保留到接收者运行时解析。
 * @return false 时记录首个诊断并使函数图缓存失效；没有 code registration 的解释器运行时视为无需链接。
 * @note 由库装载路径在函数注册后调用；不是逐次调用分派入口。
 */
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

    /* 先从已加载函数图重建契约，再信任产物行的索引与目标。 */
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
        /* BUG: 索引大于零时本次图遍历还可能因暂存分配失败返回 NULL；此前校验成功
         * 不保证此处第二次遍历成功，当前直接解引用会让加载失败变成崩溃。 */
        entry = &function->callSiteCaches[view.cacheIndex];
        /* 虚/接口及 typed-function 槽须等接收者或调用现场确定实现，不能预装静态目标。 */
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

/**
 * @brief 把本地 MethodDef token 映射到当前解释器函数图节点，供 MethodSpec 调用构造上下文。
 * @pre token 必须唯一对应 ZRP MethodDef 行，且其索引解析为非 native、带指令的函数。
 * @return 成功时输出元数据记录、定义行及函数节点；失败时非空输出保持清零。
 * @note 输出均借用 runtime/函数图；只能在该次元数据运行时与图节点存活期间使用。
 */
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
