//
// Created by HeJiahui on 2025/7/27.
//

#ifndef ZR_VM_LIBRARY_PROJECT_H
#define ZR_VM_LIBRARY_PROJECT_H

#include "zr_vm_library/conf.h"


#define ZR_LIBRARY_BINARY_FILE_EXT ".zrb"

struct ZR_STRUCT_ALIGN SZrLibrary_Project {
    TZrString *file;
    TZrString *directory;
    TZrString *name;
    TZrString *version;
    TZrString *description;
    TZrString *author;
    TZrString *email;
    TZrString *url;
    TZrString *license;
    TZrString *copyright;
    TZrString *binary;
    TZrString *source;
    TZrString *entry;
    TZrString *dependency;
    TZrString *local;
};

typedef struct SZrLibrary_Project SZrLibrary_Project;
ZR_LIBRARY_API SZrLibrary_Project *ZrLibrary_Project_New(SZrState *state, TNativeString raw, TNativeString file);

ZR_LIBRARY_API void ZrLibrary_Project_Do(SZrState *state);

ZR_LIBRARY_API TBool ZrLibrary_Project_SourceLoadImplementation(SZrState *state, TNativeString path, TNativeString md5,
                                                                SZrIo *io);

#endif // ZR_VM_LIBRARY_PROJECT_H
