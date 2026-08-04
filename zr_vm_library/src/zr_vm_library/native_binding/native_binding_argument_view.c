#include "zr_vm_library/native_binding.h"

#include "zr_vm_core/metadata_runtime.h"

#include <string.h>

static const SZrFunction *native_binding_select_metadata_registration_function(
        const SZrFunction *function) {
    if (function != ZR_NULL && function->metadataCodeRegistration == ZR_NULL &&
        function->prototypeContextFunction != ZR_NULL) {
        return function->prototypeContextFunction;
    }
    return function;
}

TZrBool ZrLib_CallContext_InlineArgumentView(
        const ZrLibCallContext *context,
        TZrSize index,
        ZrLibInlineArgumentView *outView) {
    ZrLibInlineArgumentView view = {0};
    const SZrTypeLayout *typeLayout;
    const SZrFunction *registrationFunction;

    if (outView != ZR_NULL) {
        memset(outView, 0, sizeof(*outView));
    }
    if (context == ZR_NULL || outView == ZR_NULL ||
        !ZrLib_CallContext_InlineArgumentSpan(context, index, &view.span) ||
        context->inlineFrameFunction == ZR_NULL) {
        return ZR_FALSE;
    }

    registrationFunction = native_binding_select_metadata_registration_function(
            context->inlineFrameFunction);
    if (registrationFunction != ZR_NULL &&
        registrationFunction->metadataCodeRegistration != ZR_NULL) {
        if (!ZrCore_MetadataRuntime_GetFunctionTypeLayoutRegistry(
                    context->inlineFrameFunction, &view.registry)) {
            return ZR_FALSE;
        }
        typeLayout = ZrCore_MetadataRuntime_ResolveFunctionTypeLayout(
                context->inlineFrameFunction, view.span.typeLayoutId);
    } else {
        if (!ZrCore_Function_GetPrototypeFrameTypeLayoutRegistry(
                    context->state,
                    context->inlineFrameFunction,
                    view.span.typeLayoutId,
                    &view.registry)) {
            return ZR_FALSE;
        }
        typeLayout = view.registry.layouts[view.span.typeLayoutId];
    }

    if (view.registry.layouts == ZR_NULL ||
        view.span.typeLayoutId >= view.registry.count) {
        return ZR_FALSE;
    }
    if (typeLayout == ZR_NULL ||
        view.registry.layouts[view.span.typeLayoutId] != typeLayout ||
        !ZrCore_TypeLayout_Validate(typeLayout) ||
        typeLayout->byteSize != view.span.byteSize ||
        typeLayout->byteAlign != view.span.byteAlign) {
        return ZR_FALSE;
    }

    view.typeLayout = typeLayout;
    *outView = view;
    return ZR_TRUE;
}
