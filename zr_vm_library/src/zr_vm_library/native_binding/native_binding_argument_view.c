#include "zr_vm_library/native_binding.h"

#include "zr_vm_core/metadata_runtime.h"

#include <string.h>

TZrBool ZrLib_CallContext_InlineArgumentView(
        const ZrLibCallContext *context,
        TZrSize index,
        ZrLibInlineArgumentView *outView) {
    ZrLibInlineArgumentView view = {0};
    const SZrTypeLayout *typeLayout;

    if (outView != ZR_NULL) {
        memset(outView, 0, sizeof(*outView));
    }
    if (context == ZR_NULL || outView == ZR_NULL ||
        !ZrLib_CallContext_InlineArgumentSpan(context, index, &view.span) ||
        context->inlineFrameFunction == ZR_NULL ||
        !ZrCore_MetadataRuntime_GetFunctionTypeLayoutRegistry(
                context->inlineFrameFunction, &view.registry) ||
        view.registry.layouts == ZR_NULL ||
        view.span.typeLayoutId >= view.registry.count) {
        return ZR_FALSE;
    }

    typeLayout = ZrCore_MetadataRuntime_ResolveFunctionTypeLayout(
            context->inlineFrameFunction, view.span.typeLayoutId);
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
