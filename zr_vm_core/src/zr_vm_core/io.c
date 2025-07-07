//
// Created by HeJiahui on 2025/7/6.
//

#include "zr_vm_core/io.h"

#include "zr_vm_core/memory.h"
#include "zr_vm_core/state.h"

#define ZR_IO_MALLOC_NATIVE_DATA(GLOBAL, SIZE) ZrMemoryRawMallocWithType(GLOBAL, (SIZE), ZR_VALUE_TYPE_NATIVE_DATA);

static TBool ZrIoRefill(SZrIo *io) {
    SZrState *state = io->state;
    TZrSize readSize = 0;
    ZR_THREAD_UNLOCK(state);
    TBytePtr buffer = io->read(state, io->customData, &readSize);
    ZR_THREAD_LOCK(state);
    if (buffer == ZR_NULL || readSize == 0) {
        return ZR_FALSE;
    }
    io->remained = readSize;
    io->pointer = buffer;
    return ZR_TRUE;
}

static TInt32 ZrIoReadChar(SZrIo *io) {
    if (io->remained == 0) {
        if (!ZrIoRefill(io)) {
            // EOF = -1
            return ZR_IO_EOF;
        }
    }
    // BYTE > 0 && BYTE < 256
    TInt32 c = *io->pointer;
    io->remained--;
    io->pointer++;
    return c;
}

static ZR_FORCE_INLINE TZrSize ZrIoReadSize(SZrIo *io) {
    TZrSize size;
    ZrIoRead(io, (TBytePtr) &size, sizeof(size));
    return size;
}

#define ZR_IO_READ_NATIVE_TYPE(IO, DATA, TYPE)\
ZrIoRead(IO, (TBytePtr)&(DATA), sizeof(TYPE))


static TZrString *ZrIoReadStringWithLength(SZrIo *io) {
    SZrGlobalState *global = io->state->global;
    TZrSize length = ZrIoReadSize(io);
    TNativeString nativeString = ZrMemoryRawMallocWithType(global, ZR_VALUE_TYPE_STRING, length);
    ZrIoRead(io, (TBytePtr) nativeString, length);
    TZrString *string = ZrStringCreate(io->state, nativeString, length);
    ZrMemoryRawFree(global, nativeString, length);
    return string;
}

static void ZrIoReadImports(SZrIo *io, SZrIoImport *imports, TZrSize count) {
    SZrGlobalState *global = io->state->global;
    for (TZrSize i = 0; i < count; i++) {
        SZrIoImport *import = &imports[i];
        import->name = ZrIoReadStringWithLength(io);
        import->md5 = ZrIoReadStringWithLength(io);
    }
}

static void ZrIoReadFunctions(SZrIo *io, SZrIoFunction *functions, TZrSize count) {
    SZrGlobalState *global = io->state->global;
    for (TZrSize i = 0; i < count; i++) {
        SZrIoFunction *function = &functions[i];
        function->name = ZrIoReadStringWithLength(io);
        ZR_IO_READ_NATIVE_TYPE(io, function->startLine, TUInt64);
        ZR_IO_READ_NATIVE_TYPE(io, function->endLine, TUInt64);
        ZR_IO_READ_NATIVE_TYPE(io, function->parametersLength, TZrSize);
        ZR_IO_READ_NATIVE_TYPE(io, function->hasVarArgs, TUInt64);
        ZR_IO_READ_NATIVE_TYPE(io, function->instructionsLength, TZrSize);
        // todo: read instructions ...
    }
}

static void ZrIoReadModuleDeclares(SZrIo *io, SZrIoModuleDeclare *declares, TZrSize count) {
    SZrGlobalState *global = io->state->global;
    for (TZrSize i = 0; i < count; i++) {
        SZrIoModuleDeclare *declare = &declares[i];
        // todo:
        declare->type = ZR_IO_READ_NATIVE_TYPE(io, declare->type, EZrIoClassDeclareType);
        switch (declare->type) {
            case ZR_IO_MODULE_DECLARE_TYPE_CLASS:
                // todo:
                break;
            case ZR_IO_MODULE_DECLARE_TYPE_FUNCTION:
                ZrIoReadFunctions(io, declare->function, 1);
                break;
            default:
                break;
        }
        // check type and read
    }
}

static void ZrIoReadModules(SZrIo *io, SZrIoModule *modules, TZrSize count) {
    SZrGlobalState *global = io->state->global;
    for (TZrSize i = 0; i < count; i++) {
        SZrIoModule *module = &modules[i];
        module->name = ZrIoReadStringWithLength(io);
        module->md5 = ZrIoReadStringWithLength(io);
        module->importsLength = ZR_IO_READ_NATIVE_TYPE(io, module->importsLength, TZrSize);
        module->imports = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoImport) * module->importsLength);
        ZrIoReadImports(io, module->imports, module->importsLength);
        module->declaresLength = ZR_IO_READ_NATIVE_TYPE(io, module->declaresLength, TZrSize);
        module->declares = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoModuleDeclare) * module->declaresLength);
        ZrIoReadModuleDeclares(io, module->declares, module->declaresLength);
        ZrIoReadFunctions(io, module->entryFunction, 1);
    }
}

void ZrIoInit(SZrState *state, SZrIo *io, FZrIoRead read, TZrPtr customData) {
    io->state = state;
    io->read = read;
    io->customData = customData;
    io->pointer = ZR_NULL;
    io->remained = 0;
}

TZrSize ZrIoRead(SZrIo *io, TBytePtr buffer, TZrSize size) {
    while (size > 0) {
        if (io->remained == 0) {
            if (!ZrIoRefill(io)) {
                return size;
            }
        }
        TZrSize read = (size <= io->remained) ? size : io->remained;
        ZrMemoryRawCopy(buffer, io->pointer, read);
        io->remained -= read;
        io->pointer += read;
        buffer += read;
        size -= read;
    }
    // throw error
    // TODO:
    ZrExceptionThrow(io->state, ZR_THREAD_STATUS_RUNTIME_ERROR);
    return 0;
}


SZrIoSource *ZrIoReadSourceNew(SZrIo *io) {
    SZrGlobalState *global = io->state->global;
    SZrIoSource *source = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoSource));
    ZrIoRead(io, (TBytePtr) source->signature, sizeof(source->signature));
    // todo: check signature
    ZR_IO_READ_NATIVE_TYPE(io, source->versionMajor, TUInt32);
    ZR_IO_READ_NATIVE_TYPE(io, source->versionMinor, TUInt32);
    ZR_IO_READ_NATIVE_TYPE(io, source->versionPatch, TUInt32);
    ZR_IO_READ_NATIVE_TYPE(io, source->format, TUInt64);
    ZR_IO_READ_NATIVE_TYPE(io, source->nativeIntSize, TUInt8);
    ZR_IO_READ_NATIVE_TYPE(io, source->typeSizeSize, TUInt8);
    ZR_IO_READ_NATIVE_TYPE(io, source->typeInstructionSize, TUInt8);
    ZR_IO_READ_NATIVE_TYPE(io, source->isBigEndian, TBool);
    ZR_IO_READ_NATIVE_TYPE(io, source->isDebug, TBool);
    ZrIoRead(io, (TBytePtr) source->optional, sizeof(source->optional));
    ZR_IO_READ_NATIVE_TYPE(io, source->modulesLength, TZrSize);
    source->modules = ZR_IO_MALLOC_NATIVE_DATA(global, sizeof(SZrIoModule) * source->modulesLength);
    ZrIoReadModules(io, source->modules, source->modulesLength);

    return source;
}
