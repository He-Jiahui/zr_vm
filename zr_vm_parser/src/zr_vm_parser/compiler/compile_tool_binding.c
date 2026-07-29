#include "compile_tool_binding.h"

static TZrBool compile_tool_binding_declare(
        SZrCompilerState *cs,
        SZrString *name,
        const SZrParserCompileToolModuleDescriptor *provider,
        EZrCompileToolBindingKind kind) {
    SZrCompileToolBinding binding;

    if (cs == ZR_NULL || name == ZR_NULL) {
        return ZR_FALSE;
    }

    binding.name = name;
    binding.provider = provider;
    binding.kind = kind;
    ZrCore_Array_Push(cs->state, &cs->compileToolBindings, &binding);
    return ZR_TRUE;
}

void ZrParser_CompileToolBinding_Reset(SZrCompilerState *cs) {
    if (cs != ZR_NULL) {
        cs->compileToolBindings.length = 0;
        cs->compilePhase = ZR_PARSER_COMPILE_PHASE_BUILD_FACTS;
    }
}

TZrSize ZrParser_CompileToolBinding_Mark(const SZrCompilerState *cs) {
    return cs != ZR_NULL ? cs->compileToolBindings.length : 0;
}

void ZrParser_CompileToolBinding_Restore(SZrCompilerState *cs, TZrSize mark) {
    if (cs != ZR_NULL && mark <= cs->compileToolBindings.length) {
        cs->compileToolBindings.length = mark;
    }
}

TZrBool ZrParser_CompileToolBinding_DeclareProvider(
        SZrCompilerState *cs,
        SZrString *name,
        const SZrParserCompileToolModuleDescriptor *provider) {
    return provider != ZR_NULL &&
           compile_tool_binding_declare(cs, name, provider, ZR_COMPILE_TOOL_BINDING_PROVIDER);
}

TZrBool ZrParser_CompileToolBinding_DeclareShadow(SZrCompilerState *cs, SZrString *name) {
    return compile_tool_binding_declare(cs, name, ZR_NULL, ZR_COMPILE_TOOL_BINDING_SHADOW);
}

const SZrCompileToolBinding *ZrParser_CompileToolBinding_Resolve(
        const SZrCompilerState *cs,
        SZrString *name) {
    if (cs == ZR_NULL || name == ZR_NULL) {
        return ZR_NULL;
    }

    for (TZrSize index = cs->compileToolBindings.length; index > 0; index--) {
        const SZrCompileToolBinding *binding =
                (const SZrCompileToolBinding *)ZrCore_Array_Get(
                        (SZrArray *)&cs->compileToolBindings,
                        index - 1);
        if (binding != ZR_NULL && binding->name != ZR_NULL &&
            ZrCore_String_Equal(binding->name, name)) {
            return binding;
        }
    }

    return ZR_NULL;
}
