#ifndef ZR_TEST_EXEC_IR_OWNER_FIXTURE_H
#define ZR_TEST_EXEC_IR_OWNER_FIXTURE_H
#include "zr_vm_core/exec_ir_owned_module.h"
#include "zr_vm_core/exec_ir_state_map.h"
#include "ssa_exec_ir_owned_module_allocator.h"
enum { ZR_OWNER_MODULE_SLOTS = 4, ZR_OWNER_SLOTS = 3, ZR_OWNER_WORDS = 4096 };
typedef struct SZrOwnerContents { TZrUInt64 words[ZR_OWNER_WORDS]; unsigned count; } SZrOwnerContents;
extern SZrExecIrModule g_ownerModules[ZR_OWNER_MODULE_SLOTS];
extern SZrExecIrOwnedModule *g_execOwners[ZR_OWNER_SLOTS];
void ZrTests_ExecOwner_SetUp(void);
void ZrTests_ExecOwner_TearDown(void);
SZrExecIrModule *ZrTests_ExecOwner_Build(unsigned slot, unsigned seed, unsigned functions, unsigned mode);
void ZrTests_ExecOwner_Capture(const SZrExecIrModule *module, SZrOwnerContents *contents);
void ZrTests_ExecOwner_Equal(const SZrOwnerContents *expected, const SZrExecIrModule *module);
void ZrTests_ExecOwner_Independent(const SZrExecIrModule *source, const SZrExecIrModule *copy);
void ZrTests_ExecOwner_AssertProducerLive(const SZrExecIrModule *module);
TZrUInt64 ZrTests_ExecOwner_LiveDigest(const SZrExecIrModule *module);
void ZrTests_ExecOwner_Diagnostic(const SZrExecIrDiagnostic *expected, const SZrExecIrDiagnostic *actual);
#endif
