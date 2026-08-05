#ifndef ZR_VM_CLI_REPL_SESSION_H
#define ZR_VM_CLI_REPL_SESSION_H

#include "zr_vm_core/gc_domain.h"
#include "zr_vm_parser/compiler.h"

struct SZrClosure;
struct SZrGlobalState;
struct SZrState;

typedef struct ZrCliReplSession {
    struct SZrGlobalState *global;
    struct SZrState *state;
    SZrGcRootHandle environmentRoot;
    SZrGcRootHandle sourceNameRoot;
    struct SZrClosure *activeClosure;
    struct SZrString *sourceName;
    SZrParserSubmissionBinding *bindings;
    TZrSize bindingCount;
    SZrParserSubmissionCallableSignature *callableSignatures;
    TZrSize callableSignatureCount;
    TZrUInt64 moduleGeneration;
    TZrUInt64 environmentGeneration;
    TZrUInt64 nextCellGeneration;
} ZrCliReplSession;

int ZrCli_ReplSession_Init(ZrCliReplSession *session);
void ZrCli_ReplSession_Free(ZrCliReplSession *session);
int ZrCli_ReplSession_Submit(ZrCliReplSession *session, const TZrChar *code);
int ZrCli_ReplSession_TypeQuery(ZrCliReplSession *session, const TZrChar *expression);
int ZrCli_ReplSession_Reset(ZrCliReplSession *session);

#endif
