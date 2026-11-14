// Native registration and declaration rendering regressions from the editor's public projection.
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "zr_vm_core/callback.h"
#include "zr_vm_core/global.h"
#include "zr_vm_core/state.h"
#include "zr_vm_core/string.h"
#include "zr_vm_lib_math/module.h"
#include "zr_vm_lib_container/module.h"
#include "zr_vm_parser/parser.h"
#include "features/lsp_virtual_documents.h"
#include "metadata/lsp_native_declaration_projection.h"

static int failures;
static unsigned int callables_checked;

static TZrPtr test_allocator(TZrPtr userData, TZrPtr pointer, TZrSize oldSize,
                             TZrSize newSize, TZrInt64 flags) {
    ZR_UNUSED_PARAMETER(userData);
    ZR_UNUSED_PARAMETER(oldSize);
    ZR_UNUSED_PARAMETER(flags);
    if (newSize == 0) {
        free(pointer);
        return ZR_NULL;
    }
    return realloc(pointer, newSize);
}

static void check(TZrBool condition, const TZrChar *message) {
    if (!condition) {
        fprintf(stderr, "FAIL: %s\n", message);
        failures++;
    }
}

static const ZrLibTypeDescriptor *find_type(const ZrLibModuleDescriptor *module, const TZrChar *name) {
    for (TZrSize index = 0; index < module->typeCount; index++) {
        if (strcmp(module->types[index].name, name) == 0) return &module->types[index];
    }
    return ZR_NULL;
}

/* Every argument-bearing native row must register its actual parameters, independently of hint text. */
static void check_parameters(const TZrChar *owner, const TZrChar *name,
                              const ZrLibParameterDescriptor *parameters, TZrSize count,
                              TZrUInt16 maximum) {
    callables_checked++;
    if (count != maximum || (count > 0 && parameters == ZR_NULL)) {
        fprintf(stderr, "FAIL: %s.%s has %u parameter descriptors for %u arguments\n",
                owner, name, (unsigned int)count, (unsigned int)maximum);
        failures++;
        return;
    }
    for (TZrSize index = 0; index < count; index++) {
        check(parameters[index].name != ZR_NULL && parameters[index].name[0] != '\0',
              "native parameter name is registered");
        check(parameters[index].typeName != ZR_NULL && parameters[index].typeName[0] != '\0' &&
                      strcmp(parameters[index].typeName, "unknown") != 0 &&
                      strcmp(parameters[index].typeName, "ZrMissingExactType") != 0,
              "native parameter type is registered");
    }
}

static void test_math_parameter_registration(const ZrLibModuleDescriptor *module) {
    for (TZrSize index = 0; index < module->functionCount; index++) {
        const ZrLibFunctionDescriptor *function = &module->functions[index];
        check_parameters(module->moduleName, function->name, function->parameters,
                         function->parameterCount, function->maxArgumentCount);
    }
    for (TZrSize index = 0; index < module->typeCount; index++) {
        const ZrLibTypeDescriptor *type = &module->types[index];
        for (TZrSize methodIndex = 0; methodIndex < type->methodCount; methodIndex++) {
            const ZrLibMethodDescriptor *method = &type->methods[methodIndex];
            check_parameters(type->name, method->name, method->parameters,
                             method->parameterCount, method->maxArgumentCount);
        }
        for (TZrSize metaIndex = 0; metaIndex < type->metaMethodCount; metaIndex++) {
            const ZrLibMetaMethodDescriptor *meta = &type->metaMethods[metaIndex];
            check_parameters(type->name, "meta", meta->parameters, meta->parameterCount, meta->maxArgumentCount);
        }
    }
}

/* Matrix constructor descriptors must preserve the callback's row-major argument order. */
static void test_matrix_constructor(const ZrLibModuleDescriptor *module, const TZrChar *name, TZrSize width) {
    const ZrLibTypeDescriptor *type = find_type(module, name);
    const ZrLibMetaMethodDescriptor *constructor = ZR_NULL;
    check(type != ZR_NULL, "matrix type is registered");
    if (type == ZR_NULL) return;
    for (TZrSize index = 0; index < type->metaMethodCount; index++) {
        if (type->metaMethods[index].metaType == ZR_META_CONSTRUCTOR) constructor = &type->metaMethods[index];
    }
    check(constructor != ZR_NULL && constructor->parameterCount == width * width,
          "matrix constructor registers every component");
    if (constructor == ZR_NULL || constructor->parameterCount != width * width) return;
    for (TZrSize index = 0; index < width * width; index++) {
        char expected[16];
        snprintf(expected, sizeof(expected), "m%u%u", (unsigned int)(index / width), (unsigned int)(index % width));
        check(strcmp(constructor->parameters[index].name, expected) == 0 &&
                      strcmp(constructor->parameters[index].typeName, "float") == 0,
              "matrix component name/type matches its callback position");
    }
    check(constructor->minArgumentCount == 0 && constructor->maxArgumentCount == width * width,
          "parameter registration retains matrix constructor arity");
}

static const TZrChar *render(SZrState *state, const ZrLibModuleDescriptor *descriptor) {
    SZrString *text = ZR_NULL;
    SZrString *uri = ZrLanguageServer_LspVirtualDocuments_CreateDeclarationUri(state, descriptor->moduleName);
    if (!ZrLanguageServer_LspVirtualDocuments_RenderDeclarationText(state, descriptor, uri, &text) || text == ZR_NULL) {
        check(ZR_FALSE, "native declaration renders");
        return "";
    }
    return ZrCore_String_GetNativeString(text);
}

#include "test_native_system_network_parameters.h"

static void test_math_projection(SZrState *state, const ZrLibModuleDescriptor *module) {
    const TZrChar *text = render(state, module);
    check(strstr(text, "pub static fn translation(x: float, y: float, z: float): Matrix4x4;") != ZR_NULL,
          "matrix factory renders registered names and types");
    check(strstr(text, "mulVector(vector: Vector4): Vector4;") != ZR_NULL,
          "matrix vector operand is exact");
    check(strstr(text, "@mul(other: object): object;") != ZR_NULL,
          "matrix multiplication renders its registered object contract");
    check(strstr(text, "pub fn sqrt(value: float): float;") != ZR_NULL,
          "scalar function uses ZR fn syntax and structured parameters");
    check(strstr(text, "@constructor(shape: array, data: array): Tensor;") != ZR_NULL,
          "Tensor constructor describes its two actual array inputs");
    check(strstr(text, "@constructor(/* zero-argument form also available */ m00: float, m01: float,") != ZR_NULL,
          "matrix construction previews the full parameters and retains the zero-argument form");
    check(strstr(text, "/* arity") == ZR_NULL, "math declarations have no arity-only parameter lists");
    check(strstr(text, "ZrMissingExactType") == ZR_NULL, "math declarations have no missing exact types");
}

/* Parse generated type headers with the real grammar, excluding the virtual extern presentation wrapper. */
static void check_type_header_parses(SZrState *state, const TZrChar *text, const TZrChar *prefix) {
    const TZrChar *start = strstr(text, prefix);
    const TZrChar *open = start != ZR_NULL ? strchr(start, '{') : ZR_NULL;
    char source[1024];
    SZrParserState parser;
    SZrAstNode *ast;
    check(open != ZR_NULL && (TZrSize)(open - start) + 3 < sizeof(source), "type header exists");
    if (open == ZR_NULL || (TZrSize)(open - start) + 3 >= sizeof(source)) return;
    memcpy(source, start, (TZrSize)(open - start) + 1);
    strcpy(source + (open - start) + 1, "}");
    ZrParser_State_Init(&parser, state, source, strlen(source), ZR_NULL);
    ast = ZrParser_ParseWithState(&parser);
    check(ast != ZR_NULL && !parser.hasError, "generated inheritance header parses as ZR");
    if (ast != ZR_NULL) ZrParser_Ast_Free(state, ast);
    ZrParser_State_Free(&parser);
}

static void test_inheritance_projection(SZrState *state) {
    static const TZrChar *interfaces[] = {"demo.IReadable<T>", "demo.IWritable<T>"};
    static const ZrLibGenericParameterDescriptor generic[] = {{.name = "T"}};
    static const ZrLibTypeDescriptor types[] = {
        {.name = "Derived", .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_CLASS,
         .extendsTypeName = "demo.Base<T>", .implementsTypeNames = interfaces, .implementsTypeCount = 2,
         .genericParameters = generic, .genericParameterCount = 1},
        {.name = "Value", .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_STRUCT,
         .implementsTypeNames = interfaces, .implementsTypeCount = 2,
         .genericParameters = generic, .genericParameterCount = 1},
        {.name = "Child", .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_CLASS, .extendsTypeName = "demo.Base<int>"},
        {.name = "Plain", .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_CLASS}
    };
    static const ZrLibModuleDescriptor descriptor = {.moduleName = "signature.inheritance", .types = types, .typeCount = 4};
    const TZrChar *text = render(state, &descriptor);
    check(strstr(text, "class Derived<T>: demo.Base<T>, demo.IReadable<T>, demo.IWritable<T> {") != ZR_NULL,
          "base class and interfaces share one colon inheritance list");
    check(strstr(text, "struct Value<T> /* interfaces: demo.IReadable<T>, demo.IWritable<T> */ {") != ZR_NULL,
          "native struct interfaces are exposed as metadata without changing the ZR header grammar");
    check(strstr(text, "class Child: demo.Base<int> {") != ZR_NULL, "base-only inheritance uses a colon");
    check(strstr(text, "class Plain {") != ZR_NULL, "types without inheritance omit the colon");
    check(strstr(text, " implements ") == ZR_NULL && strstr(text, " extends ") == ZR_NULL,
          "foreign inheritance keywords are absent");
    check_type_header_parses(state, text, "pub class Derived");
    check_type_header_parses(state, text, "pub struct Value");
    check_type_header_parses(state, text, "pub class Child");
    check_type_header_parses(state, text, "pub class Plain");
}

static void test_container_projection(SZrState *state) {
    const TZrChar *text = render(state, ZrVmLibContainer_GetModuleDescriptor());
    check(strstr(text, "class Array<T>: zr.builtin.IArrayLike<T>, zr.iteration.Iterable<T> {") != ZR_NULL,
          "Array renders ZR interface inheritance syntax");
    check(strstr(text, "@constructor(capacity: int /* optional */): Array<T>;") != ZR_NULL,
          "Array constructor parameter is an optional capacity");
    check(strstr(text, "pub var source: object;") != ZR_NULL,
          "Span source preserves its explicitly registered object type");
    check(strstr(text, "pub fn add(value: T): null;") != ZR_NULL,
          "ordinary native methods use the ZR fn keyword");
    check_type_header_parses(state, text, "pub class Array");
    check_type_header_parses(state, text, "pub struct Pair");
}

/* Verify that jumping by descriptor identity selects the name in the generated text. */
static void check_declaration_range(SZrState *state, const ZrLibModuleDescriptor *module,
                                    EZrLspVirtualDeclarationKind kind, const void *identity,
                                    const TZrChar *line, TZrSize prefixLength, const TZrChar *name) {
    const TZrChar *text = render(state, module);
    const TZrChar *lineStart = strstr(text, line);
    SZrString *uri = ZrLanguageServer_LspVirtualDocuments_CreateDeclarationUri(state, module->moduleName);
    SZrFileRange range;
    TZrSize nameLength = strlen(name);
    check(lineStart != ZR_NULL &&
                  ZrLanguageServer_LspNativeDeclarationProjection_Find(state, module, uri, kind, identity, &range) &&
                  range.source == uri && range.start.offset == (TZrSize)(lineStart - text) + prefixLength &&
                  range.end.offset == range.start.offset + nameLength &&
                  range.start.column == (TZrInt32)prefixLength + 1 && range.end.line == range.start.line &&
                  strncmp(text + range.start.offset, name, nameLength) == 0,
          "native declaration range selects exactly the callable name");
}

static void test_callable_navigation(SZrState *state, const ZrLibModuleDescriptor *math) {
    const ZrLibModuleDescriptor *container = ZrVmLibContainer_GetModuleDescriptor();
    const ZrLibTypeDescriptor *matrix = find_type(math, "Matrix4x4");
    const ZrLibTypeDescriptor *array = find_type(container, "Array");
    const ZrLibMethodDescriptor *translation = ZR_NULL;
    const ZrLibMetaMethodDescriptor *constructor = ZR_NULL;

    check(matrix != ZR_NULL && array != ZR_NULL, "navigation owner types are registered");
    if (matrix == ZR_NULL || array == ZR_NULL) return;
    for (TZrSize index = 0; index < matrix->methodCount; index++) {
        if (strcmp(matrix->methods[index].name, "translation") == 0) translation = &matrix->methods[index];
    }
    for (TZrSize index = 0; index < array->metaMethodCount; index++) {
        if (array->metaMethods[index].metaType == ZR_META_CONSTRUCTOR) constructor = &array->metaMethods[index];
    }
    check(translation != ZR_NULL && constructor != ZR_NULL, "navigation callables are registered");
    if (translation == ZR_NULL || constructor == ZR_NULL) return;
    check_declaration_range(state, math, ZR_LSP_VIRTUAL_DECLARATION_METHOD, translation,
                            "        pub static fn translation(x: float, y: float, z: float): Matrix4x4;",
                            sizeof("        pub static fn ") - 1, "translation");
    check_declaration_range(state, container, ZR_LSP_VIRTUAL_DECLARATION_META_METHOD, constructor,
                            "        pub @constructor(capacity: int /* optional */): Array<T>;",
                            sizeof("        pub ") - 1, "@constructor");
}

static void test_explicit_and_missing_types(SZrState *state) {
    static const ZrLibFieldDescriptor fields[] = {
        {.name = "registered", .typeName = "object"},
        {.name = "unspecified", .typeName = ZR_NULL},
        {.name = "unresolved", .typeName = "unknown"}
    };
    static const ZrLibTypeDescriptor types[] = {
        {.name = "Probe", .prototypeType = ZR_OBJECT_PROTOTYPE_TYPE_CLASS,
         .fields = fields, .fieldCount = ZR_ARRAY_COUNT(fields)}
    };
    static const ZrLibModuleDescriptor descriptor = {
        .moduleName = "signature.types", .types = types, .typeCount = ZR_ARRAY_COUNT(types)
    };
    const TZrChar *text = render(state, &descriptor);
    check(strstr(text, "registered: object;") != ZR_NULL, "an explicit object contract is preserved");
    check(strstr(text, "unspecified: ZrMissingExactType;") != ZR_NULL &&
                  strstr(text, "unresolved: ZrMissingExactType;") != ZR_NULL,
          "absent and unresolved types remain visible as missing metadata");
}

/* Optional review artifacts contain the exact same text used by the assertions above. */
static void write_preview(SZrState *state, const ZrLibModuleDescriptor *module, const TZrChar *directory) {
    char path[1024];
    int length = snprintf(path, sizeof(path), "%s/%s.zr", directory, module->moduleName);
    FILE *file;
    TZrBool written;
    check(length > 0 && (TZrSize)length < sizeof(path), "preview artifact path fits");
    if (length <= 0 || (TZrSize)length >= sizeof(path)) return;
    file = fopen(path, "wb");
    check(file != ZR_NULL, "preview artifact opens");
    if (file == ZR_NULL) return;
    written = fputs(render(state, module), file) != EOF;
    check(fclose(file) == 0 && written, "preview artifact is written completely");
}

int main(int argc, char **argv) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrCore_GlobalState_New(test_allocator, ZR_NULL, 0, &callbacks);
    SZrState *state;
    const ZrLibModuleDescriptor *math = ZrVmLibMath_GetModuleDescriptor();
    if (global == ZR_NULL || global->mainThreadState == ZR_NULL || math == ZR_NULL) return 1;
    state = global->mainThreadState;
    ZrCore_GlobalState_InitRegistry(state, global);
    test_math_parameter_registration(math);
    test_matrix_constructor(math, "Matrix3x3", 3);
    test_matrix_constructor(math, "Matrix4x4", 4);
    test_math_projection(state, math);
    test_inheritance_projection(state);
    test_container_projection(state);
    test_callable_navigation(state, math);
    test_explicit_and_missing_types(state);
    test_system_network_parameter_registration(state);
    if (failures == 0 && argc == 2) {
        write_preview(state, math, argv[1]);
        write_preview(state, ZrVmLibContainer_GetModuleDescriptor(), argv[1]);
    }
    ZrCore_GlobalState_Free(global);
    printf("Native declaration signatures: %u callables checked, %d failures\n", callables_checked, failures);
    return failures == 0 ? 0 : 1;
}
