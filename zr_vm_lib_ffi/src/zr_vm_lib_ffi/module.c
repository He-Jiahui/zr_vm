//
// Built-in zr.ffi native module registration.
//

#include "zr_vm_lib_ffi/module.h"

#include "zr_vm_lib_ffi/runtime.h"

#include "zr_vm_common/zr_contract_conf.h"

/* T 只标识脚本中的 ABI-aware 指针族；实际地址、pin 与 owner 仍由 PointerHandle 管理。 */
static const ZrLibGenericParameterDescriptor kPointerGenericParameters[] = {{"T", ZR_NULL, ZR_NULL, 0}};

/* length 投影 runtime 的 pinned byteLength，供 zr.container 的连续视图契约读取；
 * 它不是可由脚本任意扩大 native 内存访问范围的授权。
 */
static const ZrLibFieldDescriptor g_pinned_pointer_fields[] = {
        ZR_LIB_FIELD_DESCRIPTOR_ROLE_INIT(
                "length",
                "int",
                "Pinned byte length available to contiguous views.",
                ZR_MEMBER_CONTRACT_ROLE_INDEX_LENGTH),
};

static const ZrLibParameterDescriptor g_pointer_index_parameters[] = {
        {"index", "int", "Zero-based byte index."},
};

static const ZrLibParameterDescriptor g_pointer_index_value_parameters[] = {
        {"index", "int", "Zero-based byte index."},
        {"value", "u8", "Replacement byte value."},
};

/* 脚本导入 zr.ffi 后的顶层入口；无 libffi 构建仍可解析类型，
 * callback 与实际 symbol 调用会在运行时明确拒绝。
 */
static const ZrLibFunctionDescriptor g_ffi_functions[] = {
        {"loadLibrary", 1, 1, ZrFfi_LoadLibrary, "LibraryHandle",
         "Load a dynamic library by absolute or relative path.", ZR_NULL, 0},
        {"callback", 2, 2, ZrFfi_CreateCallback, "CallbackHandle",
         "Create a C-callable callback trampoline from a zr closure.", ZR_NULL, 0},
        {"sizeof", 1, 1, ZrFfi_SizeOf, "int", "Return the byte size of a runtime FFI type descriptor.", ZR_NULL, 0},
        {"alignof", 1, 1, ZrFfi_AlignOf, "int", "Return the alignment of a runtime FFI type descriptor.", ZR_NULL, 0},
        {"nullPointer", 1, 1, ZrFfi_NullPointer, "Ptr<void>", "Create a typed null pointer handle.", ZR_NULL, 0},
};

/* LibraryHandle 的逻辑关闭、动态签名与 source extern 保留契约共用同一库 owner。
 * TODO: getContractSymbol 已公开为 index: int，编译器也确实传 retained contract 索引；
 * 核实是否允许脚本手动调用依赖活动调用帧的入口，或只保留给编译器生成路径。
 * 参见 compiler_extern_declaration.c 的 symbolArguments[0] 与 runtime.c 的 ReadInt。
 */
static const ZrLibMethodDescriptor g_library_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("close", 0, 0, ZrFfi_Library_Close, "null",
                                      "Close the library handle. Idempotent.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("isClosed", 0, 0, ZrFfi_Library_IsClosed, "bool",
                                      "Return whether the library handle has been closed.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("getSymbol", 2, 2, ZrFfi_Library_GetSymbol, "SymbolHandle",
                                      "Resolve and compile a typed symbol handle.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("getContractSymbol", 1, 1, ZrFfi_Library_GetContractSymbol, "SymbolHandle",
                                      "Resolve a static native import from its retained contract.", ZR_FALSE,
                                      ZR_NULL, 0),
        /* TODO: getVersion 元数据声明 string，但缺失版本导出时返回 null；
         * 确认 returnTypeName 能否表示可空结果，以及编译器据此作出的推断。 */
        ZR_LIB_METHOD_DESCRIPTOR_INIT("getVersion", 0, 1, ZrFfi_Library_GetVersion, "string",
                                      "Read a version string exported by the library.", ZR_FALSE, ZR_NULL, 0),
};

static const ZrLibMethodDescriptor g_symbol_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("call", 1, 1, ZrFfi_Symbol_Call, "value",
                                      "Call the compiled symbol with an argument array.", ZR_FALSE, ZR_NULL, 0),
};

/* @call 让 symbol(a, b) 与 call([a, b]) 共用同一 ABI 编组与 owner 检查。 */
static const ZrLibMetaMethodDescriptor g_symbol_meta_methods[] = {
        {ZR_META_CALL, 0, (TZrUInt16)0xFFFFu, ZrFfi_Symbol_MetaCall, "value",
         "Call the compiled symbol directly with positional arguments.", ZR_NULL, 0},
};

static const ZrLibMethodDescriptor g_callback_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("close", 0, 0, ZrFfi_Callback_Close, "null",
                                      "Close the callback handle. Idempotent.", ZR_FALSE, ZR_NULL, 0),
};

/* 基础 PointerHandle 保留 byte 下标与 typed read 两种不同访问契约；
 * span 只有在显式 pin 长度有效时才向 zr.container 借出连续视图。
 */
static const ZrLibMethodDescriptor g_pointer_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_INIT("as", 1, 1, ZrFfi_Pointer_As, "Ptr<void>",
                                      "Reinterpret the pointer handle with a different type descriptor.", ZR_FALSE,
                                      ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("read", 1, 1, ZrFfi_Pointer_Read, "value",
                                      "Read a value through the pointer using the provided type descriptor.", ZR_FALSE,
                                      ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_INIT("close", 0, 0, ZrFfi_Pointer_Close, "null",
                                      "Release any ownership held by the pointer handle.", ZR_FALSE, ZR_NULL, 0),
        ZR_LIB_METHOD_DESCRIPTOR_ROLE_INIT(
                "span",
                0,
                0,
                ZrFfi_Pointer_Span,
                "Span<u8>",
                "Borrow the explicitly pinned bytes as a contiguous view.",
                ZR_FALSE,
                ZR_NULL,
                0,
                ZR_MEMBER_CONTRACT_ROLE_CONTIGUOUS_VIEW_CREATE),
};

/* 泛型指针族只补充 Span<T> 的类型投影，实际回调和内存所有权仍归基础指针处理。 */
static const ZrLibMethodDescriptor g_ptr_methods[] = {
        ZR_LIB_METHOD_DESCRIPTOR_ROLE_INIT(
                "span",
                0,
                0,
                ZrFfi_Pointer_Span,
                "Span<T>",
                "Borrow the explicitly pinned address as a contiguous view.",
                ZR_FALSE,
                ZR_NULL,
                0,
                ZR_MEMBER_CONTRACT_ROLE_CONTIGUOUS_VIEW_CREATE),
};

/* [] 始终按 pinned 字节索引；Ptr<T> 的 T 不会自动改变元素宽度。 */
static const ZrLibMetaMethodDescriptor g_pointer_meta_methods[] = {
        {.metaType = ZR_META_GET_ITEM,
         .minArgumentCount = 1u,
         .maxArgumentCount = 1u,
         .callback = ZrFfi_Pointer_GetItem,
         .returnTypeName = "u8",
         .documentation = "Read a pinned byte.",
         .parameters = g_pointer_index_parameters,
         .parameterCount = ZR_ARRAY_COUNT(g_pointer_index_parameters)},
        {.metaType = ZR_META_SET_ITEM,
         .minArgumentCount = 2u,
         .maxArgumentCount = 2u,
         .callback = ZrFfi_Pointer_SetItem,
         .returnTypeName = "null",
         .documentation = "Write a pinned byte.",
         .parameters = g_pointer_index_value_parameters,
         .parameterCount = ZR_ARRAY_COUNT(g_pointer_index_value_parameters)},
};

/* BufferHandle 拥有 native 存储；pin 借地址，slice 复制存储，二者的寿命不同。 */
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

/* 这些元数据同时被 native registry、编译器类型推断和 IDE 消费；
 * ownerMode 与 contiguous protocol 必须与 runtime 的 pin/finalizer 行为一致。
 */
static const ZrLibTypeDescriptor g_ffi_types[] = {
        /* LibraryHandle 逻辑关闭后不再允许查找或调用；已有 SymbolHandle 延迟实际卸载。 */
        ZR_LIB_TYPE_DESCRIPTOR_INIT("LibraryHandle", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0, g_library_methods,
                                    ZR_ARRAY_COUNT(g_library_methods), ZR_NULL, 0, "Managed dynamic-library handle.",
                                    ZR_NULL, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_TRUE,
                                    "LibraryHandle(path: string)", ZR_NULL, 0),
        /* native 地址和 ABI signature 必须与持有它的 LibraryHandle 一同存活。 */
        ZR_LIB_TYPE_DESCRIPTOR_INIT("SymbolHandle", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0, g_symbol_methods,
                                    ZR_ARRAY_COUNT(g_symbol_methods), g_symbol_meta_methods,
                                    ZR_ARRAY_COUNT(g_symbol_meta_methods),
                                    "Typed symbol handle compiled from a library export and ABI signature.", ZR_NULL,
                                    ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_TRUE, "SymbolHandle()", ZR_NULL, 0),
        /* 对外传出的 C 函数指针只在该 handle 和 callback 策略允许的期间有效。 */
        ZR_LIB_TYPE_DESCRIPTOR_INIT("CallbackHandle", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0, g_callback_methods,
                                    ZR_ARRAY_COUNT(g_callback_methods), ZR_NULL, 0,
                                    "C-callable callback trampoline rooted in the zr VM.", ZR_NULL, ZR_NULL, 0,
                                    ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_TRUE,
                                    "CallbackHandle(signature: object, fn: function)", ZR_NULL, 0),
        /* borrowed 地址可带 BufferHandle pin owner；类型重解释不扩展可访问字节数。 */
        {.name = "PointerHandle",
         .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_CLASS,
         .fields = g_pinned_pointer_fields,
         .fieldCount = ZR_ARRAY_COUNT(g_pinned_pointer_fields),
         .methods = g_pointer_methods,
         .methodCount = ZR_ARRAY_COUNT(g_pointer_methods),
         .metaMethods = g_pointer_meta_methods,
         .metaMethodCount = ZR_ARRAY_COUNT(g_pointer_meta_methods),
         .documentation = "Typed pointer wrapper for native addresses.",
         .allowBoxedConstruction = ZR_TRUE,
         .constructorSignature = "PointerHandle(type: object)",
         .protocolMask = ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_CONTIGUOUS_SOURCE_NATIVE_PINNED),
         .ffiLoweringKind = "pointer",
         .ffiOwnerMode = "borrowed"},
        /* owned 存储在 close 请求后等待已有 pointer pin 释放。 */
        ZR_LIB_TYPE_DESCRIPTOR_FFI_INIT("BufferHandle", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, ZR_NULL, 0, g_buffer_methods,
                                        ZR_ARRAY_COUNT(g_buffer_methods), ZR_NULL, 0,
                                        "Managed native byte buffer with pin support.", ZR_NULL, ZR_NULL, 0, ZR_NULL,
                                        0, ZR_NULL, ZR_FALSE, ZR_TRUE, "BufferHandle(size: int)", ZR_NULL, 0,
                                        "pointer", ZR_NULL, ZR_NULL, "owned", ZR_NULL),
        /* Ptr<T> 保留泛型身份；运行时仍须按实际 ABI 布局验证访问。 */
        {.name = "Ptr",
         .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_CLASS,
         .fields = g_pinned_pointer_fields,
         .fieldCount = ZR_ARRAY_COUNT(g_pinned_pointer_fields),
         .methods = g_ptr_methods,
         .methodCount = ZR_ARRAY_COUNT(g_ptr_methods),
         .documentation = "Semantic pointer family for ABI-aware FFI handles.",
         .extendsTypeName = "PointerHandle",
         .genericParameters = kPointerGenericParameters,
         .genericParameterCount = ZR_ARRAY_COUNT(kPointerGenericParameters),
         .protocolMask = ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_CONTIGUOUS_SOURCE_NATIVE_PINNED),
         .ffiLoweringKind = "pointer",
         .ffiOwnerMode = "borrowed"},
        /* Ptr32 是 32 位 ABI 语义类型，不能把其名称当作当前进程地址宽度保证。 */
        {.name = "Ptr32",
         .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_CLASS,
         .fields = g_pinned_pointer_fields,
         .fieldCount = ZR_ARRAY_COUNT(g_pinned_pointer_fields),
         .methods = g_ptr_methods,
         .methodCount = ZR_ARRAY_COUNT(g_ptr_methods),
         .documentation = "32-bit semantic pointer family for ABI-aware FFI handles.",
         .extendsTypeName = "PointerHandle",
         .genericParameters = kPointerGenericParameters,
         .genericParameterCount = ZR_ARRAY_COUNT(kPointerGenericParameters),
         .protocolMask = ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_CONTIGUOUS_SOURCE_NATIVE_PINNED),
         .ffiLoweringKind = "pointer",
         .ffiOwnerMode = "borrowed"},
        /* Ptr64 是 64 位 ABI 语义类型，实际 native 指针宽度仍由目标平台决定。 */
        {.name = "Ptr64",
         .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_CLASS,
         .fields = g_pinned_pointer_fields,
         .fieldCount = ZR_ARRAY_COUNT(g_pinned_pointer_fields),
         .methods = g_ptr_methods,
         .methodCount = ZR_ARRAY_COUNT(g_ptr_methods),
         .documentation = "64-bit semantic pointer family for ABI-aware FFI handles.",
         .extendsTypeName = "PointerHandle",
         .genericParameters = kPointerGenericParameters,
         .genericParameterCount = ZR_ARRAY_COUNT(kPointerGenericParameters),
         .protocolMask = ZR_PROTOCOL_BIT(ZR_PROTOCOL_ID_CONTIGUOUS_SOURCE_NATIVE_PINNED),
         .ffiLoweringKind = "pointer",
         .ffiOwnerMode = "borrowed"},
        ZR_LIB_TYPE_DESCRIPTOR_INIT("Char", ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, 0,
                                    "Single-byte ABI character wrapper used by FFI metadata.", ZR_NULL, ZR_NULL, 0,
                                    ZR_NULL, 0, ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL, 0),
        /* WChar 宽度取决于目标 ABI，不能仅由类型名推断为 16 或 32 位。 */
        ZR_LIB_TYPE_DESCRIPTOR_INIT("WChar", ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, 0,
                                    "Wide ABI character wrapper used by FFI metadata.", ZR_NULL, ZR_NULL, 0, ZR_NULL,
                                    0, ZR_NULL, ZR_FALSE, ZR_FALSE, ZR_NULL, ZR_NULL, 0),
};

/* 人读签名镜像；调整 descriptor 或参数读取契约时须同步这些提示。 */
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

/* 机器可读的 zr.native.hints/v1 镜像，供不直接解析 C descriptor 的工具消费。 */
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

/* pointer.span 需要 zr.container.Span；模块链接声明此类型级依赖。 */
static const ZrLibModuleLinkDescriptor g_ffi_module_links[] = {
        {"container", "zr.container", "Explicit pinned Span contracts."},
};

/* 静态注册、插件 loader 与宿主查询共用这一个 provider contract；
 * requiredCapabilities 把 FFI 所依赖的运行时能力交给 registry 验证。
 */
static const ZrLibModuleDescriptor g_ffi_module_descriptor = {
        .abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
        .moduleName = "zr.ffi",
        .functions = g_ffi_functions,
        .functionCount = ZR_ARRAY_COUNT(g_ffi_functions),
        .types = g_ffi_types,
        .typeCount = ZR_ARRAY_COUNT(g_ffi_types),
        .typeHints = g_ffi_hints,
        .typeHintCount = ZR_ARRAY_COUNT(g_ffi_hints),
        .typeHintsJson = g_ffi_type_hints_json,
        .documentation = "General-purpose foreign-function interface backed by libffi when available.",
        .moduleLinks = g_ffi_module_links,
        .moduleLinkCount = ZR_ARRAY_COUNT(g_ffi_module_links),
        .moduleVersion = "1.0.0",
        .minRuntimeAbi = ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
        .requiredCapabilities =
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
