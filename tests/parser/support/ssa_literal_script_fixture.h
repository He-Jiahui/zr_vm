#ifndef ZR_TEST_SSA_LITERAL_SCRIPT_FIXTURE_H
#define ZR_TEST_SSA_LITERAL_SCRIPT_FIXTURE_H

#include <stddef.h>
#include "zr_vm_core/exec_ir_interpreter.h"
#include "zr_vm_parser/compiler.h"

typedef enum EZrSsaLiteralScriptSource {
    ZR_TEST_SSA_LITERAL_SCRIPT_NINE = 0,
    ZR_TEST_SSA_LITERAL_SCRIPT_EIGHT = 1
} EZrSsaLiteralScriptSource;

typedef struct SZrSsaLiteralScriptFixture {
    SZrState *state;
    SZrAstNode *ast;
    SZrCompilerState compiler;
    TZrBool compilerInitialized;
    TZrBool functionRooted;
    SZrExecIrModule module;
    SZrExecIrOracleValue *constants;
    SZrExecIrOracleValue *initialValues;
    TZrUInt32 placeCalls;
    TZrInt64 expectedReturn;
} SZrSsaLiteralScriptFixture;

/* The caller registers the fixture with its teardown owner before Prepare can
 * assert. Init owns no runtime state; Free supports partially prepared fixtures.
 * Keep the state alive until Free finishes. */
void ZrTests_SsaLiteralScriptFixture_Init(SZrSsaLiteralScriptFixture *fixture, SZrState *state);
void ZrTests_SsaLiteralScriptFixture_Free(SZrSsaLiteralScriptFixture *fixture);
void ZrTests_SsaLiteralScriptFixture_Prepare(SZrSsaLiteralScriptFixture *fixture,
        EZrSsaLiteralScriptSource source);
SZrExecIrFunction *ZrTests_SsaLiteralScriptFixture_Function(SZrSsaLiteralScriptFixture *fixture);
void ZrTests_SsaLiteralScriptFixture_AssertSourceMaps(const SZrSsaLiteralScriptFixture *fixture,
        const SZrExecIrFunction *function);
/* Oracle result ownership stays with the caller, including assertion failure. */
void ZrTests_SsaLiteralScriptFixture_AssertOracle(SZrSsaLiteralScriptFixture *fixture,
        const SZrExecIrFunction *function, TZrUInt32 expectedPlaceCalls,
        SZrExecIrOracleExecutionResult *oracle);
TZrUInt64 ZrTests_SsaLiteralScriptFixture_DigestBytes(TZrUInt64 previous,
        const void *bytes, size_t size);
TZrUInt64 ZrTests_SsaLiteralScriptFixture_FunctionDigest(const SZrExecIrFunction *function);
TZrUInt64 ZrTests_SsaLiteralScriptFixture_SourceDigest(const SZrSsaLiteralScriptFixture *fixture);

#endif
