//
// Created by HeJiahui on 2025/7/27.
//

#ifndef ZR_VM_LIBRARY_FILE_H
#define ZR_VM_LIBRARY_FILE_H

#include <stdio.h>

#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_library/conf.h"

#define ZR_LIBRARY_FILE_BUFFER_SIZE BUFSIZ

typedef FILE *TZrLibrary_File_Ptr;

enum EZrLibrary_File_Exist {
    ZR_LIBRARY_FILE_NOT_EXIST = 0,
    ZR_LIBRARY_FILE_IS_FILE = 1,
    ZR_LIBRARY_FILE_IS_DIRECTORY = 2,
    ZR_LIBRARY_FILE_IS_OTHER = 3
};

typedef enum EZrLibrary_File_Exist EZrLibrary_File_Exist;

enum EZrLibrary_File_Mode {
    ZR_LIBRARY_FILE_MODE_READ = 0,
    ZR_LIBRARY_FILE_MODE_WRITE = 1,
    ZR_LIBRARY_FILE_MODE_APPEND = 2,
    ZR_LIBRARY_FILE_MODE_READ_WRITE = 3,
    ZR_LIBRARY_FILE_MODE_READ_APPEND = 4,
    ZR_LIBRARY_FILE_MODE_WRITE_APPEND = 5,
    ZR_LIBRARY_FILE_MODE_READ_WRITE_APPEND = 6
};

typedef enum EZrLibrary_File_Mode EZrLibrary_File_Mode;

struct ZR_STRUCT_ALIGN SZrLibrary_File_Reader {
    TZrSize size;
    TZrLibrary_File_Ptr file;
    TChar buffer[ZR_LIBRARY_FILE_BUFFER_SIZE];
};

typedef struct SZrLibrary_File_Reader SZrLibrary_File_Reader;

ZR_LIBRARY_API EZrLibrary_File_Exist ZrLibrary_File_Exist(TNativeString path);

ZR_LIBRARY_API TBool ZrLibrary_File_GetDirectory(TNativeString path, ZR_OUT TNativeString directory);

ZR_LIBRARY_API void ZrLibrary_File_PathJoin(TNativeString path1, TNativeString path2, ZR_OUT TNativeString result);


ZR_LIBRARY_API TNativeString ZrLibrary_File_ReadAll(SZrGlobalState *global, TNativeString path);

ZR_LIBRARY_API SZrLibrary_File_Reader *ZrLibrary_File_OpenRead(SZrGlobalState *global, TNativeString path,
                                                               TBool isBinary);

ZR_LIBRARY_API void ZrLibrary_File_CloseRead(SZrGlobalState *global, SZrLibrary_File_Reader *reader);

ZR_LIBRARY_API TBool ZrLibrary_File_SourceLoadImplementation(SZrState *state, TNativeString path, TNativeString md5,
                                                             SZrIo *io);

ZR_LIBRARY_API TBytePtr ZrLibrary_File_SourceReadImplementation(SZrState *state, TZrPtr reader, ZR_OUT TZrSize *size);

ZR_LIBRARY_API void ZrLibrary_File_SourceCloseImplementation(SZrState *state, TZrPtr reader);
#endif // ZR_VM_LIBRARY_FILE_H
