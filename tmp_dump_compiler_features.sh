set -e
ROOT=/mnt/e/Git/zr_vm
BUILD=$ROOT/build/codex-wsl-gcc-debug-current-make/tests
cat > /tmp/zr_dump_compiler_features.c <<'EOF'
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "zr_vm_core/global.h"
#include "zr_vm_core/function.h"
#include "zr_vm_core/string.h"
#include "zr_vm_core/state.h"
#include "zr_vm_parser.h"
#include "zr_vm_parser/compiler.h"

static void *test_allocator(void *userData, void *ptr, TZrSize oldSize, TZrSize newSize, TZrInt64 sizeDiff) {
    (void)userData; (void)oldSize; (void)sizeDiff;
    if (newSize == 0) { free(ptr); return NULL; }
    return realloc(ptr, newSize);
}

static SZrState *create_test_state(void) {
    SZrCallbackGlobal callbacks = {0};
    SZrGlobalState *global = ZrCore_GlobalState_New(test_allocator, NULL, 0, &callbacks);
    if (!global) return NULL;
    SZrState *state = global->mainThreadState;
    ZrCore_GlobalState_InitRegistry(state, global);
    ZrParser_ToGlobalState_Register(state);
    return state;
}

static void destroy_test_state(SZrState *state) {
    if (state && state->global) ZrCore_GlobalState_Free(state->global);
}

static const char *opcode_name(EZrInstructionCode opcode) {
    switch (opcode) {
        case ZR_INSTRUCTION_ENUM(GET_CONSTANT): return "GET_CONSTANT";
        case ZR_INSTRUCTION_ENUM(GET_STACK): return "GET_STACK";
        case ZR_INSTRUCTION_ENUM(SET_STACK): return "SET_STACK";
        case ZR_INSTRUCTION_ENUM(GET_GLOBAL): return "GET_GLOBAL";
        case ZR_INSTRUCTION_ENUM(GET_MEMBER): return "GET_MEMBER";
        case ZR_INSTRUCTION_ENUM(SET_MEMBER): return "SET_MEMBER";
        case ZR_INSTRUCTION_ENUM(GET_BY_INDEX): return "GET_BY_INDEX";
        case ZR_INSTRUCTION_ENUM(SET_BY_INDEX): return "SET_BY_INDEX";
        case ZR_INSTRUCTION_ENUM(CREATE_ARRAY): return "CREATE_ARRAY";
        case ZR_INSTRUCTION_ENUM(CREATE_OBJECT): return "CREATE_OBJECT";
        case ZR_INSTRUCTION_ENUM(FUNCTION_CALL): return "FUNCTION_CALL";
        case ZR_INSTRUCTION_ENUM(FUNCTION_RETURN): return "FUNCTION_RETURN";
        case ZR_INSTRUCTION_ENUM(TO_FLOAT): return "TO_FLOAT";
        case ZR_INSTRUCTION_ENUM(TO_STRING): return "TO_STRING";
        case ZR_INSTRUCTION_ENUM(ADD_STRING): return "ADD_STRING";
        case ZR_INSTRUCTION_ENUM(OWN_UNIQUE): return "OWN_UNIQUE";
        case ZR_INSTRUCTION_ENUM(OWN_BORROW): return "OWN_BORROW";
        case ZR_INSTRUCTION_ENUM(OWN_LOAN): return "OWN_LOAN";
        case ZR_INSTRUCTION_ENUM(OWN_SHARE): return "OWN_SHARE";
        case ZR_INSTRUCTION_ENUM(OWN_DETACH): return "OWN_DETACH";
        case ZR_INSTRUCTION_ENUM(OWN_UPGRADE): return "OWN_UPGRADE";
        case ZR_INSTRUCTION_ENUM(OWN_RELEASE): return "OWN_RELEASE";
        default: return "OTHER";
    }
}

static void dump_function(const char *label, const SZrFunction *func) {
    printf("-- %s len=%u child=%u locals=%u params=%u varargs=%d --\n",
           label,
           func ? func->instructionsLength : 0,
           func ? func->childFunctionLength : 0,
           func ? func->localVariableLength : 0,
           func ? func->parameterCount : 0,
           func ? func->hasVariableArguments : 0);
    if (!func) return;
    for (TZrUInt32 i = 0; i < func->instructionsLength; ++i) {
        printf("  [%u] %s\n", i, opcode_name((EZrInstructionCode)func->instructionsList[i].instruction.operationCode));
    }
}

static void compile_and_dump(SZrState *state, const char *title, const char *source) {
    SZrString *name = ZrCore_String_Create(state, title, strlen(title));
    SZrAstNode *ast = ZrParser_Parse(state, source, strlen(source), name);
    SZrFunction *wrapper;
    printf("==== %s ====\n", title);
    if (!ast) { printf("parse failed\n"); return; }
    wrapper = ZrParser_Compiler_Compile(state, ast);
    ZrParser_Ast_Free(state, ast);
    if (!wrapper) { printf("compile failed\n"); return; }
    dump_function("wrapper", wrapper);
    for (TZrUInt32 i = 0; i < wrapper->childFunctionLength; ++i) {
        char label[64];
        snprintf(label, sizeof(label), "child[%u]", i);
        dump_function(label, &wrapper->childFunctionList[i]);
    }
    ZrCore_Function_Free(state, wrapper);
}

int main(void) {
    SZrState *state = create_test_state();
    compile_and_dump(state, "function_params.zr", "testFunc(a, b, c) { return a + b + c; }");
    compile_and_dump(state, "varargs.zr", "sum(...args: int[]): int { return 0; }");
    compile_and_dump(state, "template.zr", "var name = \"zr\"; return `hello ${1} ${name}`;");
    compile_and_dump(state, "call_conversion.zr", "add(lhs: float, rhs: float): float { return lhs + rhs; }\nvar value = add(1, 2.0);");
    compile_and_dump(state, "fixed_array.zr", "read(): int {\n    var xs: int[3];\n    return xs.length;\n}\n");
    compile_and_dump(state, "own_shared.zr", "class Box {}\nvar owner = %unique new Box();\nvar alias = %shared(owner);");
    compile_and_dump(state, "own_more.zr", "class Box {}\nvar owner = %unique new Box();\nvar shared = %shared(owner);\nvar borrowed = %borrow(shared);\nvar loanSource = %unique new Box();\nvar loaned = %loan(loanSource);\nvar detachSource = %unique new Box();\nvar detached = %detach(detachSource);");
    compile_and_dump(state, "own_upgrade.zr", "class Box {}\nvar seed = %unique new Box();\nvar owner = %shared(seed);\nvar watcher = %weak(owner);\nvar alias = %upgrade(watcher);\nvar releasedOwner = %release(owner);\nvar stillAlive = %upgrade(watcher);\nvar releasedAlias = %release(alias);\nvar releasedStillAlive = %release(stillAlive);\nvar second = %upgrade(watcher);\n");
    destroy_test_state(state);
    return 0;
}
EOF
cd "$BUILD"
python3 - <<'PY'
from pathlib import Path
import shlex, subprocess
flags = {}
for line in Path('CMakeFiles/zr_vm_compiler_features_test.dir/flags.make').read_text().splitlines():
    if ' = ' in line:
        k, v = line.split(' = ', 1)
        flags[k] = v
cmd = ['/usr/bin/gcc']
cmd += shlex.split(flags['C_DEFINES'])
cmd += shlex.split(flags['C_INCLUDES'])
cmd += shlex.split(flags['C_FLAGS'])
cmd += ['/tmp/zr_dump_compiler_features.c', '-L../lib', '-Wl,-rpath,/mnt/e/Git/zr_vm/build/codex-wsl-gcc-debug-current-make/lib', '-lzr_vm_parser', '-lzr_vm_core', '-o', '/tmp/zr_dump_compiler_features']
print(' '.join(shlex.quote(x) for x in cmd))
subprocess.run(cmd, check=True)
PY
/tmp/zr_dump_compiler_features > /tmp/zr_dump_compiler_features.txt 2>&1
sed -n '1,320p' /tmp/zr_dump_compiler_features.txt