/* U6a: while the analyzer walks a class member, the LSP points the compiler's
 * currentTypePrototypeInfo at the owner type. Type conversion of that member can import
 * modules and grow (reallocate) cs->typePrototypes. The LSP used to keep the element
 * address, which then dangled: on this fixture the next generic-context lookup read a
 * garbage genericParameters.length and spun for ever. The analysis must finish. */
#include <stdio.h>
#include <string.h>
#if defined(__unix__) || defined(__APPLE__)
#include <signal.h>
#include <unistd.h>
#endif

#include "interface/lsp_interface_internal.h"
#include "runtime_support.h"
#include "zr_vm_core/string.h"

#if defined(__unix__) || defined(__APPLE__)
static void on_alarm(int sig) {
    static const char message[] = "FAIL: analysis did not finish within 30 s (dangling type prototype)\n";
    (void)sig;
    (void)!write(2, message, sizeof(message) - 1);
    _exit(2);
}
#endif

int main(void) {
    /* tests/fixtures/parser/swarm_user_iterable.zr */
    static const char *text =
            "let {Array} = import(\"zr.container\");\n"
            "let iteration = import(\"zr.iteration\");\n"
            "class Cursor: iteration.Enumerator<int> {\n"
            "    pub var current: int = 0;\n"
            "    pub fn moveNext(): bool { this.current += 1; return this.current <= 3; }\n"
            "}\n"
            "class Range: iteration.Iterable<int> {\n"
            "    pub fn getEnumerator(): iteration.Enumerator<int> { return new Cursor(); }\n"
            "}\n"
            "var range = new Range();\n"
            "var sum = 0;\n"
            "for (var value in range) { sum += value; }\n"
            "if (sum != 6) { throw \"P07 iterable sum\"; }\n"
            "var second = new Cursor();\n"
            "if (second.current != 0 || !second.moveNext() || second.current != 1) { throw \"per-instance iterator defaults\"; }\n"
            "class Defaults { pub var items: Array<int> = new Array<int>(); }\n"
            "var firstDefaults = new Defaults();\n"
            "var secondDefaults = new Defaults();\n"
            "firstDefaults.items.add(5);\n"
            "if (secondDefaults.items.length != 0) { throw \"shared implicit initializer\"; }\n"
            "return 42;\n";
    SZrState *state;
    SZrLspContext *context;
    SZrString *uri;
    SZrArray diagnostics = {0};
    SZrArray symbols = {0};
    TZrBool passed = ZR_FALSE;

#if defined(__unix__) || defined(__APPLE__)
    signal(SIGALRM, on_alarm);
    alarm(30);
#endif
    state = ZrTests_Runtime_State_Create(ZR_NULL);
    if (state == ZR_NULL) return 1;
    context = ZrLanguageServer_LspContext_New(state);
    uri = ZR_STRING_LITERAL(state, "file:///type-prototype-growth.zr");
    if (context != ZR_NULL && uri != ZR_NULL &&
        ZrLanguageServer_Lsp_UpdateDocument(state, context, uri, text, strlen(text), 1)) {
        passed = ZrLanguageServer_Lsp_GetDiagnostics(state, context, uri, &diagnostics) &&
                 ZrLanguageServer_Lsp_GetDocumentSymbols(state, context, uri, &symbols);
        printf("  diagnostics=%u symbols=%u\n", (unsigned)diagnostics.length, (unsigned)symbols.length);
        ZrLanguageServer_Lsp_FreeDiagnostics(state, &diagnostics);
        ZrCore_Array_Free(state, &symbols);
    }
    printf("%s: analysis finishes while member conversion grows typePrototypes\n", passed ? "PASS" : "FAIL");
    ZrLanguageServer_LspContext_Free(state, context);
    ZrTests_Runtime_State_Destroy(state);
    return passed ? 0 : 1;
}
