//
// Core UTF-8 helpers backed by utf8proc.
//

#ifndef ZR_VM_CORE_UTF8_H
#define ZR_VM_CORE_UTF8_H

#include "zr_vm_core/conf.h"

ZR_CORE_API TZrBool ZrCore_Utf8_IsValid(TZrNativeString string, TZrSize length);

ZR_CORE_API TZrBool ZrCore_Utf8_DecodeCodePoint(TZrNativeString string,
                                                TZrSize length,
                                                TZrUInt32 *outCodePoint,
                                                TZrSize *outConsumedBytes);

ZR_CORE_API TZrBool ZrCore_Utf8_EncodeCodePoint(TZrUInt32 codePoint,
                                                TZrChar *buffer,
                                                TZrSize *outLength);

ZR_CORE_API TZrBool ZrCore_Utf8_CountCodePoints(TZrNativeString string,
                                                TZrSize length,
                                                TZrSize *outCount);

ZR_CORE_API TZrBool ZrCore_Utf8_CodePointCountToByteOffset(TZrNativeString string,
                                                           TZrSize length,
                                                           TZrSize codePointCount,
                                                           TZrSize *outOffset);

#endif // ZR_VM_CORE_UTF8_H
