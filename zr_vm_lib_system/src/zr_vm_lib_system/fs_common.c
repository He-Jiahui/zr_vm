//
// Shared helpers for zr.system.fs object/runtime implementation.
//

#include "fs_internal.h"

#include "zr_vm_core/debug.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"

#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

static EZrValueType system_fs_root_value_type(const SZrObject *object) {
    if (object != ZR_NULL && object->super.type == ZR_RAW_OBJECT_TYPE_ARRAY) {
        return ZR_VALUE_TYPE_ARRAY;
    }
    return ZR_VALUE_TYPE_OBJECT;
}

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

TZrBool ZrSystem_Fs_RaiseErrnoIOException(SZrState *state, const TZrChar *action, const TZrChar *path) {
    const TZrChar *detail = strerror(errno);
    return ZrSystem_Fs_RaiseIOException(state,
                                        "%s failed for '%s': %s",
                                        action != ZR_NULL ? action : "I/O",
                                        path != ZR_NULL ? path : "<path>",
                                        detail != ZR_NULL ? detail : "unknown error");
}

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
    return ZrSystem_Fs_PopulateEntryObject(state, object, originalPath, fullPathOverride) ? object : ZR_NULL;
}

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
