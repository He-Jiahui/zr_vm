#include "modules.h"

const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void);
const ZrLibModuleDescriptor *ZrVm_GetNativeModule_v1(void) { return ZrGame_HostModule(); }
