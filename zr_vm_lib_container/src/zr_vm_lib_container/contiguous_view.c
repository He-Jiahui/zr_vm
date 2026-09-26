/* Span 和 ReadOnlySpan 共用运行时视图回调；索引可写性由各自元方法表决定。 */

#include "contiguous_view.h"

#include "zr_vm_core/debug.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/value.h"

#include <limits.h>

/* 字段名须与 module.c 的成员角色一致；PoolLease.span 也按此布局构造视图。 */
static const TZrChar *kViewSourceField = "source";
static const TZrChar *kViewStartField = "start";
static const TZrChar *kViewLengthField = "length";

/* Array.span 的接收者是数组，其余回调接收视图对象。 */
static SZrObject *contiguous_view_context_object(
        const ZrLibCallContext *context) {
    SZrTypeValue *selfValue;

    if (context == ZR_NULL || context->state == ZR_NULL) {
        return ZR_NULL;
    }

    selfValue = ZrLib_CallContext_Self(context);
    if (selfValue == ZR_NULL ||
        (selfValue->type != ZR_VALUE_TYPE_OBJECT &&
         selfValue->type != ZR_VALUE_TYPE_ARRAY) ||
        selfValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }
    return ZR_CAST_OBJECT(context->state, selfValue->value.object);
}

static TZrBool contiguous_view_read_int_field(
        SZrState *state,
        SZrObject *view,
        const TZrChar *fieldName,
        TZrInt64 *outValue) {
    const SZrTypeValue *value;

    if (state == ZR_NULL || view == ZR_NULL || fieldName == ZR_NULL ||
        outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    value = ZrLib_Object_GetFieldCString(state, view, fieldName);
    if (value == ZR_NULL || !ZR_VALUE_IS_TYPE_INT(value->type)) {
        return ZR_FALSE;
    }
    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        *outValue = value->value.nativeObject.nativeInt64;
        return ZR_TRUE;
    }
    if (value->value.nativeObject.nativeUInt64 > (TZrUInt64)INT64_MAX) {
        return ZR_FALSE;
    }
    *outValue = (TZrInt64)value->value.nativeObject.nativeUInt64;
    return ZR_TRUE;
}

/* source 作为对象字段受 GC 追踪；区间只记录投影边界，不复制底层元素。 */
static TZrBool contiguous_view_store(
        SZrState *state,
        SZrObject *view,
        const SZrTypeValue *source,
        TZrInt64 start,
        TZrInt64 length) {
    SZrTypeValue startValue;
    SZrTypeValue lengthValue;

    if (state == ZR_NULL || view == ZR_NULL || start < 0 || length < 0) {
        return ZR_FALSE;
    }

    /* TODO: 三个字段写入接口均返回 void，此处仅靠 threadStatus 判断完成；
     * 需对 Object_SetFieldCString 的分配/pin 失败注入，核对部分初始化视图是否会被当作成功发布。 */
    if (source != ZR_NULL) {
        ZrLib_Object_SetFieldCString(
                state, view, kViewSourceField, source);
    } else {
        SZrTypeValue nullValue;
        ZrLib_Value_SetNull(&nullValue);
        ZrLib_Object_SetFieldCString(
                state, view, kViewSourceField, &nullValue);
    }
    ZrLib_Value_SetInt(state, &startValue, start);
    ZrLib_Value_SetInt(state, &lengthValue, length);
    ZrLib_Object_SetFieldCString(
            state, view, kViewStartField, &startValue);
    ZrLib_Object_SetFieldCString(
            state, view, kViewLengthField, &lengthValue);
    return state->threadStatus == ZR_THREAD_STATUS_FINE;
}

/* 切片保留传入原型；跨可写性转换或 Array.span 才按类型名取得新原型。 */
static SZrObject *contiguous_view_new(
        ZrLibCallContext *context,
        SZrObjectPrototype *prototype,
        const TZrChar *fallbackTypeName,
        const SZrTypeValue *source,
        TZrInt64 start,
        TZrInt64 length) {
    SZrObject *view;

    if (context == ZR_NULL || context->state == ZR_NULL) {
        return ZR_NULL;
    }

    view = prototype != ZR_NULL
                   ? ZrLib_Type_NewInstanceWithPrototype(
                             context->state, prototype)
                   : ZrLib_Type_NewInstance(
                             context->state, fallbackTypeName);
    if (view == ZR_NULL ||
        !contiguous_view_store(
                context->state, view, source, start, length)) {
        return ZR_NULL;
    }
    return view;
}

/* source 可为空以表示合法空视图；非空时仍保留原 source 对象，索引会继续走其元方法。
 * 因此以 PoolLease 为 source 的视图不会绕过租约关闭检查。 */
static TZrBool contiguous_view_read_state(
        ZrLibCallContext *context,
        SZrObject **outView,
        SZrTypeValue **outSource,
        TZrInt64 *outStart,
        TZrInt64 *outLength) {
    SZrObject *view;
    const SZrTypeValue *source;

    if (outView != ZR_NULL) {
        *outView = ZR_NULL;
    }
    if (outSource != ZR_NULL) {
        *outSource = ZR_NULL;
    }
    if (context == ZR_NULL || outView == ZR_NULL || outSource == ZR_NULL ||
        outStart == ZR_NULL || outLength == ZR_NULL) {
        return ZR_FALSE;
    }

    view = contiguous_view_context_object(context);
    if (view == ZR_NULL ||
        !contiguous_view_read_int_field(
                context->state, view, kViewStartField, outStart) ||
        !contiguous_view_read_int_field(
                context->state, view, kViewLengthField, outLength) ||
        *outStart < 0 || *outLength < 0) {
        return ZR_FALSE;
    }

    source = ZrLib_Object_GetFieldCString(
            context->state, view, kViewSourceField);
    if (source != ZR_NULL && source->value.object != ZR_NULL &&
        (source->type == ZR_VALUE_TYPE_OBJECT ||
         source->type == ZR_VALUE_TYPE_ARRAY)) {
        *outSource = (SZrTypeValue *)source;
    }
    *outView = view;
    return ZR_TRUE;
}

static TZrInt64 contiguous_view_checked_absolute_index(
        ZrLibCallContext *context,
        TZrInt64 start,
        TZrInt64 length,
        TZrInt64 index) {
    if (context == ZR_NULL || index < 0 ||
        (TZrUInt64)index >= (TZrUInt64)length ||
        start > INT64_MAX - index) {
        ZrCore_Debug_RunError(
                context != ZR_NULL ? context->state : ZR_NULL,
                "Contiguous view index out of range");
    }
    return start + index;
}

TZrBool ZrVmLibContainer_ContiguousView_FromArray(
        ZrLibCallContext *context,
        SZrTypeValue *result) {
    SZrTypeValue *selfValue;
    SZrObject *array;
    const SZrTypeValue *lengthValue;
    TZrInt64 length;
    SZrObject *view;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    selfValue = ZrLib_CallContext_Self(context);
    array = contiguous_view_context_object(context);
    lengthValue = array != ZR_NULL
                          ? ZrLib_Object_GetFieldCString(
                                    context->state,
                                    array,
                                    kViewLengthField)
                          : ZR_NULL;
    if (selfValue == ZR_NULL || lengthValue == ZR_NULL ||
        !contiguous_view_read_int_field(
                context->state, array, kViewLengthField, &length) ||
        length < 0) {
        return ZR_FALSE;
    }

    view = contiguous_view_new(
            context, ZR_NULL, "Span", selfValue, 0, length);
    if (view == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrLib_Value_SetObject(
            context->state, result, view, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

TZrBool ZrVmLibContainer_ContiguousView_Construct(
        ZrLibCallContext *context,
        SZrTypeValue *result) {
    SZrObject *view;
    SZrObjectPrototype *prototype;

    if (context == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }

    view = contiguous_view_context_object(context);
    prototype = ZrLib_CallContext_GetConstructTargetPrototype(context);
    if (view == ZR_NULL) {
        if (prototype == ZR_NULL) {
            prototype = ZrLib_CallContext_OwnerPrototype(context);
        }
        view = contiguous_view_new(
                context, prototype, ZR_NULL, ZR_NULL, 0, 0);
    } else if (!contiguous_view_store(
                       context->state, view, ZR_NULL, 0, 0)) {
        return ZR_FALSE;
    }
    if (view == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrLib_Value_SetObject(
            context->state, result, view, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

TZrBool ZrVmLibContainer_ContiguousView_Slice(
        ZrLibCallContext *context,
        SZrTypeValue *result) {
    SZrObject *view;
    SZrTypeValue *source;
    TZrInt64 start;
    TZrInt64 length;
    TZrInt64 sliceStart;
    TZrInt64 sliceLength;
    SZrObject *slice;

    if (result == ZR_NULL ||
        !contiguous_view_read_state(
                context, &view, &source, &start, &length) ||
        !ZrLib_CallContext_ReadInt(context, 0, &sliceStart) ||
        !ZrLib_CallContext_ReadInt(context, 1, &sliceLength)) {
        return ZR_FALSE;
    }
    if (sliceStart < 0 || sliceLength < 0 ||
        (TZrUInt64)sliceStart > (TZrUInt64)length ||
        (TZrUInt64)sliceLength >
                (TZrUInt64)(length - sliceStart) ||
        start > INT64_MAX - sliceStart) {
        ZrCore_Debug_RunError(
                context->state, "Contiguous view slice out of range");
    }

    slice = contiguous_view_new(
            context,
            ZrLib_CallContext_OwnerPrototype(context),
            ZR_NULL,
            source,
            start + sliceStart,
            sliceLength);
    if (slice == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrLib_Value_SetObject(
            context->state, result, slice, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

TZrBool ZrVmLibContainer_ContiguousView_AsReadOnly(
        ZrLibCallContext *context,
        SZrTypeValue *result) {
    SZrObject *view;
    SZrTypeValue *source;
    TZrInt64 start;
    TZrInt64 length;
    SZrObject *readOnlyView;

    if (result == ZR_NULL ||
        !contiguous_view_read_state(
                context, &view, &source, &start, &length)) {
        return ZR_FALSE;
    }

    readOnlyView = contiguous_view_new(
            context,
            ZR_NULL,
            "ReadOnlySpan",
            source,
            start,
            length);
    if (readOnlyView == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrLib_Value_SetObject(
            context->state, result, readOnlyView, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

/* 相对区间校验只保证视图自身边界；底层数组长度或租约存活性在 source 索引时再检查。 */
TZrBool ZrVmLibContainer_ContiguousView_GetItem(
        ZrLibCallContext *context,
        SZrTypeValue *result) {
    SZrObject *view;
    SZrTypeValue *source;
    TZrInt64 start;
    TZrInt64 length;
    TZrInt64 index;
    TZrInt64 absoluteIndex;
    SZrTypeValue key;

    if (result == ZR_NULL ||
        !contiguous_view_read_state(
                context, &view, &source, &start, &length) ||
        !ZrLib_CallContext_ReadInt(context, 0, &index)) {
        return ZR_FALSE;
    }
    absoluteIndex = contiguous_view_checked_absolute_index(
            context, start, length, index);
    if (source == ZR_NULL) {
        ZrCore_Debug_RunError(
                context->state, "Cannot dereference an empty contiguous view");
    }

    ZrLib_Value_SetInt(context->state, &key, absoluteIndex);
    if (!ZrCore_Object_SuperArrayGetInt(
                context->state, source, &key, result)) {
        ZrCore_Debug_RunError(
                context->state, "Contiguous view source is unavailable");
    }
    return ZR_TRUE;
}

TZrBool ZrVmLibContainer_ContiguousView_SetItem(
        ZrLibCallContext *context,
        SZrTypeValue *result) {
    SZrObject *view;
    SZrTypeValue *source;
    SZrTypeValue *value;
    TZrInt64 start;
    TZrInt64 length;
    TZrInt64 index;
    TZrInt64 absoluteIndex;
    SZrTypeValue key;

    if (result == ZR_NULL ||
        !contiguous_view_read_state(
                context, &view, &source, &start, &length) ||
        !ZrLib_CallContext_ReadInt(context, 0, &index)) {
        return ZR_FALSE;
    }
    value = ZrLib_CallContext_Argument(context, 1);
    if (value == ZR_NULL) {
        return ZR_FALSE;
    }
    absoluteIndex = contiguous_view_checked_absolute_index(
            context, start, length, index);
    if (source == ZR_NULL) {
        ZrCore_Debug_RunError(
                context->state, "Cannot dereference an empty contiguous view");
    }

    ZrLib_Value_SetInt(context->state, &key, absoluteIndex);
    if (!ZrCore_Object_SuperArraySetInt(
                context->state, source, &key, value)) {
        ZrCore_Debug_RunError(
                context->state, "Contiguous view source is unavailable");
    }
    ZrCore_Value_CopyNoProfile(context->state, result, value);
    return ZR_TRUE;
}
