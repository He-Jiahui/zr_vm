#ifndef ZR_VM_LIBRARY_PROJECT_MANIFEST_V2_H
#define ZR_VM_LIBRARY_PROJECT_MANIFEST_V2_H

#include "cJSON/cJSON.h"

#include "zr_vm_library/project.h"

TZrBool library_project_manifest_validate_version(cJSON *manifestJson, TZrUInt32 *outManifestVersion);

TZrBool library_project_manifest_v2_validate_base(cJSON *manifestJson);

TZrBool library_project_manifest_v2_parse_declarations(SZrState *state,
                                                        SZrLibrary_Project *project,
                                                        cJSON *manifestJson);

TZrBool library_project_manifest_v2_validate_writer_input(const SZrLibrary_Project *project);

void library_project_manifest_v2_free_declarations(SZrGlobalState *global,
                                                    SZrLibrary_Project *project);

#endif
