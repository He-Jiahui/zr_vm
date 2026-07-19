#ifndef ZR_VM_CORE_ARTIFACT_SCHEMA_INTERNAL_H
#define ZR_VM_CORE_ARTIFACT_SCHEMA_INTERNAL_H

#include "zr_vm_core/artifact_schema.h"

void zr_artifact_diagnostic_clear(SZrArtifactDiagnostic *diagnostic);

EZrArtifactStatus zr_artifact_fail(SZrArtifactDiagnostic *diagnostic,
                                   EZrArtifactStatus status,
                                   TZrUInt32 sectionKind,
                                   TZrUInt32 rowIndex,
                                   TZrUInt32 byteOffset);

TZrUInt16 zr_artifact_read_u16(const TZrByte *bytes);
TZrUInt32 zr_artifact_read_u32(const TZrByte *bytes);
TZrUInt64 zr_artifact_read_u64(const TZrByte *bytes);
void zr_artifact_write_u16(TZrByte *bytes, TZrUInt16 value);
void zr_artifact_write_u32(TZrByte *bytes, TZrUInt32 value);
void zr_artifact_write_u64(TZrByte *bytes, TZrUInt64 value);

TZrBool zr_artifact_kind_is_valid(EZrArtifactKind kind);
TZrBool zr_artifact_section_is_known(TZrUInt32 kind);
TZrBool zr_artifact_section_is_allowed(EZrArtifactKind artifactKind, TZrUInt32 sectionKind);
TZrUInt32 zr_artifact_section_element_size(TZrUInt32 kind);
void zr_artifact_write_section_payload(TZrByte *bytes, const SZrArtifactSectionInput *section);

EZrArtifactStatus zr_artifact_decode_directory_entry(const SZrArtifactView *view,
                                                      TZrUInt32 index,
                                                      SZrArtifactSectionView *outSection,
                                                      SZrArtifactDiagnostic *diagnostic);

#endif // ZR_VM_CORE_ARTIFACT_SCHEMA_INTERNAL_H
