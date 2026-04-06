//
// zr.system.exception descriptor registry.
//

#include "zr_vm_lib_system/exception_registry.h"

#ifndef ZR_ARRAY_COUNT
#define ZR_ARRAY_COUNT(value) (sizeof(value) / sizeof((value)[0]))
#endif

const ZrLibModuleDescriptor *ZrSystem_ExceptionRegistry_GetModule(void) {
    static const ZrLibFieldDescriptor kErrorFields[] = {
            ZR_LIB_FIELD_DESCRIPTOR_INIT("message", "string", "Human-readable exception message."),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("stacks", "StackFrame[]", "Structured stack frames, throw-site first."),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("exception", "value", "Original thrown payload or exception object."),
    };
    static const ZrLibFieldDescriptor kStackFrameFields[] = {
            ZR_LIB_FIELD_DESCRIPTOR_INIT("functionName", "string", "Function name for this frame."),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("sourceFile", "string", "Source file for this frame."),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("sourceLine", "int", "1-based source line for this frame."),
            ZR_LIB_FIELD_DESCRIPTOR_INIT("instructionOffset", "int", "Instruction offset inside the function."),
    };
    static const ZrLibMetaMethodDescriptor kExceptionConstructors[] = {
            {ZR_META_CONSTRUCTOR, 0, 1, ZrSystem_Exception_Constructor, "null", "Initialize an exception instance.", ZR_NULL, 0},
    };
    static const ZrLibTypeDescriptor kTypes[] = {
            ZR_LIB_TYPE_DESCRIPTOR_INIT("Error", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, kErrorFields,
                                        ZR_ARRAY_COUNT(kErrorFields), ZR_NULL, 0, kExceptionConstructors,
                                        ZR_ARRAY_COUNT(kExceptionConstructors), "Base VM exception type.", ZR_NULL,
                                        ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE, "Error(message?: any)",
                                        ZR_NULL, 0),
            ZR_LIB_TYPE_DESCRIPTOR_INIT("StackFrame", ZR_OBJECT_PROTOTYPE_TYPE_STRUCT, kStackFrameFields,
                                        ZR_ARRAY_COUNT(kStackFrameFields), ZR_NULL, 0, ZR_NULL, 0,
                                        "Structured VM stack frame.", ZR_NULL, ZR_NULL, 0, ZR_NULL, 0, ZR_NULL,
                                        ZR_TRUE, ZR_TRUE, "StackFrame()", ZR_NULL, 0),
            ZR_LIB_TYPE_DESCRIPTOR_INIT("RuntimeError", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, kErrorFields,
                                        ZR_ARRAY_COUNT(kErrorFields), ZR_NULL, 0, kExceptionConstructors,
                                        ZR_ARRAY_COUNT(kExceptionConstructors), "Runtime execution error.", "Error",
                                        ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE,
                                        "RuntimeError(message?: any)", ZR_NULL, 0),
            ZR_LIB_TYPE_DESCRIPTOR_INIT("IOException", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, kErrorFields,
                                        ZR_ARRAY_COUNT(kErrorFields), ZR_NULL, 0, kExceptionConstructors,
                                        ZR_ARRAY_COUNT(kExceptionConstructors), "Filesystem and stream I/O error.",
                                        "RuntimeError", ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE,
                                        "IOException(message?: any)", ZR_NULL, 0),
            ZR_LIB_TYPE_DESCRIPTOR_INIT("TypeError", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, kErrorFields,
                                        ZR_ARRAY_COUNT(kErrorFields), ZR_NULL, 0, kExceptionConstructors,
                                        ZR_ARRAY_COUNT(kExceptionConstructors), "Type mismatch error.", "Error",
                                        ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE,
                                        "TypeError(message?: any)", ZR_NULL, 0),
            ZR_LIB_TYPE_DESCRIPTOR_INIT("MemoryError", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, kErrorFields,
                                        ZR_ARRAY_COUNT(kErrorFields), ZR_NULL, 0, kExceptionConstructors,
                                        ZR_ARRAY_COUNT(kExceptionConstructors), "Allocation or memory pressure error.",
                                        "Error", ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE,
                                        "MemoryError(message?: any)", ZR_NULL, 0),
            ZR_LIB_TYPE_DESCRIPTOR_INIT("ExceptionError", ZR_OBJECT_PROTOTYPE_TYPE_CLASS, kErrorFields,
                                        ZR_ARRAY_COUNT(kErrorFields), ZR_NULL, 0, kExceptionConstructors,
                                        ZR_ARRAY_COUNT(kExceptionConstructors), "Generic VM exception state error.",
                                        "Error", ZR_NULL, 0, ZR_NULL, 0, ZR_NULL, ZR_TRUE, ZR_TRUE,
                                        "ExceptionError(message?: any)", ZR_NULL, 0),
    };
    static const ZrLibFunctionDescriptor kFunctions[] = {
            {"registerUnhandledException",
             1,
             1,
             ZrSystem_Exception_RegisterUnhandledException,
             "null",
             "Register a single global unhandled exception observer.",
             ZR_NULL,
             0},
    };
    static const ZrLibTypeHintDescriptor kHints[] = {
            {"registerUnhandledException",
             "function",
             "registerUnhandledException(handler: (error: Error) -> bool): null",
             "Register a single global unhandled exception observer."},
            {"Error", "type", "class Error { message, stacks, exception }", "Base VM exception type."},
            {"StackFrame",
             "type",
             "struct StackFrame { functionName, sourceFile, sourceLine, instructionOffset }",
             "Structured VM stack frame."},
            {"RuntimeError", "type", "class RuntimeError extends Error", "Runtime execution error."},
            {"IOException", "type", "class IOException extends RuntimeError", "Filesystem and stream I/O error."},
            {"TypeError", "type", "class TypeError extends Error", "Type mismatch error."},
            {"MemoryError", "type", "class MemoryError extends Error", "Allocation or memory pressure error."},
            {"ExceptionError", "type", "class ExceptionError extends Error", "Generic VM exception state error."},
    };
    static const TZrChar kHintsJson[] =
            "{\n"
            "  \"schema\": \"zr.native.hints/v1\",\n"
            "  \"module\": \"zr.system.exception\"\n"
            "}\n";
    static const ZrLibModuleDescriptor kModule = {
            ZR_VM_NATIVE_PLUGIN_ABI_VERSION,
            "zr.system.exception",
            ZR_NULL,
            0,
            kFunctions,
            ZR_ARRAY_COUNT(kFunctions),
            kTypes,
            ZR_ARRAY_COUNT(kTypes),
            kHints,
            ZR_ARRAY_COUNT(kHints),
            kHintsJson,
            "Exception hierarchy and unhandled exception hooks.",
            ZR_NULL,
            0,
            "1.0.0",
            ZR_VM_NATIVE_RUNTIME_ABI_VERSION,
            0,
    };

    return &kModule;
}
