#include "compiler_internal.h"

#include "zr_vm_parser/semantic_facts.h"
#include "zr_vm_parser/semantic_query.h"

TZrBool ZrParser_Compiler_PublishSemanticQueryDiagnostics(SZrCompilerState *cs) {
    SZrParserSemanticQueryScope scope;
    SZrParserSemanticQueryDiagnostics diagnostics;

    if (cs == ZR_NULL || cs->semanticContext == ZR_NULL) {
        return ZR_FALSE;
    }

    if (!ZrParser_SemanticFacts_ResolveLinearDefiniteAssignments(cs->semanticContext)) {
        return ZR_FALSE;
    }
    if (!ZrParser_SemanticFacts_ResolveLinearReachingDefinitions(cs->semanticContext)) {
        return ZR_FALSE;
    }
    if (cs->scriptAst != ZR_NULL &&
        !ZrParser_SemanticFacts_ResolveControlFlowDefiniteAssignments(cs->semanticContext, cs->scriptAst)) {
        return ZR_FALSE;
    }
    if (cs->scriptAst != ZR_NULL &&
        !ZrParser_SemanticFacts_ResolveControlFlowReachingDefinitions(cs->semanticContext, cs->scriptAst)) {
        return ZR_FALSE;
    }

    ZrParser_SemanticQueryScope_Module(&scope);
    return ZrParser_SemanticQuery_Diagnostics(cs->semanticContext, &scope, &diagnostics);
}
