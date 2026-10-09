#define ZR_TEST_FUNCTION_EXEC_IR_AOT_NO_REDIRECT
#include "ssa_function_exec_ir_owner_aot_shim.h"
#include <string.h>
#include <stdio.h>
#include "unity.h"
enum { FN_VM_ROWS=32768,FN_AOT_ROWS=256,FN_ERRORS=64 };
typedef struct SFnRow { void *pointer;size_t bytes;TZrInt64 type;unsigned frees; } SFnRow;
typedef struct SFnError { unsigned reason,row;size_t allocated,original,requested;TZrInt64 priorType,type; } SFnError;
static SFnRow g_vm[FN_VM_ROWS],g_aot[FN_AOT_ROWS];
static SZrFnHeapStats g_vmStats,g_aotStats;
static SFnError g_errors[FN_ERRORS];static unsigned g_errorCount;
static void vm_error(unsigned reason,unsigned row,size_t original,size_t bytes,TZrInt64 flag) {
    SFnError *e;++g_vmStats.errors;if(g_errorCount==FN_ERRORS)return;e=&g_errors[g_errorCount++];
    e->reason=reason;e->row=row;e->original=original;e->requested=bytes;e->type=flag;
    if(row<g_vmStats.records){e->allocated=g_vm[row].bytes;e->priorType=g_vm[row].type;}
}
TZrPtr ZrTests_FnExec_VmAllocate(TZrPtr user,TZrPtr pointer,TZrSize original,TZrSize bytes,TZrInt64 flag) {
    unsigned row=FN_VM_ROWS;const TZrBool wasNew=pointer==ZR_NULL;void *replacement;(void)user;++g_vmStats.attempts;
    if(!wasNew){for(unsigned i=0u;i<g_vmStats.records;++i)if(g_vm[i].pointer!=ZR_NULL&&g_vm[i].pointer==pointer){row=i;break;}}
    else if(g_vmStats.records<FN_VM_ROWS)row=g_vmStats.records;
    if(row==FN_VM_ROWS||(wasNew&&(original!=0u||bytes==0u))){vm_error(1u,row,original,bytes,flag);return ZR_NULL;}
    if(!wasNew&&(g_vm[row].bytes!=original||g_vm[row].type!=flag))vm_error(2u,row,original,bytes,flag);
    if(bytes==0u){g_vm[row].pointer=ZR_NULL;++g_vm[row].frees;++g_vmStats.frees;--g_vmStats.live;free(pointer);return ZR_NULL;}
    replacement=realloc(pointer,bytes);
    if(replacement==ZR_NULL){vm_error(3u,row,original,bytes,flag);return ZR_NULL;}
    if(wasNew){++g_vmStats.records;++g_vmStats.live;}
    g_vm[row].pointer=replacement;g_vm[row].bytes=bytes;g_vm[row].type=flag;return replacement;
}
void *ZrTests_FnExec_AotMalloc(size_t bytes) {
    unsigned row=g_aotStats.records;void *p;++g_aotStats.attempts;
    if(bytes==0u||row==FN_AOT_ROWS){++g_aotStats.errors;return ZR_NULL;}
    p=malloc(bytes);if(p==ZR_NULL){++g_aotStats.errors;return ZR_NULL;}
    g_aot[row].pointer=p;g_aot[row].bytes=bytes;++g_aotStats.records;++g_aotStats.live;return p;
}
void ZrTests_FnExec_AotFree(void *pointer) {
    if(pointer==ZR_NULL)return;
    for(unsigned i=0u;i<g_aotStats.records;++i)if(g_aot[i].pointer!=ZR_NULL&&g_aot[i].pointer==pointer){
        g_aot[i].pointer=ZR_NULL;++g_aot[i].frees;++g_aotStats.frees;--g_aotStats.live;free(pointer);return;}
    ++g_aotStats.errors;
}
void ZrTests_FnExec_LedgerReset(void) {
    TEST_ASSERT_EQUAL_UINT(0u,g_vmStats.live);TEST_ASSERT_EQUAL_UINT(0u,g_aotStats.live);
    memset(g_vm,0,sizeof(g_vm));memset(g_aot,0,sizeof(g_aot));memset(g_errors,0,sizeof(g_errors));g_errorCount=0u;
    memset(&g_vmStats,0,sizeof(g_vmStats));memset(&g_aotStats,0,sizeof(g_aotStats));
}
SZrFnHeapStats ZrTests_FnExec_VmStats(void){return g_vmStats;}
SZrFnHeapStats ZrTests_FnExec_AotStats(void){return g_aotStats;}
unsigned ZrTests_FnExec_VmRecord(const void *p){unsigned result=FN_VM_ROWS;
    for(unsigned i=0u;i<g_vmStats.records;++i)if(g_vm[i].pointer!=ZR_NULL&&g_vm[i].pointer==p){result=i;break;}
    TEST_ASSERT_LESS_THAN_UINT(FN_VM_ROWS,result);return result;}
size_t ZrTests_FnExec_VmBytes(unsigned r){TEST_ASSERT_LESS_THAN_UINT(g_vmStats.records,r);return g_vm[r].bytes;}
TZrInt64 ZrTests_FnExec_VmType(unsigned r){TEST_ASSERT_LESS_THAN_UINT(g_vmStats.records,r);return g_vm[r].type;}
unsigned ZrTests_FnExec_VmFrees(unsigned r){TEST_ASSERT_LESS_THAN_UINT(g_vmStats.records,r);return g_vm[r].frees;}
unsigned ZrTests_FnExec_AotRecord(const void *p){unsigned result=FN_AOT_ROWS;
    for(unsigned i=0u;i<g_aotStats.records;++i)if(g_aot[i].pointer!=ZR_NULL&&g_aot[i].pointer==p){result=i;break;}
    TEST_ASSERT_LESS_THAN_UINT(FN_AOT_ROWS,result);return result;}
unsigned ZrTests_FnExec_AotFrees(unsigned r){TEST_ASSERT_LESS_THAN_UINT(g_aotStats.records,r);return g_aot[r].frees;}
void ZrTests_FnExec_LedgerRecover(void){
    if(g_vmStats.live!=0u)++g_vmStats.errors;if(g_aotStats.live!=0u)++g_aotStats.errors;
    if(g_vmStats.errors||g_aotStats.errors)fprintf(stderr,"FUNCTION_EXEC_LEDGER vm_errors=%u vm_live=%u aot_errors=%u aot_live=%u\n",
            g_vmStats.errors,g_vmStats.live,g_aotStats.errors,g_aotStats.live);
    for(unsigned i=0u;i<g_errorCount;++i){const SFnError *e=&g_errors[i];
        fprintf(stderr,"FUNCTION_EXEC_LEDGER reason=%u row=%u allocated=%llu original=%llu requested=%llu priorType=%lld type=%lld\n",
                e->reason,e->row,(unsigned long long)e->allocated,(unsigned long long)e->original,(unsigned long long)e->requested,(long long)e->priorType,(long long)e->type);}
    for(unsigned i=0u;i<g_vmStats.records;++i)if(g_vm[i].pointer!=ZR_NULL){void *p=g_vm[i].pointer;g_vm[i].pointer=ZR_NULL;
        ++g_vm[i].frees;++g_vmStats.frees;--g_vmStats.live;free(p);}
    for(unsigned i=0u;i<g_aotStats.records;++i)if(g_aot[i].pointer!=ZR_NULL)ZrTests_FnExec_AotFree(g_aot[i].pointer);
}
void ZrTests_FnExec_LedgerBalanced(void){
    TEST_ASSERT_EQUAL_UINT(0u,g_vmStats.live);TEST_ASSERT_EQUAL_UINT(0u,g_vmStats.errors);
    TEST_ASSERT_EQUAL_UINT(g_vmStats.records,g_vmStats.frees);
    TEST_ASSERT_EQUAL_UINT(0u,g_aotStats.live);TEST_ASSERT_EQUAL_UINT(0u,g_aotStats.errors);
    TEST_ASSERT_EQUAL_UINT(g_aotStats.records,g_aotStats.frees);
    for(unsigned i=0u;i<g_vmStats.records;++i)TEST_ASSERT_EQUAL_UINT(1u,g_vm[i].frees);
    for(unsigned i=0u;i<g_aotStats.records;++i)TEST_ASSERT_EQUAL_UINT(1u,g_aot[i].frees);
}
