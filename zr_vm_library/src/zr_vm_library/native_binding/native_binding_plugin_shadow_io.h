#ifndef ZR_VM_LIBRARY_NATIVE_BINDING_PLUGIN_SHADOW_IO_H
#define ZR_VM_LIBRARY_NATIVE_BINDING_PLUGIN_SHADOW_IO_H

#include "zr_vm_library/conf.h"
#include <stdio.h>

/* Private owning-stream boundary. Both valid, distinct streams are closed exactly
 * once, including after read, write, or either close failure. False reports any
 * such failure; the caller owns paths and removes only its incomplete target. */
TZrBool native_registry_copy_plugin_streams(FILE *source, FILE *target);

/* Create a shadow image under the platform temporary cache; no registry is published. */
TZrBool native_registry_prepare_shadow_plugin_path(const TZrChar *moduleName,
                                                   const TZrChar *sourcePath,
                                                   TZrChar *buffer,
                                                   TZrSize bufferSize);

#endif
