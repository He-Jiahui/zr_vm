//
// Shared helpers for zr.system.fs object/runtime implementation; see fs_internal.h for related contracts.
//

#include "fs/fs_internal.h"

#include "zr_vm_core/debug.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

/* See the detailed @fn system_fs_root_value_type contract at end of file. */
static EZrValueType system_fs_root_value_type(const SZrObject *object) {
    if (object != ZR_NULL && object->super.type == ZR_RAW_OBJECT_TYPE_ARRAY) {
        return ZR_VALUE_TYPE_ARRAY;
    }
    return ZR_VALUE_TYPE_OBJECT;
}
/* See the detailed @fn system_fs_begin_rooted_object contract at end of file. */
static TZrBool system_fs_begin_rooted_object(SZrState *state, SZrObject *object, ZrLibTempValueRoot *root) {
    if (state == ZR_NULL || object == ZR_NULL || root == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!ZrLib_TempValueRoot_Begin(state, root)) {
        return ZR_FALSE;
    }
    if (!ZrLib_TempValueRoot_SetObject(root, object, system_fs_root_value_type(object))) {
        ZrLib_TempValueRoot_End(root);
        return ZR_FALSE;
    }
    return ZR_TRUE;
}
/* See the detailed @fn system_fs_rooted_object contract at end of file. */
static SZrObject *system_fs_rooted_object(SZrState *state, ZrLibTempValueRoot *root) {
    SZrTypeValue *rootValue;

    if (state == ZR_NULL || root == ZR_NULL) {
        return ZR_NULL;
    }

    rootValue = ZrLib_TempValueRoot_Value(root);
    if (rootValue == ZR_NULL || rootValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(state, rootValue->value.object);
}

/* See the detailed @fn system_fs_set_message_field contract at end of file. */
static void system_fs_set_message_field(SZrState *state, SZrObject *object, const TZrChar *message) {
    SZrTypeValue fieldValue;
    ZrLibTempValueRoot objectRoot;
    SZrObject *rootedObject;

    if (state == ZR_NULL || object == ZR_NULL) {
        return;
    }

    if (!system_fs_begin_rooted_object(state, object, &objectRoot)) {
        return;
    }

    ZrLib_Value_SetString(state, &fieldValue, message != ZR_NULL ? message : "I/O error");
    rootedObject = system_fs_rooted_object(state, &objectRoot);
    if (rootedObject != ZR_NULL) {
        ZrLib_Object_SetFieldCString(state, rootedObject, "message", &fieldValue);
    }
    ZrLib_TempValueRoot_End(&objectRoot);
}

/* See the detailed @fn system_fs_make_io_exception contract at end of file. */
static TZrBool system_fs_make_io_exception(SZrState *state,
                                           const TZrChar *message,
                                           ZR_OUT SZrTypeValue *outValue) {
    SZrObject *object;
    SZrObject *stacksArray;
    SZrObject *rootedObject;
    SZrTypeValue objectValue;
    SZrTypeValue stacksValue;
    ZrLibTempValueRoot objectRoot;

    if (state == ZR_NULL || outValue == ZR_NULL) {
        return ZR_FALSE;
    }

    object = ZrLib_Type_NewInstance(state, "IOException");
    if (object == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!system_fs_begin_rooted_object(state, object, &objectRoot)) {
        return ZR_FALSE;
    }

    stacksArray = ZrLib_Array_New(state);
    rootedObject = system_fs_rooted_object(state, &objectRoot);
    if (rootedObject == ZR_NULL || stacksArray == ZR_NULL) {
        ZrLib_TempValueRoot_End(&objectRoot);
        return ZR_FALSE;
    }

    ZrLib_Value_SetObject(state, &objectValue, rootedObject, ZR_VALUE_TYPE_OBJECT);
    ZrLib_Value_SetObject(state, &stacksValue, stacksArray, ZR_VALUE_TYPE_ARRAY);
    ZrLib_Object_SetFieldCString(state, rootedObject, "stacks", &stacksValue);
    rootedObject = system_fs_rooted_object(state, &objectRoot);
    if (rootedObject == ZR_NULL) {
        ZrLib_TempValueRoot_End(&objectRoot);
        return ZR_FALSE;
    }
    ZrLib_Value_SetObject(state, &objectValue, rootedObject, ZR_VALUE_TYPE_OBJECT);
    ZrLib_Object_SetFieldCString(state, rootedObject, "exception", &objectValue);
    rootedObject = system_fs_rooted_object(state, &objectRoot);
    if (rootedObject == ZR_NULL) {
        ZrLib_TempValueRoot_End(&objectRoot);
        return ZR_FALSE;
    }
    system_fs_set_message_field(state, rootedObject, message);
    rootedObject = system_fs_rooted_object(state, &objectRoot);
    if (rootedObject == ZR_NULL) {
        ZrLib_TempValueRoot_End(&objectRoot);
        return ZR_FALSE;
    }
    ZrLib_Value_SetObject(state, outValue, rootedObject, ZR_VALUE_TYPE_OBJECT);
    ZrLib_TempValueRoot_End(&objectRoot);
    return ZR_TRUE;
}

/* 字段助手共同物化 FileSystemEntry、SystemFileInfo、FileStream；底层 Object_SetFieldCString 为 void/软失败接口。 */
void ZrSystem_Fs_WriteIntField(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrInt64 value) {
    SZrTypeValue fieldValue;
    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return;
    }
    ZrLib_Value_SetInt(state, &fieldValue, value);
    ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue);
}

void ZrSystem_Fs_WriteBoolField(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrBool value) {
    SZrTypeValue fieldValue;
    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return;
    }
    ZrLib_Value_SetBool(state, &fieldValue, value);
    ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue);
}
/** BUG: 短串 cache miss 跨驻留表阈值且自定义 allocator 仅本次 HASH_BUCKET 扩容失败时，SetString 不写 fieldValue，后续 void setter 仍读取未初始化槽；不泛化其他 OOM。 */
void ZrSystem_Fs_WriteStringField(SZrState *state, SZrObject *object, const TZrChar *fieldName, const TZrChar *value) {
    SZrTypeValue fieldValue;
    ZrLibTempValueRoot objectRoot;
    SZrObject *rootedObject;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return;
    }

    if (!system_fs_begin_rooted_object(state, object, &objectRoot)) {
        return;
    }

    ZrLib_Value_SetString(state, &fieldValue, value != ZR_NULL ? value : "");
    rootedObject = system_fs_rooted_object(state, &objectRoot);
    if (rootedObject != ZR_NULL) {
        ZrLib_Object_SetFieldCString(state, rootedObject, fieldName, &fieldValue);
    }
    ZrLib_TempValueRoot_End(&objectRoot);
}

void ZrSystem_Fs_WriteNullField(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    SZrTypeValue fieldValue;
    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return;
    }
    ZrLib_Value_SetNull(&fieldValue);
    ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue);
}

void ZrSystem_Fs_WriteObjectField(SZrState *state,
                                  SZrObject *object,
                                  const TZrChar *fieldName,
                                  SZrObject *fieldObject,
                                  EZrValueType valueType) {
    SZrTypeValue fieldValue;
    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL || fieldObject == ZR_NULL) {
        return;
    }
    ZrLib_Value_SetObject(state, &fieldValue, fieldObject, valueType);
    ZrLib_Object_SetFieldCString(state, object, fieldName, &fieldValue);
}

const SZrTypeValue *ZrSystem_Fs_GetFieldValue(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }
    return ZrLib_Object_GetFieldCString(state, object, fieldName);
}

const TZrChar *ZrSystem_Fs_GetStringField(SZrState *state, SZrObject *object, const TZrChar *fieldName) {
    const SZrTypeValue *value = ZrSystem_Fs_GetFieldValue(state, object, fieldName);
    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_STRING || value->value.object == ZR_NULL) {
        return ZR_NULL;
    }
    return ZrCore_String_GetNativeString(ZR_CAST_STRING(state, value->value.object));
}

TZrInt64 ZrSystem_Fs_GetIntField(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrInt64 defaultValue) {
    const SZrTypeValue *value = ZrSystem_Fs_GetFieldValue(state, object, fieldName);
    if (value == ZR_NULL) {
        return defaultValue;
    }
    if (ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)) {
        return value->value.nativeObject.nativeInt64;
    }
    if (ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type)) {
        return (TZrInt64)value->value.nativeObject.nativeUInt64;
    }
    return defaultValue;
}

/* TODO: 当前全仓仅此定义与 fs_internal.h 声明，没有实调用方。
 * 需在后续清理时核对是否为旧 closed 字段读取路径遗留，再决定是否删除。 */
TZrBool ZrSystem_Fs_GetBoolField(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrBool defaultValue) {
    const SZrTypeValue *value = ZrSystem_Fs_GetFieldValue(state, object, fieldName);
    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_BOOL) {
        return defaultValue;
    }
    return value->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE;
}

SZrObject *ZrSystem_Fs_SelfObject(const ZrLibCallContext *context) {
    SZrTypeValue *selfValue;
    if (context == ZR_NULL) {
        return ZR_NULL;
    }
    selfValue = ZrLib_CallContext_Self(context);
    if (selfValue == ZR_NULL || selfValue->type != ZR_VALUE_TYPE_OBJECT || selfValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }
    return ZR_CAST_OBJECT(context->state, selfValue->value.object);
}

/* native 构造复用调度器已创建的子类实例；否则按 construct target/owner 原型新建。 */
SZrObject *ZrSystem_Fs_ResolveConstructTarget(ZrLibCallContext *context) {
    SZrObject *self;
    SZrObjectPrototype *ownerPrototype;
    SZrObjectPrototype *targetPrototype;

    if (context == ZR_NULL || context->state == ZR_NULL) {
        return ZR_NULL;
    }

    self = ZrSystem_Fs_SelfObject(context);
    ownerPrototype = ZrLib_CallContext_OwnerPrototype(context);
    if (self != ZR_NULL && ownerPrototype != ZR_NULL && ZrCore_Object_IsInstanceOfPrototype(self, ownerPrototype)) {
        return self;
    }

    targetPrototype = ZrLib_CallContext_GetConstructTargetPrototype(context);
    if (targetPrototype == ZR_NULL) {
        targetPrototype = ownerPrototype;
    }
    return ZrLib_Type_NewInstanceWithPrototype(context->state, targetPrototype);
}

TZrBool ZrSystem_Fs_FinishObjectResult(SZrState *state, SZrTypeValue *result, SZrObject *object) {
    if (state == ZR_NULL || result == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrLib_Value_SetObject(state, result, object, ZR_VALUE_TYPE_OBJECT);
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_ReadStringArgument(const ZrLibCallContext *context, TZrSize index, const TZrChar **outText) {
    SZrString *stringObject = ZR_NULL;
    if (outText != ZR_NULL) {
        *outText = ZR_NULL;
    }
    if (context == ZR_NULL || outText == ZR_NULL || !ZrLib_CallContext_ReadString(context, index, &stringObject) ||
        stringObject == ZR_NULL) {
        return ZR_FALSE;
    }
    *outText = ZrCore_String_GetNativeString(stringObject);
    return *outText != ZR_NULL;
}

/* 将短模式、seek 原点等可选参数复制到调用方缓冲；越界失败不修改输出缓冲。 */
TZrBool ZrSystem_Fs_ReadOptionalStringArgument(const ZrLibCallContext *context,
                                               TZrSize index,
                                               const TZrChar *defaultValue,
                                               ZR_OUT TZrChar *buffer,
                                               TZrSize bufferSize) {
    const TZrChar *text = defaultValue;
    if (buffer == ZR_NULL || bufferSize == 0) {
        return ZR_FALSE;
    }
    if (context != ZR_NULL && index < ZrLib_CallContext_ArgumentCount(context)) {
        if (!ZrSystem_Fs_ReadStringArgument(context, index, &text)) {
            return ZR_FALSE;
        }
    }
    if (text == ZR_NULL) {
        text = "";
    }
    if (strlen(text) + 1 > bufferSize) {
        return ZR_FALSE;
    }
    memcpy(buffer, text, strlen(text) + 1);
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_ReadOptionalBoolArgument(const ZrLibCallContext *context,
                                             TZrSize index,
                                             TZrBool defaultValue,
                                             TZrBool *outValue) {
    const SZrTypeValue *value;
    if (outValue == ZR_NULL) {
        return ZR_FALSE;
    }
    *outValue = defaultValue;
    if (context == ZR_NULL || index >= ZrLib_CallContext_ArgumentCount(context)) {
        return ZR_TRUE;
    }
    value = ZrLib_CallContext_Argument(context, index);
    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_BOOL) {
        return ZR_FALSE;
    }
    *outValue = value->value.nativeObject.nativeBool ? ZR_TRUE : ZR_FALSE;
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_ReadOptionalIntArgument(const ZrLibCallContext *context,
                                            TZrSize index,
                                            TZrInt64 defaultValue,
                                            TZrInt64 *outValue) {
    if (outValue == ZR_NULL) {
        return ZR_FALSE;
    }
    *outValue = defaultValue;
    if (context == ZR_NULL || index >= ZrLib_CallContext_ArgumentCount(context)) {
        return ZR_TRUE;
    }
    return ZrLib_CallContext_ReadInt(context, index, outValue);
}

TZrBool ZrSystem_Fs_ReadArrayArgument(const ZrLibCallContext *context, TZrSize index, SZrObject **outArray) {
    if (outArray != ZR_NULL) {
        *outArray = ZR_NULL;
    }
    return context != ZR_NULL && outArray != ZR_NULL && ZrLib_CallContext_ReadArray(context, index, outArray);
}

/* 将已格式化的宿主失败转为 IOException 状态；回调按 C false 返回，VM 异常链可能非局部退出。 */
TZrBool ZrSystem_Fs_RaiseIOException(SZrState *state, const TZrChar *format, ...) {
    TZrChar message[512];
    va_list arguments;
    SZrTypeValue exceptionValue;

    if (state == ZR_NULL || format == ZR_NULL) {
        return ZR_FALSE;
    }

    va_start(arguments, format);
    vsnprintf(message, sizeof(message), format, arguments);
    va_end(arguments);

    if (!system_fs_make_io_exception(state, message, &exceptionValue)) {
        ZrCore_Debug_RunError(state, "%s", message);
    }

    if (!ZrCore_Exception_NormalizeThrownValue(state,
                                               &exceptionValue,
                                               state->callInfoList,
                                               ZR_THREAD_STATUS_RUNTIME_ERROR) &&
        !ZrCore_Exception_NormalizeStatus(state, ZR_THREAD_STATUS_EXCEPTION_ERROR)) {
        ZrCore_Debug_RunError(state, "%s", message);
    }

    state->threadStatus = state->currentExceptionStatus != ZR_THREAD_STATUS_FINE
                                  ? state->currentExceptionStatus
                                  : ZR_THREAD_STATUS_RUNTIME_ERROR;
    return ZR_FALSE;
}

/* 立即读取最近一次平台错误；调用方须在覆盖 errno 前调用，action/path 同步格式化为消息。 */
TZrBool ZrSystem_Fs_RaiseErrnoIOException(SZrState *state, const TZrChar *action, const TZrChar *path) {
    const TZrChar *detail = strerror(errno);
    return ZrSystem_Fs_RaiseIOException(state,
                                        "%s failed for '%s': %s",
                                        action != ZR_NULL ? action : "I/O",
                                        path != ZR_NULL ? path : "<path>",
                                        detail != ZR_NULL ? detail : "unknown error");
}

/* 将栈上宿主 File_Info 复制为脚本快照；每次字段分配后从根重取 receiver。 */
SZrObject *ZrSystem_Fs_MakeInfoObject(SZrState *state, const SZrLibrary_File_Info *info) {
    SZrObject *object;
    ZrLibTempValueRoot objectRoot;
    if (state == ZR_NULL || info == ZR_NULL) {
        return ZR_NULL;
    }

    object = ZrLib_Type_NewInstance(state, "SystemFileInfo");
    if (object == ZR_NULL) {
        return ZR_NULL;
    }
    if (!system_fs_begin_rooted_object(state, object, &objectRoot)) {
        return ZR_NULL;
    }

    object = system_fs_rooted_object(state, &objectRoot);
    if (object == ZR_NULL) {
        ZrLib_TempValueRoot_End(&objectRoot);
        return ZR_NULL;
    }

    ZrSystem_Fs_WriteStringField(state, object, "path", info->path);
    object = system_fs_rooted_object(state, &objectRoot);
    if (object != ZR_NULL) {
        ZrSystem_Fs_WriteIntField(state, object, "size", info->size);
    }
    object = system_fs_rooted_object(state, &objectRoot);
    if (object != ZR_NULL) {
        ZrSystem_Fs_WriteBoolField(state, object, "isFile", info->existence == ZR_LIBRARY_FILE_IS_FILE);
    }
    object = system_fs_rooted_object(state, &objectRoot);
    if (object != ZR_NULL) {
        ZrSystem_Fs_WriteBoolField(state, object, "isDirectory", info->existence == ZR_LIBRARY_FILE_IS_DIRECTORY);
    }
    object = system_fs_rooted_object(state, &objectRoot);
    if (object != ZR_NULL) {
        ZrSystem_Fs_WriteIntField(state, object, "modifiedMilliseconds", info->modifiedMilliseconds);
    }
    object = system_fs_rooted_object(state, &objectRoot);
    if (object != ZR_NULL) {
        ZrSystem_Fs_WriteBoolField(state, object, "exists", info->exists);
    }
    object = system_fs_rooted_object(state, &objectRoot);
    if (object != ZR_NULL) {
        ZrSystem_Fs_WriteStringField(state, object, "name", info->name);
    }
    object = system_fs_rooted_object(state, &objectRoot);
    if (object != ZR_NULL) {
        ZrSystem_Fs_WriteStringField(state, object, "extension", info->extension);
    }
    object = system_fs_rooted_object(state, &objectRoot);
    if (object != ZR_NULL) {
        ZrSystem_Fs_WriteStringField(state, object, "parentPath", info->parentPath);
    }
    object = system_fs_rooted_object(state, &objectRoot);
    if (object != ZR_NULL) {
        ZrSystem_Fs_WriteIntField(state, object, "createdMilliseconds", info->createdMilliseconds);
    }
    object = system_fs_rooted_object(state, &objectRoot);
    if (object != ZR_NULL) {
        ZrSystem_Fs_WriteIntField(state, object, "accessedMilliseconds", info->accessedMilliseconds);
    }
    object = system_fs_rooted_object(state, &objectRoot);
    ZrLib_TempValueRoot_End(&objectRoot);
    return object;
}

/* 目录列举、复制/移动结果都用同一构造路径，以维持 File/Folder 的字段形状。 */
SZrObject *ZrSystem_Fs_NewEntryObject(SZrState *state,
                                      const TZrChar *typeName,
                                      const TZrChar *originalPath,
                                      const TZrChar *fullPathOverride) {
    SZrObject *object;
    if (state == ZR_NULL || typeName == ZR_NULL) {
        return ZR_NULL;
    }
    object = ZrLib_Type_NewInstance(state, typeName);
    if (object == ZR_NULL) {
        return ZR_NULL;
    }
    /* PopulateEntryObject 在 VM 分配期间根住对象并重新读取；当前普通 OBJECT 晋升保持地址稳定。
     * 因此这里返回该对象指针不构成已证实的移动缺陷；path/string 借用窗口单独记录在声明契约中。
     */
    return ZrSystem_Fs_PopulateEntryObject(state, object, originalPath, fullPathOverride) ? object : ZR_NULL;
}

/* path 保留调用者拼写，fullPath/fileInfo 记录查询快照；pathSource 可能跨 VM 分配，借用约束见声明。 */
TZrBool ZrSystem_Fs_PopulateEntryObject(SZrState *state,
                                        SZrObject *object,
                                        const TZrChar *originalPath,
                                        const TZrChar *fullPathOverride) {
    SZrLibrary_File_Info info;
    SZrObject *parentObject = ZR_NULL;
    SZrObject *infoObject = ZR_NULL;
    ZrLibTempValueRoot objectRoot;
    ZrLibTempValueRoot infoRoot;
    ZrLibTempValueRoot parentRoot;
    const TZrChar *pathSource = fullPathOverride != ZR_NULL ? fullPathOverride : originalPath;
    TZrBool hasInfoRoot = ZR_FALSE;
    TZrBool hasParentRoot = ZR_FALSE;
    TZrBool result = ZR_FALSE;

    if (state == ZR_NULL || object == ZR_NULL || pathSource == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!system_fs_begin_rooted_object(state, object, &objectRoot)) {
        return ZR_FALSE;
    }

    memset(&info, 0, sizeof(info));
    if (!ZrLibrary_File_QueryInfo((TZrNativeString)pathSource, &info)) {
        goto cleanup;
    }

    infoObject = ZrSystem_Fs_MakeInfoObject(state, &info);
    if (infoObject == ZR_NULL) {
        goto cleanup;
    }
    if (!system_fs_begin_rooted_object(state, infoObject, &infoRoot)) {
        goto cleanup;
    }
    hasInfoRoot = ZR_TRUE;
    /* parent 递归包装的是上级目录，而不是把宿主 Info 指针借给脚本。 */
    if (info.parentPath[0] != '\0') {
        parentObject = ZrSystem_Fs_NewEntryObject(state, "Folder", info.parentPath, info.parentPath);
        if (parentObject != ZR_NULL) {
            if (!system_fs_begin_rooted_object(state, parentObject, &parentRoot)) {
                goto cleanup;
            }
            hasParentRoot = ZR_TRUE;
        }
    }

    object = system_fs_rooted_object(state, &objectRoot);
    infoObject = hasInfoRoot ? system_fs_rooted_object(state, &infoRoot) : infoObject;
    parentObject = hasParentRoot ? system_fs_rooted_object(state, &parentRoot) : parentObject;
    if (object == ZR_NULL || infoObject == ZR_NULL) {
        goto cleanup;
    }

    ZrSystem_Fs_WriteStringField(state, object, "path", originalPath != ZR_NULL ? originalPath : info.path);
    object = system_fs_rooted_object(state, &objectRoot);
    if (object == ZR_NULL) {
        goto cleanup;
    }
    ZrSystem_Fs_WriteStringField(state, object, "fullPath", info.path);
    object = system_fs_rooted_object(state, &objectRoot);
    if (object == ZR_NULL) {
        goto cleanup;
    }
    ZrSystem_Fs_WriteStringField(state, object, "name", info.name);
    object = system_fs_rooted_object(state, &objectRoot);
    if (object == ZR_NULL) {
        goto cleanup;
    }
    ZrSystem_Fs_WriteStringField(state, object, "extension", info.extension);
    object = system_fs_rooted_object(state, &objectRoot);
    if (object == ZR_NULL) {
        goto cleanup;
    }
    if (parentObject != ZR_NULL) {
        ZrSystem_Fs_WriteObjectField(state, object, "parent", parentObject, ZR_VALUE_TYPE_OBJECT);
    } else {
        ZrSystem_Fs_WriteNullField(state, object, "parent");
    }
    object = system_fs_rooted_object(state, &objectRoot);
    infoObject = hasInfoRoot ? system_fs_rooted_object(state, &infoRoot) : infoObject;
    if (object == ZR_NULL || infoObject == ZR_NULL) {
        goto cleanup;
    }
    ZrSystem_Fs_WriteObjectField(state, object, "fileInfo", infoObject, ZR_VALUE_TYPE_OBJECT);
    result = ZR_TRUE;

cleanup:
    if (hasParentRoot) {
        ZrLib_TempValueRoot_End(&parentRoot);
    }
    if (hasInfoRoot) {
        ZrLib_TempValueRoot_End(&infoRoot);
    }
    ZrLib_TempValueRoot_End(&objectRoot);
    return result;
}

/* refresh 仅更新 fileInfo；原 path/name/parent 不重算，输出结构即使字段 setter 软失败仍可成功。 */
TZrBool ZrSystem_Fs_RefreshEntryObject(SZrState *state,
                                       SZrObject *object,
                                       ZR_OUT SZrLibrary_File_Info *outInfo,
                                       ZR_OUT SZrObject **outInfoObject) {
    SZrLibrary_File_Info info;
    SZrObject *infoObject;
    const TZrChar *fullPath;

    if (outInfo != ZR_NULL) {
        memset(outInfo, 0, sizeof(*outInfo));
    }
    if (outInfoObject != ZR_NULL) {
        *outInfoObject = ZR_NULL;
    }

    if (state == ZR_NULL || object == ZR_NULL) {
        return ZR_FALSE;
    }

    fullPath = ZrSystem_Fs_GetStringField(state, object, "fullPath");
    if (fullPath == ZR_NULL) {
        return ZR_FALSE;
    }
    memset(&info, 0, sizeof(info));
    if (!ZrLibrary_File_QueryInfo((TZrNativeString)fullPath, &info)) {
        return ZR_FALSE;
    }

    infoObject = ZrSystem_Fs_MakeInfoObject(state, &info);
    if (infoObject == ZR_NULL) {
        return ZR_FALSE;
    }

    ZrSystem_Fs_WriteObjectField(state, object, "fileInfo", infoObject, ZR_VALUE_TYPE_OBJECT);
    if (outInfo != ZR_NULL) {
        *outInfo = info;
    }
    if (outInfoObject != ZR_NULL) {
        *outInfoObject = infoObject;
    }
    return ZR_TRUE;
}

/**
 * @fn static EZrValueType system_fs_root_value_type(const SZrObject *object)
 * ## Summary
 * 为临时强根选择与对象种类匹配的 VM 值标签。
 * ## Parameters
 * @param[in] object 待标记对象；本函数只检查 raw-object kind，NULL 按普通对象标签处理。
 * ## Returns
 * @return 数组返回 ZR_VALUE_TYPE_ARRAY，其他对象（包括 NULL）返回 ZR_VALUE_TYPE_OBJECT。
 * ## Constraints and lifecycle
 * 本函数不创建根、不持有对象，也不延长指针寿命；由调用方保证传入对象在读取其 kind 时有效。
 * ## References
 * system_fs_begin_rooted_object() 使用此标签调用 ZrLib_TempValueRoot_SetObject()。
 */

/**
 * @fn static TZrBool system_fs_begin_rooted_object(SZrState *state, SZrObject *object, ZrLibTempValueRoot *root)
 * ## Summary
 * 开始一个临时值根并把对象写入根槽；写入失败时配对结束已开始的根。
 * ## Parameters
 * @param[in,out] state 当前 VM state，必须与对象和后续根读取属于同一调用。
 * @param[in] object 要保活的对象；调用者须在进入本函数时持有有效引用。
 * @param[out] root 调用者提供的栈上根槽；成功时变为 active。
 * ## Returns
 * @return 成功并激活根时返回 ZR_TRUE；参数无效、Begin 失败或 SetObject 失败时返回 ZR_FALSE。
 * ## Constraints and lifecycle
 * 正常返回 ZR_TRUE 后调用者必须恰好调用一次 ZrLib_TempValueRoot_End()；返回 ZR_FALSE 时若本函数已 Begin，会先自行 End，调用者不得再 End。后续 VM 分配后应从根槽重新取得对象地址。VM 异常可能非局部退出，不能据此推断所有异常路径都执行配对清理。
 * ## References
 * system_fs_root_value_type()、system_fs_rooted_object()、ZrLib_TempValueRoot_Begin()、ZrLib_TempValueRoot_SetObject()。
 */

/**
 * @fn static SZrObject *system_fs_rooted_object(SZrState *state, ZrLibTempValueRoot *root)
 * ## Summary
 * 从当前 active 的临时值根读取对象并转换为公开对象视图。
 * ## Parameters
 * @param[in] state 创建该根的 VM state。
 * @param[in] root 由 system_fs_begin_rooted_object() 成功激活、且尚未 End 的根槽。
 * ## Returns
 * @return 根参数/值无效或根值为空时返回 ZR_NULL；否则返回根中当前对象视图。
 * ## Constraints and lifecycle
 * 返回的是借用指针，不增加引用也不延长根寿命；任何可能触发 VM 分配的调用后都应重新读取根槽，且不得在 End() 后使用。
 * ## References
 * system_fs_begin_rooted_object()、ZrLib_TempValueRoot_Value()、ZrLib_TempValueRoot_End()。
 */

/**
 * @fn static void system_fs_set_message_field(SZrState *state, SZrObject *object, const TZrChar *message)
 * ## Summary
 * 在临时根保护的异常对象上物化并尝试写入 message 字段。
 * ## Parameters
 * @param[in,out] state 当前 VM state。
 * @param[in,out] object 要更新的异常对象；本函数先建立临时根。
 * @param[in] message 同步读取的 NUL 结尾 C 字符串；为 ZR_NULL 时使用 I/O error 回退文本。
 * ## Returns
 * 本函数无返回值；无效 state/object 或建立临时根失败时直接返回，字段写入结果也不会向调用者报告。
 * ## Constraints and lifecycle
 * 字符串创建可能触发分配，因此写字段前从根槽重新读取对象；成功建立根后本函数负责 End。ZrLib_Value_SetString() 与 ZrLib_Object_SetFieldCString() 都是 void/软失败边界。BUG: 若短串缓存 miss 恰逢 HASH_BUCKET 扩容，且自定义 allocator 仅令该扩容失败而其他步骤成功，SetString 不初始化局部 fieldValue，随后 setter/value-pin 路径仍读取该槽；未做故障注入。
 * ## References
 * system_fs_make_io_exception()、ZrLib_Value_SetString()、ZrLib_Object_SetFieldCString()；完整窄故障链见本批 coverage 行。
 */

/**
 * @fn static TZrBool system_fs_make_io_exception(SZrState *state, const TZrChar *message, SZrTypeValue *outValue)
 * ## Summary
 * 创建 IOException，根住对象并尝试写入其 stacks、exception 与 message 字段。
 * ## Parameters
 * @param[in,out] state 当前 VM state。
 * @param[in] message 已准备好的 NUL 结尾错误消息；下游按 C 字符串处理。
 * @param[out] outValue 成功时接收指向新异常对象的 OBJECT typed value；失败时保持调用者原值。
 * ## Returns
 * @return 成功创建并发布对象时返回 ZR_TRUE；参数无效、实例/数组创建失败或根对象无法恢复时返回 ZR_FALSE。
 * ## Constraints and lifecycle
 * 实例建立后先临时根住 receiver；数组创建及字段写入后都从根槽重取 receiver。只有最终输出写入前的所有显式检查通过才返回 true；字段 setter 为 void，故 true 不证明每个字段均已成功写入。message 的短串缓存窄软失败风险由 system_fs_set_message_field() 记录。
 * ## References
 * ZrSystem_Fs_RaiseIOException()、ZrLib_Type_NewInstance()、ZrLib_Array_New()、system_fs_begin_rooted_object()、system_fs_set_message_field()。
 */