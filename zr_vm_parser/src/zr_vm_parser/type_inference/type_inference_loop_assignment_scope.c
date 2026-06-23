//
// Created by Auto on 2026/06/23.
//

#include "zr_vm_parser/type_inference.h"
#include "zr_vm_parser/compiler.h"

#include "type_inference_loop_assignment_join_internal.h"

#include "zr_vm_core/string.h"

#include <string.h>

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
    ZrParser_InferredType_Copy(cs->state, &binding.type, type);
    ZrCore_Array_Push(cs->state, &cs->typeEnv->variableTypes, &binding);
    return ZR_TRUE;
}
