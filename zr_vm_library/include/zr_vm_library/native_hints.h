//
// Native module hint export helpers.
//

#ifndef ZR_VM_LIBRARY_NATIVE_HINTS_H
#define ZR_VM_LIBRARY_NATIVE_HINTS_H

#include "zr_vm_library/native_binding.h"

#define ZR_VM_NATIVE_HINTS_SCHEMA_ID "zr.native.hints/v1"

ZR_LIBRARY_API const TZrChar *ZrLibrary_NativeHints_GetSchemaId(void);
ZR_LIBRARY_API const TZrChar *ZrLibrary_NativeHints_GetModuleJson(const ZrLibModuleDescriptor *descriptor);
ZR_LIBRARY_API TZrBool ZrLibrary_NativeHints_WriteSidecar(const ZrLibModuleDescriptor *descriptor,
                                                          const TZrChar *outputPath);

#endif // ZR_VM_LIBRARY_NATIVE_HINTS_H
