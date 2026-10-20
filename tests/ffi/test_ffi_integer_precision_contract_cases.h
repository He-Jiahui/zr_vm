#ifndef ZR_TEST_FFI_INTEGER_PRECISION_CONTRACT_CASES_H
#define ZR_TEST_FFI_INTEGER_PRECISION_CONTRACT_CASES_H

static void ffi_assert_signed64_precision_contract(void) {
    static const char *sourceTemplate =
            "var ffi = import(\"zr.ffi\");\n"
            "var lib = ffi.loadLibrary(\"%s\");\n"
            "var identity = lib.getSymbol(\"zr_ffi_identity_i64\", {\n"
            "  returnType: \"i64\", parameters: [{ type: \"i64\" }]\n"
            "});\n"
            "var high = identity.call([9007199254740993]);\n"
            "var low = identity.call([-9007199254740993]);\n"
            "return high == 9007199254740993 && low == -9007199254740993;\n";
    char source[4096];
    char escapedPath[4096];
    SZrState *state = create_test_state();
    SZrFunction *function = ZR_NULL;
    SZrTypeValue result = {0};
    TZrBool compiled = ZR_FALSE;
    TZrBool executed = ZR_FALSE;
    TZrBool exact = ZR_FALSE;

    if (state != ZR_NULL) {
        escape_for_zr_string_literal(
                escapedPath, sizeof(escapedPath), ZR_VM_FFI_FIXTURE_PATH);
        snprintf(source, sizeof(source), sourceTemplate, escapedPath);
        function = compile_source(state, source, "ffi_signed64_precision_contract.zr");
        compiled = (TZrBool)(function != ZR_NULL);
        if (compiled) {
            executed = ZrTests_Runtime_Function_Execute(state, function, &result);
            exact = (TZrBool)(executed && result.type == ZR_VALUE_TYPE_BOOL &&
                              result.value.nativeObject.nativeBool);
            ZrCore_Function_Free(state, function);
        }
        destroy_test_state(state);
    }

    TEST_ASSERT_TRUE_MESSAGE(compiled, "signed64 precision source must compile");
    TEST_ASSERT_TRUE_MESSAGE(executed, "signed64 precision source must execute");
    TEST_ASSERT_TRUE_MESSAGE(exact, "i64 arguments must preserve positive and negative 2^53+1");
}

#endif
