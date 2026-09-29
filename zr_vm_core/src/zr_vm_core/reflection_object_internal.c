#include "reflection_object_internal.h"

#include "zr_vm_core/gc.h"
#include "zr_vm_core/global.h"

#include <string.h>
/* BUG: ignored registry 的查询、插入和索引写入未跨 RUNNING mutator 同步。 */
TZrBool ZrCore_Reflection_ObjectPinRaw(
        SZrState *state,
        SZrRawObject *object,
        TZrBool *addedByCaller) {
    return ZrCore_GarbageCollector_IgnoreObjectIfNeededFast(
            state != ZR_NULL ? state->global : ZR_NULL,
            state,
            object,
            addedByCaller);
}
/* BUG: ignored registry 的 swap-delete 未同步；addedByCaller 不是引用计数。 */
void ZrCore_Reflection_ObjectUnpinRaw(
        SZrGlobalState *global,
        SZrRawObject *object,
        TZrBool addedByCaller) {
    if (addedByCaller && global != ZR_NULL && object != ZR_NULL) {
        ZrCore_GarbageCollector_UnignoreObject(global, object);
    }
}

TZrBool ZrCore_Reflection_ObjectPinValue(
        SZrState *state,
        const SZrTypeValue *value,
        TZrBool *addedByCaller) {
    if (addedByCaller != ZR_NULL) {
        *addedByCaller = ZR_FALSE;
    }
    /* TODO: state 为空时 GC 值也会返回成功，但不会建立 root。 */
    if (state == ZR_NULL || value == ZR_NULL || !ZrCore_Value_IsGarbageCollectable(value)) {
        return ZR_TRUE;
    }
    return ZrCore_Reflection_ObjectPinRaw(
            state, ZrCore_Value_GetRawObject(value), addedByCaller);
}

void ZrCore_Reflection_ObjectUnpinValue(
        SZrGlobalState *global,
        const SZrTypeValue *value,
        TZrBool addedByCaller) {
    if (addedByCaller && value != ZR_NULL && ZrCore_Value_IsGarbageCollectable(value)) {
        ZrCore_Reflection_ObjectUnpinRaw(
                global, ZrCore_Value_GetRawObject(value), ZR_TRUE);
    }
}

SZrString *ZrCore_Reflection_ObjectMakeString(
        SZrState *state,
        const TZrChar *text) {
    if (state == ZR_NULL || text == ZR_NULL) {
        return ZR_NULL;
    }
    return ZrCore_String_Create(state, (TZrNativeString)text, strlen(text));
}

TZrBool ZrCore_Reflection_ObjectSetFieldValue(
        SZrState *state,
        SZrObject *object,
        const TZrChar *fieldName,
        const SZrTypeValue *value) {
    SZrString *fieldString;
    SZrTypeValue key;
    TZrBool objectPinned = ZR_FALSE;
    TZrBool keyPinned = ZR_FALSE;
    TZrBool valuePinned = ZR_FALSE;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || value == ZR_NULL ||
        !ZrCore_Reflection_ObjectPinRaw(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(object), &objectPinned) ||
        !ZrCore_Reflection_ObjectPinValue(state, value, &valuePinned)) {
        ZrCore_Reflection_ObjectUnpinValue(
                state != ZR_NULL ? state->global : ZR_NULL, value, valuePinned);
        ZrCore_Reflection_ObjectUnpinRaw(
                state != ZR_NULL ? state->global : ZR_NULL,
                object != ZR_NULL ? ZR_CAST_RAW_OBJECT_AS_SUPER(object) : ZR_NULL,
                objectPinned);
        return ZR_FALSE;
    }

    fieldString = ZrCore_Reflection_ObjectMakeString(state, fieldName);
    if (fieldString == ZR_NULL ||
        !ZrCore_Reflection_ObjectPinRaw(
                state, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString), &keyPinned)) {
        ZrCore_Reflection_ObjectUnpinValue(state->global, value, valuePinned);
        ZrCore_Reflection_ObjectUnpinRaw(
                state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(object), objectPinned);
        return ZR_FALSE;
    }

    ZrCore_Value_InitAsRawObject(
            state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    key.type = ZR_VALUE_TYPE_STRING;
    /*
     * BUG: Object_SetValue 没有成功返回值；domain 校验、对象初始化、rehash
     * 或 HashSet_Add 失败都可不写入，而本包装器仍在下面返回 true。
     * BUG: HashSet_Add 可经 GcMalloc/Exception_Throw 非局部跳转，绕过下面三次
     * unpin，留下 ignored registry root。
     */
    ZrCore_Object_SetValue(state, object, &key, value);
    ZrCore_Reflection_ObjectUnpinRaw(
            state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString), keyPinned);
    ZrCore_Reflection_ObjectUnpinValue(state->global, value, valuePinned);
    ZrCore_Reflection_ObjectUnpinRaw(
            state->global, ZR_CAST_RAW_OBJECT_AS_SUPER(object), objectPinned);
    return ZR_TRUE;
}

TZrBool ZrCore_Reflection_ObjectSetString(
        SZrState *state,
        SZrObject *object,
        const TZrChar *fieldName,
        const TZrChar *value) {
    /* BUG: 先创建 value 字符串，委托 helper 才 pin receiver；未 root 的对象可能在重试 GC 中被回收。 */
    SZrString *stringValue = ZrCore_Reflection_ObjectMakeString(state, value);
    SZrTypeValue fieldValue;

    if (stringValue == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrCore_Value_InitAsRawObject(
            state, &fieldValue, ZR_CAST_RAW_OBJECT_AS_SUPER(stringValue));
    fieldValue.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Reflection_ObjectSetFieldValue(
            state, object, fieldName, &fieldValue);
}

TZrBool ZrCore_Reflection_ObjectSetBool(
        SZrState *state,
        SZrObject *object,
        const TZrChar *fieldName,
        TZrBool value) {
    SZrTypeValue fieldValue;
    ZrCore_Value_InitAsBool(state, &fieldValue, value);
    return ZrCore_Reflection_ObjectSetFieldValue(
            state, object, fieldName, &fieldValue);
}

TZrBool ZrCore_Reflection_ObjectSetObject(
        SZrState *state,
        SZrObject *object,
        const TZrChar *fieldName,
        SZrObject *value,
        EZrValueType valueType) {
    SZrTypeValue fieldValue;

    if (value == ZR_NULL) {
        ZrCore_Value_ResetAsNull(&fieldValue);
    } else {
        ZrCore_Value_InitAsRawObject(
                state, &fieldValue, ZR_CAST_RAW_OBJECT_AS_SUPER(value));
        fieldValue.type = valueType;
    }
    return ZrCore_Reflection_ObjectSetFieldValue(
            state, object, fieldName, &fieldValue);
}

const SZrTypeValue *ZrCore_Reflection_ObjectGetFieldValue(
        SZrState *state,
        SZrObject *object,
        const TZrChar *fieldName) {
    SZrString *fieldString;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }
    /* TODO: 未 pin 的长字符串 key 若以后走 raw-array materialization，分配触发 GC 时可能失效。 */
    fieldString = ZrCore_Reflection_ObjectMakeString(state, fieldName);
    if (fieldString == ZR_NULL) {
        return ZR_NULL;
    }
    ZrCore_Value_InitAsRawObject(
            state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldString));
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(state, object, &key);
}
