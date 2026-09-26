//
// FileStream runtime and shared fd marshal helpers for zr.system.fs.
//

#include "fs/fs_internal.h"

#include "zr_vm_core/memory.h"
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

/* 把 File.open 刚创建的句柄绑定为一个 owned wrapper，同时提供 FFI 所需的整数视图。 */
SZrObject *ZrSystem_Fs_NewStreamObject(SZrState *state,
                                       const TZrChar *path,
                                       const SZrLibrary_File_StreamOpenResult *openResult) {
    SZrObject *object;
    ZrSystemFsStreamData *data;
    ZrLibTempValueRoot root;

    if (state == ZR_NULL || path == ZR_NULL || openResult == ZR_NULL) {
        return ZR_NULL;
    }

    if (!ZrLib_TempValueRoot_Begin(state, &root)) {
        return ZR_NULL;
    }

    object = ZrLib_Type_NewInstance(state, "FileStream");
    data = (ZrSystemFsStreamData *)malloc(sizeof(*data));
    if (object == ZR_NULL || data == ZR_NULL) {
        free(data);
        ZrLib_TempValueRoot_End(&root);
        return ZR_NULL;
    }

    memset(data, 0, sizeof(*data));
    data->handle = openResult->handle;
    data->readable = openResult->readable;
    data->writable = openResult->writable;
    data->append = openResult->append;
    ZrLib_TempValueRoot_SetObject(&root, object, ZR_VALUE_TYPE_OBJECT);

    object->super.scanMarkGcFunction = ZrSystem_Fs_StreamFinalize;
    system_fs_set_hidden_native_pointer(state, object, ZR_SYSTEM_FS_HIDDEN_STREAM_FIELD, data);
    ZrSystem_Fs_WriteIntField(state, object, ZR_SYSTEM_FS_HIDDEN_HANDLE_ID_FIELD, openResult->handle);
    ZrSystem_Fs_WriteStringField(state, object, "path", path);
    ZrSystem_Fs_WriteStringField(state, object, "mode", openResult->normalizedMode);
    ZrSystem_Fs_WriteBoolField(state, object, "closed", ZR_FALSE);
    ZrSystem_Fs_StreamSyncFields(state, object, data);
    ZrLib_TempValueRoot_End(&root);
    return object;
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

/* 显式 close 和 using 共用此幂等入口；GC 最终只负责尚未关闭的句柄和 malloc 状态。 */
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

/* 二进制路径保留所有字节，数组中的元素固定为 0..255 整数。 */
TZrBool ZrSystem_Fs_ReadBytesFromHandle(SZrState *state,
                                        TZrLibrary_File_Handle handle,
                                        TZrInt64 count,
                                        SZrTypeValue *result,
                                        ZR_OUT TZrInt64 *outReadCount) {
    SZrObject *array;
    unsigned char *bytes = ZR_NULL;
    TZrSize size = 0;
    TZrSize index;

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

    array = ZrLib_Array_New(state);
    if (array == ZR_NULL) {
        free(bytes);
        return ZR_FALSE;
    }

    for (index = 0; index < size; index++) {
        SZrTypeValue value;
        ZrLib_Value_SetInt(state, &value, (TZrInt64)bytes[index]);
        if (!ZrLib_Array_PushValue(state, array, &value)) {
            free(bytes);
            return ZR_FALSE;
        }
    }

    free(bytes);
    ZrLib_Value_SetObject(state, result, array, ZR_VALUE_TYPE_ARRAY);
    if (outReadCount != ZR_NULL) {
        *outReadCount = (TZrInt64)size;
    }
    return ZR_TRUE;
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
    /* BUG: 含 U+0000 的文件经 File.readText/Stream.readText 读取时，句柄已消费全部 size 字节，
     * 但 SetString -> CreateTryHitCache 按 strlen 建串，会把 A\0B 返回为 A。
     * 该路径必须保留显式长度，才能让文本结果与已读取数据一致。 */
    ZrLib_Value_SetString(state, result, text);
    if (outReadCount != ZR_NULL) {
        *outReadCount = (TZrInt64)size;
    }
    free(text);
    return ZR_TRUE;
}

/* 先校验整组整数再写入，避免数组中较晚出现非法元素时已部分写出。 */
TZrBool ZrSystem_Fs_WriteBytesToHandle(SZrState *state,
                                       TZrLibrary_File_Handle handle,
                                       SZrObject *array,
                                       ZR_OUT TZrInt64 *outWrittenCount) {
    TZrSize length;
    unsigned char *bytes;
    TZrSize index;

    if (outWrittenCount != ZR_NULL) {
        *outWrittenCount = 0;
    }
    if (state == ZR_NULL || array == ZR_NULL) {
        errno = EINVAL;
        return ZR_FALSE;
    }

    length = ZrLib_Array_Length(array);
    bytes = (unsigned char *)malloc(length > 0 ? length : 1);
    if (bytes == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < length; index++) {
        const SZrTypeValue *value = ZrLib_Array_Get(state, array, index);
        TZrInt64 intValue = 0;
        if (value == ZR_NULL ||
            (!ZR_VALUE_IS_TYPE_SIGNED_INT(value->type) && !ZR_VALUE_IS_TYPE_UNSIGNED_INT(value->type))) {
            free(bytes);
            errno = EINVAL;
            return ZR_FALSE;
        }
        intValue = ZR_VALUE_IS_TYPE_SIGNED_INT(value->type)
                           ? value->value.nativeObject.nativeInt64
                           : (TZrInt64)value->value.nativeObject.nativeUInt64;
        if (intValue < 0 || intValue > 255) {
            free(bytes);
            errno = EINVAL;
            return ZR_FALSE;
        }
        bytes[index] = (unsigned char)intValue;
    }

    if (!system_fs_write_all(handle, bytes, length)) {
        free(bytes);
        return ZR_FALSE;
    }

    free(bytes);
    if (outWrittenCount != ZR_NULL) {
        *outWrittenCount = (TZrInt64)length;
    }
    return ZR_TRUE;
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
