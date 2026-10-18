#include <stdlib.h>
#include "ssa_phi_identity_fixture.h"

/* Lifecycle is shared by minimal, semantic, budget and transaction modules. */
static SPiTotals totals;
void Pi_SetUp(void) {
    /* A prior leaked record must survive to the next case and fail there. */
    SZrOwnerHeapStats st=ZrTests_ExecOwner_Stats();
    if(!st.live&&!st.errors)ZrTests_ExecOwner_Reset();
}
void Pi_TearDown(void) { ZrTests_ExecOwner_Disarm(); }
SPiTotals Pi_Totals(void) { return totals; }
void Pi_NoteFault(unsigned measured,unsigned completed) {
    totals.faultMeasured+=measured;totals.faultCompleted+=completed;
}
int Pi_Dispose(SPiFixture *f) { return Pi_DisposeFault(f,0u); }
int Pi_DisposeFault(SPiFixture *f,unsigned failures) {
    SZrOwnerHeapStats st;
    ZrTests_ExecOwner_Disarm();
    ZrParser_ExecIr_AnalysisCacheFree(&f->cache);
    ZrParser_ExecIr_RemarkSinkFree(&f->remarks);
    ZrCore_ExecIr_FreeModule(&f->module);
    st=ZrTests_ExecOwner_Stats();
    totals.attempts+=st.attempts;
    totals.live+=st.live;totals.errors+=st.errors;
    /* Never Recover: leaked owners remain visible in the independent ledger. */
    return st.live==0u&&st.errors==0u&&st.failures==failures;
}
