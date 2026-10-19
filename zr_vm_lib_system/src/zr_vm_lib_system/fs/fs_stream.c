//
// FileStream runtime and shared fd marshal helpers for zr.system.fs.
//

#include "fs/fs_internal.h"
#include "fs_resource_roots_internal.h"

#include "zr_vm_core/memory.h"
#include "zr_vm_core/exception.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/value.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

/* 私有 malloc 状态不受 VM GC 追踪；FileStream 的清理回调通过此字段找到并释放它。 */
static TZrBool system_fs_set_hidden_native_pointer(SZrState *state,
                                                   SZrObject *object,
                                                   const TZrChar *fieldName,
                                                   TZrPtr pointerValue) {
    SZrTypeValue value;
    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_FALSE;
    }
    ZrLib_Value_SetNativePointer(state, &value, pointerValue);
    ZrLib_Object_SetFieldCString(state, object, fieldName, &value);
    return ZR_TRUE;
}

/* 将流读取与 VM 值构造分开：成功后调用方拥有 malloc 缓冲，失败则在此释放。
 * count 为负时读到 EOF，0 不消费输入；此层不会关闭调用方的句柄。 */
static TZrBool system_fs_read_buffer_from_handle(TZrLibrary_File_Handle handle,
                                                 TZrInt64 count,
                                                 unsigned char **outBytes,
                                                 TZrSize *outSize) {
    unsigned char *bytes = ZR_NULL;
    TZrSize usedSize = 0;
    TZrSize capacity = 0;
    TZrBool success = ZR_FALSE;

    if (outBytes != ZR_NULL) {
        *outBytes = ZR_NULL;
    }
    if (outSize != ZR_NULL) {
        *outSize = 0;
    }
    if (outBytes == ZR_NULL || outSize == ZR_NULL) {
        errno = EINVAL;
        return ZR_FALSE;
    }

    capacity = (count >= 0 && count < 4096) ? (TZrSize)count : 4096U;
    if (capacity == 0) {
        capacity = 1;
    }

    bytes = (unsigned char *)malloc(capacity);
    if (bytes == ZR_NULL) {
        return ZR_FALSE;
    }

    for (;;) {
        TZrSize readSize = 0;
        TZrSize chunkSize = 4096U;
        unsigned char chunk[4096];
        if (count >= 0) {
            TZrInt64 remaining = count - (TZrInt64)usedSize;
            if (remaining <= 0) {
                break;
            }
            if ((TZrSize)remaining < chunkSize) {
                chunkSize = (TZrSize)remaining;
            }
        }

        if (!ZrLibrary_File_ReadHandle(handle, chunk, chunkSize, &readSize)) {
            goto cleanup;
        }
        if (readSize == 0) {
            break;
        }
        if (usedSize + readSize > capacity) {
            TZrSize newCapacity = capacity;
            unsigned char *newBytes;
            while (newCapacity < usedSize + readSize) {
                newCapacity *= 2U;
            }
            newBytes = (unsigned char *)realloc(bytes, newCapacity);
            if (newBytes == ZR_NULL) {
                goto cleanup;
            }
            bytes = newBytes;
            capacity = newCapacity;
        }
        memcpy(bytes + usedSize, chunk, readSize);
        usedSize += readSize;
    }

    *outBytes = bytes;
    *outSize = usedSize;
    success = ZR_TRUE;

cleanup:
    if (!success) {
        free(bytes);
    }
    return success;
}

/* 宿主 write 允许短写；只有全部写入才报告成功，失败时已写部分无法回滚。 */
static TZrBool system_fs_write_all(TZrLibrary_File_Handle handle, const unsigned char *bytes, TZrSize size) {
    TZrSize written = 0;

    while (written < size) {
        TZrSize step = 0;
        if (!ZrLibrary_File_WriteHandle(handle, bytes + written, size - written, &step)) {
            return ZR_FALSE;
        }
        if (step == 0) {
            errno = EIO;
            return ZR_FALSE;
        }
        written += step;
    }

    return ZR_TRUE;
}

/* Caller-owned state survives Throw; only a published hidden owner may finalize data. */
typedef struct ZrSystemFsStreamConstruction {
    const TZrChar *path;
    const SZrLibrary_File_StreamOpenResult *openResult;
    ZrLibTempValueRoot root;
    TZrBool rootStarted;
    TZrBool transferred;
    ZrSystemFsStreamData *data;
    SZrObject *result;
} ZrSystemFsStreamConstruction;

static SZrObject *system_fs_stream_construction_object(ZrSystemFsStreamConstruction *context) {
    SZrTypeValue *value = ZrLib_TempValueRoot_Value(&context->root);
    return value != ZR_NULL ? (SZrObject *)ZrCore_Value_GetRawObject(value) : ZR_NULL;
}

static void system_fs_construct_stream(SZrState *state, TZrPtr arguments) {
    ZrSystemFsStreamConstruction *context = arguments;
    SZrObject *object;
    if (!ZrLib_TempValueRoot_Begin(state, &context->root)) {
        return;
    }
    context->rootStarted = ZR_TRUE;
    object = ZrLib_Type_NewInstance(state, "FileStream");
    if (object == ZR_NULL || !ZrLib_TempValueRoot_SetObject(&context->root, object, ZR_VALUE_TYPE_OBJECT)) {
        return;
    }
    context->data = (ZrSystemFsStreamData *)malloc(sizeof(*context->data));
    if (context->data == ZR_NULL) {
        return;
    }
    memset(context->data, 0, sizeof(*context->data));
    context->data->handle = context->openResult->handle;
    context->data->readable = context->openResult->readable;
    context->data->writable = context->openResult->writable;
    context->data->append = context->openResult->append;
    object->super.scanMarkGcFunction = ZrSystem_Fs_StreamFinalize;
    system_fs_set_hidden_native_pointer(state, object, ZR_SYSTEM_FS_HIDDEN_STREAM_FIELD, context->data);
    /* No fallible work between field publication and ownership bookkeeping. */
    context->transferred = ZR_TRUE;
    object = system_fs_stream_construction_object(context);
    ZrSystem_Fs_WriteIntField(state, object, ZR_SYSTEM_FS_HIDDEN_HANDLE_ID_FIELD, context->openResult->handle);
    object = system_fs_stream_construction_object(context);
    ZrSystem_Fs_WriteStringField(state, object, "path", context->path);
    object = system_fs_stream_construction_object(context);
    ZrSystem_Fs_WriteStringField(state, object, "mode", context->openResult->normalizedMode);
    object = system_fs_stream_construction_object(context);
    ZrSystem_Fs_WriteBoolField(state, object, "closed", ZR_FALSE);
    object = system_fs_stream_construction_object(context);
    ZrSystem_Fs_StreamSyncFields(state, object, context->data);
    context->result = system_fs_stream_construction_object(context);
}

/* Return captured status so File.open can close its untransferred handle before Throw. */
SZrObject *ZrSystem_Fs_NewStreamObject(SZrState *state,
                                       const TZrChar *path,
                                       const SZrLibrary_File_StreamOpenResult *openResult,
                                       TZrBool *outHandleTransferred,
                                       EZrThreadStatus *outStatus) {
    ZrSystemFsStreamConstruction context;
    EZrThreadStatus status;
    if (outHandleTransferred != ZR_NULL) *outHandleTransferred = ZR_FALSE;
    if (outStatus != ZR_NULL) *outStatus = ZR_THREAD_STATUS_FINE;
    if (state == ZR_NULL || path == ZR_NULL || openResult == ZR_NULL ||
        outHandleTransferred == ZR_NULL || outStatus == ZR_NULL) {
        return ZR_NULL;
    }
    memset(&context, 0, sizeof(context));
    context.path = path;
    context.openResult = openResult;
    status = ZrCore_Exception_TryRun(state, system_fs_construct_stream, &context);
    *outHandleTransferred = context.transferred;
    *outStatus = status;
    if (!context.transferred) free(context.data);
    if (context.rootStarted) ZrLib_TempValueRoot_End(&context.root);
    return status == ZR_THREAD_STATUS_FINE ? context.result : ZR_NULL;
}

ZrSystemFsStreamData *ZrSystem_Fs_GetStreamData(SZrState *state, SZrObject *object) {
    const SZrTypeValue *value = ZrSystem_Fs_GetFieldValue(state, object, ZR_SYSTEM_FS_HIDDEN_STREAM_FIELD);
    if (value == ZR_NULL || value->type != ZR_VALUE_TYPE_NATIVE_POINTER) {
        return ZR_NULL;
    }
    return (ZrSystemFsStreamData *)value->value.nativeObject.nativePointer;
}

/* 可读/可写检查之前统一拒绝已关闭资源，避免脚本继续使用复用后的平台句柄号。 */
TZrBool ZrSystem_Fs_StreamEnsureOpen(SZrState *state,
                                     SZrObject *object,
                                     ZrSystemFsStreamData **outData) {
    ZrSystemFsStreamData *data = ZrSystem_Fs_GetStreamData(state, object);
    if (outData != ZR_NULL) {
        *outData = data;
    }
    if (data == ZR_NULL || data->closed || data->handle == ZR_LIBRARY_FILE_INVALID_HANDLE) {
        return ZrSystem_Fs_RaiseIOException(state, "FileStream is closed");
    }
    return ZR_TRUE;
}

/* position/length 是最近一次流操作后的快照；关闭后保留最后快照供脚本检查。 */
TZrBool ZrSystem_Fs_StreamSyncFields(SZrState *state, SZrObject *object, ZrSystemFsStreamData *data) {
    TZrInt64 position = 0;
    TZrInt64 length = 0;

    if (state == ZR_NULL || object == ZR_NULL || data == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!data->closed && data->handle != ZR_LIBRARY_FILE_INVALID_HANDLE) {
        if (!ZrLibrary_File_GetHandlePosition(data->handle, &position) ||
            !ZrLibrary_File_GetHandleLength(data->handle, &length)) {
            return ZR_FALSE;
        }
    } else {
        position = ZrSystem_Fs_GetIntField(state, object, "position", 0);
        length = ZrSystem_Fs_GetIntField(state, object, "length", 0);
    }

    ZrSystem_Fs_WriteIntField(state, object, "position", position);
    ZrSystem_Fs_WriteIntField(state, object, "length", length);
    ZrSystem_Fs_WriteBoolField(state, object, "closed", data->closed ? ZR_TRUE : ZR_FALSE);
    return ZR_TRUE;
}

/* 显式 close 和 using 共用此幂等入口；GC finalizer 独立关闭并释放 native record，不调用本函数。 */
TZrBool ZrSystem_Fs_StreamCloseInternal(SZrState *state, SZrObject *object, ZrSystemFsStreamData *data) {
    if (state == ZR_NULL || object == ZR_NULL || data == ZR_NULL) {
        return ZR_FALSE;
    }
    if (data->closed) {
        ZrSystem_Fs_WriteBoolField(state, object, "closed", ZR_TRUE);
        ZrSystem_Fs_WriteIntField(state, object, ZR_SYSTEM_FS_HIDDEN_HANDLE_ID_FIELD, -1);
        return ZR_TRUE;
    }

    ZrSystem_Fs_StreamSyncFields(state, object, data);
    /* TODO: 平台 close 失败后保留了有效 handle/closed=false，后续 finalizer 会再次 close。
     * 需核对 close 错误下句柄是否已被 OS 释放，并以故障注入验证不会误关复用的句柄号；
     * ZrLibrary_File_CloseHandle 当前仅直接转发 _close/close。 */
    if (data->handle != ZR_LIBRARY_FILE_INVALID_HANDLE && !ZrLibrary_File_CloseHandle(data->handle)) {
        return ZR_FALSE;
    }

    data->handle = ZR_LIBRARY_FILE_INVALID_HANDLE;
    data->closed = ZR_TRUE;
    ZrSystem_Fs_WriteBoolField(state, object, "closed", ZR_TRUE);
    ZrSystem_Fs_WriteIntField(state, object, ZR_SYSTEM_FS_HIDDEN_HANDLE_ID_FIELD, -1);
    return ZR_TRUE;
}

/* 对象释放时的兜底清理，不能向脚本返回错误；显式 close/using 负责可观察的失败诊断。 */
void ZrSystem_Fs_StreamFinalize(SZrState *state, SZrRawObject *rawObject) {
    SZrObject *object = ZR_CAST_OBJECT(state, rawObject);
    ZrSystemFsStreamData *data;
    if (object == ZR_NULL) {
        return;
    }

    data = ZrSystem_Fs_GetStreamData(state, object);
    if (data == ZR_NULL || data->finalized) {
        return;
    }
    data->finalized = ZR_TRUE;

    if (!data->closed && data->handle != ZR_LIBRARY_FILE_INVALID_HANDLE) {
        ZrLibrary_File_CloseHandle(data->handle);
        data->handle = ZR_LIBRARY_FILE_INVALID_HANDLE;
        data->closed = ZR_TRUE;
    }

    free(data);
    system_fs_set_hidden_native_pointer(state, object, ZR_SYSTEM_FS_HIDDEN_STREAM_FIELD, ZR_NULL);
}

/* Native bytes stay owned by the caller through VM array construction. */
typedef struct ZrSystemFsBytesConversion {
    unsigned char *bytes;
    TZrSize size;
    TZrLibrary_File_Handle handle;
    SZrTypeValue *result;
    ZrSystemFsResourceRoots roots;
    TZrBool success;
} ZrSystemFsBytesConversion;

static void system_fs_publish_bytes(SZrState *state, TZrPtr arguments) {
    ZrSystemFsBytesConversion *context = arguments;
    SZrObject *array = ZrLib_Array_New(state);
    if (array == ZR_NULL) return;
    context->roots.objects[0] = ZR_CAST_RAW_OBJECT_AS_SUPER(array);
    for (TZrSize index = 0; index < context->size; ++index) {
        SZrTypeValue value;
        ZrLib_Value_SetInt(state, &value, (TZrInt64)context->bytes[index]);
        array = system_fs_resource_object(state, &context->roots, 0u);
        if (!ZrLib_Array_PushValue(state, array, &value)) return;
    }
    array = system_fs_resource_object(state, &context->roots, 0u);
    ZrLib_Value_SetObject(state, context->result, array, ZR_VALUE_TYPE_ARRAY);
    context->success = ZR_TRUE;
}

TZrBool ZrSystem_Fs_ReadBytesFromHandle(SZrState *state,
                                        TZrLibrary_File_Handle handle,
                                        TZrInt64 count,
                                        SZrTypeValue *result,
                                        ZR_OUT TZrInt64 *outReadCount) {
    ZrSystemFsBytesConversion context = {0};
    if (outReadCount != ZR_NULL) *outReadCount = 0;
    if (state == ZR_NULL || result == ZR_NULL) {
        errno = EINVAL;
        return ZR_FALSE;
    }
    if (!system_fs_read_buffer_from_handle(handle, count, &context.bytes, &context.size)) {
        return ZR_FALSE;
    }
    if (!system_fs_resource_roots_begin(state, &context.roots)) {
        free(context.bytes);
        return ZR_FALSE;
    }
    context.result = result;
    EZrThreadStatus status = ZrCore_Exception_TryRun(state, system_fs_publish_bytes, &context);
    int savedErrno = errno;
    free(context.bytes);
    system_fs_resource_roots_end(state, &context.roots);
    if (status != ZR_THREAD_STATUS_FINE) {
        ZrCore_Exception_Throw(state, status);
        return ZR_FALSE;
    }
    errno = savedErrno;
    if (context.success && outReadCount != ZR_NULL) *outReadCount = (TZrInt64)context.size;
    return context.success;
}

typedef struct ZrSystemFsTextPublication {
    TZrNativeString text;
    TZrSize size;
    SZrTypeValue *result;
} ZrSystemFsTextPublication;

static void system_fs_publish_text(SZrState *state, TZrPtr arguments) {
    ZrSystemFsTextPublication *context = arguments;
    SZrString *string = ZrCore_String_Create(state, context->text, context->size);
    if (string == ZR_NULL) {
        ZrCore_Exception_Throw(state, ZR_THREAD_STATUS_MEMORY_ERROR);
        return;
    }
    ZrLib_Value_SetStringObject(state, context->result, string);
}

TZrBool ZrSystem_Fs_ReadTextFromHandle(SZrState *state,
                                       TZrLibrary_File_Handle handle,
                                       TZrInt64 count,
                                       SZrTypeValue *result,
                                       ZR_OUT TZrInt64 *outReadCount) {
    unsigned char *bytes = ZR_NULL;
    TZrSize size = 0;
    TZrChar *text;

    if (outReadCount != ZR_NULL) {
        *outReadCount = 0;
    }
    if (state == ZR_NULL || result == ZR_NULL) {
        errno = EINVAL;
        return ZR_FALSE;
    }
    if (!system_fs_read_buffer_from_handle(handle, count, &bytes, &size)) {
        return ZR_FALSE;
    }

    text = (TZrChar *)malloc(size + 1);
    if (text == ZR_NULL) {
        free(bytes);
        return ZR_FALSE;
    }

    if (size > 0) {
        memcpy(text, bytes, size);
    }
    text[size] = '\0';
    free(bytes);
    /* Preserve byte count, and release caller-owned bytes before propagating Throw. */
    ZrSystemFsTextPublication publication = {text, size, result};
    EZrThreadStatus status = ZrCore_Exception_TryRun(state, system_fs_publish_text, &publication);
    free(text);
    if (status != ZR_THREAD_STATUS_FINE) {
        ZrCore_Exception_Throw(state, status);
        return ZR_FALSE;
    }
    if (outReadCount != ZR_NULL) {
        *outReadCount = (TZrInt64)size;
    }
    return ZR_TRUE;
}

/* Validate every byte before the first write. Array_Get may materialize raw
 * storage and throw; the caller still owns bytes during that conversion. */
static void system_fs_write_bytes(SZrState *state, TZrPtr arguments) {
    ZrSystemFsBytesConversion *context = arguments;
    for (TZrSize index = 0; index < context->size; ++index) {
        SZrObject *array = system_fs_resource_object(state, &context->roots, 0u);
        const SZrTypeValue *value = ZrLib_Array_Get(state, array, index);
        if (value == ZR_NULL ||
            (!ZR_VALUE_IS_TYPE_SIGNED_INT(value->type) && !ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type))) {
            errno = EINVAL;
            return;
        }
        TZrInt64 intValue = ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)
            ? value->value.nativeObject.nativeInt64 : (TZrInt64)value->value.nativeObject.nativeUInt64;
        if (intValue < 0 || intValue > 255) {
            errno = EINVAL;
            return;
        }
        context->bytes[index] = (unsigned char)intValue;
    }
    context->success = system_fs_write_all(context->handle, context->bytes, context->size);
}

TZrBool ZrSystem_Fs_WriteBytesToHandle(SZrState *state,
                                       TZrLibrary_File_Handle handle,
                                       SZrObject *array,
                                       ZR_OUT TZrInt64 *outWrittenCount) {
    ZrSystemFsBytesConversion context = {0};
    if (outWrittenCount != ZR_NULL) *outWrittenCount = 0;
    if (state == ZR_NULL || array == ZR_NULL) {
        errno = EINVAL;
        return ZR_FALSE;
    }
    if (!system_fs_resource_roots_begin(state, &context.roots)) return ZR_FALSE;
    context.roots.objects[0] = ZR_CAST_RAW_OBJECT_AS_SUPER(array);
    context.size = ZrLib_Array_Length(array);
    context.bytes = (unsigned char *)malloc(context.size > 0 ? context.size : 1);
    if (context.bytes == ZR_NULL) {
        system_fs_resource_roots_end(state, &context.roots);
        return ZR_FALSE;
    }
    context.handle = handle;
    EZrThreadStatus status = ZrCore_Exception_TryRun(state, system_fs_write_bytes, &context);
    int savedErrno = errno;
    free(context.bytes);
    system_fs_resource_roots_end(state, &context.roots);
    if (status != ZR_THREAD_STATUS_FINE) {
        ZrCore_Exception_Throw(state, status);
        return ZR_FALSE;
    }
    errno = savedErrno;
    if (context.success && outWrittenCount != ZR_NULL) *outWrittenCount = (TZrInt64)context.size;
    return context.success;
}

/* 文本写入使用 NUL 结尾的宿主字符串；需要保留任意二进制字节时使用 writeBytes。 */
TZrBool ZrSystem_Fs_WriteTextToHandle(SZrState *state,
                                      TZrLibrary_File_Handle handle,
                                      const TZrChar *text,
                                      ZR_OUT TZrInt64 *outWrittenCount) {
    TZrSize length;

    if (outWrittenCount != ZR_NULL) {
        *outWrittenCount = 0;
    }
    if (state == ZR_NULL || text == ZR_NULL) {
        errno = EINVAL;
        return ZR_FALSE;
    }

    length = strlen(text);
    if (!system_fs_write_all(handle, (const unsigned char *)text, length)) {
        return ZR_FALSE;
    }

    if (outWrittenCount != ZR_NULL) {
        *outWrittenCount = (TZrInt64)length;
    }
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_Stream_ReadBytes(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = ZrSystem_Fs_SelfObject(context);
    ZrSystemFsStreamData *data = ZR_NULL;
    TZrInt64 count = -1;
    if (self == ZR_NULL || result == ZR_NULL ||
        !ZrSystem_Fs_ReadOptionalIntArgument(context, 0, -1, &count) ||
        !ZrSystem_Fs_StreamEnsureOpen(context->state, self, &data)) {
        return ZR_FALSE;
    }
    if (!data->readable) {
        return ZrSystem_Fs_RaiseIOException(context->state, "FileStream mode '%s' is not readable",
                                            ZrSystem_Fs_GetStringField(context->state, self, "mode"));
    }
    if (!ZrSystem_Fs_ReadBytesFromHandle(context->state, data->handle, count, result, ZR_NULL) ||
        !ZrSystem_Fs_StreamSyncFields(context->state, self, data)) {
        return ZrSystem_Fs_RaiseErrnoIOException(context->state, "readBytes", ZrSystem_Fs_GetStringField(context->state, self, "path"));
    }
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_Stream_ReadText(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = ZrSystem_Fs_SelfObject(context);
    ZrSystemFsStreamData *data = ZR_NULL;
    TZrInt64 count = -1;
    if (self == ZR_NULL || result == ZR_NULL ||
        !ZrSystem_Fs_ReadOptionalIntArgument(context, 0, -1, &count) ||
        !ZrSystem_Fs_StreamEnsureOpen(context->state, self, &data)) {
        return ZR_FALSE;
    }
    if (!data->readable) {
        return ZrSystem_Fs_RaiseIOException(context->state, "FileStream mode '%s' is not readable",
                                            ZrSystem_Fs_GetStringField(context->state, self, "mode"));
    }
    /* TODO: count 以字节限制读取，合法 UTF-8 文件首字符如 é 用 readText(1) 会只读入 C3。
     * ReadTextFromHandle 不验证字符边界；需补跨字符边界用例，明确结果是否允许不完整 UTF-8。 */
    if (!ZrSystem_Fs_ReadTextFromHandle(context->state, data->handle, count, result, ZR_NULL) ||
        !ZrSystem_Fs_StreamSyncFields(context->state, self, data)) {
        return ZrSystem_Fs_RaiseErrnoIOException(context->state, "readText", ZrSystem_Fs_GetStringField(context->state, self, "path"));
    }
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_Stream_WriteBytes(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = ZrSystem_Fs_SelfObject(context);
    ZrSystemFsStreamData *data = ZR_NULL;
    SZrObject *array = ZR_NULL;
    TZrInt64 written = 0;
    if (self == ZR_NULL || result == ZR_NULL ||
        !ZrSystem_Fs_ReadArrayArgument(context, 0, &array) ||
        !ZrSystem_Fs_StreamEnsureOpen(context->state, self, &data)) {
        return ZR_FALSE;
    }
    if (!data->writable) {
        return ZrSystem_Fs_RaiseIOException(context->state, "FileStream mode '%s' is not writable",
                                            ZrSystem_Fs_GetStringField(context->state, self, "mode"));
    }
    if (!ZrSystem_Fs_WriteBytesToHandle(context->state, data->handle, array, &written) ||
        !ZrSystem_Fs_StreamSyncFields(context->state, self, data)) {
        return ZrSystem_Fs_RaiseErrnoIOException(context->state, "writeBytes", ZrSystem_Fs_GetStringField(context->state, self, "path"));
    }
    ZrLib_Value_SetInt(context->state, result, written);
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_Stream_WriteText(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = ZrSystem_Fs_SelfObject(context);
    ZrSystemFsStreamData *data = ZR_NULL;
    const TZrChar *text = ZR_NULL;
    TZrInt64 written = 0;
    if (self == ZR_NULL || result == ZR_NULL ||
        !ZrSystem_Fs_ReadStringArgument(context, 0, &text) ||
        !ZrSystem_Fs_StreamEnsureOpen(context->state, self, &data)) {
        return ZR_FALSE;
    }
    if (!data->writable) {
        return ZrSystem_Fs_RaiseIOException(context->state, "FileStream mode '%s' is not writable",
                                            ZrSystem_Fs_GetStringField(context->state, self, "mode"));
    }
    if (!ZrSystem_Fs_WriteTextToHandle(context->state, data->handle, text, &written) ||
        !ZrSystem_Fs_StreamSyncFields(context->state, self, data)) {
        return ZrSystem_Fs_RaiseErrnoIOException(context->state, "writeText", ZrSystem_Fs_GetStringField(context->state, self, "path"));
    }
    ZrLib_Value_SetInt(context->state, result, written);
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_Stream_Flush(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = ZrSystem_Fs_SelfObject(context);
    ZrSystemFsStreamData *data = ZR_NULL;
    if (self == ZR_NULL || result == ZR_NULL || !ZrSystem_Fs_StreamEnsureOpen(context->state, self, &data)) {
        return ZR_FALSE;
    }
    if (!ZrLibrary_File_FlushHandle(data->handle)) {
        return ZrSystem_Fs_RaiseErrnoIOException(context->state, "flush", ZrSystem_Fs_GetStringField(context->state, self, "path"));
    }
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_Stream_Seek(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = ZrSystem_Fs_SelfObject(context);
    ZrSystemFsStreamData *data = ZR_NULL;
    TZrInt64 offset = 0;
    TZrChar originBuffer[16];
    int origin = SEEK_SET;
    TZrInt64 position = 0;
    if (self == ZR_NULL || result == ZR_NULL ||
        !ZrLib_CallContext_ReadInt(context, 0, &offset) ||
        !ZrSystem_Fs_ReadOptionalStringArgument(context, 1, "begin", originBuffer, sizeof(originBuffer)) ||
        !ZrSystem_Fs_StreamEnsureOpen(context->state, self, &data)) {
        return ZR_FALSE;
    }

    if (strcmp(originBuffer, "begin") == 0) {
        origin = SEEK_SET;
    } else if (strcmp(originBuffer, "current") == 0) {
        origin = SEEK_CUR;
    } else if (strcmp(originBuffer, "end") == 0) {
        origin = SEEK_END;
    } else {
        return ZrSystem_Fs_RaiseIOException(context->state, "Invalid seek origin '%s'", originBuffer);
    }

    if (!ZrLibrary_File_SeekHandle(data->handle, offset, origin, &position) ||
        !ZrSystem_Fs_StreamSyncFields(context->state, self, data)) {
        return ZrSystem_Fs_RaiseErrnoIOException(context->state, "seek", ZrSystem_Fs_GetStringField(context->state, self, "path"));
    }
    ZrLib_Value_SetInt(context->state, result, position);
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_Stream_SetLength(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = ZrSystem_Fs_SelfObject(context);
    ZrSystemFsStreamData *data = ZR_NULL;
    TZrInt64 length = 0;
    TZrInt64 position = 0;
    if (self == ZR_NULL || result == ZR_NULL ||
        !ZrLib_CallContext_ReadInt(context, 0, &length) ||
        !ZrSystem_Fs_StreamEnsureOpen(context->state, self, &data)) {
        return ZR_FALSE;
    }
    if (!data->writable) {
        return ZrSystem_Fs_RaiseIOException(context->state, "FileStream mode '%s' is not writable",
                                            ZrSystem_Fs_GetStringField(context->state, self, "mode"));
    }
    if (!ZrLibrary_File_SetHandleLength(data->handle, length)) {
        return ZrSystem_Fs_RaiseErrnoIOException(context->state, "setLength", ZrSystem_Fs_GetStringField(context->state, self, "path"));
    }
    if (ZrLibrary_File_GetHandlePosition(data->handle, &position) && position > length) {
        ZrLibrary_File_SeekHandle(data->handle, length, SEEK_SET, &position);
    }
    if (!ZrSystem_Fs_StreamSyncFields(context->state, self, data)) {
        return ZrSystem_Fs_RaiseErrnoIOException(context->state, "setLength", ZrSystem_Fs_GetStringField(context->state, self, "path"));
    }
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

TZrBool ZrSystem_Fs_Stream_Close(ZrLibCallContext *context, SZrTypeValue *result) {
    SZrObject *self = ZrSystem_Fs_SelfObject(context);
    ZrSystemFsStreamData *data;
    if (self == ZR_NULL || result == ZR_NULL) {
        return ZR_FALSE;
    }
    data = ZrSystem_Fs_GetStreamData(context->state, self);
    if (data == ZR_NULL) {
        return ZR_FALSE;
    }
    if (!ZrSystem_Fs_StreamCloseInternal(context->state, self, data)) {
        return ZrSystem_Fs_RaiseErrnoIOException(context->state, "close", ZrSystem_Fs_GetStringField(context->state, self, "path"));
    }
    ZrLib_Value_SetNull(result);
    return ZR_TRUE;
}

/**
 * @fn static TZrBool system_fs_set_hidden_native_pointer(SZrState *state, SZrObject *object, const TZrChar *fieldName, TZrPtr pointerValue)
 * ## Summary
 * 将不由 VM GC 管理的 native 记录地址写入 wrapper 的隐藏字段。该 helper 只统一构造字段值和
 * 调用 void setter，供 `NewStreamObject` 建立 owner、`StreamFinalize` 清除 owner 使用。
 * ## Parameters
 * @param[in] state 当前有效 VM state；调用期间负责字段写入所需的运行时资源。
 * @param[in] object 要写字段的 VM 对象；其生命周期由调用方的临时根或 GC 回调保证。
 * @param[in] fieldName 非空 C 字符串字段名，借用至 setter 返回。
 * @param[in] pointerValue 要写入的外部地址；写入不转移 native 记录的释放责任。
 * ## Returns
 * @return 输入有效且字段 setter 被调用时返回 `ZR_TRUE`；返回值不证明 void setter 已完成字段写入。
 * ## Constraints and lifecycle
 * `ZrLib_Object_SetFieldCString` 不报告软分配或 pin 失败；因此 owner 写失败可能留下没有记录指针的
 * wrapper，调用方必须把该窗口作为资源交付风险处理。此函数不释放旧指针或新指针。
 * ## References
 * `ZrSystem_Fs_NewStreamObject` :160、`ZrSystem_Fs_StreamFinalize` :264、
 * `ZrLib_Value_SetNativePointer` 与 `ZrLib_Object_SetFieldCString`。
 */

/**
 * @fn static TZrBool system_fs_read_buffer_from_handle(TZrLibrary_File_Handle handle, TZrInt64 count, unsigned char **outBytes, TZrSize *outSize)
 * ## Summary
 * 从已打开的宿主句柄读取原始字节，统一供 byte 和 text 结果构造使用。
 * ## Parameters
 * @param[in] handle 当前仍归调用者所有且可读的宿主句柄；本 helper 不关闭它。
 * @param[in] count 非负时为最多读取的字节数，负值表示读取至 EOF，零值不消费输入。
 * @param[out] outBytes 成功时接收 malloc 缓冲区；所有权转给调用方，失败时写为 NULL。
 * @param[out] outSize 成功时接收实际字节数，失败或无数据时写为 0。
 * ## Returns
 * @return 完整读取循环成功（包括 EOF 或达到 count）返回 `ZR_TRUE`；参数、malloc、宿主读或 realloc
 * 失败返回 `ZR_FALSE` 并释放本层缓冲。TODO: 极大输入下 size_t 加法和容量倍增的上界仍需核对。
 * ## Constraints and lifecycle
 * 缓冲区只在成功返回后有效，调用方必须在构造 VM 值后 `free`；宿主短读会继续循环，部分已读数据在
 * 后续错误时不发布给调用方。该层不改变句柄关闭状态，跨线程读序由宿主负责。
 * ## References
 * `ZrSystem_Fs_ReadBytesFromHandle` :286、`ZrSystem_Fs_ReadTextFromHandle` :329、
 * `ZrLibrary_File_ReadHandle`。
 */

/**
 * @fn static TZrBool system_fs_write_all(TZrLibrary_File_Handle handle, const unsigned char *bytes, TZrSize size)
 * ## Summary
 * 处理宿主 API 的短写，直到指定字节全部写完或报告失败。
 * ## Parameters
 * @param[in] handle 当前可写且由上层保持开放的宿主句柄。
 * @param[in] bytes 非空或 size 为零的待写缓冲；只在调用期间借用。
 * @param[in] size 要交付的字节数。
 * ## Returns
 * @return 所有字节均已交给宿主返回 `ZR_TRUE`；宿主报错或返回零进度返回 `ZR_FALSE`。
 * ## Constraints and lifecycle
 * 已写前缀不能回滚；零进度被转成 `EIO`，不会关闭句柄。调用方据 false 抛 IOException，并不得把
 * false 当成文件保持原状。
 * ## References
 * `ZrSystem_Fs_WriteBytesToHandle` :398、`ZrSystem_Fs_WriteTextToHandle` :426、
 * `ZrLibrary_File_WriteHandle`。
 */

/**
 * @fn SZrObject *ZrSystem_Fs_NewStreamObject(SZrState *, const TZrChar *, const SZrLibrary_File_StreamOpenResult *, TZrBool *, EZrThreadStatus *)
 * @brief 为已打开句柄建立 wrapper；捕获异常供 File.open 完成句柄清理后传播。
 * @param[out] outHandleTransferred hidden owner 字段成功发布后为真，与最终返回对象是否为空无关。
 * @param[out] outStatus 局部 TryRun 捕获状态；普通失败为 FINE，不伪造 MemoryError。
 * @return 成功交付 wrapper；普通失败或捕获异常返回 NULL。
 * @note root/data/transfer 均属于 TryRun 调用方 context。未提交 data 在返回前释放，临时根必配对结束；
 * 已提交 data/handle 归 wrapper finalizer，调用方不得重复 free/close。path 同步借用。
 * @note 初始 StreamSyncFields 普通 false 仍沿用既有忽略行为；此独立问题不在本组证据范围。
 */

/**
 * @fn ZrSystemFsStreamData *ZrSystem_Fs_GetStreamData(SZrState *state, SZrObject *object)
 * ## Summary
 * 从 `FileStream` 隐藏字段取出 native stream record，供所有操作 callback 读取当前句柄状态。
 * ## Parameters
 * @param[in] state 有效 VM state，用于字段读取和对象转换。
 * @param[in] object 仍存活且应为 FileStream wrapper 的对象；返回值是其中记录的借用地址。
 * ## Returns
 * @return 字段是 native pointer 时返回 `ZrSystemFsStreamData *`；字段缺失、类型不符或对象无效返回 NULL。
 * ## Constraints and lifecycle
 * 返回地址指向 malloc record，本身不会被 GC 搬迁；有效期依赖 wrapper 仍持有 hidden owner 且尚未 finalized。
 * 活跃 callback 可跨 VM 分配使用它的前提是分派/lane 仍保活 wrapper；不得跨 callback、close 后操作或 finalizer
 * 保存该指针。owner 丢失时本函数无法恢复记录或句柄，调用方须把 NULL 视作无效路径。
 * ## References
 * `StreamEnsureOpen`、`StreamFinalize`、`Stream_Close` 及 `ZrSystem_Fs_GetFieldValue`。
 */

/**
 * @fn TZrBool ZrSystem_Fs_StreamEnsureOpen(SZrState *state, SZrObject *object, ZrSystemFsStreamData **outData)
 * ## Summary
 * 在读写、定位、刷新或调整长度前统一确认 wrapper 仍持有可用句柄。
 * ## Parameters
 * @param[in] state 有效 VM state；关闭错误需要在其中登记 IOException。
 * @param[in] object FileStream wrapper，借用至检查完成。
 * @param[out] outData 可选输出槽；接收查到的借用 record，即使 record 已 closed；无 owner 时为 NULL。
 * ## Returns
 * @return record 存在、未 closed 且 handle 非 invalid 时返回 `ZR_TRUE`；否则抛 closed IOException 并返回 `ZR_FALSE`。
 * ## Constraints and lifecycle
 * 不验证 readable/writable，权限由具体 callback 检查；不会持有额外根或延长 record 生命周期。record 是
 * malloc 地址，活跃 callback 可跨 VM 分配继续使用，但前提是 dispatcher/lane 持续保活 wrapper；不能跨 callback、
 * close 或 finalizer 留存。FileStream.close 直接读取 owner，不调用本函数。
 * ## References
 * `Stream_ReadBytes/ReadText/WriteBytes/WriteText/Flush/Seek/SetLength`、`ZrSystem_Fs_GetStreamData`、
 * `ZrSystem_Fs_RaiseIOException`；`Stream_Close` 不经此检查。
 */

/**
 * @fn TZrBool ZrSystem_Fs_StreamSyncFields(SZrState *state, SZrObject *object, ZrSystemFsStreamData *data)
 * ## Summary
 * 把宿主句柄的 position/length 以及 closed 状态尽力同步到公开 wrapper 字段。
 * ## Parameters
 * @param[in] state 有效 VM state；字段写入由 VM setter 管理。
 * @param[in] object FileStream wrapper，借用至字段更新完成。
 * @param[in] data 当前 record；开放句柄时用于查询宿主位置和长度。
 * ## Returns
 * @return 开放句柄的位置/长度查询成功，或关闭状态下成功读取旧快照时返回 `ZR_TRUE`；查询前置无效或宿主查询失败返回 `ZR_FALSE`。
 * ## Constraints and lifecycle
 * 关闭后使用对象字段作为最后快照；开放状态下任一 position/length 查询失败不会覆盖现有字段。三个字段
 * setter 为 void，`ZR_TRUE` 只表示查询阶段完成，不保证每个字段写入成功。读写/seek/resize 已产生的副作用不回滚。
 * ## References
 * `Stream_ReadBytes/ReadText/WriteBytes/WriteText/Seek/SetLength` 在底层操作后调用本函数；
 * `ZrSystem_Fs_GetIntField`、`WriteIntField`、`WriteBoolField`。
 */

/**
 * @fn TZrBool ZrSystem_Fs_StreamCloseInternal(SZrState *state, SZrObject *object, ZrSystemFsStreamData *data)
 * ## Summary
 * 执行显式 `close` callback 的幂等关闭路径；GC finalizer 使用独立的无错误返回清理逻辑。
 * ## Parameters
 * @param[in] state 有效 VM state，供快照同步和字段写入使用。
 * @param[in] object FileStream wrapper，借用至状态字段更新完成。
 * @param[in,out] data 当前 record；成功时 handle 变为 invalid、closed 置 true。
 * ## Returns
 * @return 已关闭或本次宿主 close 成功返回 `ZR_TRUE`；输入无效或宿主 close 失败返回 `ZR_FALSE`。
 * ## Constraints and lifecycle
 * close 前尝试同步快照但忽略其返回值；宿主失败时保留原 handle 与 `closed=false` 以允许重试，OS 是否已
 * 释放句柄仍取决于平台语义。成功后的字段写入是 void setter，不能保证公开字段已更新；本函数不由 finalizer 调用。
 * ## References
 * `ZrSystem_Fs_Stream_Close` :604、`ZrLibrary_File_CloseHandle`、`StreamFinalize` :257。
 */

/**
 * @fn void ZrSystem_Fs_StreamFinalize(SZrState *state, SZrRawObject *rawObject)
 * ## Summary
 * GC 对象 sweep 时的兜底资源回收回调，尝试关闭未关闭句柄并释放 malloc record。
 * ## Parameters
 * @param[in] state 当前 GC state；不得触发脚本可观察异常。
 * @param[in,out] rawObject 正在被 GC 处理的 raw object；其 hidden owner 只在对象仍可转换时读取。
 * ## Returns
 * 无返回值；close 和清除 owner 的失败不会向脚本报告。
 * ## Constraints and lifecycle
 * `NewStreamObject` 将本函数赋给 `scanMarkGcFunction`；Core GC 的对象 sweep 路径调用它。`finalized`
 * 先置位防止重复清理；`CloseHandle` 返回值被忽略，随后把 handle 置 invalid、closed 置 true 并 free record，
 * 故不能据私有状态证明宿主关闭成功。若 hidden owner setter 曾软失败，函数取不到 record，无法补救。
 * ## References
 * `gc_cycle.c:2892` 的 sweep 回收调用 `gc_object.c:1013`，后者派发对象 callback；另见
 * `NewStreamObject` :159、`GetStreamData` :251。
 */

/**
 * @fn TZrBool ZrSystem_Fs_ReadBytesFromHandle(SZrState *state, TZrLibrary_File_Handle handle, TZrInt64 count, SZrTypeValue *result, TZrInt64 *outReadCount)
 * ## Summary
 * 从句柄读取字节并构造 VM array；数组元素逐一保存 0..255 整数。
 * ## Parameters
 * @param[in] state 有效 VM state；数组分配和元素写入在其中进行。
 * @param[in] handle 可读宿主句柄，借用且不由本函数关闭。
 * @param[in] count 非负字节上限，负值读到 EOF。
 * @param[out] result 成功时写入新 array；失败时不保证结果槽内容。
 * @param[out] outReadCount 可选实际读取字节数；进入函数先清零，成功后写入完整读取量。
 * ## Returns
 * @return 缓冲读取、数组构造及所有 push 成功返回 `ZR_TRUE`；任一普通失败返回 `ZR_FALSE`。成功返回只表示
 * array 已完成构造，不代表此前输入位置可回滚。
 * ## Constraints and lifecycle
 * caller-owned context 在 Array_New/PushValue 异常后释放精确 native bytes，再按原 status 重抛。临时 AOT 根保留未发布数组，调用者仍拥有 handle；读取位置不回滚。成功后才发布 outReadCount。
 *
 * ## References
 * `Stream_ReadBytes` :449、`fs_entry.c` 的 `ReadBytes` aliases、`system_fs_read_buffer_from_handle`、`ZrLib_Array_PushValue`。
 */

/**
 * @fn TZrBool ZrSystem_Fs_ReadTextFromHandle(SZrState *, TZrLibrary_File_Handle, TZrInt64, SZrTypeValue *, TZrInt64 *)
 * @brief 按字节上限读取文本并使用实际 size 创建 VM String，保留内嵌 NUL。
 * @param[in] handle 借用句柄；不由本函数关闭。
 * @param[in] count 非负为字节上限，负值读到 EOF；不验证 UTF-8 边界。
 * @param[out] outReadCount 成功才写实际消费字节数；失败保持初始零。
 * @return 读取和发布成功为真；普通宿主分配/I/O失败为假，VM失败释放 text 后传播原异常。
 * @note 失败不回滚已消费句柄位置；一次性 file helper 的异常关闭不由本叶函数提供。
 * @note 本组实际回归覆盖 NUL 与正常文本，不授予 CRT malloc OOM 清理的独立动态信用。
 */

/**
 * @fn TZrBool ZrSystem_Fs_WriteBytesToHandle(SZrState *state, TZrLibrary_File_Handle handle, SZrObject *array, TZrInt64 *outWrittenCount)
 * ## Summary
 * 预校验 VM byte array 后将其完整写入宿主句柄。
 * ## Parameters
 * @param[in] state 有效 VM state，用于读取数组元素。
 * @param[in] handle 可写宿主句柄，借用且不由本函数关闭。
 * @param[in] array 元素必须是有符号/无符号整数且位于 0..255。
 * @param[out] outWrittenCount 可选写入字节数；进入函数清零，全部完成后写入长度。
 * ## Returns
 * @return 预校验和 write-all 成功返回 `ZR_TRUE`；元素类型/范围、原生 malloc 或宿主写失败返回 `ZR_FALSE`；VM 分配异常可非局部退出。
 * ## Constraints and lifecycle
 * 所有元素在首次写前复制并校验；Array_Get 的 RAW 物化异常经 caller-owned TryRun 返回后释放 native bytes，再传播原 status。临时 AOT 根保活并重取输入 array。handle 始终借用，不关闭；宿主部分写失败不回滚前缀。
 *
 * ## References
 * `Stream_WriteBytes` :492、`fs_entry.c` 的 bytes aliases、`system_fs_write_all`、`ZrLib_Array_Get`。
 */

/**
 * @fn TZrBool ZrSystem_Fs_WriteTextToHandle(SZrState *state, TZrLibrary_File_Handle handle, const TZrChar *text, TZrInt64 *outWrittenCount)
 * ## Summary
 * 将 C-string 文本写入宿主句柄并返回交付字节数。
 * ## Parameters
 * @param[in] state 有效 VM state；仅用于统一参数错误状态。
 * @param[in] handle 可写宿主句柄，借用且不由本函数关闭。
 * @param[in] text 非空 NUL 结尾字符串，借用至写循环完成。
 * @param[out] outWrittenCount 可选写入字节数；成功为 `strlen(text)`。
 * ## Returns
 * @return write-all 完成返回 `ZR_TRUE`；参数或宿主写失败返回 `ZR_FALSE`。
 * ## Constraints and lifecycle
 * 长度由 `strlen` 决定，嵌入 NUL 只写首 NUL 前缀；不验证 UTF-8。部分宿主写失败留下不可回滚前缀，句柄保持
 * 由上层 stream record 管理。
 * ## References
 * `Stream_WriteText` :514、`fs_entry.c` 的 text aliases、`system_fs_write_all`。
 */

/**
 * @fn TZrBool ZrSystem_Fs_Stream_ReadBytes(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * FileStream reader callback；读取当前字节位置的数据并返回 array。
 * ## Parameters
 * @param[in] context 注册 dispatcher 提供的有效 context；可选 count 为 VM 借用实参。
 * @param[out] result dispatcher 提供的可写 VM 结果槽。
 * ## Returns
 * @return 成功返回 `ZR_TRUE` 和 byte array；closed、不可读、I/O 或同步失败走 IOException 并返回 `ZR_FALSE`。
 * ## Constraints and lifecycle
 * count 缺省 -1，单位为字节；读取后同步 position/length，已消费字节和游标不回滚。接口 descriptor 与
 * FileStream descriptor 都绑定本 callback，arity 由 dispatcher 先检查。
 * ## References
 * `fs_registry.c:193-195,213-215`、`ReadBytesFromHandle`、`StreamEnsureOpen`、`StreamSyncFields`。
 */

/**
 * @fn TZrBool ZrSystem_Fs_Stream_ReadText(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * FileStream reader callback；按字节读取并尝试构造文本 VM string。
 * ## Parameters
 * @param[in] context 有效 native context；可选 count 借用至 callback 返回。
 * @param[out] result 可写 VM 结果槽。
 * ## Returns
 * @return 成功返回 `ZR_TRUE`；closed、不可读、读取或字段同步失败抛 IOException 并返回 `ZR_FALSE`。
 * ## Constraints and lifecycle
 * count 缺省 -1 且按字节计；保留内嵌 NUL，UTF-8 不验证，切断多字节边界仍待用例明确。I/O
 * 已消费后再同步公开字段，查询失败不回滚位置。
 * ReadTextFromHandle 按实际 byte count 创建 STRING；创建失败经释放文本缓冲后传播 MemoryError，已读字节不回滚。
 * ## References
 * `fs_registry.c:196-198,216-218`、`ReadTextFromHandle`、UTF-8 byte-count TODO（Stream_ReadText）。
 */

/**
 * @fn TZrBool ZrSystem_Fs_Stream_WriteBytes(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * FileStream writer callback；校验并写入 byte array，返回写入字节数。
 * ## Parameters
 * @param[in] context 有效 native context；索引 0 为 VM array 借用值。
 * @param[out] result 可写 VM 整数结果槽。
 * ## Returns
 * @return 完整写入并同步成功返回 `ZR_TRUE`；closed、不可写、类型/范围、I/O 或同步失败返回 `ZR_FALSE`/IOException。
 * ## Constraints and lifecycle
 * 底层先完成写入再同步 position/length；同步失败不撤销已写前缀。数组元素必须是 0..255 整数，descriptor
 * arity 为一个显式参数。
 * ## References
 * `fs_registry.c:202-204,219-221`、`WriteBytesToHandle`、`StreamEnsureOpen`、`StreamSyncFields`。
 */

/**
 * @fn TZrBool ZrSystem_Fs_Stream_WriteText(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * FileStream writer callback；按 C-string 语义写文本并返回实际字节数。
 * ## Parameters
 * @param[in] context 有效 native context；索引 0 为借用 VM string。
 * @param[out] result 可写 VM 整数结果槽。
 * ## Returns
 * @return 写入并同步成功返回 `ZR_TRUE` 和字节数；closed、不可写、类型或宿主失败返回 `ZR_FALSE`/IOException。
 * ## Constraints and lifecycle
 * `strlen` 计算长度，嵌入 NUL 只写首前缀，不保证 UTF-8；已写前缀与位置在同步失败时不回滚。
 * ## References
 * `fs_registry.c:205-207,222-224`、`WriteTextToHandle`、`StreamSyncFields`。
 */

/**
 * @fn TZrBool ZrSystem_Fs_Stream_Flush(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * FileStream writer callback；刷新当前拥有句柄的宿主缓冲。
 * ## Parameters
 * @param[in] context 有效 native context，无显式 VM 参数。
 * @param[out] result 成功时写入 null 的可写结果槽。
 * ## Returns
 * @return 宿主 flush 成功返回 `ZR_TRUE`；closed 或 flush 失败抛 IOException 并返回 `ZR_FALSE`。
 * ## Constraints and lifecycle
 * 只刷新 owner handle，不同步 position/length 字段、不关闭或转移句柄；当前测试脚本在 takeReader 前失败，
 * 没有 Flush 行为断言。
 * ## References
 * `fs_registry.c:208-210,225-226`、`ZrLibrary_File_FlushHandle`、`StreamEnsureOpen`。
 */

/**
 * @fn TZrBool ZrSystem_Fs_Stream_Seek(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * FileStream callback；按 begin/current/end 调整字节游标并返回绝对位置。
 * ## Parameters
 * @param[in] context 有效 native context；offset 必需 int，origin 可选 string。
 * @param[out] result 成功时写入绝对位置的 VM 整数槽。
 * ## Returns
 * @return 定位和字段同步成功返回 `ZR_TRUE`；closed、非法 origin、平台 seek 或同步失败返回 IOException/`ZR_FALSE`。
 * ## Constraints and lifecycle
 * origin 缺省 begin，偏移按字节计；定位后同步快照失败不回滚已改变的游标，类型错误由 ReadInt 非局部抛出。
 * ## References
 * `fs_registry.c:227-229`、`ReadOptionalStringArgument`、`ZrLibrary_File_SeekHandle`、`StreamSyncFields`。
 */

/**
 * @fn TZrBool ZrSystem_Fs_Stream_SetLength(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * FileStream callback；设置宿主文件字节长度并同步公开快照。
 * ## Parameters
 * @param[in] context 有效 native context；索引 0 为长度 int。
 * @param[out] result 成功时写入 null 的可写结果槽。
 * ## Returns
 * @return 宿主 resize 和后续 position/length 查询成功返回 `ZR_TRUE`；closed、不可写、平台或查询失败返回 IOException/`ZR_FALSE`。
 * ## Constraints and lifecycle
 * 负长度交给平台错误路径；若位置查询成功且游标超过新长度会尝试 seek 到末端，但该内部 seek 失败被忽略。
 * 截断/扩展及可能的 seek 副作用不因字段查询失败回滚；descriptor arity 为一个显式参数。
 * ## References
 * `fs_registry.c:230-232`、`ZrLibrary_File_SetHandleLength`、`SeekHandle`、`StreamSyncFields`。
 */

/**
 * @fn TZrBool ZrSystem_Fs_Stream_Close(ZrLibCallContext *context, SZrTypeValue *result)
 * ## Summary
 * FileStream close callback；显式关闭 owner handle，并作为 using 的 close meta callback。
 * ## Parameters
 * @param[in] context 有效 native context，无显式 VM 参数。
 * @param[out] result 成功时写入 null 的可写结果槽。
 * ## Returns
 * @return close 成功或已成功关闭时返回 `ZR_TRUE`；owner 缺失时普通返回 `ZR_FALSE`，宿主 close 失败经 IOException 返回 `ZR_FALSE`。
 * ## Constraints and lifecycle
 * 显式 FileStream.close method descriptor 为零 VM 实参；ZR_META_CLOSE descriptor 另计 close-state 参数，
 * receiver 由分派框架处理。两条入口共用此函数，成功后重复调用幂等。
 * 失败保持私有 closed=false/handle 以允许重试，但底层 OS 是否已释放未知；GC finalizer 是独立兜底路径，不能
 * 当作此函数的调用者。
 * ## References
 * `fs_registry.c:85-86,233-234`、`StreamCloseInternal`、`StreamFinalize`、`FileStream` using lowering。
 */

/**
 * @file fs_stream.c
 * ## Summary
 * 实现 FileStream 的 native owner、句柄快照、字节/文本读写和注册回调；共享 helper 负责把宿主 I/O
 * 结果转换为 VM array/string/整数值。
 * ## Constraints and lifecycle
 * `ZrSystemFsStreamData` 的宿主句柄由成功包装的 FileStream 独占；显式 close 与 using callback 走可报告
 * 错误的路径，GC 通过 `scanMarkGcFunction` 走无返回值的兜底路径。公开字段 setter 是 void，任何 true
 * 结果只代表本层流程完成，不保证所有快照字段已写入。文本使用 C-string 语义，二进制数据必须走 byte API。
 * ## References
 * `fs_internal.h` 的声明与字段约束、`fs_entry.c:394` 的 File_Open、`fs_registry.c:193-234` 的 descriptor
 * 绑定，以及 Core `gc_cycle.c:2892`→`gc_object.c:1013` 的对象 finalizer 派发。
 */
