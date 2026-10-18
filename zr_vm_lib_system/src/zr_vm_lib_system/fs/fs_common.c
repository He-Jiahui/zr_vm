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

/* 字段助手共同物化 FileSystemEntry、SystemFileInfo、FileStream；void setter 失败经 Throw 退出，不提供布尔成功状态。 */
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
/** 字符串创建失败经 SetString 的 MEMORY_ERROR Throw 退出；正常后才继续写字段，局部 fieldValue 不按旧软失败路径使用。 */
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

/* refresh 只更新 fileInfo，不重算 path/name/parent；字段提交失败可 Throw，普通成功才发布查询快照输出。 */
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
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:26。
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
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:19；zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:40；zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:909；zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1036。
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
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:26；zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:995；zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1063。
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
 * 本函数无返回值；无效 state/object 或建立根失败可普通返回，字符串/字段失败可 Throw；它不返回逐字段成功标志。
 * ## Constraints and lifecycle
 * 字符串创建后从根槽重取对象，普通末尾负责 End。有效 SetString 创建失败 Throw MEMORY_ERROR，void 字段 setter 失败也 Throw，不会按旧软失败假设继续读取未初始化槽；非局部退出可能跳过本函数的 End。
 * ## References
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:78；zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1315；zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1322；zr_vm_library/src/zr_vm_library/native_binding/native_binding_object_write.c:12；zr_vm_core/src/zr_vm_core/exception/exception.c:31。
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
 * 根住 receiver，并在数组创建/字段写入后重取。true 只说明显式检查通过且 outValue 已发布；message helper 的无状态普通提前返回不受读回验证。outValue 必须独立可写且不与输入或根存储重叠；普通 FALSE 保持原值。字段失败可能 Throw，不回滚既有字段，也不保证执行普通 End。
 * ## References
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:357；zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:26；zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:56；zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1315；zr_vm_library/src/zr_vm_library/native_binding/native_binding_object_write.c:12。
 */

/**
 * @fn void ZrSystem_Fs_WriteIntField(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrInt64 value)
 * ## Summary
 * 写入整数字段。构造/同步 FileSystemEntry、SystemFileInfo、FileStream 快照时使用。
 * ## Parameters
 * @param[in,out] state 当前 VM state，所属线程/调用者必须提供外部同步，不可并发共享可变 state。
 * @param[in,out] object 当前有效 VM 对象；不会转移所有权。
 * @param[in] fieldName NUL 结尾字段名，覆盖 getter/setter 同步使用。
 * @param[in] value 写入字段的有符号 64 位整数值；按值封装，不转移资源。
 * ## Returns
 * void；不提供可忽略的布尔失败状态。
 * ## Constraints and lifecycle
 * 无效 state/object/fieldName 普通返回；本地 typed value 初始化后交 void setter。底层 key 创建和字段提交可能分配并 Throw；本函数无自己的 receiver 根和跨字段事务，调用者负责对象有效性。
 * 仅同步调用；VM 对象/结果仍由 VM 和外层根管理。本函数不建立跨线程同步；宿主文件系统也不是不可变快照。
 * ## References
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:134。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:172。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:250。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:251。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:263。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:278。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_internal.h:64。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:423。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:435。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:455。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:459。
 */

/**
 * @fn void ZrSystem_Fs_WriteBoolField(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrBool value)
 * ## Summary
 * 写入布尔字段。同步 exists、kind、closed 等快照字段时使用。
 * ## Parameters
 * @param[in,out] state 当前 VM state，所属线程/调用者必须提供外部同步，不可并发共享可变 state。
 * @param[in,out] object 当前有效 VM 对象；不会转移所有权。
 * @param[in] fieldName NUL 结尾字段名，覆盖 getter/setter 同步使用。
 * @param[in] value 写入字段的 TZrBool 原值；直接封装，不归一化为 0/1。
 * ## Returns
 * void。
 * ## Constraints and lifecycle
 * NULL 参数普通返回；值设置器直接封装传入的 TZrBool，不额外归一化为 0/1，字段提交走可能 Throw 的 void setter。对象和字段名由调用方同步借用，不延长生存期，不保证与其他字段的原子更新。
 * 仅同步调用；VM 对象/结果仍由 VM 和外层根管理。本函数不建立跨线程同步；宿主文件系统也不是不可变快照。
 * ## References
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:143。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:178。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:252。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:262。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:277。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_internal.h:82。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:427。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:431。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:439。
 */

/**
 * @fn void ZrSystem_Fs_WriteStringField(SZrState *state, SZrObject *object, const TZrChar *fieldName, const TZrChar *value)
 * ## Summary
 * 物化字符串并写字段。需要把同步 C 字符串复制为 VM 字段时使用。
 * ## Parameters
 * @param[in,out] state 当前 VM state，所属线程/调用者必须提供外部同步，不可并发共享可变 state。
 * @param[in,out] object 当前有效 VM 对象；不会转移所有权。
 * @param[in] fieldName NUL 结尾字段名，覆盖 getter/setter 同步使用。
 * @param[in] value 同步读取的 NUL 结尾字符串；NULL 按空串处理，字符存储须覆盖字符串创建的读取窗口。
 * ## Returns
 * void；无效参数/建立根失败可普通返回，其他失败可能非局部退出。
 * ## Constraints and lifecycle
 * 先建立 receiver 根，NULL value 当空串；SetString 成功后重取 receiver 并提交字段，普通末尾 End。有效参数下字符串创建失败 Throw MEMORY_ERROR，因此旧未初始化 fieldValue 继续进入 setter 的 BUG 前提不成立；字段失败同样 Throw。输入字符存储须覆盖创建读取窗口，根 receiver 不自动根住该字符来源。
 * 仅同步调用；VM 对象/结果仍由 VM 和外层根管理。本函数不建立跨线程同步；宿主文件系统也不是不可变快照。
 * ## References
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:152。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:174。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:176。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_internal.h:99。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:420。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:443。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:447。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:451。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:539。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:544。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:549。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:554。
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1315。
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1322。
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_object_write.c:12。
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_object_write.c:15。
 * zr_vm_core/src/zr_vm_core/object/object_write.c:123。
 * zr_vm_core/src/zr_vm_core/object/object_write.c:138。
 * zr_vm_core/src/zr_vm_core/object/object_write.c:161。
 * zr_vm_core/src/zr_vm_core/exception/exception.c:31。
 */

/**
 * @fn void ZrSystem_Fs_WriteNullField(SZrState *state, SZrObject *object, const TZrChar *fieldName)
 * ## Summary
 * 写入 NULL 字段。parent 缺失等需要显式置空字段时使用。
 * ## Parameters
 * @param[in,out] state 当前 VM state，所属线程/调用者必须提供外部同步，不可并发共享可变 state。
 * @param[in,out] object 当前有效 VM 对象；不会转移所有权。
 * @param[in] fieldName NUL 结尾字段名，覆盖 getter/setter 同步使用。
 * ## Returns
 * void。
 * ## Constraints and lifecycle
 * 先初始化 NULL typed value；无效参数普通返回。字段提交可能分配/Throw；不拥有对象、字段名或调用者根，不承担跨字段回滚。
 * 仅同步调用；VM 对象/结果仍由 VM 和外层根管理。本函数不建立跨线程同步；宿主文件系统也不是不可变快照。
 * ## References
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:173。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_internal.h:115。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:562。
 */

/**
 * @fn void ZrSystem_Fs_WriteObjectField(SZrState *state, SZrObject *object, const TZrChar *fieldName, SZrObject *fieldObject, EZrValueType valueType)
 * ## Summary
 * 写入对象或数组字段。已有 VM 对象需要发布到另一个对象字段时使用。
 * ## Parameters
 * @param[in,out] state 当前 VM state，所属线程/调用者必须提供外部同步，不可并发共享可变 state。
 * @param[in,out] object 当前有效 VM 对象；不会转移所有权。
 * @param[in] fieldName NUL 结尾字段名，覆盖 getter/setter 同步使用。
 * @param[in] fieldObject 有效 VM 对象，调用方保证在发布前保活。
 * @param[in] valueType 与 fieldObject 实际种类一致的 VM 标签。
 * ## Returns
 * void。
 * ## Constraints and lifecycle
 * state/receiver/name/fieldObject 任一 NULL 普通返回；valueType 必须与 fieldObject 的实际表示相容，由调用者保证，不能用该入口表示 NULL。void setter 可能 Throw；对象仍由 VM 管理。
 * 仅同步调用；VM 对象/结果仍由 VM 和外层根管理。本函数不建立跨线程同步；宿主文件系统也不是不可变快照。
 * ## References
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:182。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_internal.h:133。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:560。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:569。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:617。
 */

/**
 * @fn const SZrTypeValue *ZrSystem_Fs_GetFieldValue(SZrState *state, SZrObject *object, const TZrChar *fieldName)
 * ## Summary
 * 借用字段 typed-value 槽。字段消费者需要同步检查类型和取值时使用。
 * ## Parameters
 * @param[in,out] state 当前 VM state，所属线程/调用者必须提供外部同步，不可并发共享可变 state。
 * @param[in] object 当前有效 VM 对象；不会转移所有权。
 * @param[in] fieldName NUL 结尾字段名，覆盖 getter/setter 同步使用。
 * ## Returns
 * @return 字段槽借用指针，缺失/无效时 NULL。
 * ## Constraints and lifecycle
 * NULL 参数返回 NULL；委托 Object_GetFieldCString，键物化可能分配并依赖 VM 异常边界。返回槽不复制、不新增根；不得跨字段表变更、释放或可使借用失效的 VM 操作保留。
 * 仅同步调用；VM 对象/结果仍由 VM 和外层根管理。本函数不建立跨线程同步；宿主文件系统也不是不可变快照。
 * ## References
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:195。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:210。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_internal.h:153。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:203。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:211。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:227。
 */

/**
 * @fn const TZrChar *ZrSystem_Fs_GetStringField(SZrState *state, SZrObject *object, const TZrChar *fieldName)
 * ## Summary
 * 借用字符串字段的 native view。路径查询和错误消息需要同步 C 字符串时使用。
 * ## Parameters
 * @param[in,out] state 当前 VM state，所属线程/调用者必须提供外部同步，不可并发共享可变 state。
 * @param[in] object 当前有效 VM 对象；不会转移所有权。
 * @param[in] fieldName NUL 结尾字段名，覆盖 getter/setter 同步使用。
 * ## Returns
 * @return 字符串内部 C 指针，缺失/类型不符时 NULL。
 * ## Constraints and lifecycle
 * 仅 STRING 且 object 非 NULL 才取 view。其底层是 VM 字符串存储，不是调用者 malloc 内存；必须由对象/值保活并在任何可能使借用失效的 VM 操作前消费或复制，getter 不建立持久根。
 * 仅同步调用；VM 对象/结果仍由 VM 和外层根管理。本函数不建立跨线程同步；宿主文件系统也不是不可变快照。
 * ## References
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:202。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:517。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:521。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:537。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:543。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:560。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:564。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:582。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:586。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:599。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:631。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:649。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:652。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:658。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:675。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_internal.h:169。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:33。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:603。
 */

/**
 * @fn TZrInt64 ZrSystem_Fs_GetIntField(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrInt64 defaultValue)
 * ## Summary
 * 读取整数字段或默认值。stream 无法查询宿主位置/长度而读取已有快照时使用。
 * ## Parameters
 * @param[in,out] state 当前 VM state，所属线程/调用者必须提供外部同步，不可并发共享可变 state。
 * @param[in] object 当前有效 VM 对象；不会转移所有权。
 * @param[in] fieldName NUL 结尾字段名，覆盖 getter/setter 同步使用。
 * @param[in] defaultValue 缺参或不匹配时使用的默认值，具体规则见返回与约束。
 * ## Returns
 * @return 整数值或默认值。
 * ## Constraints and lifecycle
 * 仅 signed/unsigned integer 标签；unsigned 到 signed 采用 C cast，不做范围诊断；其他类型/缺失返回 defaultValue。字段槽只同步读取，不借出所有权。
 * 仅同步调用；VM 对象/结果仍由 VM 和外层根管理。本函数不建立跨线程同步；宿主文件系统也不是不可变快照。
 * ## References
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:210。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:246。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:247。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_internal.h:190。
 */

/**
 * @fn TZrBool ZrSystem_Fs_GetBoolField(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrBool defaultValue)
 * ## Summary
 * 读取布尔字段或默认值。需要严格 BOOL 字段读取时使用；当前仓内未检出实调用。
 * ## Parameters
 * @param[in,out] state 当前 VM state，所属线程/调用者必须提供外部同步，不可并发共享可变 state。
 * @param[in] object 当前有效 VM 对象；不会转移所有权。
 * @param[in] fieldName NUL 结尾字段名，覆盖 getter/setter 同步使用。
 * @param[in] defaultValue 缺参或不匹配时使用的默认值，具体规则见返回与约束。
 * ## Returns
 * @return 规范化布尔值或 defaultValue。
 * ## Constraints and lifecycle
 * 只接受 BOOL，规范化为 TRUE/FALSE；缺失或其他类型返回默认值。没有实调用不能证明仓外宿主不用，也不能据此删除。
 * 仅同步调用；VM 对象/结果仍由 VM 和外层根管理。本函数不建立跨线程同步；宿主文件系统也不是不可变快照。
 * TODO: 核对旧 closed 字段读取设计及仓外宿主入口，决定是否保留当前无仓内调用的 helper；未据搜索判死代码。
 * ## References
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:226。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_internal.h:207。
 */

/**
 * @fn SZrObject *ZrSystem_Fs_SelfObject(const ZrLibCallContext *context)
 * ## Summary
 * 取得 native callback 的 self 对象。File/Folder/Stream 实例方法入口检查 receiver 时使用。
 * ## Parameters
 * @param[in] context 当前同步 native 调用上下文，不在返回后保留。
 * ## Returns
 * @return self OBJECT 视图或 NULL。
 * ## Constraints and lifecycle
 * context 无效或 self 不是非空 OBJECT 返回 NULL；不接受 ARRAY。返回借用 context/state 的对象视图，不新增根，调用方负责后续分配与使用顺序。
 * 仅同步调用；VM 对象/结果仍由 VM 和外层根管理。本函数不建立跨线程同步；宿主文件系统也不是不可变快照。
 * ## References
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:234。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:507。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:527。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:549。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:571。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:593。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:606。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:638。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:665。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_internal.h:221。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_file_io.c:139。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_file_io.c:152。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_file_io.c:170。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_file_io.c:188。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_file_io.c:201。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_file_io.c:219。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:167。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:178。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:191。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:222。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:239。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:262。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:284。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:298。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:316。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:335。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:354。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_directory_listing.c:77。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_directory_listing.c:105。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:256。
 */

/**
 * @fn SZrObject *ZrSystem_Fs_ResolveConstructTarget(ZrLibCallContext *context)
 * ## Summary
 * 选择构造应填充的实例。File/Folder native construct 开始填充字段之前使用。
 * ## Parameters
 * @param[in] context 当前同步 native 调用上下文，不在返回后保留。
 * ## Returns
 * @return 复用/新建对象，创建失败时 NULL。
 * ## Constraints and lifecycle
 * state/context 无效返回 NULL；self 属于 owner prototype 时复用，否则优先 construct target prototype，再回退 owner prototype 创建实例。可能分配/Throw；创建对象归 VM，调用者接续 Populate 和结果发布。
 * 仅同步调用；VM 对象/结果仍由 VM 和外层根管理。本函数不建立跨线程同步；宿主文件系统也不是不可变快照。
 * ## References
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:247。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_internal.h:235。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:159。
 */

/**
 * @fn TZrBool ZrSystem_Fs_FinishObjectResult(SZrState *state, SZrTypeValue *result, SZrObject *object)
 * ## Summary
 * 发布 OBJECT callback 结果。构造、复制/移动和新 entry 返回前使用。
 * ## Parameters
 * @param[in,out] state 当前 VM state，所属线程/调用者必须提供外部同步，不可并发共享可变 state。
 * @param[out] result 独立可写 typed-value 结果槽；无效输入时不修改。
 * @param[in,out] object 当前有效 VM 对象；不会转移所有权。
 * ## Returns
 * @return 已填入 OBJECT 返回 TRUE，否则 FALSE。
 * ## Constraints and lifecycle
 * state/result/object 任一 NULL 返回 FALSE，result 保持原值；成功仅写 typed result，不分配、不新增长期根。result 必须是独立可写有效槽，VM/调用栈负责发布后对象生命周期。
 * 仅同步调用；VM 对象/结果仍由 VM 和外层根管理。本函数不建立跨线程同步；宿主文件系统也不是不可变快照。
 * ## References
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:269。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_internal.h:251。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:163。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:218。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:257。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:280。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:331。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:350。
 */

/**
 * @fn TZrBool ZrSystem_Fs_ReadStringArgument(const ZrLibCallContext *context, TZrSize index, const TZrChar **outText)
 * ## Summary
 * 读取并借出字符串参数。callback 必需字符串参数转为宿主 API 输入前使用。
 * ## Parameters
 * @param[in] context 当前同步 native 调用上下文，不在返回后保留。
 * @param[in] index 从零开始的实参数组下标。
 * @param[out] outText 独立可写指针槽，进入时先置 NULL。
 * ## Returns
 * @return 非空 native 字符串视图返回 TRUE，否则 FALSE。
 * ## Constraints and lifecycle
 * 可写 outText 先清 NULL；context/out 无效或读取失败返回 FALSE；下层缺参/错类型可抛 VM 错误。成功仅借用参数字符串内部存储，不能跨可能使其失效的 VM 操作保留；输出槽须独立。
 * 仅同步调用；VM 对象/结果仍由 VM 和外层根管理。本函数不建立跨线程同步；宿主文件系统也不是不可变快照。
 * ## References
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:277。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:576。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_internal.h:267。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_file_io.c:97。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_file_io.c:111。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_file_io.c:112。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_file_io.c:127。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_file_io.c:128。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_file_io.c:158。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_file_io.c:176。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:51。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:60。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:69。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:78。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:88。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:100。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:113。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:136。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:156。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:246。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:269。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:323。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:342。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_directory_listing.c:113。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:301。
 */

/**
 * @fn TZrBool ZrSystem_Fs_ReadOptionalStringArgument(const ZrLibCallContext *context, TZrSize index, const TZrChar *defaultValue, ZR_OUT TZrChar *buffer, TZrSize bufferSize)
 * ## Summary
 * 复制可选字符串到调用者缓冲。mode、seek origin 等短参数要在 VM 调用后继续使用时使用。
 * ## Parameters
 * @param[in] context 当前同步 native 调用上下文，不在返回后保留。
 * @param[in] index 从零开始的实参数组下标。
 * @param[in] defaultValue 缺参或不匹配时使用的默认值，具体规则见返回与约束。
 * @param[out] buffer 独立缓冲区，容量须包含终止 NUL。
 * @param[in] bufferSize buffer 的可写字节容量。
 * ## Returns
 * @return 完整含 NUL 拷贝成功 TRUE；参数/类型/容量失败 FALSE 或下层 Throw。
 * ## Constraints and lifecycle
 * 缺参/context NULL 取 defaultValue，NULL 默认当空串；有参委托必需字符串读取。buffer 非空且容量含 NUL；容量不足时 FALSE 且不写缓冲；memcpy 要求输入输出不重叠。成功拷贝不转移源所有权。
 * 仅同步调用；VM 对象/结果仍由 VM 和外层根管理。本函数不建立跨线程同步；宿主文件系统也不是不可变快照。
 * ## References
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:291。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:614。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_internal.h:285。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:200。
 */

/**
 * @fn TZrBool ZrSystem_Fs_ReadOptionalBoolArgument(const ZrLibCallContext *context, TZrSize index, TZrBool defaultValue, TZrBool *outValue)
 * ## Summary
 * 读取可选布尔参数。recursive、overwrite 等 callback 开关读取时使用。
 * ## Parameters
 * @param[in] context 当前同步 native 调用上下文，不在返回后保留。
 * @param[in] index 从零开始的实参数组下标。
 * @param[in] defaultValue 缺参或不匹配时使用的默认值，具体规则见返回与约束。
 * @param[out] outValue 独立可写结果槽；具体缺省/失败写入规则见约束。
 * ## Returns
 * @return 接受缺省或有效 BOOL 返回 TRUE，否则 FALSE。
 * ## Constraints and lifecycle
 * outValue NULL 返回 FALSE；其余先写 defaultValue。缺参/context NULL 为 TRUE；有参非 BOOL 普通 FALSE，保留默认输出，不自行设置 VM 异常。实际 BOOL 参数分支按零/非零写入 TRUE/FALSE；缺参时保持 defaultValue 原值，输出槽独立可写。
 * 仅同步调用；VM 对象/结果仍由 VM 和外层根管理。本函数不建立跨线程同步；宿主文件系统也不是不可变快照。
 * ## References
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:315。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_internal.h:306。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:227。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:247。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:270。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:303。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:324。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:343。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:359。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_directory_listing.c:114。
 */

/**
 * @fn TZrBool ZrSystem_Fs_ReadOptionalIntArgument(const ZrLibCallContext *context, TZrSize index, TZrInt64 defaultValue, TZrInt64 *outValue)
 * ## Summary
 * 读取可选整数参数。stream 可选 count 读取时使用。
 * ## Parameters
 * @param[in] context 当前同步 native 调用上下文，不在返回后保留。
 * @param[in] index 从零开始的实参数组下标。
 * @param[in] defaultValue 缺参或不匹配时使用的默认值，具体规则见返回与约束。
 * @param[out] outValue 独立可写结果槽；具体缺省/失败写入规则见约束。
 * ## Returns
 * @return 缺省或下层读取成功 TRUE；否则 FALSE 或下层 Throw。
 * ## Constraints and lifecycle
 * outValue NULL 返回 FALSE；先写默认值。缺参/context NULL 返回 TRUE；有参由 ReadInt 处理其整数/允许浮点转换、范围及类型异常，不能将它等同 GetIntField 的 strict integer-only 合同。
 * 仅同步调用；VM 对象/结果仍由 VM 和外层根管理。本函数不建立跨线程同步；宿主文件系统也不是不可变快照。
 * ## References
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:335。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:511。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:531。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_internal.h:326。
 */

/**
 * @fn TZrBool ZrSystem_Fs_ReadArrayArgument(const ZrLibCallContext *context, TZrSize index, SZrObject **outArray)
 * ## Summary
 * 读取并借出数组参数。read/write bytes 等 callback 检查 ARRAY 参数时使用。
 * ## Parameters
 * @param[in] context 当前同步 native 调用上下文，不在返回后保留。
 * @param[in] index 从零开始的实参数组下标。
 * @param[out] outArray 独立可写数组对象指针槽，进入时先置 NULL。
 * ## Returns
 * @return 下层接受 ARRAY 返回 TRUE，否则 FALSE/Throw。
 * ## Constraints and lifecycle
 * 非空 outArray 先清 NULL，NULL context/out 短路 FALSE；下层缺参/类型错误可能 Throw。返回对象借用 VM 参数，不延长寿命，不是调用者可 free 的 C 数组。
 * 仅同步调用；VM 对象/结果仍由 VM 和外层根管理。本函数不建立跨线程同步；宿主文件系统也不是不可变快照。
 * ## References
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:349。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:554。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_internal.h:345。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_file_io.c:207。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_file_io.c:225。
 */

/**
 * @fn TZrBool ZrSystem_Fs_RaiseIOException(SZrState *state, const TZrChar *format, ...)
 * ## Summary
 * 把格式化宿主失败转换为 IOException 状态。同步宿主操作失败后返回 native callback FALSE 前使用。
 * ## Parameters
 * @param[in,out] state 当前 VM state，所属线程/调用者必须提供外部同步，不可并发共享可变 state。
 * @param[in] format NUL 结尾 printf 格式串，与后续可变参数类型匹配。
 * ## Returns
 * @return 普通路径总返回 FALSE，同时保留规范化后的异常状态。
 * ## Constraints and lifecycle
 * state/format NULL 返回 FALSE；512 字节栈缓冲 vsnprintf 截断且不检查其返回值。构造异常失败或两级规范化失败用 Debug_RunError，可能非局部退出；通常设置 threadStatus 后返回 FALSE，不等同每次立即 Throw。format/变参需类型匹配且读到结束前有效，不回滚已完成文件操作。
 * 仅同步调用；VM 对象/结果仍由 VM 和外层根管理。本函数不建立跨线程同步；宿主文件系统也不是不可变快照。
 * 可变参数必须逐项符合 format；不由此函数取得参数存储所有权。
 * ## References
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:357。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:226。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:516。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:536。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:559。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:581。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:626。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:648。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_internal.h:360。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:391。
 * zr_vm_core/src/zr_vm_core/exception/exception.c:31。
 * zr_vm_core/src/zr_vm_core/exception/exception_try_run.c:52。
 * tests/system/test_system_fs_module.c:1060。
 */

/**
 * @fn TZrBool ZrSystem_Fs_RaiseErrnoIOException(SZrState *state, const TZrChar *action, const TZrChar *path)
 * ## Summary
 * 读取当前 errno 并生成 IOException。宿主失败后、errno 被其他调用覆盖之前使用。
 * ## Parameters
 * @param[in,out] state 当前 VM state，所属线程/调用者必须提供外部同步，不可并发共享可变 state。
 * @param[in] action 同步动作名，NULL 使用 I/O。
 * @param[in] path 同步路径文本，NULL 使用回退文本。
 * ## Returns
 * @return 委托 RaiseIOException 的 FALSE；异常构造可能非局部退出。
 * ## Constraints and lifecycle
 * 进入 helper 才调用 strerror(errno)；action/path NULL 有回退，文本随后同步格式化。调用参数求值发生在进入之前；若 path 通过 GetStringField 获取，其键物化可能分配，是否实际覆盖 errno 须另核，不能承诺捕获的是宿主失败值。strerror 缓冲不归本函数。
 * 仅同步调用；VM 对象/结果仍由 VM 和外层根管理。本函数不建立跨线程同步；宿主文件系统也不是不可变快照。
 * TODO: 在 fs_stream 的 path getter 参数求值链核实键分配/errno 影响；未复现 errno 被覆盖，不升级 BUG。
 * ## References
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:389。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:521。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:543。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:564。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:586。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:599。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:631。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:652。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:658。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_stream.c:675。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_internal.h:376。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_file_io.c:101。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_file_io.c:116。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_file_io.c:132。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_file_io.c:145。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_file_io.c:162。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_file_io.c:180。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_file_io.c:194。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_file_io.c:211。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_file_io.c:229。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:92。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:104。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:119。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:123。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:126。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:205。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:231。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:251。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:274。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:290。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:308。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:328。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:347。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:363。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_directory_listing.c:85。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_directory_listing.c:118。
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1387。
 * zr_vm_library/src/zr_vm_library/native_binding/native_binding_dispatch.c:1404。
 */

/**
 * @fn SZrObject *ZrSystem_Fs_MakeInfoObject(SZrState *state, const SZrLibrary_File_Info *info)
 * ## Summary
 * 把宿主 File_Info 物化为 SystemFileInfo。QueryInfo 成功后为脚本对象建立快照时使用。
 * ## Parameters
 * @param[in,out] state 当前 VM state，所属线程/调用者必须提供外部同步，不可并发共享可变 state。
 * @param[in] info 调用期间有效的宿主快照及内嵌字符数组。
 * ## Returns
 * @return 已创建对象视图或显式失败 NULL。
 * ## Constraints and lifecycle
 * state/info NULL 返回 NULL；新 receiver 根住后逐字段写 path、size、kind、时间、exists、名称/扩展/父路径，每次可能分配后重取。普通结束 End 后返回 VM 对象；不是宿主 Info 指针借出。字段 helper 的普通根失败无状态，因此返回非 NULL 不证明所有字段读回完整；Throw 不回滚字段。info 及其内嵌字符数组覆盖整个同步调用。
 * 仅同步调用；VM 对象/结果仍由 VM 和外层根管理。本函数不建立跨线程同步；宿主文件系统也不是不可变快照。
 * ## References
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:399。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_internal.h:391。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:143。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:513。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:612。
 * tests/system/test_system_fs_module.c:625。
 * tests/system/test_system_fs_module.c:733。
 */

/**
 * @fn SZrObject *ZrSystem_Fs_NewEntryObject(SZrState *state, const TZrChar *typeName, const TZrChar *originalPath, const TZrChar *fullPathOverride)
 * ## Summary
 * 新建并填充 File/Folder entry。列举以及 Copy/Move 新结果对象包装时使用。
 * ## Parameters
 * @param[in,out] state 当前 VM state，所属线程/调用者必须提供外部同步，不可并发共享可变 state。
 * @param[in] typeName 待创建 VM 类型名称。
 * @param[in] originalPath 调用方原始路径拼写，须覆盖内部 VM 分配后的读取。
 * @param[in] fullPathOverride 可选查询路径，非 NULL 时优先；不转移字符存储所有权。
 * ## Returns
 * @return Populate 成功的对象，其他显式失败 NULL。
 * ## Constraints and lifecycle
 * 先按 typeName 建 VM 实例，再 Populate；state/typeName 无效或创建/填充显式失败返回 NULL。未发布对象由 VM 管理，不使用 C free。普通 OBJECT 当前原址晋升；输入路径仍须满足 Populate 跨 VM 分配借用约束，不能用对象地址稳定推论字符串稳定。
 * 仅同步调用；VM 对象/结果仍由 VM 和外层根管理。本函数不建立跨线程同步；宿主文件系统也不是不可变快照。
 * ## References
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:467。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_internal.h:428。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:256。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:279。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:330。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:349。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_directory_listing.c:28。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:523。
 * tests/system/test_system_fs_module.c:873。
 */

/**
 * @fn TZrBool ZrSystem_Fs_PopulateEntryObject(SZrState *state, SZrObject *object, const TZrChar *originalPath, const TZrChar *fullPathOverride)
 * ## Summary
 * 查询路径并逐字段填充 entry。construct 或新 entry 在发布前构造 path/fullPath/name/extension/parent/fileInfo 时使用。
 * ## Parameters
 * @param[in,out] state 当前 VM state，所属线程/调用者必须提供外部同步，不可并发共享可变 state。
 * @param[in,out] object 当前有效 VM 对象；不会转移所有权。
 * @param[in] originalPath 调用方原始路径拼写，须覆盖内部 VM 分配后的读取。
 * @param[in] fullPathOverride 可选查询路径，非 NULL 时优先；不转移字符存储所有权。
 * ## Returns
 * @return 完成显式步骤并发布 fileInfo 返回 TRUE，显式失败 FALSE；不证明所有字段读回/文件系统原子快照。
 * ## Constraints and lifecycle
 * pathSource 优先 fullPathOverride，否则 originalPath；先根 receiver 并查询宿主，再构造 info 和递归 parent。parent 构造返回 NULL 可以继续写 parent=NULL。普通 cleanup 按 parent、info、receiver 逆序 End；Throw 不保证走 cleanup，已写字段不回滚。originalPath 在分配后仍会读取；receiver 根不自动保活该字符串来源。
 * 仅同步调用；VM 对象/结果仍由 VM 和外层根管理。本函数不建立跨线程同步；宿主文件系统也不是不可变快照。
 * TODO: 逐 callback 核实 originalPath/fullPathOverride 的参数根与字符串存储 lane；在 VM 分配后仍读取 originalPath 的借用安全尚需证据。
 * ## References
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:486。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_internal.h:408。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:160。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:482。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:496。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:509。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:523。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:539。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:572。
 * tests/system/test_system_fs_module.c:733。
 */

/**
 * @fn TZrBool ZrSystem_Fs_RefreshEntryObject(SZrState *state, SZrObject *object, ZR_OUT SZrLibrary_File_Info *outInfo, ZR_OUT SZrObject **outInfoObject)
 * ## Summary
 * 重查 fullPath 并更新 fileInfo。文件操作完成后刷新脚本快照和可选宿主输出时使用。
 * ## Parameters
 * @param[in,out] state 当前 VM state，所属线程/调用者必须提供外部同步，不可并发共享可变 state。
 * @param[in,out] object 当前有效 VM 对象；不会转移所有权。
 * @param[out] outInfo 可选独立宿主快照槽，首先全零。
 * @param[out] outInfoObject 可选独立对象指针槽，首先 NULL。
 * ## Returns
 * @return 查询、创建和字段提交普通返回后 TRUE；显式失败 FALSE，异常可能非局部退出。
 * ## Constraints and lifecycle
 * 先清零非空 outInfo、清 NULL outInfoObject，再检查 state/object。输出必须独立可写且不与输入/各自重叠。借用 fullPath 后立即宿主查询，再 MakeInfo 和写 fileInfo；不重算 path/name/parent。普通 FALSE 保持已清输出；成功才复制 info/对象；setter 失败可 Throw，无文件操作或字段回滚。
 * 仅同步调用；VM 对象/结果仍由 VM 和外层根管理。本函数不建立跨线程同步；宿主文件系统也不是不可变快照。
 * ## References
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:584。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_internal.h:448。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_file_io.c:147。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_file_io.c:164。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_file_io.c:182。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_file_io.c:196。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_file_io.c:213。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_file_io.c:231。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:170。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:181。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:233。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:253。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:276。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:292。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:310。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_entry.c:365。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:592。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:603。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:608。
 * zr_vm_lib_system/src/zr_vm_lib_system/fs/fs_common.c:617。
 * tests/system/test_system_fs_module.c:733。
 */
