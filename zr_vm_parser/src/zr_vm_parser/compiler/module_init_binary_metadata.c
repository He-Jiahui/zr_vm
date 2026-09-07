#include "module_init_analysis.h"

#include "zr_vm_core/io.h"

TZrBool ZrParser_ModuleInitAnalysis_TryLoadBinaryMetadataSourceFromIo(SZrState *state,
                                                                  const SZrIo *io,
                                                                  SZrIoSource **outSource) {
    SZrIo directIo;

    if (outSource != ZR_NULL) {
        *outSource = ZR_NULL;
    }
    if (state == ZR_NULL || io == ZR_NULL || outSource == ZR_NULL ||
        (io->read == ZR_NULL && io->remained == 0)) {
        return ZR_FALSE;
    }

    directIo = *io;
    directIo.state = state;
    *outSource = ZrCore_Io_ReadSourceNew(&directIo);
    return *outSource != ZR_NULL;
}

void ZrParser_ModuleInitAnalysis_FreeBinaryMetadataSource(SZrGlobalState *global, SZrIoSource *source) {
    ZrCore_Io_ReadSourceFree(global, source);
}
