#ifndef ZR_VM_CLI_METADATA_ZRP_METADATA_DUMP_H
#define ZR_VM_CLI_METADATA_ZRP_METADATA_DUMP_H

#include <stdio.h>

#include "zr_vm_cli/conf.h"

TZrBool ZrCli_ZrpMetadataDump_WriteSummary(FILE *output,
                                           const TZrByte *buffer,
                                           TZrSize bufferLength,
                                           TZrChar *errorBuffer,
                                           TZrSize errorBufferSize);

TZrBool ZrCli_ZrpMetadataDump_WriteDiffSummary(FILE *output,
                                               const TZrByte *beforeBuffer,
                                               TZrSize beforeBufferLength,
                                               const TZrByte *afterBuffer,
                                               TZrSize afterBufferLength,
                                               TZrChar *errorBuffer,
                                               TZrSize errorBufferSize);

TZrBool ZrCli_ZrpMetadataDump_WriteVersionCheck(FILE *output,
                                                const TZrByte *buffer,
                                                TZrSize bufferLength,
                                                TZrChar *errorBuffer,
                                                TZrSize errorBufferSize);

int ZrCli_ZrpMetadataDump_RunPath(const TZrChar *path, FILE *output, FILE *errorOutput);
int ZrCli_ZrpMetadataDump_RunDiffPath(const TZrChar *beforePath,
                                      const TZrChar *afterPath,
                                      FILE *output,
                                      FILE *errorOutput);
int ZrCli_ZrpMetadataDump_RunVersionCheckPath(const TZrChar *path, FILE *output, FILE *errorOutput);

#endif // ZR_VM_CLI_METADATA_ZRP_METADATA_DUMP_H
