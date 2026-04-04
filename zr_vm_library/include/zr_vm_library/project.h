//
// Created by HeJiahui on 2025/7/27.
//

#ifndef ZR_VM_LIBRARY_PROJECT_H
#define ZR_VM_LIBRARY_PROJECT_H

#include "zr_vm_library/conf.h"


#define ZR_LIBRARY_BINARY_FILE_EXT ZR_VM_BINARY_MODULE_FILE_EXTENSION

struct ZR_STRUCT_ALIGN SZrLibrary_Project {
    SZrString *file;
    SZrString *directory;
    SZrString *name;
    SZrString *version;
    SZrString *description;
    SZrString *author;
    SZrString *email;
    SZrString *url;
    SZrString *license;
    SZrString *copyright;
    SZrString *binary;
    SZrString *source;
    SZrString *entry;
    SZrString *dependency;
    SZrString *local;
    TZrBool supportMultithread;
    TZrBool autoCoroutine;
    TZrPtr aotRuntime;
};

typedef struct SZrLibrary_Project SZrLibrary_Project;
ZR_LIBRARY_API SZrLibrary_Project *ZrLibrary_Project_New(SZrState *state, TZrNativeString raw, TZrNativeString file);

ZR_LIBRARY_API void ZrLibrary_Project_Free(SZrState *state, SZrLibrary_Project *project);

ZR_LIBRARY_API EZrThreadStatus ZrLibrary_Project_Run(SZrState *state, SZrTypeValue *result);

ZR_LIBRARY_API void ZrLibrary_Project_Do(SZrState *state);

ZR_LIBRARY_API TZrBool ZrLibrary_Project_SourceLoadImplementation(SZrState *state, TZrNativeString path, TZrNativeString md5,
                                                                SZrIo *io);

#endif // ZR_VM_LIBRARY_PROJECT_H
