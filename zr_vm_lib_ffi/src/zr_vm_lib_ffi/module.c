//
// Built-in zr.ffi native module registration.
//

#include "zr_vm_lib_ffi/module.h"

#include "zr_vm_lib_ffi/runtime.h"

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

static const ZrLibFunctionDescriptor g_ffi_functions[] = {
        {"loadLibrary", 1, 1, ZrFfi_LoadLibrary, "LibraryHandle",
         "Load a dynamic library by absolute or relative path.", ZR_NULL, 0},
        {"callback", 2, 2, ZrFfi_CreateCallback, "CallbackHandle",
         "Create a C-callable callback trampoline from a zr closure.", ZR_NULL, 0},
        {"sizeof", 1, 1, ZrFfi_SizeOf, "int", "Return the byte size of a runtime FFI type descriptor.", ZR_NULL, 0},
        {"alignof", 1, 1, ZrFfi_AlignOf, "int", "Return the alignment of a runtime FFI type descriptor.", ZR_NULL, 0},
        {"nullPointer", 1, 1, ZrFfi_NullPointer, "PointerHandle", "Create a typed null pointer handle.", ZR_NULL, 0},
};

static const ZrLibMethodDescriptor g_library_methods[] = {
        {"close", 0, 0, ZrFfi_Library_Close, "null", "Close the library handle. Idempotent.", ZR_FALSE, ZR_NULL, 0},
        {"isClosed", 0, 0, ZrFfi_Library_IsClosed, "bool", "Return whether the library handle has been closed.",
         ZR_FALSE, ZR_NULL, 0},
        {"getSymbol", 2, 2, ZrFfi_Library_GetSymbol, "SymbolHandle", "Resolve and compile a typed symbol handle.",
         ZR_FALSE, ZR_NULL, 0},
        {"getVersion", 0, 1, ZrFfi_Library_GetVersion, "string", "Read a version string exported by the library.",
         ZR_FALSE, ZR_NULL, 0},
};

static const ZrLibMethodDescriptor g_symbol_methods[] = {
        {"call", 1, 1, ZrFfi_Symbol_Call, "value", "Call the compiled symbol with an argument array.", ZR_FALSE, ZR_NULL, 0},
};

static const ZrLibMetaMethodDescriptor g_symbol_meta_methods[] = {
        {ZR_META_CALL, 0, (TZrUInt16)0xFFFFu, ZrFfi_Symbol_MetaCall, "value",
         "Call the compiled symbol directly with positional arguments.", ZR_NULL, 0},
};

static const ZrLibMethodDescriptor g_callback_methods[] = {
        {"close", 0, 0, ZrFfi_Callback_Close, "null", "Close the callback handle. Idempotent.", ZR_FALSE, ZR_NULL, 0},
};

static const ZrLibMethodDescriptor g_pointer_methods[] = {
        {"as", 1, 1, ZrFfi_Pointer_As, "PointerHandle",
         "Reinterpret the pointer handle with a different type descriptor.", ZR_FALSE, ZR_NULL, 0},
        {"read", 1, 1, ZrFfi_Pointer_Read, "value",
         "Read a value through the pointer using the provided type descriptor.", ZR_FALSE, ZR_NULL, 0},
        {"close", 0, 0, ZrFfi_Pointer_Close, "null", "Release any ownership held by the pointer handle.", ZR_FALSE, ZR_NULL, 0},
};

static const ZrLibMethodDescriptor g_buffer_methods[] = {
        {"allocate", 1, 1, ZrFfi_Buffer_Allocate, "BufferHandle", "Allocate a managed native buffer.", ZR_TRUE, ZR_NULL, 0},
        {"close", 0, 0, ZrFfi_Buffer_Close, "null", "Close the buffer handle. Idempotent.", ZR_FALSE, ZR_NULL, 0},
        {"pin", 0, 0, ZrFfi_Buffer_Pin, "PointerHandle", "Pin the buffer and return a pointer view.", ZR_FALSE, ZR_NULL, 0},
        {"read", 2, 2, ZrFfi_Buffer_Read, "array", "Read a byte range from the buffer.", ZR_FALSE, ZR_NULL, 0},
        {"write", 2, 2, ZrFfi_Buffer_Write, "int", "Write bytes into the buffer from an array.", ZR_FALSE, ZR_NULL, 0},
        {"slice", 2, 2, ZrFfi_Buffer_Slice, "BufferHandle", "Create a copied slice of the buffer.", ZR_FALSE, ZR_NULL, 0},
};

static const ZrLibTypeDescriptor g_ffi_types[] = {
        {"LibraryHandle", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0, g_library_methods,
         ZR_ARRAY_COUNT(g_library_methods), ZR_NULL, 0, "Managed dynamic-library handle.", ZR_NULL, ZR_NULL, 0, ZR_NULL,
         0, ZR_NULL, ZR_FALSE, ZR_TRUE, "LibraryHandle(path: string)", ZR_NULL, 0},
        {"SymbolHandle", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0, g_symbol_methods, ZR_ARRAY_COUNT(g_symbol_methods),
         g_symbol_meta_methods, ZR_ARRAY_COUNT(g_symbol_meta_methods),
         "Typed symbol handle compiled from a library export and ABI signature.", ZR_NULL, ZR_NULL, 0,
         ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_TRUE, "SymbolHandle()", ZR_NULL, 0},
        {"CallbackHandle", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0, g_callback_methods,
         ZR_ARRAY_COUNT(g_callback_methods), ZR_NULL, 0, "C-callable callback trampoline rooted in the zr VM.", ZR_NULL,
         ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_TRUE, "CallbackHandle(signature: object, fn: function)", ZR_NULL, 0},
        {"PointerHandle", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0, g_pointer_methods,
         ZR_ARRAY_COUNT(g_pointer_methods), ZR_NULL, 0, "Typed pointer wrapper for native addresses.", ZR_NULL, ZR_NULL,
         0, ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_TRUE, "PointerHandle(type: object)", ZR_NULL, 0},
        {"BufferHandle", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0, g_buffer_methods, ZR_ARRAY_COUNT(g_buffer_methods),
         ZR_NULL, 0, "Managed native byte buffer with pin support.", ZR_NULL, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_FALSE,
         ZR_TRUE, "BufferHandle(size: int)", ZR_NULL, 0},
};

static const ZrLibTypeHintDescriptor g_ffi_hints[] = {
        {"loadLibrary", "function", "loadLibrary(path: string): LibraryHandle", "Load a dynamic library by path."},
        {"callback", "function", "callback(signature: object, fn: function): CallbackHandle",
         "Create a C-callable callback trampoline."},
        {"sizeof", "function", "sizeof(type: object): int", "Return the byte size of a runtime FFI type descriptor."},
        {"alignof", "function", "alignof(type: object): int", "Return the alignment of a runtime FFI type descriptor."},
        {"nullPointer", "function", "nullPointer(type: object): PointerHandle", "Create a typed null pointer handle."},
        {"LibraryHandle", "type", "class LibraryHandle", "Managed dynamic-library handle."},
        {"SymbolHandle", "type", "class SymbolHandle", "Typed symbol handle compiled from an ABI signature."},
        {"CallbackHandle", "type", "class CallbackHandle", "C-callable callback trampoline rooted in the zr VM."},
        {"PointerHandle", "type", "class PointerHandle", "Typed pointer wrapper for native addresses."},
        {"BufferHandle", "type", "class BufferHandle", "Managed native byte buffer with pin support."},
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
        "    {\"name\":\"nullPointer\",\"signature\":\"nullPointer(type: object): PointerHandle\"}\n"
        "  ],\n"
        "  \"types\": [\n"
        "    {\"name\":\"LibraryHandle\",\"kind\":\"class\"},\n"
        "    {\"name\":\"SymbolHandle\",\"kind\":\"class\"},\n"
        "    {\"name\":\"CallbackHandle\",\"kind\":\"class\"},\n"
        "    {\"name\":\"PointerHandle\",\"kind\":\"class\"},\n"
        "    {\"name\":\"BufferHandle\",\"kind\":\"class\"}\n"
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
