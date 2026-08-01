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

TZrBool library_project_manifest_v2_package_identity_to_literal(
        const SZrLibrary_ModuleIdentity *identity,
        TZrChar *outLiteral,
        TZrSize outLiteralSize);

TZrBool library_project_manifest_v2_dependency_index_at_ordinal(
        const SZrLibrary_ProjectManifestDependency *dependencies,
        TZrSize dependencyCount,
        TZrSize ordinal,
        TZrSize *outIndex);

const TZrChar *library_project_manifest_v2_dependency_source_field(
        EZrLibrary_ProjectManifestDependencySourceKind sourceKind);

void library_project_manifest_v2_free_declarations(SZrGlobalState *global,
                                                    SZrLibrary_Project *project);

#endif
