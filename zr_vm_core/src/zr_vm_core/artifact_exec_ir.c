#include "zr_vm_core/artifact_exec_ir.h"

#include <stdlib.h>
#include <string.h>

static void put16(TZrByte *p, TZrUInt16 v) { p[0]=(TZrByte)v; p[1]=(TZrByte)(v>>8); }
static void put32(TZrByte *p, TZrUInt32 v) { for (TZrUInt32 i=0;i<4u;i++) p[i]=(TZrByte)(v>>(8u*i)); }
static void put64(TZrByte *p, TZrUInt64 v) { for (TZrUInt32 i=0;i<8u;i++) p[i]=(TZrByte)(v>>(8u*i)); }
static TZrUInt16 get16(const TZrByte *p) { return (TZrUInt16)p[0] | (TZrUInt16)((TZrUInt16)p[1]<<8); }
static TZrUInt32 get32(const TZrByte *p) { TZrUInt32 v=0u; for (TZrUInt32 i=0;i<4u;i++) v|=(TZrUInt32)p[i]<<(8u*i); return v; }
static TZrUInt64 get64(const TZrByte *p) { TZrUInt64 v=0u; for (TZrUInt32 i=0;i<8u;i++) v|=(TZrUInt64)p[i]<<(8u*i); return v; }

static EZrArtifactExecIrStatus fail(SZrArtifactExecIrDiagnostic *d, EZrArtifactExecIrStatus s, TZrUInt32 off, TZrUInt32 sec, TZrUInt32 row) {
    if (d) { d->status=s; d->byteOffset=off; d->sectionKind=sec; d->rowIndex=row; d->token=0u; }
    return s;
}
static TZrBool range_ok(TZrUInt32 off, TZrUInt32 len, TZrUInt32 total) {
    return (TZrBool)((TZrUInt64)off + len <= total);
}

TZrUInt64 ZrCore_ArtifactExecIr_HashBytes(const TZrByte *bytes, TZrUInt32 length) {
    TZrUInt64 h=UINT64_C(1469598103934665603);
    if (!bytes && length) return 0u;
    for (TZrUInt32 i=0;i<length;i++) { h ^= bytes[i]; h *= UINT64_C(1099511628211); }
    return h;
}

EZrArtifactExecIrStatus ZrCore_ArtifactExecIr_GetEncodedSize(const SZrArtifactExecIrDocument *d, TZrUInt32 *out, SZrArtifactExecIrDiagnostic *diag) {
    TZrUInt64 total=ZR_ARTIFACT_EXEC_IR_HEADER_SIZE;
    if (!d || !out || d->sectionCount>ZR_ARTIFACT_EXEC_IR_MAX_SECTIONS || (d->sectionCount && !d->sections)) return fail(diag,ZR_ARTIFACT_EXEC_IR_INVALID_ARGUMENT,0,0,0);
    total += (TZrUInt64)d->sectionCount * ZR_ARTIFACT_EXEC_IR_SECTION_SIZE;
    for (TZrUInt32 i=0;i<d->sectionCount;i++) {
        const SZrArtifactExecIrSectionInput *s=&d->sections[i];
        if (s->kind<ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_IR || s->kind>ZR_ARTIFACT_EXEC_IR_SECTION_RELOCATIONS || (s->flags & ~ZR_ARTIFACT_EXEC_IR_SECTION_OPTIONAL) || (s->byteLength && !s->data) || (s->elementCount && !s->elementSize) || (s->elementSize && (TZrUInt64)s->elementCount*s->elementSize != s->byteLength)) return fail(diag,ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,0,s->kind,i);
        for (TZrUInt32 j=0;j<i;j++) if (d->sections[j].kind==s->kind) return fail(diag,ZR_ARTIFACT_EXEC_IR_DUPLICATE_SECTION,0,s->kind,i);
        total += s->byteLength;
    }
    if (total>ZR_ARTIFACT_EXEC_IR_MAX_BYTES || total>UINT32_MAX) return fail(diag,ZR_ARTIFACT_EXEC_IR_LIMIT,0,0,0);
    *out=(TZrUInt32)total; return fail(diag,ZR_ARTIFACT_EXEC_IR_OK,0,0,0);
}

EZrArtifactExecIrStatus ZrCore_ArtifactExecIr_Write(const SZrArtifactExecIrDocument *d, TZrByte *buffer, TZrUInt32 capacity, TZrUInt32 *written, SZrArtifactExecIrDiagnostic *diag) {
    TZrUInt32 total; EZrArtifactExecIrStatus st=ZrCore_ArtifactExecIr_GetEncodedSize(d,&total,diag);
    if (st!=ZR_ARTIFACT_EXEC_IR_OK) return st;
    if (!buffer || !written || capacity<total) return fail(diag,ZR_ARTIFACT_EXEC_IR_TRUNCATED,0,0,0);
    for (TZrUInt32 i=0u; i<d->sectionCount; ++i) {
        const SZrArtifactExecIrSectionInput *s = &d->sections[i];
        TZrUInt64 expected = s->kind == ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_IR
                                     ? d->execIrHash
                                     : (s->kind == ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_BC
                                                ? d->execBcHash
                                                : 0u);
        if (expected != 0u &&
            ZrCore_ArtifactExecIr_HashBytes(s->data, s->byteLength) != expected) {
            return fail(diag, ZR_ARTIFACT_EXEC_IR_HASH_MISMATCH, 0u, s->kind, i);
        }
    }
    memset(buffer,0,total); put32(buffer,ZR_ARTIFACT_EXEC_IR_MAGIC); put16(buffer+4,ZR_ARTIFACT_EXEC_IR_SCHEMA_VERSION); put16(buffer+6,d->flags); put16(buffer+8,d->abiVersion); put64(buffer+12,d->moduleHash); put64(buffer+20,d->execIrHash); put64(buffer+28,d->execBcHash); put32(buffer+36,d->sectionCount); put32(buffer+40,ZR_ARTIFACT_EXEC_IR_HEADER_SIZE); put32(buffer+44,total);
    TZrUInt32 dir=ZR_ARTIFACT_EXEC_IR_HEADER_SIZE, data=dir+d->sectionCount*ZR_ARTIFACT_EXEC_IR_SECTION_SIZE;
    for (TZrUInt32 i=0;i<d->sectionCount;i++) { const SZrArtifactExecIrSectionInput *s=&d->sections[i]; put32(buffer+dir,s->kind); put32(buffer+dir+4,s->flags); put32(buffer+dir+8,s->elementCount); put32(buffer+dir+12,s->elementSize); put32(buffer+dir+16,data); put32(buffer+dir+20,s->byteLength); if(s->byteLength) memcpy(buffer+data,s->data,s->byteLength); dir+=ZR_ARTIFACT_EXEC_IR_SECTION_SIZE; data+=s->byteLength; }
    *written=total; return fail(diag,ZR_ARTIFACT_EXEC_IR_OK,0,0,0);
}

EZrArtifactExecIrStatus ZrCore_ArtifactExecIr_Read(const TZrByte *buffer, TZrUInt32 length, SZrArtifactExecIrView *out, SZrArtifactExecIrDiagnostic *diag) {
    if (!buffer || !out) {
        return fail(diag,ZR_ARTIFACT_EXEC_IR_INVALID_ARGUMENT,0,0,0);
    }
    memset(out,0,sizeof(*out));
    if (length<ZR_ARTIFACT_EXEC_IR_HEADER_SIZE) return fail(diag,ZR_ARTIFACT_EXEC_IR_TRUNCATED,0,0,0);
    if (get32(buffer)!=ZR_ARTIFACT_EXEC_IR_MAGIC) return fail(diag,ZR_ARTIFACT_EXEC_IR_BAD_MAGIC,0,0,0);
    if (get16(buffer+4)!=ZR_ARTIFACT_EXEC_IR_SCHEMA_VERSION) return fail(diag,ZR_ARTIFACT_EXEC_IR_UNSUPPORTED_VERSION,4,0,0);
    TZrUInt32 count=get32(buffer+36), dirOff=get32(buffer+40), total=get32(buffer+44);
    if (count>ZR_ARTIFACT_EXEC_IR_MAX_SECTIONS || dirOff<ZR_ARTIFACT_EXEC_IR_HEADER_SIZE || !range_ok(dirOff,count*ZR_ARTIFACT_EXEC_IR_SECTION_SIZE,length) || total!=length || total>ZR_ARTIFACT_EXEC_IR_MAX_BYTES) return fail(diag,ZR_ARTIFACT_EXEC_IR_INVALID_DIRECTORY,36,0,0);
    out->abiVersion=get16(buffer+8); out->flags=get16(buffer+6); out->moduleHash=get64(buffer+12); out->execIrHash=get64(buffer+20); out->execBcHash=get64(buffer+28); out->sectionCount=count; out->buffer=buffer; out->bufferLength=length;
    for (TZrUInt32 i=0;i<count;i++) { const TZrByte *p=buffer+dirOff+i*ZR_ARTIFACT_EXEC_IR_SECTION_SIZE; EZrArtifactExecIrSectionKind kind=(EZrArtifactExecIrSectionKind)get32(p); TZrUInt32 flags=get32(p+4), elems=get32(p+8), size=get32(p+12), off=get32(p+16), bytes=get32(p+20); if(kind<ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_IR||kind>ZR_ARTIFACT_EXEC_IR_SECTION_RELOCATIONS) return fail(diag,ZR_ARTIFACT_EXEC_IR_UNKNOWN_SECTION,dirOff+i*ZR_ARTIFACT_EXEC_IR_SECTION_SIZE,kind,i); if(flags&~ZR_ARTIFACT_EXEC_IR_SECTION_OPTIONAL || (size && (TZrUInt64)elems*size!=bytes) || !range_ok(off,bytes,length) || off<dirOff+count*ZR_ARTIFACT_EXEC_IR_SECTION_SIZE) return fail(diag,ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,off,kind,i); for(TZrUInt32 j=0;j<i;j++){const SZrArtifactExecIrSectionView *q=&out->sections[j]; if(q->kind==kind) return fail(diag,ZR_ARTIFACT_EXEC_IR_DUPLICATE_SECTION,off,kind,i); if(bytes && q->byteLength && ((off<q->byteOffset+q->byteLength)&&(q->byteOffset<off+bytes))) return fail(diag,ZR_ARTIFACT_EXEC_IR_OVERLAP,off,kind,i);} out->sections[i]=(SZrArtifactExecIrSectionView){kind,flags,elems,size,off,bytes,buffer+off}; }
    for (TZrUInt32 i=0u; i<count; ++i) {
        if (out->sections[i].kind == ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_IR &&
            out->execIrHash != 0u &&
            ZrCore_ArtifactExecIr_HashBytes(out->sections[i].data,
                                             out->sections[i].byteLength) != out->execIrHash) {
            return fail(diag, ZR_ARTIFACT_EXEC_IR_HASH_MISMATCH,
                         out->sections[i].byteOffset, out->sections[i].kind, i);
        }
        if (out->sections[i].kind == ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_BC &&
            out->execBcHash != 0u &&
            ZrCore_ArtifactExecIr_HashBytes(out->sections[i].data,
                                             out->sections[i].byteLength) != out->execBcHash) {
            return fail(diag, ZR_ARTIFACT_EXEC_IR_HASH_MISMATCH,
                         out->sections[i].byteOffset, out->sections[i].kind, i);
        }
    }
    return fail(diag,ZR_ARTIFACT_EXEC_IR_OK,0,0,0);
}

EZrArtifactExecIrStatus ZrCore_ArtifactExecIr_FindSection(const SZrArtifactExecIrView *v, EZrArtifactExecIrSectionKind kind, const SZrArtifactExecIrSectionView **out, SZrArtifactExecIrDiagnostic *diag) { if(!v||!out) return fail(diag,ZR_ARTIFACT_EXEC_IR_INVALID_ARGUMENT,0,0,0); *out=ZR_NULL; for(TZrUInt32 i=0;i<v->sectionCount;i++) if(v->sections[i].kind==kind){*out=&v->sections[i];return fail(diag,ZR_ARTIFACT_EXEC_IR_OK,0,kind,0);} return fail(diag,ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,0,kind,0); }

EZrArtifactExecIrStatus ZrCore_ArtifactExecIr_ValidateRelocations(const SZrArtifactExecIrView *v, FZrArtifactExecIrResolve resolver, TZrUInt32 *resolved, TZrUInt32 capacity, TZrPtr user, SZrArtifactExecIrDiagnostic *diag) {
    const SZrArtifactExecIrSectionView *s;
    TZrUInt32 *temporary;
    if(!v||!resolved||!resolver) return fail(diag,ZR_ARTIFACT_EXEC_IR_INVALID_ARGUMENT,0,0,0);
    EZrArtifactExecIrStatus st=ZrCore_ArtifactExecIr_FindSection(v,ZR_ARTIFACT_EXEC_IR_SECTION_RELOCATIONS,&s,diag);
    if(st!=ZR_ARTIFACT_EXEC_IR_OK) return st;
    if(s->elementSize!=ZR_ARTIFACT_EXEC_IR_RELOCATION_SIZE || capacity<s->elementCount) return fail(diag,ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,s->byteOffset,s->kind,0);
    if (s->elementCount > (TZrUInt32)(SIZE_MAX / sizeof(*temporary))) return fail(diag,ZR_ARTIFACT_EXEC_IR_LIMIT,s->byteOffset,s->kind,0);
    temporary = (TZrUInt32 *)malloc((TZrSize)s->elementCount * sizeof(*temporary));
    if (temporary == ZR_NULL && s->elementCount != 0u) return fail(diag,ZR_ARTIFACT_EXEC_IR_LIMIT,s->byteOffset,s->kind,0);
    for(TZrUInt32 i=0;i<s->elementCount;i++){const TZrByte *p=s->data+i*ZR_ARTIFACT_EXEC_IR_RELOCATION_SIZE; TZrUInt32 token=get32(p), kind=get32(p+4), code=get32(p+8); TZrUInt64 hash=get64(p+16), contract=get64(p+24); if(!token || !hash || !contract || (TZrUInt64)code>=v->bufferLength || !resolver(token,kind,hash,contract,&temporary[i],user)){ if(diag) diag->token=token; free(temporary); return fail(diag,ZR_ARTIFACT_EXEC_IR_RESOLUTION_FAILED, s->byteOffset+i*s->elementSize,s->kind,i); }}
    if (s->elementCount) memcpy(resolved, temporary, (TZrSize)s->elementCount * sizeof(*temporary));
    free(temporary); return fail(diag,ZR_ARTIFACT_EXEC_IR_OK,0,0,0);
}

const TZrChar *ZrCore_ArtifactExecIr_StatusName(EZrArtifactExecIrStatus s){switch(s){case ZR_ARTIFACT_EXEC_IR_OK:return "ok";case ZR_ARTIFACT_EXEC_IR_BAD_MAGIC:return "bad-magic";case ZR_ARTIFACT_EXEC_IR_UNSUPPORTED_VERSION:return "unsupported-version";case ZR_ARTIFACT_EXEC_IR_OVERLAP:return "overlap";case ZR_ARTIFACT_EXEC_IR_RESOLUTION_FAILED:return "resolution-failed";default:return "invalid-artifact";}}
