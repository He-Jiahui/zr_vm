#include "compiler/compile_time/compile_time_executor_internal.h"

#include "zr_vm_core/array.h"
#include "zr_vm_core/module.h"
#include "zr_vm_core/value.h"

TZrBool ct_value_from_compile_time_function(SZrCompilerState *cs,
                                                 SZrCompileTimeFunction *func,
                                                 SZrTypeValue *result) {
    SZrObjectModule *module;
    const SZrTypeValue *projectedValue;

    if (cs == ZR_NULL || func == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    if (func->isRuntimeProjection &&
        func->runtimeProjectionModuleName != ZR_NULL &&
        func->runtimeProjectionExportName != ZR_NULL) {
        module = ZrCore_Module_ImportByPath(cs->state, func->runtimeProjectionModuleName);
        if (module == ZR_NULL) {
            return ZR_FALSE;
        }

        projectedValue = ZrCore_Module_GetProExport(cs->state, module, func->runtimeProjectionExportName);
        if (projectedValue == ZR_NULL) {
            projectedValue = ZrCore_Module_GetPubExport(cs->state, module, func->runtimeProjectionExportName);
        }
        if (projectedValue == ZR_NULL) {
            return ZR_FALSE;
        }

        *result = *projectedValue;
        return ZR_TRUE;
    }

    ZrCore_Value_InitAsNativePointer(cs->state, result, func);
    return ZR_TRUE;
}

static SZrCompileTimeFunction *ct_registered_function_in_array(
        SZrArray *functions,
        TZrPtr pointerValue) {
    if (functions == ZR_NULL || !functions->isValid ||
        functions->head == ZR_NULL ||
        functions->elementSize != sizeof(SZrCompileTimeFunction *)) {
        return ZR_NULL;
    }

    for (TZrSize index = 0U; index < functions->length; ++index) {
        SZrCompileTimeFunction **function =
                (SZrCompileTimeFunction **)ZrCore_Array_Get(functions, index);
        if (*function != ZR_NULL && (TZrPtr)*function == pointerValue) {
            return *function;
        }
    }
    return ZR_NULL;
}

TZrBool ct_value_try_get_compile_time_function(SZrCompilerState *cs,
                                                    const SZrTypeValue *value,
                                                    SZrCompileTimeFunction **result) {
    TZrPtr pointerValue;

    if (cs == ZR_NULL || value == ZR_NULL || result == ZR_NULL ||
        value->type != ZR_VALUE_TYPE_NATIVE_POINTER) {
        return ZR_FALSE;
    }

    pointerValue = value->value.nativeObject.nativePointer;
    if (pointerValue == ZR_NULL) {
        return ZR_FALSE;
    }

    /* A callable value may escape its provider; validate membership before
     * the caller reads the matched record's declaration or owner. */
    for (SZrCompilerState *owner = cs; owner != ZR_NULL;
         owner = owner->compileToolProviderParent) {
        SZrCompileTimeFunction *function = ct_registered_function_in_array(
                &owner->compileTimeFunctions, pointerValue);
        if (function != ZR_NULL) {
            *result = function;
            return ZR_TRUE;
        }

        SZrArray *modules = &owner->importedCompileTimeModules;
        if (!modules->isValid || modules->head == ZR_NULL ||
            modules->elementSize != sizeof(SZrImportedCompileTimeModule *)) {
            continue;
        }
        for (TZrSize index = 0U; index < modules->length; ++index) {
            SZrImportedCompileTimeModule **module =
                    (SZrImportedCompileTimeModule **)ZrCore_Array_Get(modules, index);
            if (*module == ZR_NULL) {
                continue;
            }
            function = ct_registered_function_in_array(
                    &(*module)->compileTimeFunctions, pointerValue);
            if (function != ZR_NULL) {
                *result = function;
                return ZR_TRUE;
            }
        }
    }

    return ZR_FALSE;
}
