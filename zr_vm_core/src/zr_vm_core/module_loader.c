//
// Module import and source loading helpers.
//

#include "module_internal.h"
#include "zr_vm_core/reflection.h"

#include <stdio.h>

static const TZrChar *module_loader_debug_string(SZrState *state, SZrString *value) {
    return (state != ZR_NULL && value != ZR_NULL) ? ZrCore_String_GetNativeString(value) : "<null>";
}

static TZrBool refill_io_chunk(SZrIo *io) {
    SZrState *state;
    TZrSize readSize;
    TZrBytePtr buffer;

    if (io == ZR_NULL || io->read == ZR_NULL || io->state == ZR_NULL) {
        return ZR_FALSE;
    }

    state = io->state;
    readSize = 0;
    ZR_THREAD_UNLOCK(state);
    buffer = io->read(state, io->customData, &readSize);
    ZR_THREAD_LOCK(state);
    if (buffer == ZR_NULL || readSize == 0) {
        return ZR_FALSE;
    }

    io->pointer = buffer;
    io->remained = readSize;
    return ZR_TRUE;
}

static TZrBytePtr read_all_from_io(SZrState *state, SZrIo *io, TZrSize *outSize) {
    SZrGlobalState *global;
    TZrSize capacity;
    TZrBytePtr buffer;
    TZrSize totalSize;

    if (state == ZR_NULL || io == ZR_NULL || outSize == ZR_NULL) {
        return ZR_NULL;
    }

    global = state->global;
    capacity = (io->remained > 0) ? io->remained : ZR_VM_READ_ALL_IO_FALLBACK_CAPACITY;
    buffer = (TZrBytePtr)ZrCore_Memory_RawMallocWithType(global, capacity + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
    if (buffer == ZR_NULL) {
        return ZR_NULL;
    }

    totalSize = 0;
    while (io->remained > 0 || refill_io_chunk(io)) {
        if (totalSize + io->remained + 1 > capacity) {
            TZrSize newCapacity = capacity;
            TZrBytePtr newBuffer;

            while (totalSize + io->remained + 1 > newCapacity) {
                newCapacity *= 2;
            }

            newBuffer =
                    (TZrBytePtr)ZrCore_Memory_RawMallocWithType(global, newCapacity + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            if (newBuffer == ZR_NULL) {
                ZrCore_Memory_RawFreeWithType(global, buffer, capacity + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
                return ZR_NULL;
            }

            if (totalSize > 0) {
                ZrCore_Memory_RawCopy(newBuffer, buffer, totalSize);
            }

            ZrCore_Memory_RawFreeWithType(global, buffer, capacity + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            buffer = newBuffer;
            capacity = newCapacity;
        }

        ZrCore_Memory_RawCopy(buffer + totalSize, io->pointer, io->remained);
        totalSize += io->remained;
        io->pointer += io->remained;
        io->remained = 0;
    }

    buffer[totalSize] = '\0';
    *outSize = totalSize;
    return buffer;
}

TZrInt64 ZrCore_Module_ImportNativeEntry(SZrState *state) {
    TZrStackValuePointer functionBase;
    TZrStackValuePointer argBase;
    SZrFunctionStackAnchor functionBaseAnchor;
    SZrTypeValue *result;
    SZrTypeValue *pathValue;
    SZrString *path;
    struct SZrObjectModule *module;

    if (state == ZR_NULL || state->callInfoList == ZR_NULL) {
        return 0;
    }

    functionBase = state->callInfoList->functionBase.valuePointer;
    ZrCore_Function_StackAnchorInit(state, functionBase, &functionBaseAnchor);
    argBase = functionBase + 1;
    result = ZrCore_Stack_GetValue(functionBase);

    if (state->stackTop.valuePointer <= argBase) {
        functionBase = ZrCore_Function_StackAnchorRestore(state, &functionBaseAnchor);
        result = ZrCore_Stack_GetValue(functionBase);
        ZrCore_Value_ResetAsNull(result);
        state->stackTop.valuePointer = functionBase + 1;
        return 1;
    }

    pathValue = ZrCore_Stack_GetValue(argBase);
    if (pathValue == ZR_NULL || pathValue->type != ZR_VALUE_TYPE_STRING) {
        functionBase = ZrCore_Function_StackAnchorRestore(state, &functionBaseAnchor);
        result = ZrCore_Stack_GetValue(functionBase);
        ZrCore_Value_ResetAsNull(result);
        state->stackTop.valuePointer = functionBase + 1;
        return 1;
    }

    path = ZR_CAST_STRING(state, pathValue->value.object);
    module = ZrCore_Module_ImportByPath(state, path);

    functionBase = ZrCore_Function_StackAnchorRestore(state, &functionBaseAnchor);
    result = ZrCore_Stack_GetValue(functionBase);
    if (module == ZR_NULL) {
        ZrCore_Value_ResetAsNull(result);
    } else {
        ZrCore_Value_InitAsRawObject(state, result, ZR_CAST_RAW_OBJECT_AS_SUPER(module));
        result->type = ZR_VALUE_TYPE_OBJECT;
    }

    state->stackTop.valuePointer = functionBase + 1;
    return 1;
}

struct SZrObjectModule *ZrCore_Module_ImportByPath(SZrState *state, SZrString *path) {
    struct SZrObjectModule *cachedModule;
    SZrGlobalState *global;
    TZrNativeString pathStr;
    TZrSize pathLen;
    SZrIo io;
    SZrFunction *func;
    SZrClosure *closure;
    struct SZrObjectModule *module;
    TZrUInt64 pathHash;
    TZrStackValuePointer savedStackTop;
    TZrStackValuePointer callBase;
    SZrFunctionStackAnchor callBaseAnchor;

    if (state == ZR_NULL || state->global == ZR_NULL || path == ZR_NULL) {
        return ZR_NULL;
    }

    cachedModule = ZrCore_Module_GetFromCache(state, path);
    if (cachedModule != ZR_NULL) {
        return cachedModule;
    }

    global = state->global;
    if (global->aotModuleLoader != ZR_NULL) {
        struct SZrObjectModule *aotModule =
                global->aotModuleLoader(state, path, global->aotModuleLoaderUserData);
        if (aotModule != ZR_NULL) {
            if (aotModule->fullPath == ZR_NULL || aotModule->moduleName == ZR_NULL) {
                TZrUInt64 aotPathHash = ZrCore_Module_CalculatePathHash(state, path);
                ZrCore_Module_SetInfo(state, aotModule, path, aotPathHash, path);
            }

            ZrCore_Module_AddToCache(state, path, aotModule);
            return aotModule;
        }

        if (state->threadStatus != ZR_THREAD_STATUS_FINE) {
            return ZR_NULL;
        }
    }

    if (global->nativeModuleLoader != ZR_NULL) {
        struct SZrObjectModule *nativeModule =
                global->nativeModuleLoader(state, path, global->nativeModuleLoaderUserData);
        if (nativeModule != ZR_NULL) {
            if (nativeModule->fullPath == ZR_NULL || nativeModule->moduleName == ZR_NULL) {
                TZrUInt64 nativePathHash = ZrCore_Module_CalculatePathHash(state, path);
                ZrCore_Module_SetInfo(state, nativeModule, path, nativePathHash, path);
            }

            ZrCore_Module_AddToCache(state, path, nativeModule);
            return nativeModule;
        }
    }

    if (global->sourceLoader == ZR_NULL) {
        return ZR_NULL;
    }

    if (path->shortStringLength < ZR_VM_LONG_STRING_FLAG) {
        pathStr = ZrCore_String_GetNativeStringShort(path);
        pathLen = path->shortStringLength;
    } else {
        pathStr = *ZrCore_String_GetNativeStringLong(path);
        pathLen = path->longStringLength;
    }

    if (pathStr == ZR_NULL || pathLen == 0) {
        return ZR_NULL;
    }

    if (!global->sourceLoader(state, pathStr, ZR_NULL, &io)) {
        return ZR_NULL;
    }

    func = ZR_NULL;
    if (io.isBinary) {
        SZrIoSource *ioSource = ZrCore_Io_ReadSourceNew(&io);
        if (io.close != ZR_NULL) {
            io.close(state, io.customData);
        }

        if (ioSource == ZR_NULL) {
            return ZR_NULL;
        }

        func = ZrCore_Io_LoadEntryFunctionToRuntime(state, ioSource);
        ZrCore_Io_ReadSourceFree(global, ioSource);
        if (func == ZR_NULL) {
            return ZR_NULL;
        }
    } else {
        TZrSize sourceSize = 0;
        TZrBytePtr sourceBuffer = read_all_from_io(state, &io, &sourceSize);
        if (sourceBuffer == ZR_NULL) {
            if (io.close != ZR_NULL) {
                io.close(state, io.customData);
            }
            return ZR_NULL;
        }

        if (io.close != ZR_NULL) {
            io.close(state, io.customData);
        }

        if (sourceSize == 0) {
            ZrCore_Memory_RawFreeWithType(global, sourceBuffer, sourceSize + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            return ZR_NULL;
        }

        if (global->compileSource == ZR_NULL) {
            ZrCore_Memory_RawFreeWithType(global, sourceBuffer, sourceSize + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);
            return ZR_NULL;
        }

        {
            SZrString *sourceName = ZrCore_String_Create(state, pathStr, pathLen);
            func = global->compileSource(state, (const TZrChar *)sourceBuffer, sourceSize, sourceName);
        }
        ZrCore_Memory_RawFreeWithType(global, sourceBuffer, sourceSize + 1, ZR_MEMORY_NATIVE_TYPE_GLOBAL);

        if (func == ZR_NULL) {
            return ZR_NULL;
        }
    }

    closure = ZrCore_Closure_New(state, 0);
    if (closure == ZR_NULL) {
        ZrCore_Function_Free(state, func);
        return ZR_NULL;
    }
    closure->function = func;

    module = ZrCore_Module_Create(state);
    if (module == ZR_NULL) {
        return ZR_NULL;
    }

    pathHash = ZrCore_Module_CalculatePathHash(state, path);
    ZrCore_Module_SetInfo(state, module, ZR_NULL, pathHash, path);
    ZrCore_Reflection_AttachModuleRuntimeMetadata(state, module, func);

    if (func != ZR_NULL) {
        ZrCore_Module_CreatePrototypesFromConstants(state, module, func);
    }

    ZrCore_Module_AddToCache(state, path, module);

    savedStackTop = state->stackTop.valuePointer;
    callBase = ZrCore_Function_CheckStackAndAnchor(state, 1, savedStackTop, savedStackTop, &callBaseAnchor);
    ZrCore_Value_ResetAsNull(ZrCore_Stack_GetValue(callBase));
    ZrCore_Stack_SetRawObjectValue(state, callBase, ZR_CAST_RAW_OBJECT_AS_SUPER(closure));
    state->stackTop.valuePointer = callBase + 1;
    callBase = ZrCore_Function_CallAndRestoreAnchor(state, &callBaseAnchor, 0);

    if (func != ZR_NULL && func->exportedVariables != ZR_NULL && func->exportedVariableLength > 0) {
        TZrStackValuePointer exportedValuesTop = callBase + 1 + func->stackSize;
        for (TZrUInt32 i = 0; i < func->exportedVariableLength; i++) {
            struct SZrFunctionExportedVariable *exportVar = &func->exportedVariables[i];
            if (exportVar->name == ZR_NULL) {
                continue;
            }

            {
                TZrStackValuePointer varPointer = callBase + 1 + exportVar->stackSlot;
                if (varPointer >= exportedValuesTop) {
                    continue;
                }

                {
                    SZrTypeValue *varValue = ZrCore_Stack_GetValue(varPointer);
                    if (varValue == ZR_NULL) {
                        continue;
                    }

                    if (exportVar->accessModifier == ZR_ACCESS_CONSTANT_PUBLIC) {
                        ZrCore_Module_AddPubExport(state, module, exportVar->name, varValue);
                    } else if (exportVar->accessModifier == ZR_ACCESS_CONSTANT_PROTECTED) {
                        ZrCore_Module_AddProExport(state, module, exportVar->name, varValue);
                    }
                }
            }
        }
    }

    state->stackTop.valuePointer = savedStackTop;
    return module;
}
