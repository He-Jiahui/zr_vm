/* Private path contracts shared by host I/O, copying and normalization. */
#ifndef ZR_VM_LIBRARY_FILE_PATH_INTERNAL_H
#define ZR_VM_LIBRARY_FILE_PATH_INTERNAL_H
#include "zr_vm_library/file.h"
TZrBool file_is_separator(TZrChar ch);
TZrBool file_string_equals(const TZrChar *lhs, const TZrChar *rhs);
TZrBool file_copy_text(TZrNativeString destination, TZrSize destinationSize, const TZrChar *source);
TZrBool file_path_is_root(const TZrChar *path);
TZrSize file_extract_root(const TZrChar *path, TZrChar *root, TZrSize rootSize);
TZrBool file_fill_path_parts(const TZrChar *normalizedPath, SZrLibrary_File_Info *outInfo);
const TZrChar *file_basename_ptr(const TZrChar *path);
TZrBool file_join_into(const TZrChar *path1, const TZrChar *path2, TZrChar *result, TZrSize resultSize);
#endif
