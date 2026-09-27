#ifndef ZR_VM_TESTS_MODULE_FIXTURE_SUPPORT_H
#define ZR_VM_TESTS_MODULE_FIXTURE_SUPPORT_H

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/function.h"
#include "zr_vm_core/io.h"
#include "zr_vm_core/object.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/value.h"
#include "zr_vm_parser.h"
#include "zr_vm_parser/writer.h"

/** @brief 将测试持有的源码或二进制缓冲区映射到模块导入路径；缓冲区必须活到读取结束。 */
typedef struct ZrTestsFixtureSource {
    const TZrChar *path;
    const TZrChar *source;
    const TZrByte *bytes;
    TZrSize length;
    TZrBool isBinary;
} ZrTestsFixtureSource;

#define ZR_TESTS_FIXTURE_SOURCE_TEXT(pathValue, sourceValue) \
    {                                                        \
            (pathValue),                                     \
            (sourceValue),                                   \
            ZR_NULL,                                         \
            0,                                               \
            ZR_FALSE,                                        \
    }

/** @brief 一次性 Io 读取游标；bytes 始终借用，reader 可由调用方置于栈或堆。 */
typedef struct ZrTestsFixtureReader {
    const TZrByte *bytes;
    TZrSize length;
    TZrBool consumed;
} ZrTestsFixtureReader;

static inline TZrBool ZrTests_Fixture_StringEqualsCString(SZrString *value, const TZrChar *expected) {
    const TZrChar *nativeString;

    if (value == ZR_NULL || expected == ZR_NULL) {
        return ZR_FALSE;
    }

    nativeString = ZrCore_String_GetNativeString(value);
    return nativeString != ZR_NULL && strcmp(nativeString, expected) == 0;
}

static inline const SZrTypeValue *ZrTests_Fixture_GetObjectFieldValue(SZrState *state,
                                                                      SZrObject *object,
                                                                      const TZrChar *fieldName) {
    SZrString *fieldNameString;
    SZrTypeValue key;

    if (state == ZR_NULL || object == ZR_NULL || fieldName == ZR_NULL) {
        return ZR_NULL;
    }

    fieldNameString = ZrCore_String_Create(state, (TZrNativeString)fieldName, strlen(fieldName));
    if (fieldNameString == ZR_NULL) {
        return ZR_NULL;
    }

    /* 对象字段键使用 VM 字符串；返回值仍由 object 持有，调用方不得释放。 */
    ZrCore_Value_InitAsRawObject(state, &key, ZR_CAST_RAW_OBJECT_AS_SUPER(fieldNameString));
    key.type = ZR_VALUE_TYPE_STRING;
    return ZrCore_Object_GetValue(state, object, &key);
}

static inline TZrSize ZrTests_Fixture_GetArrayLength(SZrObject *array) {
    if (array == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return 0;
    }

    return ZrCore_Object_SuperArrayLength(array);
}

static inline SZrObject *ZrTests_Fixture_GetArrayEntryObject(SZrState *state, SZrObject *array, TZrSize index) {
    SZrTypeValue key;
    const SZrTypeValue *entryValue;

    if (state == ZR_NULL || array == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsInt(state, &key, (TZrInt64)index);
    entryValue = ZrCore_Object_GetValue(state, array, &key);
    if (entryValue == ZR_NULL || entryValue->type != ZR_VALUE_TYPE_OBJECT || entryValue->value.object == ZR_NULL) {
        return ZR_NULL;
    }

    return ZR_CAST_OBJECT(state, entryValue->value.object);
}

static inline const SZrTypeValue *ZrTests_Fixture_GetArrayEntryValue(SZrState *state, SZrObject *array, TZrSize index) {
    SZrTypeValue key;

    if (state == ZR_NULL || array == ZR_NULL || array->internalType != ZR_OBJECT_INTERNAL_TYPE_ARRAY) {
        return ZR_NULL;
    }

    ZrCore_Value_InitAsInt(state, &key, (TZrInt64)index);
    return ZrCore_Object_GetValue(state, array, &key);
}

/** @brief 向 Io 一次性交付借用的 fixture 字节，后续读取返回 EOF。 */
static inline TZrBytePtr ZrTests_Fixture_ReaderRead(SZrState *state, TZrPtr customData, TZrSize *size) {
    ZrTestsFixtureReader *reader = (ZrTestsFixtureReader *)customData;

    ZR_UNUSED_PARAMETER(state);

    if (reader == ZR_NULL || size == ZR_NULL || reader->consumed) {
        if (size != ZR_NULL) {
            *size = 0;
        }
        return ZR_NULL;
    }

    reader->consumed = ZR_TRUE;
    *size = reader->length;
    return (TZrBytePtr)reader->bytes;
}

/** @brief 仅释放堆分配的 reader，不释放借用的字节；栈上 reader 不可传入。 */
static inline void ZrTests_Fixture_ReaderClose(SZrState *state, TZrPtr customData) {
    ZR_UNUSED_PARAMETER(state);

    if (customData != ZR_NULL) {
        free(customData);
    }
}

/** @brief 读取二进制 fixture；成功时返回需由调用方 free 的字节缓冲区。 */
static inline TZrByte *ZrTests_Fixture_ReadFileBytes(const TZrChar *path, TZrSize *outLength) {
    FILE *file;
    long fileSize;
    TZrByte *buffer;

    if (path == ZR_NULL || outLength == ZR_NULL) {
        return ZR_NULL;
    }

    file = fopen(path, "rb");
    if (file == ZR_NULL) {
        return ZR_NULL;
    }

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return ZR_NULL;
    }

    fileSize = ftell(file);
    if (fileSize < 0 || fseek(file, 0, SEEK_SET) != 0) {
        fclose(file);
        return ZR_NULL;
    }

    buffer = (TZrByte *)malloc((size_t)fileSize);
    if (buffer == ZR_NULL) {
        fclose(file);
        return ZR_NULL;
    }

    if (fileSize > 0 && fread(buffer, 1, (size_t)fileSize, file) != (size_t)fileSize) {
        free(buffer);
        fclose(file);
        return ZR_NULL;
    }

    fclose(file);
    *outLength = (TZrSize)fileSize;
    return buffer;
}

static inline TZrSize ZrTests_Fixture_SkipWhitespace(const TZrChar *text, TZrSize index) {
    while (text != ZR_NULL && text[index] != '\0' &&
           (text[index] == ' ' || text[index] == '\t' || text[index] == '\r' || text[index] == '\n')) {
        index++;
    }

    return index;
}

/** @brief 为编译器构造模块源名，缺少可识别声明时退回测试给定路径。 */
static inline SZrString *ZrTests_Fixture_CreateSourceNameForModule(SZrState *state,
                                                                   const TZrChar *moduleSource,
                                                                   const TZrChar *fallbackPath) {
    const TZrChar *moduleKeyword = "module";
    const TZrChar *moduleMarker;

    if (state == ZR_NULL) {
        return ZR_NULL;
    }

    /* TODO: 此处按文本查找 module，注释或字符串中的同名片段可能抢先命中。
     * 现有调用见 test_module_system.c 的二进制 fixture；需用含前置注释的模块核对源名。 */
    moduleMarker = moduleSource != ZR_NULL ? strstr(moduleSource, moduleKeyword) : ZR_NULL;
    if (moduleMarker != ZR_NULL) {
        TZrSize index = (TZrSize)(moduleMarker - moduleSource) + strlen(moduleKeyword);
        TZrSize moduleStart;
        TZrSize moduleLength;

        index = ZrTests_Fixture_SkipWhitespace(moduleSource, index);
        moduleStart = index;
        while ((moduleSource[index] >= 'a' && moduleSource[index] <= 'z') ||
               (moduleSource[index] >= 'A' && moduleSource[index] <= 'Z') ||
               (moduleSource[index] >= '0' && moduleSource[index] <= '9') ||
               moduleSource[index] == '_' || moduleSource[index] == '.') {
            index++;
        }
        moduleLength = index - moduleStart;
        if (moduleLength > 0U) {
            TZrSize suffixLength = strlen(".zr");
            TZrChar *buffer = (TZrChar *)malloc(moduleLength + suffixLength + 1U);

            if (buffer == ZR_NULL) {
                return ZR_NULL;
            }

            memcpy(buffer, moduleSource + moduleStart, moduleLength);
            memcpy(buffer + moduleLength, ".zr", suffixLength + 1U);

            {
                SZrString *sourceName = ZrCore_String_Create(state, buffer, moduleLength + suffixLength);
                free(buffer);
                if (sourceName != ZR_NULL) {
                    return sourceName;
                }
            }
        }
    }

    if (fallbackPath == ZR_NULL) {
        return ZR_NULL;
    }

    return ZrCore_String_Create(state, (TZrNativeString)fallbackPath, strlen(fallbackPath));
}

/** @brief 暂时切换编译支持位生成二进制 fixture，恢复配置后返回调用方持有的文件字节。 */
static inline TZrByte *ZrTests_Fixture_BuildBinaryFile(SZrState *state,
                                                       const TZrChar *moduleSource,
                                                       const TZrChar *binaryPath,
                                                       TZrBool emitCompileTimeRuntimeSupport,
                                                       TZrSize *outLength) {
    SZrString *sourceName;
    SZrFunction *function;
    TZrBool oldEmitCompileTimeRuntimeSupport = ZR_FALSE;
    TZrByte *bytes;

    if (state == ZR_NULL || moduleSource == ZR_NULL || binaryPath == ZR_NULL || outLength == ZR_NULL) {
        return ZR_NULL;
    }

    sourceName = ZrTests_Fixture_CreateSourceNameForModule(state, moduleSource, binaryPath);
    if (sourceName == ZR_NULL) {
        return ZR_NULL;
    }

    /* 此标志只影响这一次编译，失败路径也必须在返回前恢复。 */
    if (state->global != ZR_NULL) {
        oldEmitCompileTimeRuntimeSupport = state->global->emitCompileTimeRuntimeSupport;
        state->global->emitCompileTimeRuntimeSupport = emitCompileTimeRuntimeSupport;
    }
    function = ZrParser_Source_Compile(state, moduleSource, strlen(moduleSource), sourceName);
    if (state->global != ZR_NULL) {
        state->global->emitCompileTimeRuntimeSupport = oldEmitCompileTimeRuntimeSupport;
    }
    if (function == ZR_NULL) {
        return ZR_NULL;
    }
    if (!ZrParser_Writer_WriteBinaryFile(state, function, binaryPath)) {
        ZrCore_Function_Free(state, function);
        return ZR_NULL;
    }

    ZrCore_Function_Free(state, function);
    bytes = ZrTests_Fixture_ReadFileBytes(binaryPath, outLength);
    return bytes;
}

/** @brief 按导入路径将数组 fixture 注册为 Io；成功后 Io 消费方负责调用 reader close。 */
static inline TZrBool ZrTests_Fixture_SourceLoaderFromArray(SZrState *state,
                                                            TZrNativeString sourcePath,
                                                            TZrNativeString md5,
                                                            SZrIo *io,
                                                            const ZrTestsFixtureSource *fixtures,
                                                            TZrSize fixtureCount) {
    TZrSize index;

    ZR_UNUSED_PARAMETER(md5);

    if (state == ZR_NULL || sourcePath == ZR_NULL || io == ZR_NULL || fixtures == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < fixtureCount; index++) {
        const ZrTestsFixtureSource *fixture = &fixtures[index];
        ZrTestsFixtureReader *reader;

        if (fixture->path == ZR_NULL || strcmp(fixture->path, sourcePath) != 0) {
            continue;
        }

        if (fixture->source == ZR_NULL && (fixture->bytes == ZR_NULL || fixture->length == 0)) {
            return ZR_FALSE;
        }

        reader = (ZrTestsFixtureReader *)malloc(sizeof(ZrTestsFixtureReader));
        if (reader == ZR_NULL) {
            return ZR_FALSE;
        }

        if (fixture->bytes != ZR_NULL && fixture->length > 0) {
            reader->bytes = fixture->bytes;
            reader->length = fixture->length;
        } else {
            reader->bytes = (const TZrByte *)fixture->source;
            reader->length = fixture->source != ZR_NULL ? strlen(fixture->source) : 0;
        }
        reader->consumed = ZR_FALSE;

        ZrCore_Io_Init(state, io, ZrTests_Fixture_ReaderRead, ZrTests_Fixture_ReaderClose, reader);
        io->isBinary = fixture->isBinary;
        return ZR_TRUE;
    }

    return ZR_FALSE;
}

#endif
