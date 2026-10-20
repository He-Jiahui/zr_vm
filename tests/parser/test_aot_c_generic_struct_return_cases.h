#ifndef ZR_TEST_AOT_C_GENERIC_STRUCT_RETURN_CASES_H
#define ZR_TEST_AOT_C_GENERIC_STRUCT_RETURN_CASES_H

#include "backend_aot/backend_aot_exec_ir_frame.h"
#include "backend_aot/backend_aot_exec_ir_return_layout.h"
#include "backend_aot/backend_aot_function_table.h"

static const char *aot_generic_struct_return_type_name(const SZrString *name) {
    return name != ZR_NULL ? ZrCore_String_GetNativeString(name) : "<null>";
}

static void aot_generic_struct_return_dump_type(const char *label,
                                                const SZrFunctionTypedTypeRef *type) {
    fprintf(stderr,
            "  %s base=%u name=%s staticCType=%u staticCTypeId=%u nullable=%u "
            "ownership=%u array=%u elementBase=%u elementName=%s\n",
            label, (unsigned)type->baseType,
            aot_generic_struct_return_type_name(type->typeName),
            (unsigned)type->staticCType, (unsigned)type->staticCTypeId,
            (unsigned)type->isNullable, (unsigned)type->ownershipQualifier,
            (unsigned)type->isArray, (unsigned)type->elementBaseType,
            aot_generic_struct_return_type_name(type->elementTypeName));
}

static void aot_generic_struct_return_dump_function(const SZrFunction *function,
                                                    TZrUInt32 flatIndex) {
    if (function == ZR_NULL) {
        fprintf(stderr, "[aot-generic-struct-return] function=%u missing\n", (unsigned)flatIndex);
        return;
    }
    fprintf(stderr,
            "[aot-generic-struct-return] function=%u name=%s lines=%u..%u "
            "hasCallableReturnType=%u semIrCount=%u rawFrameSlotCount=%u\n",
            (unsigned)flatIndex, aot_generic_struct_return_type_name(function->functionName),
            (unsigned)function->lineInSourceStart, (unsigned)function->lineInSourceEnd,
            (unsigned)function->hasCallableReturnType, (unsigned)function->semIrInstructionLength,
            (unsigned)function->frameSlotLayoutLength);
    aot_generic_struct_return_dump_type("callableReturnType", &function->callableReturnType);
    fprintf(stderr,
            "  frame parameters=%u stackSize=%u byteSize=%u byteAlign=%u typedBindingCount=%u\n",
            (unsigned)function->parameterCount, (unsigned)function->stackSize,
            (unsigned)function->frameByteSize, (unsigned)function->frameByteAlign,
            (unsigned)function->typedLocalBindingLength);
    if (function->typedLocalBindingLength > 0u && function->typedLocalBindings == ZR_NULL) {
        fputs("  typed local binding table missing\n", stderr);
    }
    for (TZrUInt32 index = 0u;
         function->typedLocalBindings != ZR_NULL && index < function->typedLocalBindingLength; index++) {
        const SZrFunctionTypedLocalBinding *binding = &function->typedLocalBindings[index];
        fprintf(stderr,
                "  typedBinding[%u] name=%s stackSlot=%u roleFlags=%u symbolId=%u typeId=%u placeId=%u\n",
                (unsigned)index, aot_generic_struct_return_type_name(binding->name),
                (unsigned)binding->stackSlot, (unsigned)binding->roleFlags,
                (unsigned)binding->symbolId, (unsigned)binding->typeId, (unsigned)binding->placeId);
        aot_generic_struct_return_dump_type("bindingType", &binding->type);
    }
    if (function->frameSlotLayoutLength > 0u && function->frameSlotLayouts == ZR_NULL) {
        fputs("  raw frame slot table missing\n", stderr);
    }
    for (TZrUInt32 index = 0u;
         function->frameSlotLayouts != ZR_NULL && index < function->frameSlotLayoutLength; index++) {
        const SZrFunctionFrameSlotLayout *slot = &function->frameSlotLayouts[index];
        fprintf(stderr,
                "  rawFrame[%u] stackSlot=%u kind=%u offset=%u size=%u align=%u "
                "typeLayoutId=%u parameter=%u flags=%u\n",
                (unsigned)index, (unsigned)slot->stackSlot, (unsigned)slot->slotKind,
                (unsigned)slot->byteOffset, (unsigned)slot->byteSize, (unsigned)slot->byteAlign,
                (unsigned)slot->typeLayoutId, (unsigned)slot->isParameter, (unsigned)slot->reserved0);
    }
    if (function->semIrInstructionLength > 0u && function->semIrInstructions == ZR_NULL) {
        fputs("  SemIR instruction table missing\n", stderr);
    }
    for (TZrUInt32 index = 0u;
         function->semIrInstructions != ZR_NULL && index < function->semIrInstructionLength; index++) {
        const SZrSemIrInstruction *instruction = &function->semIrInstructions[index];
        if (instruction->opcode != (TZrUInt32)ZR_SEMIR_OPCODE_RETURN_TYPED) {
            continue;
        }
        fprintf(stderr, "  RETURN_TYPED semIr=%u exec=%u typeTableIndex=%u sourceSlot=%u\n",
                (unsigned)index, (unsigned)instruction->execInstructionIndex,
                (unsigned)instruction->typeTableIndex, (unsigned)instruction->operand0);
        if (function->semIrTypeTable != ZR_NULL &&
            instruction->typeTableIndex < function->semIrTypeTableLength) {
            aot_generic_struct_return_dump_type(
                    "sourceType", &function->semIrTypeTable[instruction->typeTableIndex]);
        } else {
            fprintf(stderr, "  source type unavailable: table=%s count=%u\n",
                    function->semIrTypeTable != ZR_NULL ? "present" : "missing",
                    (unsigned)function->semIrTypeTableLength);
        }
    }
}

/* Probe only after the original module build; its failed partial projection is not observable. */
static void aot_generic_struct_return_dump_failure(SZrState *state, SZrFunction *root) {
    SZrAotFunctionTable table = {0};
    if (!backend_aot_build_function_table(state, root, &table)) {
        fputs("[aot-generic-struct-return] diagnostic function table unavailable\n", stderr);
        backend_aot_release_function_table(state, &table);
        fflush(stderr);
        return;
    }
    for (TZrUInt32 index = 0u; index < table.count; index++) {
        const SZrFunction *function = table.entries[index].function;
        SZrAotExecIrFunction probe = {0};
        TZrBool frameBuilt;
        TZrBool returnProbed = ZR_FALSE;
        TZrBool returnProjected = ZR_FALSE;
        aot_generic_struct_return_dump_function(function, table.entries[index].flatIndex);
        if (function == ZR_NULL) {
            continue;
        }
        probe.function = function;
        probe.metadataEntryFunction = root;
        probe.flatIndex = table.entries[index].flatIndex;
        if (function->hasCallableReturnType == ZR_TRUE) {
            probe.callableReturnTypeKnown = ZR_TRUE;
            probe.callableReturnType = function->callableReturnType;
        }
        frameBuilt = backend_aot_exec_ir_build_frame_layout(state, function, &probe.frameLayout);
        if (frameBuilt && (function->hasCallableReturnType == ZR_FALSE ||
                           function->hasCallableReturnType == ZR_TRUE)) {
            returnProbed = ZR_TRUE;
            returnProjected = backend_aot_exec_ir_project_direct_inline_return_layout(state, function, &probe);
        }
        fprintf(stderr,
                "  post-failure probe frameBuilt=%u returnProbed=%u returnProjected=%u "
                "callableReturnTypeKnown=%u directInlineReturnLayoutKnown=%u typeLayoutId=%u\n",
                (unsigned)frameBuilt, (unsigned)returnProbed, (unsigned)returnProjected,
                (unsigned)probe.callableReturnTypeKnown, (unsigned)probe.directInlineReturnLayoutKnown,
                (unsigned)probe.directInlineReturnTypeLayoutId);
        backend_aot_exec_ir_release_frame_layout(state, &probe.frameLayout);
    }
    backend_aot_release_function_table(state, &table);
    fflush(stderr);
}

static void test_aot_exec_ir_builds_cli_generic_struct_return_fixture(void) {
    static const char source[] =
            "struct Stamp {\n"
            "    pub var value: int;\n"
            "    pub @constructor(value: int) { this.value = value; }\n"
            "}\n"
            "class RefA { }\n"
            "fn stamp<T>(value: T): Stamp where T: class {\n"
            "    var local: Stamp = init Stamp(42);\n"
            "    return local;\n"
            "}\n"
            "var input: RefA;\n"
            "var returned: Stamp = stamp<RefA>(input);\n"
            "return returned.value;";
    static const char sourceNameText[] = "cli_generic_struct_return.zr";
    SZrState *state = ZrTests_Runtime_State_Create(ZR_NULL);
    SZrString *sourceName;
    SZrFunction *root;
    SZrAotExecIrModule module = {0};
    TZrBool oldEmitCompileTimeRuntimeSupport;
    TZrBool built;

    if (state == ZR_NULL) {
        TEST_FAIL_MESSAGE("fixture prerequisite: could not create the compiler state");
        return;
    }
    sourceName = ZrCore_String_Create(state, (TZrNativeString)sourceNameText, sizeof(sourceNameText) - 1u);
    if (sourceName == ZR_NULL) {
        ZrTests_Runtime_State_Destroy(state);
        TEST_FAIL_MESSAGE("fixture prerequisite: could not create the source name");
        return;
    }
    oldEmitCompileTimeRuntimeSupport = state->global->emitCompileTimeRuntimeSupport;
    state->global->emitCompileTimeRuntimeSupport = ZR_TRUE;
    root = ZrParser_Source_Compile(state, source, sizeof(source) - 1u, sourceName);
    state->global->emitCompileTimeRuntimeSupport = oldEmitCompileTimeRuntimeSupport;
    if (root == ZR_NULL) {
        ZrTests_Runtime_State_Destroy(state);
        TEST_FAIL_MESSAGE("fixture prerequisite: CLI Stamp source did not compile");
        return;
    }
    built = backend_aot_exec_ir_build_module(state, root, &module);
    if (!built) {
        aot_generic_struct_return_dump_failure(state, root);
    }
    backend_aot_exec_ir_release_module(state, &module);
    ZrCore_Function_Free(state, root);
    ZrTests_Runtime_State_Destroy(state);
    TEST_ASSERT_TRUE_MESSAGE(built, "CLI Stamp source must build an AOT ExecIR module");
}

#endif
