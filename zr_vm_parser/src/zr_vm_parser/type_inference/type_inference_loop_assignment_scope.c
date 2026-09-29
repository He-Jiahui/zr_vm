//
// Created by Auto on 2026/06/23.
//

#include "zr_vm_parser/type_inference.h"
#include "zr_vm_parser/compiler.h"

#include "type_inference_loop_assignment_join_internal.h"

#include "zr_vm_core/string.h"

#include <string.h>

// 将循环赋值计划的回放限制在临时子环境中：父级名字仍可查找，新范围只写入当前覆盖层。
TZrBool ZrParser_TypeInferenceLoopAssignment_PushReplayScope(
        SZrCompilerState *cs,
        SZrTypeInferenceBranchScope *scope) {
    SZrTypeEnvironment *newEnv;

    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->typeEnv == ZR_NULL || scope == ZR_NULL) {
        return ZR_FALSE;
    }

    memset(scope, 0, sizeof(*scope));
    newEnv = ZrParser_TypeEnvironment_New(cs->state);
    if (newEnv == ZR_NULL) {
        return ZR_FALSE;
    }

    newEnv->parent = cs->typeEnv;
    newEnv->semanticContext = ZR_NULL;
    scope->savedEnv = cs->typeEnv;
    scope->isActive = ZR_TRUE;
    cs->typeEnv = newEnv;
    return ZR_TRUE;
}

// 回放结束先恢复原环境，再释放临时覆盖层，避免编译器状态继续指向已销毁环境。
void ZrParser_TypeInferenceLoopAssignment_PopReplayScope(
        SZrCompilerState *cs,
        SZrTypeInferenceBranchScope *scope) {
    SZrTypeEnvironment *activeEnv;

    if (cs == ZR_NULL ||
        cs->state == ZR_NULL ||
        scope == ZR_NULL ||
        !scope->isActive ||
        scope->savedEnv == ZR_NULL) {
        return;
    }

    activeEnv = cs->typeEnv;
    cs->typeEnv = scope->savedEnv;
    if (activeEnv != ZR_NULL) {
        ZrParser_TypeEnvironment_Free(cs->state, activeEnv);
    }
    memset(scope, 0, sizeof(*scope));
}

// 首次写入把父绑定复制到当前覆盖层并保留语义身份；同名后续写入只更新推断类型。
TZrBool ZrParser_TypeInferenceLoopAssignment_StoreBindingInCurrentScope(
        SZrCompilerState *cs,
        SZrString *name,
        const SZrInferredType *type) {
    const SZrTypeBinding *sourceBinding;
    SZrTypeBinding binding;
    TZrSize index;

    if (cs == ZR_NULL || cs->state == ZR_NULL || cs->typeEnv == ZR_NULL || name == ZR_NULL || type == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < cs->typeEnv->variableTypes.length; index++) {
        SZrTypeBinding *existing = (SZrTypeBinding *)ZrCore_Array_Get(&cs->typeEnv->variableTypes, index);
        if (existing != ZR_NULL && existing->name != ZR_NULL && ZrCore_String_Equal(existing->name, name)) {
            ZrParser_InferredType_Free(cs->state, &existing->type);
            ZrParser_InferredType_Copy(cs->state, &existing->type, type);
            return ZR_TRUE;
        }
    }

    sourceBinding = cs->typeEnv->parent != ZR_NULL
                        ? ZrParser_TypeEnvironment_FindVariableBinding(cs->typeEnv->parent, name)
                        : ZR_NULL;
    if (sourceBinding == ZR_NULL) {
        return ZR_FALSE;
    }

    binding.name = name;
    binding.declarationRange = sourceBinding->declarationRange;
    binding.hasDeclarationRange = sourceBinding->hasDeclarationRange;
    binding.typeId = sourceBinding->typeId;
    binding.symbolId = sourceBinding->symbolId;
    binding.placeId = sourceBinding->placeId;
    binding.originKind = sourceBinding->originKind;
    binding.runtimeRootKind = sourceBinding->runtimeRootKind;
    binding.originToken = sourceBinding->originToken;
    binding.originIndex = sourceBinding->originIndex;
    ZrParser_InferredType_Copy(cs->state, &binding.type, type);
    // BUG: TypeEnvironment_New 不检查 variableTypes 的 Array_Init 分配结果，PushReplayScope 仍会切换到该环境；
    //      首次复制父绑定时会对空 head 执行 Array_Push（调试构建断言，非断言构建向空指针写入）。
    // BUG: 受支持的循环体可按 body->count 收集任意多赋值；覆盖项超过初始容量后若 Allocate 失败，Array_Push 会覆盖 head 再 RawCopy。
    //      Array_Push 无失败返回，StoreBindingInCurrentScope 的 bool 契约不能安全报告这两类分配失败。
    ZrCore_Array_Push(cs->state, &cs->typeEnv->variableTypes, &binding);
    return ZR_TRUE;
}
