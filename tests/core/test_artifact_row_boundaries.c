/* Boundary contracts for public fixed-row readers, linked against actual Core.
 * Encoded fields intentionally need no cross-table/token graph validity. */
#include "zr_vm_core/artifact_schema.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifdef _WIN32
#include <windows.h>
#include <crtdbg.h>
#endif

typedef union Row {
    SZrArtifactTypeDefRow TypeDef;
    SZrArtifactTypeIdentityRow TypeIdentity;
    SZrArtifactMemberDefRow MemberDef;
    SZrArtifactPropertyDefRow PropertyDef;
    SZrArtifactContractRow Contract;
    SZrArtifactLayoutRow Layout;
    SZrArtifactMetadataStateRow MetadataState;
    SZrArtifactMetadataRecordRow MetadataRecord;
    SZrArtifactDomainTransferRow DomainTransfer;
    SZrArtifactSchedulerContractRow SchedulerContract;
    SZrArtifactRelocationRow Relocation;
} Row;
typedef EZrArtifactStatus (*Reader)(const SZrArtifactSectionView *, TZrUInt32,
                                   Row *, SZrArtifactDiagnostic *);
typedef int (*Projection)(const Row *);
typedef struct ReaderCase {
    const char *name;
    TZrUInt32 kind;
    TZrUInt32 size;
    TZrUInt32 hashOffset;
    Reader read;
    Projection projected;
} ReaderCase;

#define FIRST_VALUE ((TZrUInt32)0x76543210u)
#define HASH_VALUE ((TZrUInt64)0xfedcba9876543210ULL)

static EZrArtifactStatus read_TypeDef(const SZrArtifactSectionView *section,
        TZrUInt32 index, Row *out, SZrArtifactDiagnostic *diagnostic) {
    return ZrCore_Artifact_ReadTypeDefRow(section, index, &out->TypeDef, diagnostic);
}
static int projected_TypeDef(const Row *row) {
    return row->TypeDef.token == FIRST_VALUE &&
           row->TypeDef.constructorContractHash == HASH_VALUE;
}

static EZrArtifactStatus read_TypeIdentity(const SZrArtifactSectionView *section,
        TZrUInt32 index, Row *out, SZrArtifactDiagnostic *diagnostic) {
    return ZrCore_Artifact_ReadTypeIdentityRow(section, index, &out->TypeIdentity, diagnostic);
}
static int projected_TypeIdentity(const Row *row) {
    return row->TypeIdentity.token == FIRST_VALUE &&
           row->TypeIdentity.layoutHash == HASH_VALUE;
}

static EZrArtifactStatus read_MemberDef(const SZrArtifactSectionView *section,
        TZrUInt32 index, Row *out, SZrArtifactDiagnostic *diagnostic) {
    return ZrCore_Artifact_ReadMemberDefRow(section, index, &out->MemberDef, diagnostic);
}
static int projected_MemberDef(const Row *row) {
    return row->MemberDef.token == FIRST_VALUE &&
           row->MemberDef.contractHash == HASH_VALUE;
}

static EZrArtifactStatus read_PropertyDef(const SZrArtifactSectionView *section,
        TZrUInt32 index, Row *out, SZrArtifactDiagnostic *diagnostic) {
    return ZrCore_Artifact_ReadPropertyDefRow(section, index, &out->PropertyDef, diagnostic);
}
static int projected_PropertyDef(const Row *row) {
    return row->PropertyDef.token == FIRST_VALUE &&
           row->PropertyDef.contractHash == HASH_VALUE;
}

static EZrArtifactStatus read_Contract(const SZrArtifactSectionView *section,
        TZrUInt32 index, Row *out, SZrArtifactDiagnostic *diagnostic) {
    return ZrCore_Artifact_ReadContractRow(section, index, &out->Contract, diagnostic);
}
static int projected_Contract(const Row *row) {
    return row->Contract.memberToken == FIRST_VALUE &&
           row->Contract.contractHash == HASH_VALUE;
}

static EZrArtifactStatus read_Layout(const SZrArtifactSectionView *section,
        TZrUInt32 index, Row *out, SZrArtifactDiagnostic *diagnostic) {
    return ZrCore_Artifact_ReadLayoutRow(section, index, &out->Layout, diagnostic);
}
static int projected_Layout(const Row *row) {
    return row->Layout.typeToken == FIRST_VALUE &&
           row->Layout.stableSlotContractHash == HASH_VALUE;
}

static EZrArtifactStatus read_MetadataState(const SZrArtifactSectionView *section,
        TZrUInt32 index, Row *out, SZrArtifactDiagnostic *diagnostic) {
    return ZrCore_Artifact_ReadMetadataStateRow(section, index, &out->MetadataState, diagnostic);
}
static int projected_MetadataState(const Row *row) {
    return row->MetadataState.typeToken == FIRST_VALUE &&
           row->MetadataState.metadataHash == HASH_VALUE;
}

static EZrArtifactStatus read_MetadataRecord(const SZrArtifactSectionView *section,
        TZrUInt32 index, Row *out, SZrArtifactDiagnostic *diagnostic) {
    return ZrCore_Artifact_ReadMetadataRecordRow(section, index, &out->MetadataRecord, diagnostic);
}
static int projected_MetadataRecord(const Row *row) {
    return row->MetadataRecord.ownerToken == FIRST_VALUE &&
           row->MetadataRecord.recordHash == HASH_VALUE;
}

static EZrArtifactStatus read_DomainTransfer(const SZrArtifactSectionView *section,
        TZrUInt32 index, Row *out, SZrArtifactDiagnostic *diagnostic) {
    return ZrCore_Artifact_ReadDomainTransferRow(section, index, &out->DomainTransfer, diagnostic);
}
static int projected_DomainTransfer(const Row *row) {
    return row->DomainTransfer.typeToken == FIRST_VALUE &&
           row->DomainTransfer.providerContractHash == HASH_VALUE;
}

static EZrArtifactStatus read_SchedulerContract(const SZrArtifactSectionView *section,
        TZrUInt32 index, Row *out, SZrArtifactDiagnostic *diagnostic) {
    return ZrCore_Artifact_ReadSchedulerContractRow(section, index, &out->SchedulerContract, diagnostic);
}
static int projected_SchedulerContract(const Row *row) {
    return row->SchedulerContract.schedulerTypeToken == FIRST_VALUE &&
           row->SchedulerContract.schedulerContractHash == HASH_VALUE;
}

static EZrArtifactStatus read_Relocation(const SZrArtifactSectionView *section,
        TZrUInt32 index, Row *out, SZrArtifactDiagnostic *diagnostic) {
    return ZrCore_Artifact_ReadRelocationRow(section, index, &out->Relocation, diagnostic);
}
static int projected_Relocation(const Row *row) {
    return row->Relocation.codeOffset == FIRST_VALUE &&
           row->Relocation.expectedModuleHash == HASH_VALUE;
}

static const ReaderCase readers[] = {
    {"TypeDef", ZR_ARTIFACT_SECTION_TYPE_DEF_TABLE,
     ZR_ARTIFACT_TYPE_DEF_ROW_ENCODED_SIZE, 32u, read_TypeDef, projected_TypeDef},
    {"TypeIdentity", ZR_ARTIFACT_SECTION_TYPE_REF_TABLE,
     ZR_ARTIFACT_TYPE_IDENTITY_ROW_ENCODED_SIZE, 40u, read_TypeIdentity, projected_TypeIdentity},
    {"MemberDef", ZR_ARTIFACT_SECTION_MEMBER_DEF_TABLE,
     ZR_ARTIFACT_MEMBER_DEF_ROW_ENCODED_SIZE, 32u, read_MemberDef, projected_MemberDef},
    {"PropertyDef", ZR_ARTIFACT_SECTION_PROPERTY_DEF_TABLE,
     ZR_ARTIFACT_PROPERTY_DEF_ROW_ENCODED_SIZE, 32u, read_PropertyDef, projected_PropertyDef},
    {"Contract", ZR_ARTIFACT_SECTION_CONTRACT_TABLE,
     ZR_ARTIFACT_CONTRACT_ROW_ENCODED_SIZE, 32u, read_Contract, projected_Contract},
    {"Layout", ZR_ARTIFACT_SECTION_LAYOUT_TABLE,
     ZR_ARTIFACT_LAYOUT_ROW_ENCODED_SIZE, 40u, read_Layout, projected_Layout},
    {"MetadataState", ZR_ARTIFACT_SECTION_METADATA_STATE_TABLE,
     ZR_ARTIFACT_METADATA_STATE_ROW_ENCODED_SIZE, 56u, read_MetadataState, projected_MetadataState},
    {"MetadataRecord", ZR_ARTIFACT_SECTION_METADATA_RECORD_TABLE,
     ZR_ARTIFACT_METADATA_RECORD_ROW_ENCODED_SIZE, 32u, read_MetadataRecord, projected_MetadataRecord},
    {"DomainTransfer", ZR_ARTIFACT_SECTION_DOMAIN_TRANSFER_TABLE,
     ZR_ARTIFACT_DOMAIN_TRANSFER_ROW_ENCODED_SIZE, 40u, read_DomainTransfer, projected_DomainTransfer},
    {"SchedulerContract", ZR_ARTIFACT_SECTION_SCHEDULER_CONTRACT_TABLE,
     ZR_ARTIFACT_SCHEDULER_CONTRACT_ROW_ENCODED_SIZE, 40u, read_SchedulerContract, projected_SchedulerContract},
    {"Relocation", ZR_ARTIFACT_SECTION_RELOCATION_BINDING_TABLE,
     ZR_ARTIFACT_RELOCATION_ROW_ENCODED_SIZE, 32u, read_Relocation, projected_Relocation},
    {"TypeSpec", ZR_ARTIFACT_SECTION_TYPE_SPEC_TABLE,
     ZR_ARTIFACT_TYPE_IDENTITY_ROW_ENCODED_SIZE, 40u, read_TypeIdentity, projected_TypeIdentity}
};

static void put_u32(TZrByte *bytes, TZrUInt32 value) {
    unsigned int i;
    for (i = 0; i < 4; ++i) bytes[i] = (TZrByte)(value >> (i * 8));
}
static void put_u64(TZrByte *bytes, TZrUInt64 value) {
    unsigned int i;
    for (i = 0; i < 8; ++i) bytes[i] = (TZrByte)(value >> (i * 8));
}
static int diagnostic_is_clear(const SZrArtifactDiagnostic *d) {
    return d->status == ZR_ARTIFACT_STATUS_OK &&
           d->sectionKind == 0 && d->rowIndex == 0 && d->byteOffset == 0 &&
           d->expectedToken == 0 && d->actualToken == 0 &&
           d->expectedVersion == 0 && d->actualVersion == 0 &&
           d->expectedHash == 0 && d->actualHash == 0;
}
static int run_rows(const char *mode) {
    size_t i;
    int failures = 0;
    for (i = 0; i < sizeof(readers) / sizeof(readers[0]); ++i) {
        const ReaderCase *r = &readers[i];
        /* Enough real bytes even when the declared view is truncated. */
        TZrByte bytes[2 * ZR_ARTIFACT_METADATA_STATE_ROW_ENCODED_SIZE] = {0};
        SZrArtifactSectionView section;
        SZrArtifactDiagnostic diagnostic;
        Row out;
        Row before;
        TZrUInt32 index = 1;
        EZrArtifactStatus status;
        int success = strcmp(mode, "valid_rows") == 0 ||
                      strcmp(mode, "diagnostic_reuse") == 0;
        memset(&section, 0, sizeof(section));
        section.kind = r->kind;
        section.elementSize = r->size;
        section.elementCount = 2;
        section.byteLength = 2 * r->size;
        section.data = bytes;
        put_u32(bytes + r->size, FIRST_VALUE);
        put_u64(bytes + r->size + r->hashOffset, HASH_VALUE);
        memset(&out, 0xa5, sizeof(out));
        memcpy(&before, &out, sizeof(out));
        memset(&diagnostic, 0, sizeof(diagnostic));
        if (strcmp(mode, "wrong_kind") == 0)
            section.kind = ZR_ARTIFACT_SECTION_STRING_HEAP;
        else if (strcmp(mode, "null_data") == 0)
            section.data = NULL;
        else if (strcmp(mode, "truncated") == 0)
            --section.byteLength;
        else if (strcmp(mode, "inconsistent_count") == 0)
            section.elementCount = 3;
        else if (strcmp(mode, "huge_count") == 0) {
            section.elementCount = UINT32_MAX;
            index = 0; /* Never ask the old implementation to access outside storage. */
        } else if (strcmp(mode, "invalid_row") == 0)
            index = section.elementCount;
        else if (strcmp(mode, "diagnostic_reuse") == 0) {
            status = r->read(&section, section.elementCount, &out, &diagnostic);
            if (status == ZR_ARTIFACT_STATUS_OK) {
                fprintf(stderr, "%s: invalid-row preflight accepted\n", r->name);
                ++failures;
            }
            /* Seed every field, including fields the failure helper normally clears. */
            memset(&diagnostic, 0x5a, sizeof(diagnostic));
        } else if (!success) {
            fprintf(stderr, "unknown row case: %s\n", mode);
            return 2;
        }
        status = r->read(&section, index, &out, &diagnostic);
        if (success) {
            if (status != ZR_ARTIFACT_STATUS_OK || !r->projected(&out) ||
                (strcmp(mode, "diagnostic_reuse") == 0 && !diagnostic_is_clear(&diagnostic))) {
                fprintf(stderr, "%s: %s failed status=%d projection=%d cleared=%d\n",
                        r->name, mode, (int)status, r->projected(&out),
                        diagnostic_is_clear(&diagnostic));
                ++failures;
            }
        } else if (status == ZR_ARTIFACT_STATUS_OK ||
                   memcmp(&out, &before, sizeof(out)) != 0) {
            fprintf(stderr, "%s: %s accepted or changed output (status=%d)\n",
                    r->name, mode, (int)status);
            ++failures;
        }
    }
    return failures != 0;
}
static int status_names(void) {
    static const struct { EZrArtifactStatus status; const char *name; } cases[] = {
        {ZR_ARTIFACT_STATUS_SCHEDULER_POLICY_MISMATCH, "scheduler-policy-mismatch"},
        {ZR_ARTIFACT_STATUS_SCHEDULER_REQUIREMENT_MISMATCH, "scheduler-requirement-mismatch"},
        {ZR_ARTIFACT_STATUS_SCHEDULER_ABI_MISMATCH, "scheduler-abi-mismatch"},
        {ZR_ARTIFACT_STATUS_TRANSPORT_CONTRACT_MISMATCH, "transport-contract-mismatch"},
        {ZR_ARTIFACT_STATUS_SCHEDULER_CONTRACT_MISMATCH, "scheduler-contract-mismatch"},
        {(EZrArtifactStatus)-1, "unknown"},
        {(EZrArtifactStatus)UINT32_MAX, "unknown"}
    };
    size_t i;
    int failures = 0;
    for (i = 0; i < sizeof(cases) / sizeof(cases[0]); ++i) {
        const char *actual = ZrCore_Artifact_StatusName(cases[i].status);
        if (actual == NULL || strcmp(actual, cases[i].name) != 0) {
            fprintf(stderr, "status %d: expected %s, got %s\n",
                    (int)cases[i].status, cases[i].name, actual ? actual : "(null)");
            ++failures;
        }
    }
    return failures != 0;
}
int main(int argc, char **argv) {
#ifdef _WIN32
    SetErrorMode(SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX | SEM_NOOPENFILEERRORBOX);
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
#endif
    if (argc != 2) return 2;
    return strcmp(argv[1], "status_names") == 0 ? status_names() : run_rows(argv[1]);
}
