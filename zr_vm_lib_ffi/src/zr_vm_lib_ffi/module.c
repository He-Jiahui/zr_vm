//
// Built-in zr.ffi native module registration.
//

#include "zr_vm_lib_ffi/module.h"

#include "zr_vm_lib_ffi/runtime.h"

static const ZrLibGenericParameterDescriptor kPointerGenericParameters[] = {{"T", ZR_NULL, ZR_NULL, 0}};

static const ZrLibFunctionDescriptor g_ffi_functions[] = {
        {"loadLibrary", 1, 1, ZrFfi_LoadLibrary, "LibraryHandle",
         "Load a dynamic library by absolute or relative path.", ZR_NULL, 0},
        {"callback", 2, 2, ZrFfi_CreateCallback, "CallbackHandle",
         "Create a C-callable callback trampoline from a zr closure.", ZR_NULL, 0},
        {"sizeof", 1, 1, ZrFfi_SizeOf, "int", "Return the byte size of a runtime FFI type descriptor.", ZR_NULL, 0},
        {"alignof", 1, 1, ZrFfi_AlignOf, "int", "Return the alignment of a runtime FFI type descriptor.", ZR_NULL, 0},
        {"nullPointer", 1, 1, ZrFfi_NullPointer, "Ptr<void>", "Create a typed null pointer handle.", ZR_NULL, 0},
};

static const ZrLibMethodDescriptor g_library_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("close", 0, 0, ZrFfi_Library_Close, "null",
                                      "Close the library handle. Idempotent.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("isClosed", 0, 0, ZrFfi_Library_IsClosed, "bool",
                                      "Return whether the library handle has been closed.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("getSymbol", 2, 2, ZrFfi_Library_GetSymbol, "SymbolHandle",
                                      "Resolve and compile a typed symbol handle.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("getVersion", 0, 1, ZrFfi_Library_GetVersion, "string",
                                      "Read a version string exported by the library.", ZR_FALSE, ZR_NULL, 0),
};

static const ZrLibMethodDescriptor g_symbol_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("call", 1, 1, ZrFfi_Symbol_Call, "value",
                                      "Call the compiled symbol with an argument array.", ZR_FALSE, ZR_NULL, 0),
};

static const ZrLibMetaMethodDescriptor g_symbol_meta_methods[] = {
        {ZR_META_CALL, 0, (TZrUInt16)0xFFFFu, ZrFfi_Symbol_MetaCall, "value",
         "Call the compiled symbol directly with positional arguments.", ZR_NULL, 0},
};

static const ZrLibMethodDescriptor g_callback_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("close", 0, 0, ZrFfi_Callback_Close, "null",
                                      "Close the callback handle. Idempotent.", ZR_FALSE, ZR_NULL, 0),
};

static const ZrLibMethodDescriptor g_pointer_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("as", 1, 1, ZrFfi_Pointer_As, "Ptr<void>",
                                      "Reinterpret the pointer handle with a different type descriptor.", ZR_FALSE,
                                      ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("read", 1, 1, ZrFfi_Pointer_Read, "value",
                                      "Read a value through the pointer using the provided type descriptor.", ZR_FALSE,
                                      ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("close", 0, 0, ZrFfi_Pointer_Close, "null",
                                      "Release any ownership held by the pointer handle.", ZR_FALSE, ZR_NULL, 0),
};

static const ZrLibMethodDescriptor g_buffer_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("allocate", 1, 1, ZrFfi_Buffer_Allocate, "BufferHandle",
                                      "Allocate a managed native buffer.", ZR_TRUE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("close", 0, 0, ZrFfi_Buffer_Close, "null",
                                      "Close the buffer handle. Idempotent.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("pin", 0, 0, ZrFfi_Buffer_Pin, "Ptr<u8>",
                                      "Pin the buffer and return a pointer view.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("read", 2, 2, ZrFfi_Buffer_Read, "array", "Read a byte range from the buffer.",
                                      ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("write", 2, 2, ZrFfi_Buffer_Write, "int",
                                      "Write bytes into the buffer from an array.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("slice", 2, 2, ZrFfi_Buffer_Slice, "BufferHandle",
                                      "Create a copied slice of the buffer.", ZR_FALSE, ZR_NULL, 0),
};

static const ZrLibTypeDescriptor g_ffi_types[] = {
        ZR_LIB_TYPE_DESCRIPTOR_INIT("LibraryHandle", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0, g_library_methods,
                                    ZR_ARRAY_COUNT(g_library_methods), ZR_NULL, 0, "Managed dynamic-library handle.",
                                    ZR_NULL, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_TRUE,
                                    "LibraryHandle(path: string)", ZR_NULL, 0),
        ZR_LIB_TYPE_DESCRIPTOR_INIT("SymbolHandle", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0, g_symbol_methods,
                                    ZR_ARRAY_COUNT(g_symbol_methods), g_symbol_meta_methods,
                                    ZR_ARRAY_COUNT(g_symbol_meta_methods),
                                    "Typed symbol handle compiled from a library export and ABI signature.", ZR_NULL,
                                    ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_TRUE, "SymbolHandle()", ZR_NULL, 0),
        ZR_LIB_TYPE_DESCRIPTOR_INIT("CallbackHandle", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0, g_callback_methods,
                                    ZR_ARRAY_COUNT(g_callback_methods), ZR_NULL, 0,
                                    "C-callable callback trampoline rooted in the zr VM.", ZR_NULL, ZR_NULL, 0,
                                    ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_TRUE,
                                    "CallbackHandle(signature: object, fn: function)", ZR_NULL, 0),
        ZR_LIB_TYPE_DESCRIPTOR_FFI_INIT("PointerHandle", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0, g_pointer_methods,
                                        ZR_ARRAY_COUNT(g_pointer_methods), ZR_NULL, 0,
                                        "Typed pointer wrapper for native addresses.", ZR_NULL, ZR_NULL, 0, ZR_NULL,
                                        0, ZR_NULL, ZR_FALSE, ZR_TRUE, "PointerHandle(type: object)", ZR_NULL, 0,
                                        "pointer", ZR_NULL, ZR_NULL, "borrowed", ZR_NULL),
        ZR_LIB_TYPE_DESCRIPTOR_FFI_INIT("BufferHandle", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0, g_buffer_methods,
                                        ZR_ARRAY_COUNT(g_buffer_methods), ZR_NULL, 0,
                                        "Managed native byte buffer with pin support.", ZR_NULL, ZR_NULL, 0, ZR_NULL,
                                        0, ZR_NULL, ZR_FALSE, ZR_TRUE, "BufferHandle(size: int)", ZR_NULL, 0,
                                        "pointer", ZR_NULL, ZR_NULL, "owned", ZR_NULL),
        ZR_LIB_TYPE_DESCRIPTOR_INIT("Ptr", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, 0,
                                    "Semantic pointer family for ABI-aware FFI handles.", "PointerHandle", ZR_NULL, 0,
                                    ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL, kPointerGenericParameters,
                                    ZR_ARRAY_COUNT(kPointerGenericParameters)),
        ZR_LIB_TYPE_DESCRIPTOR_INIT("Ptr32", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, 0,
                                    "32-bit semantic pointer family for ABI-aware FFI handles.", "PointerHandle",
                                    ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL,
                                    kPointerGenericParameters, ZR_ARRAY_COUNT(kPointerGenericParameters)),
        ZR_LIB_TYPE_DESCRIPTOR_INIT("Ptr64", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, 0,
                                    "64-bit semantic pointer family for ABI-aware FFI handles.", "PointerHandle",
                                    ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL,
                                    kPointerGenericParameters, ZR_ARRAY_COUNT(kPointerGenericParameters)),
        ZR_LIB_TYPE_DESCRIPTOR_INIT("Char", ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, 0,
                                    "Single-byte ABI character wrapper used by FFI metadata.", ZR_NULL, ZR_NULL, 0,
                                    ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL, 0),
        ZR_LIB_TYPE_DESCRIPTOR_INIT("WChar", ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, 0,
                                    "Wide ABI character wrapper used by FFI metadata.", ZR_NULL, ZR_NULL, 0, ZR_NULL,
                                    0, ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL, 0),
};

static const ZrLibTypeHintDescriptor g_ffi_hints[] = {
        {"loadLibrary", "function", "loadLibrary(path: string): LibraryHandle", "Load a dynamic library by path."},
        {"callback", "function", "callback(signature: object, fn: function): CallbackHandle",
         "Create a C-callable callback trampoline."},
        {"sizeof", "function", "sizeof(type: object): int", "Return the byte size of a runtime FFI type descriptor."},
        {"alignof", "function", "alignof(type: object): int", "Return the alignment of a runtime FFI type descriptor."},
        {"nullPointer", "function", "nullPointer(type: object): Ptr<void>", "Create a typed null pointer handle."},
        {"LibraryHandle", "type", "class LibraryHandle", "Managed dynamic-library handle."},
        {"SymbolHandle", "type", "class SymbolHandle", "Typed symbol handle compiled from an ABI signature."},
        {"CallbackHandle", "type", "class CallbackHandle", "C-callable callback trampoline rooted in the zr VM."},
        {"PointerHandle", "type", "class PointerHandle", "Typed pointer wrapper for native addresses."},
        {"BufferHandle", "type", "class BufferHandle", "Managed native byte buffer with pin support."},
        {"Ptr", "type", "class Ptr<T>", "Semantic pointer family for ABI-aware FFI handles."},
        {"Ptr32", "type", "class Ptr32<T>", "32-bit semantic pointer family for ABI-aware FFI handles."},
        {"Ptr64", "type", "class Ptr64<T>", "64-bit semantic pointer family for ABI-aware FFI handles."},
        {"Char", "type", "struct Char", "Single-byte ABI character wrapper used by FFI metadata."},
        {"WChar", "type", "struct WChar", "Wide ABI character wrapper used by FFI metadata."},
};

static const TZrChar g_ffi_type_hints_json[] =
        "{\n"
        "  \"schema\": \"zr.native.hints/v1\",\n"
        "  \"module\": \"zr.ffi\",\n"
        "  \"functions\": [\n"
        "    {\"name\":\"loadLibrary\",\"signature\":\"loadLibrary(path: string): LibraryHandle\"},\n"
        "    {\"name\":\"callback\",\"signature\":\"callback(signature: object, fn: function): CallbackHandle\"},\n"
        "    {\"name\":\"sizeof\",\"signature\":\"sizeof(type: object): int\"},\n"
        "    {\"name\":\"alignof\",\"signature\":\"alignof(type: object): int\"},\n"
        "    {\"name\":\"nullPointer\",\"signature\":\"nullPointer(type: object): Ptr<void>\"}\n"
        "  ],\n"
        "  \"types\": [\n"
        "    {\"name\":\"LibraryHandle\",\"kind\":\"class\"},\n"
        "    {\"name\":\"SymbolHandle\",\"kind\":\"class\"},\n"
        "    {\"name\":\"CallbackHandle\",\"kind\":\"class\"},\n"
        "    {\"name\":\"PointerHandle\",\"kind\":\"class\"},\n"
        "    {\"name\":\"BufferHandle\",\"kind\":\"class\"},\n"
        "    {\"name\":\"Ptr\",\"kind\":\"class\"},\n"
        "    {\"name\":\"Ptr32\",\"kind\":\"class\"},\n"
        "    {\"name\":\"Ptr64\",\"kind\":\"class\"},\n"
        "    {\"name\":\"Char\",\"kind\":\"struct\"},\n"
        "    {\"name\":\"WChar\",\"kind\":\"struct\"}\n"
        "  ]\n"
        "}\n";

static const ZrLibModuleDescriptor g_ffi_module_descriptor = {
        ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        "zr.ffi",
        ZR_NULL,
        0,
        g_ffi_functions,
        ZR_ARRAY_COUNT(g_ffi_functions),
        g_ffi_types,
        ZR_ARRAY_COUNT(g_ffi_types),
        g_ffi_hints,
        ZR_ARRAY_COUNT(g_ffi_hints),
        g_ffi_type_hints_json,
        "General-purpose foreign-function interface backed by libffi when available.",
        ZR_NULL,
        0,
        "1.0.0",
        ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
        (TZrUInt64) (ZR_LIB_MODULE_CAPABILITY_TYPE_HINTS | ZR_LIB_MODULE_CAPABILITY_TYPE_METADATA |
                     ZR_LIB_MODULE_CAPABILITY_SAFE_CALL_HELPERS | ZR_LIB_MODULE_CAPABILITY_FFI_RUNTIME),
};

const ZrLibModuleDescriptor *ZrVmLibFfiRuntime_GetModuleDescriptor(void) { return &g_ffi_module_descriptor; }

const ZrLibModuleDescriptor *ZrVmLibFfi_GetModuleDescriptor(void) { return ZrVmLibFfiRuntime_GetModuleDescriptor(); }

TZrBool ZrVmLibFfi_Register(SZrGlobalState *global) {
    return ZrLibrary_NativeRegistry_RegisterModule(global, ZrVmLibFfiRuntime_GetModuleDescriptor());
}

#if defined(ZR_LIBRARY_TYPE_SHARED)
const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void) { return ZrVmLibFfiRuntime_GetModuleDescriptor(); }
#endif
