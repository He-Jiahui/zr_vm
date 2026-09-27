#include "zr_vm_library/native_binding.h"

#include "zr_vm_core/metadata_runtime.h"

#include <string.h>

/* 内联帧可由包装函数执行，但布局注册在其原型上下文函数上。 */
static const SZrFunction *native_binding_select_metadata_registration_function(
        const SZrFunction *function) {
    if (function != ZR_NULL && function->metadataCodeRegistration == ZR_NULL &&
        function->prototypeContextFunction != ZR_NULL) {
        return function->prototypeContextFunction;
    }
    return function;
}

/* 为池化等零复制调用方同时提供帧内借用地址与同一函数的权威布局表。 */
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
    /* 元数据产物与解释/原型帧有不同的布局来源，不能仅凭 slotLayout 的 ID 解释内存。 */
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
    /* 两侧布局 ID、尺寸和对齐必须一致，才可把借用字节暴露给 Native 消费者。 */
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
