//
// zr.system.assembly callbacks.
//

#include "zr_vm_lib_system/assembly.h"

#include "zr_vm_core/conversion.h"
#include "zr_vm_core/debug.h"
#include "zr_vm_core/string.h"
#include "zr_vm_library/file.h"
#include "zr_vm_library/project.h"
#include "zr_vm_library/zrm.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static TZrBool system_assembly_read_resource_name(const ZrLibCallContext *context,
                                                  TZrSize index,
                                                  const TZrChar **outName) {
    SZrString *nameString = ZR_NULL;

    if (outName != ZR_NULL) {
        *outName = ZR_NULL;
    }
    if (context == ZR_NULL || outName == ZR_NULL ||
        !ZrLib_CallContext_ReadString(context, index, &nameString) ||
        nameString == ZR_NULL) {
        return ZR_FALSE;
    }

    *outName = ZrCore_String_GetNativeString(nameString);
    return *outName != ZR_NULL && ZrLibrary_Zrm_ValidateLogicalName(*outName);
}

static TZrBool system_assembly_open_current_archive(ZrLibCallContext *context,
                                                    SZrLibrary_ZrmArchive *archive,
                                                    TZrChar *errorBuffer,
                                                    TZrSize errorBufferSize) {
    const SZrLibrary_Project *project;
    TZrChar archivePath[ZR_LIBRARY_MAX_PATH_LENGTH];

    if (archive != ZR_NULL) {
        memset(archive, 0, sizeof(*archive));
    }
    if (errorBuffer != ZR_NULL && errorBufferSize > 0) {
        errorBuffer[0] = '\0';
    }
    if (context == ZR_NULL || context->state == ZR_NULL || context->state->global == ZR_NULL ||
        archive == ZR_NULL) {
        return ZR_FALSE;
    }

    project = ZrLibrary_Project_GetFromGlobal(context->state->global);
    if (project == ZR_NULL ||
        !ZrLibrary_Project_ResolveAssemblyOutputPath(project, archivePath, sizeof(archivePath)) ||
        ZrLibrary_File_Exist((TZrNativeString)archivePath) != ZR_LIBRARY_FILE_IS_FILE) {
        return ZR_FALSE;
    }

    return ZrLibrary_Zrm_Open(archivePath, archive, errorBuffer, errorBufferSize);
}

static TZrBool system_assembly_find_current_resource(ZrLibCallContext *context,
                                                     const TZrChar *resourceName,
                                                     SZrLibrary_ZrmArchive *archive,
                                                     const SZrLibrary_ZrmEntryInfo **outEntry,
                                                     TZrChar *errorBuffer,
                                                     TZrSize errorBufferSize) {
    const SZrLibrary_ZrmEntryInfo *entry;

    if (outEntry != ZR_NULL) {
        *outEntry = ZR_NULL;
    }
    if (resourceName == ZR_NULL || archive == ZR_NULL || outEntry == ZR_NULL ||
        !ZrLibrary_Zrm_ValidateLogicalName(resourceName) ||
        !system_assembly_open_current_archive(context, archive, errorBuffer, errorBufferSize)) {
        return ZR_FALSE;
    }

    entry = ZrLibrary_Zrm_FindResource(archive, resourceName);
    if (entry == ZR_NULL) {
        ZrLibrary_Zrm_Close(archive);
        return ZR_FALSE;
    }

    *outEntry = entry;
    return ZR_TRUE;
}

static TZrBool system_assembly_make_byte_array(SZrState *state,
                                               const TZrByte *bytes,
                                               TZrSize byteCount,
                                               SZrTypeValue *result) {
    SZrObject *array;
    TZrSize index;

    if (state == ZR_NULL || result == ZR_NULL || (bytes == ZR_NULL && byteCount > 0)) {
        errno = EINVAL;
        return ZR_FALSE;
    }

    array = ZrLib_Array_New(state);
    if (array == ZR_NULL) {
        return ZR_FALSE;
    }

    for (index = 0; index < byteCount; index++) {
        SZrTypeValue entryValue;
        ZrLib_Value_SetInt(state, &entryValue, (TZrInt64)bytes[index]);
        if (!ZrLib_Array_PushValue(state, array, &entryValue)) {
            return ZR_FALSE;
        }
    }

    ZrLib_Value_SetObject(state, result, array, ZR_VALUE_TYPE_ARRAY);
    return ZR_TRUE;
}

static TZrBool system_assembly_raise_resource_error(SZrState *state,
                                                    const TZrChar *resourceName,
                                                    const TZrChar *detail) {
    ZrCore_Debug_RunError(state,
                          "assembly resource '%s' is not available%s%s",
                          resourceName != ZR_NULL ? resourceName : "<resource>",
                          detail != ZR_NULL && detail[0] != '\0' ? ": " : "",
                          detail != ZR_NULL ? detail : "");
    return ZR_FALSE;
}

TZrBool ZrSystem_Assembly_ResourceExists(ZrLibCallContext *context, SZrTypeValue *result) {
    const TZrChar *resourceName = ZR_NULL;
    SZrLibrary_ZrmArchive archive;
    const SZrLibrary_ZrmEntryInfo *entry = ZR_NULL;
    TZrChar error[ZR_LIBRARY_ZRM_ERROR_BUFFER_LENGTH];

    if (context == ZR_NULL || result == ZR_NULL ||
        !system_assembly_read_resource_name(context, 0, &resourceName)) {
        return ZR_FALSE;
    }

    memset(error, 0, sizeof(error));
    if (!system_assembly_find_current_resource(context, resourceName, &archive, &entry, error, sizeof(error))) {
        ZrLib_Value_SetBool(context->state, result, ZR_FALSE);
        return ZR_TRUE;
    }

    ZrLibrary_Zrm_Close(&archive);
    ZrLib_Value_SetBool(context->state, result, entry != ZR_NULL);
    return ZR_TRUE;
}

TZrBool ZrSystem_Assembly_ReadResourceText(ZrLibCallContext *context, SZrTypeValue *result) {
    const TZrChar *resourceName = ZR_NULL;
    SZrLibrary_ZrmArchive archive;
    const SZrLibrary_ZrmEntryInfo *entry = ZR_NULL;
    TZrByte *bytes = ZR_NULL;
    TZrSize byteCount = 0;
    TZrChar *text;
    TZrChar error[ZR_LIBRARY_ZRM_ERROR_BUFFER_LENGTH];
    TZrBool success = ZR_FALSE;

    if (context == ZR_NULL || result == ZR_NULL ||
        !system_assembly_read_resource_name(context, 0, &resourceName)) {
        return ZR_FALSE;
    }

    memset(error, 0, sizeof(error));
    if (!system_assembly_find_current_resource(context, resourceName, &archive, &entry, error, sizeof(error))) {
        return system_assembly_raise_resource_error(context->state, resourceName, error);
    }

    if (!ZrLibrary_Zrm_ReadEntry(&archive, entry->entryName, &bytes, &byteCount, error, sizeof(error))) {
        ZrLibrary_Zrm_Close(&archive);
        return system_assembly_raise_resource_error(context->state, resourceName, error);
    }

    text = (TZrChar *)malloc(byteCount + 1U);
    if (text != ZR_NULL) {
        if (byteCount > 0) {
            memcpy(text, bytes, byteCount);
        }
        text[byteCount] = '\0';
        ZrLib_Value_SetString(context->state, result, text);
        success = ZR_TRUE;
        free(text);
    }

    ZrLibrary_Zrm_FreeBytes(bytes);
    ZrLibrary_Zrm_Close(&archive);
    return success;
}

TZrBool ZrSystem_Assembly_ReadResourceBytes(ZrLibCallContext *context, SZrTypeValue *result) {
    const TZrChar *resourceName = ZR_NULL;
    SZrLibrary_ZrmArchive archive;
    const SZrLibrary_ZrmEntryInfo *entry = ZR_NULL;
    TZrByte *bytes = ZR_NULL;
    TZrSize byteCount = 0;
    TZrChar error[ZR_LIBRARY_ZRM_ERROR_BUFFER_LENGTH];
    TZrBool success;

    if (context == ZR_NULL || result == ZR_NULL ||
        !system_assembly_read_resource_name(context, 0, &resourceName)) {
        return ZR_FALSE;
    }

    memset(error, 0, sizeof(error));
    if (!system_assembly_find_current_resource(context, resourceName, &archive, &entry, error, sizeof(error))) {
        return system_assembly_raise_resource_error(context->state, resourceName, error);
    }

    if (!ZrLibrary_Zrm_ReadEntry(&archive, entry->entryName, &bytes, &byteCount, error, sizeof(error))) {
        ZrLibrary_Zrm_Close(&archive);
        return system_assembly_raise_resource_error(context->state, resourceName, error);
    }

    success = system_assembly_make_byte_array(context->state, bytes, byteCount, result);
    ZrLibrary_Zrm_FreeBytes(bytes);
    ZrLibrary_Zrm_Close(&archive);
    return success;
}
