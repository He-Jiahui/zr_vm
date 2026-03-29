//
// Built-in zr.math native module registration.
//

#include "zr_vm_lib_math/module.h"

#include "zr_vm_lib_math/complex_registry.h"
#include "zr_vm_lib_math/math_common.h"
#include "zr_vm_lib_math/matrix3x3_registry.h"
#include "zr_vm_lib_math/matrix4x4_registry.h"
#include "zr_vm_lib_math/quaternion_registry.h"
#include "zr_vm_lib_math/scalar_registry.h"
#include "zr_vm_lib_math/tensor_registry.h"
#include "zr_vm_lib_math/vector2_registry.h"
#include "zr_vm_lib_math/vector3_registry.h"
#include "zr_vm_lib_math/vector4_registry.h"

#include <math.h>
#include <string.h>

enum {
    ZR_MATH_MAX_FUNCTIONS = 48,
    ZR_MATH_MAX_TYPES = 8,
    ZR_MATH_MAX_HINTS = 32
};

static const ZrLibConstantDescriptor g_math_constants[] = {
        {"PI", ZR_LIB_CONSTANT_KIND_FLOAT, 0, M_PI, ZR_NULL, ZR_FALSE, ZR_NULL},
        {"TAU", ZR_LIB_CONSTANT_KIND_FLOAT, 0, M_PI * 2.0, ZR_NULL, ZR_FALSE, ZR_NULL},
        {"E", ZR_LIB_CONSTANT_KIND_FLOAT, 0, 2.71828182845904523536, ZR_NULL, ZR_FALSE, ZR_NULL},
        {"EPSILON", ZR_LIB_CONSTANT_KIND_FLOAT, 0, ZR_MATH_EPSILON, ZR_NULL, ZR_FALSE, ZR_NULL},
        {"INF", ZR_LIB_CONSTANT_KIND_FLOAT, 0, INFINITY, ZR_NULL, ZR_FALSE, ZR_NULL},
        {"NAN", ZR_LIB_CONSTANT_KIND_FLOAT, 0, NAN, ZR_NULL, ZR_FALSE, ZR_NULL},
};

static ZrLibFunctionDescriptor g_math_functions[ZR_MATH_MAX_FUNCTIONS];
static ZrLibTypeDescriptor g_math_types[ZR_MATH_MAX_TYPES];
static ZrLibTypeHintDescriptor g_math_hints[ZR_MATH_MAX_HINTS];
static ZrLibModuleDescriptor g_math_module_descriptor;
static TZrBool g_math_initialized = ZR_FALSE;

static const TZrChar *g_math_type_hints_json =
        "{\n"
        "  \"schema\": \"zr.native.hints/v1\",\n"
        "  \"module\": \"zr.math\",\n"
        "  \"functions\": [\n"
        "    {\"name\":\"abs\",\"signature\":\"abs(value: float): float\"},\n"
        "    {\"name\":\"invokeCallback\",\"signature\":\"invokeCallback(callback: function, value: float): float\"}\n"
        "  ],\n"
        "  \"types\": [\n"
        "    {\"name\":\"Vector2\",\"kind\":\"struct\"},\n"
        "    {\"name\":\"Vector3\",\"kind\":\"struct\"},\n"
        "    {\"name\":\"Vector4\",\"kind\":\"struct\"},\n"
        "    {\"name\":\"Quaternion\",\"kind\":\"struct\"},\n"
        "    {\"name\":\"Complex\",\"kind\":\"struct\"},\n"
        "    {\"name\":\"Matrix3x3\",\"kind\":\"struct\"},\n"
        "    {\"name\":\"Matrix4x4\",\"kind\":\"struct\"},\n"
        "    {\"name\":\"Tensor\",\"kind\":\"class\"}\n"
        "  ]\n"
        "}\n";

static void zr_math_append_functions(const ZrLibFunctionDescriptor *descriptors, TZrSize count, TZrSize *offset) {
    if (descriptors == ZR_NULL || offset == ZR_NULL || count == 0) {
        return;
    }
    memcpy(&g_math_functions[*offset], descriptors, sizeof(ZrLibFunctionDescriptor) * count);
    *offset += count;
}

static void zr_math_append_type(const ZrLibTypeDescriptor *descriptor, TZrSize *offset) {
    if (descriptor == ZR_NULL || offset == ZR_NULL) {
        return;
    }
    g_math_types[*offset] = *descriptor;
    (*offset)++;
}

static void zr_math_append_hints(const ZrLibTypeHintDescriptor *descriptors, TZrSize count, TZrSize *offset) {
    if (descriptors == ZR_NULL || offset == ZR_NULL || count == 0) {
        return;
    }
    memcpy(&g_math_hints[*offset], descriptors, sizeof(ZrLibTypeHintDescriptor) * count);
    *offset += count;
}

#define ZR_MATH_APPEND_FUNCTION_REGISTRY(getter)             \
    do {                                                     \
        const ZrLibFunctionDescriptor *descriptors = getter(&count); \
        zr_math_append_functions(descriptors, count, &offset);        \
    } while (0)

#define ZR_MATH_APPEND_HINT_REGISTRY(getter)                 \
    do {                                                     \
        const ZrLibTypeHintDescriptor *descriptors = getter(&count);  \
        zr_math_append_hints(descriptors, count, &offset);            \
    } while (0)

static void zr_math_initialize_module_descriptor(void) {
    TZrSize count = 0;
    TZrSize offset = 0;

    if (g_math_initialized) {
        return;
    }

    ZR_MATH_APPEND_FUNCTION_REGISTRY(ZrMath_ScalarRegistry_GetFunctions);
    ZR_MATH_APPEND_FUNCTION_REGISTRY(ZrMath_Vector2Registry_GetFunctions);
    ZR_MATH_APPEND_FUNCTION_REGISTRY(ZrMath_Vector3Registry_GetFunctions);
    ZR_MATH_APPEND_FUNCTION_REGISTRY(ZrMath_Vector4Registry_GetFunctions);
    ZR_MATH_APPEND_FUNCTION_REGISTRY(ZrMath_QuaternionRegistry_GetFunctions);
    ZR_MATH_APPEND_FUNCTION_REGISTRY(ZrMath_ComplexRegistry_GetFunctions);
    ZR_MATH_APPEND_FUNCTION_REGISTRY(ZrMath_Matrix3x3Registry_GetFunctions);
    ZR_MATH_APPEND_FUNCTION_REGISTRY(ZrMath_Matrix4x4Registry_GetFunctions);
    ZR_MATH_APPEND_FUNCTION_REGISTRY(ZrMath_TensorRegistry_GetFunctions);
    g_math_module_descriptor.functions = g_math_functions;
    g_math_module_descriptor.functionCount = offset;

    offset = 0;
    zr_math_append_type(ZrMath_Vector2Registry_GetType(), &offset);
    zr_math_append_type(ZrMath_Vector3Registry_GetType(), &offset);
    zr_math_append_type(ZrMath_Vector4Registry_GetType(), &offset);
    zr_math_append_type(ZrMath_QuaternionRegistry_GetType(), &offset);
    zr_math_append_type(ZrMath_ComplexRegistry_GetType(), &offset);
    zr_math_append_type(ZrMath_Matrix3x3Registry_GetType(), &offset);
    zr_math_append_type(ZrMath_Matrix4x4Registry_GetType(), &offset);
    zr_math_append_type(ZrMath_TensorRegistry_GetType(), &offset);
    g_math_module_descriptor.types = g_math_types;
    g_math_module_descriptor.typeCount = offset;

    offset = 0;
    ZR_MATH_APPEND_HINT_REGISTRY(ZrMath_ScalarRegistry_GetHints);
    ZR_MATH_APPEND_HINT_REGISTRY(ZrMath_Vector2Registry_GetHints);
    ZR_MATH_APPEND_HINT_REGISTRY(ZrMath_Vector3Registry_GetHints);
    ZR_MATH_APPEND_HINT_REGISTRY(ZrMath_Vector4Registry_GetHints);
    ZR_MATH_APPEND_HINT_REGISTRY(ZrMath_QuaternionRegistry_GetHints);
    ZR_MATH_APPEND_HINT_REGISTRY(ZrMath_ComplexRegistry_GetHints);
    ZR_MATH_APPEND_HINT_REGISTRY(ZrMath_Matrix3x3Registry_GetHints);
    ZR_MATH_APPEND_HINT_REGISTRY(ZrMath_Matrix4x4Registry_GetHints);
    ZR_MATH_APPEND_HINT_REGISTRY(ZrMath_TensorRegistry_GetHints);
    g_math_module_descriptor.typeHints = g_math_hints;
    g_math_module_descriptor.typeHintCount = offset;

    g_math_module_descriptor.abiVersion = ZR_VM_NATIVE_PLUGIN_ABI_VERSION;
    g_math_module_descriptor.moduleName = "zr.math";
    g_math_module_descriptor.constants = g_math_constants;
    g_math_module_descriptor.constantCount = ZR_ARRAY_COUNT(g_math_constants);
    g_math_module_descriptor.typeHintsJson = g_math_type_hints_json;
    g_math_module_descriptor.documentation =
            "Built-in numeric algorithms, vector and matrix types, complex values, quaternions and tensors.";

    g_math_initialized = ZR_TRUE;
}

#undef ZR_MATH_APPEND_FUNCTION_REGISTRY
#undef ZR_MATH_APPEND_HINT_REGISTRY

const ZrLibModuleDescriptor *ZrVmLibMath_GetModuleDescriptor(void) {
    zr_math_initialize_module_descriptor();
    return &g_math_module_descriptor;
}

TZrBool ZrVmLibMath_Register(SZrGlobalState *global) {
    zr_math_initialize_module_descriptor();
    return ZrLibrary_NativeRegistry_RegisterModule(global, &g_math_module_descriptor);
}

#if defined(ZR_LIBRARY_TYPE_SHARED)
const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void) {
    return ZrVmLibMath_GetModuleDescriptor();
}
#endif
