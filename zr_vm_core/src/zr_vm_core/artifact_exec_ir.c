#include "zr_vm_core/artifact_exec_ir.h"

#include <stdlib.h>
#include <string.h>

/* 各宽度字段按小端写入；调用方先证明目标至少有两个字节。 */
static void put16(TZrByte *p, TZrUInt16 v) { p[0]=(TZrByte)v; p[1]=(TZrByte)(v>>8); }
/* 目录、长度与 token 共用固定四字节线格式，不依赖主机字节序。 */
static void put32(TZrByte *p, TZrUInt32 v) { for (TZrUInt32 i=0;i<4u;i++) p[i]=(TZrByte)(v>>(8u*i)); }
/* 身份哈希与 ABI 相关哈希固定为八字节小端字段。 */
static void put64(TZrByte *p, TZrUInt64 v) { for (TZrUInt32 i=0;i<8u;i++) p[i]=(TZrByte)(v>>(8u*i)); }
/* reader 在读取前已核对 header 长度；这里仅恢复线格式数值。 */
static TZrUInt16 get16(const TZrByte *p) { return (TZrUInt16)p[0] | (TZrUInt16)((TZrUInt16)p[1]<<8); }
/* 目录项经整体边界校验后才交给此函数读取。 */
static TZrUInt32 get32(const TZrByte *p) { TZrUInt32 v=0u; for (TZrUInt32 i=0;i<4u;i++) v|=(TZrUInt32)p[i]<<(8u*i); return v; }
/* 八字节读取仅用于 header 与已验证宽度的 relocation 行。 */
static TZrUInt64 get64(const TZrByte *p) { TZrUInt64 v=0u; for (TZrUInt32 i=0;i<8u;i++) v|=(TZrUInt64)p[i]<<(8u*i); return v; }

/* 统一返回状态并写入可选诊断；其他输出由各入口负责。 */
static EZrArtifactExecIrStatus fail(SZrArtifactExecIrDiagnostic *d, EZrArtifactExecIrStatus s, TZrUInt32 off, TZrUInt32 sec, TZrUInt32 row) {
    if (d) { d->status=s; d->byteOffset=off; d->sectionKind=sec; d->rowIndex=row; d->token=0u; }
    return s;
}
/* Relocation failures carry the stable target token even when resolution fails. */
static EZrArtifactExecIrStatus fail_relocation(
        SZrArtifactExecIrDiagnostic *diagnostic,
        EZrArtifactExecIrStatus status, TZrUInt32 byteOffset,
        TZrUInt32 rowIndex, TZrUInt32 token) {
    fail(diagnostic, status, byteOffset,
         ZR_ARTIFACT_EXEC_IR_SECTION_RELOCATIONS, rowIndex);
    if (diagnostic != ZR_NULL) diagnostic->token = token;
    return status;
}
/* 用 64 位和校验目录或节片段的半开区间，避免 32 位加法回绕。 */
static TZrBool range_ok(TZrUInt32 off, TZrUInt32 len, TZrUInt32 total) {
    return (TZrBool)((TZrUInt64)off + len <= total);
}

/** @brief 对线格式字节计算稳定哈希，供写入、读回、ExecBC 与 hotpatch 校验共用。
 * @note 不拥有输入；空指针且长度非零返回零，空片段返回初始哈希值。 */
TZrUInt64 ZrCore_ArtifactExecIr_HashBytes(const TZrByte *bytes, TZrUInt32 length) {
    TZrUInt64 h=UINT64_C(1469598103934665603);
    if (!bytes && length) return 0u;
    for (TZrUInt32 i=0;i<length;i++) { h ^= bytes[i]; h *= UINT64_C(1099511628211); }
    return h;
}

/** @brief 为 parser writer 计算包含 header、目录和全部 payload 的编码长度。
 * @note 只读借用的 section 数组及数据；失败不写 *out，诊断可为空。 */
EZrArtifactExecIrStatus ZrCore_ArtifactExecIr_GetEncodedSize(const SZrArtifactExecIrDocument *d, TZrUInt32 *out, SZrArtifactExecIrDiagnostic *diag) {
    TZrUInt64 total=ZR_ARTIFACT_EXEC_IR_HEADER_SIZE;
    if (!d || !out || d->sectionCount>ZR_ARTIFACT_EXEC_IR_MAX_SECTIONS || (d->sectionCount && !d->sections)) return fail(diag,ZR_ARTIFACT_EXEC_IR_INVALID_ARGUMENT,0,0,0);
    /* 先按 64 位累加，再限制整个线格式长度；节的计数和槽宽必须自洽。 */
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

/** @brief 在容量与节内容校验通过后编码无指针的 ExecIR/ExecBC 产物。
 * @note 输入节由调用方持有；成功写出长度，失败保留 *written 的原值，
 * parser 包装层另将其置零。 */
EZrArtifactExecIrStatus ZrCore_ArtifactExecIr_Write(const SZrArtifactExecIrDocument *d, TZrByte *buffer, TZrUInt32 capacity, TZrUInt32 *written, SZrArtifactExecIrDiagnostic *diag) {
    TZrUInt32 total; EZrArtifactExecIrStatus st=ZrCore_ArtifactExecIr_GetEncodedSize(d,&total,diag);
    if (st!=ZR_ARTIFACT_EXEC_IR_OK) return st;
    if (!buffer || !written || capacity<total) return fail(diag,ZR_ARTIFACT_EXEC_IR_TRUNCATED,0,0,0);
    /* 写目标前先核非零的 ExecIR/ExecBC 期望哈希，失败时不产生部分编码。 */
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
    /* TODO: 核定输入节 data 与输出 buffer 是否允许重叠；清零目标先于 payload 拷贝，
     * 当前 parser 测试使用独立缓冲区，重叠时可能破坏尚未读取的源字节。 */
    memset(buffer,0,total); put32(buffer,ZR_ARTIFACT_EXEC_IR_MAGIC); put16(buffer+4,ZR_ARTIFACT_EXEC_IR_SCHEMA_VERSION); put16(buffer+6,d->flags); put16(buffer+8,d->abiVersion); put64(buffer+12,d->moduleHash); put64(buffer+20,d->execIrHash); put64(buffer+28,d->execBcHash); put32(buffer+36,d->sectionCount); put32(buffer+40,ZR_ARTIFACT_EXEC_IR_HEADER_SIZE); put32(buffer+44,total);
    TZrUInt32 dir=ZR_ARTIFACT_EXEC_IR_HEADER_SIZE, data=dir+d->sectionCount*ZR_ARTIFACT_EXEC_IR_SECTION_SIZE;
    for (TZrUInt32 i=0;i<d->sectionCount;i++) { const SZrArtifactExecIrSectionInput *s=&d->sections[i]; put32(buffer+dir,s->kind); put32(buffer+dir+4,s->flags); put32(buffer+dir+8,s->elementCount); put32(buffer+dir+12,s->elementSize); put32(buffer+dir+16,data); put32(buffer+dir+20,s->byteLength); if(s->byteLength) memcpy(buffer+data,s->data,s->byteLength); dir+=ZR_ARTIFACT_EXEC_IR_SECTION_SIZE; data+=s->byteLength; }
    *written=total; return fail(diag,ZR_ARTIFACT_EXEC_IR_OK,0,0,0);
}

/** @brief 从有界字节流恢复借用原缓冲区的 view，并校验目录与非零期望哈希。
 * @note out 内 section.data 指向 buffer，调用方须维持字节存活；失败时 out 清零。 */
EZrArtifactExecIrStatus ZrCore_ArtifactExecIr_Read(const TZrByte *buffer, TZrUInt32 length, SZrArtifactExecIrView *out, SZrArtifactExecIrDiagnostic *diag) {
    SZrArtifactExecIrView temporary;
    if (out == ZR_NULL) {
        return fail(diag,ZR_ARTIFACT_EXEC_IR_INVALID_ARGUMENT,0,0,0);
    }
    memset(out,0,sizeof(*out));
    if (buffer == ZR_NULL)
        return fail(diag,ZR_ARTIFACT_EXEC_IR_INVALID_ARGUMENT,0,0,0);
    if (length<ZR_ARTIFACT_EXEC_IR_HEADER_SIZE) return fail(diag,ZR_ARTIFACT_EXEC_IR_TRUNCATED,0,0,0);
    if (get32(buffer)!=ZR_ARTIFACT_EXEC_IR_MAGIC) return fail(diag,ZR_ARTIFACT_EXEC_IR_BAD_MAGIC,0,0,0);
    if (get16(buffer+4)!=ZR_ARTIFACT_EXEC_IR_SCHEMA_VERSION) return fail(diag,ZR_ARTIFACT_EXEC_IR_UNSUPPORTED_VERSION,4,0,0);
    if (get16(buffer+10)!=0u) return fail(diag,ZR_ARTIFACT_EXEC_IR_INVALID_DIRECTORY,10,0,0);
    TZrUInt32 count=get32(buffer+36), dirOff=get32(buffer+40), total=get32(buffer+44);
    if (count>ZR_ARTIFACT_EXEC_IR_MAX_SECTIONS || dirOff<ZR_ARTIFACT_EXEC_IR_HEADER_SIZE || !range_ok(dirOff,count*ZR_ARTIFACT_EXEC_IR_SECTION_SIZE,length) || total!=length || total>ZR_ARTIFACT_EXEC_IR_MAX_BYTES) return fail(diag,ZR_ARTIFACT_EXEC_IR_INVALID_DIRECTORY,36,0,0);
    memset(&temporary,0,sizeof(temporary));
    temporary.abiVersion=get16(buffer+8); temporary.flags=get16(buffer+6);
    temporary.moduleHash=get64(buffer+12); temporary.execIrHash=get64(buffer+20);
    temporary.execBcHash=get64(buffer+28); temporary.sectionCount=count;
    temporary.buffer=buffer; temporary.bufferLength=length;
    for (TZrUInt32 i=0;i<count;i++) {
        const TZrByte *p=buffer+dirOff+i*ZR_ARTIFACT_EXEC_IR_SECTION_SIZE;
        EZrArtifactExecIrSectionKind kind=(EZrArtifactExecIrSectionKind)get32(p);
        TZrUInt32 flags=get32(p+4), elems=get32(p+8), size=get32(p+12);
        TZrUInt32 off=get32(p+16), bytes=get32(p+20);
        if(kind<ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_IR ||
           kind>ZR_ARTIFACT_EXEC_IR_SECTION_RELOCATIONS)
            return fail(diag,ZR_ARTIFACT_EXEC_IR_UNKNOWN_SECTION,
                        dirOff+i*ZR_ARTIFACT_EXEC_IR_SECTION_SIZE,kind,i);
        if((flags&~ZR_ARTIFACT_EXEC_IR_SECTION_OPTIONAL) ||
           (elems && !size) ||
           (size && (TZrUInt64)elems*size!=bytes) ||
           !range_ok(off,bytes,length) ||
           off<dirOff+count*ZR_ARTIFACT_EXEC_IR_SECTION_SIZE)
            return fail(diag,ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,off,kind,i);
        for(TZrUInt32 j=0;j<i;j++) {
            const SZrArtifactExecIrSectionView *q=&temporary.sections[j];
            if(q->kind==kind)
                return fail(diag,ZR_ARTIFACT_EXEC_IR_DUPLICATE_SECTION,off,kind,i);
            if(bytes && q->byteLength &&
               off<q->byteOffset+q->byteLength && q->byteOffset<off+bytes)
                return fail(diag,ZR_ARTIFACT_EXEC_IR_OVERLAP,off,kind,i);
        }
        temporary.sections[i]=(SZrArtifactExecIrSectionView){
            kind,flags,elems,size,off,bytes,buffer+off};
    }
    for (TZrUInt32 i=0u; i<count; ++i) {
        if (temporary.sections[i].kind == ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_IR &&
            temporary.execIrHash != 0u &&
            ZrCore_ArtifactExecIr_HashBytes(temporary.sections[i].data,
                                             temporary.sections[i].byteLength) != temporary.execIrHash) {
            return fail(diag, ZR_ARTIFACT_EXEC_IR_HASH_MISMATCH,
                         temporary.sections[i].byteOffset,
                         temporary.sections[i].kind, i);
        }
        if (temporary.sections[i].kind == ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_BC &&
            temporary.execBcHash != 0u &&
            ZrCore_ArtifactExecIr_HashBytes(temporary.sections[i].data,
                                             temporary.sections[i].byteLength) != temporary.execBcHash) {
            return fail(diag, ZR_ARTIFACT_EXEC_IR_HASH_MISMATCH,
                         temporary.sections[i].byteOffset,
                         temporary.sections[i].kind, i);
        }
    }
    *out=temporary;
    return fail(diag,ZR_ARTIFACT_EXEC_IR_OK,0,0,0);
}

/** @brief 从已验证的 view 中按节种类取借用的 section 指针。
 * @return 缺失节返回 INVALID_SECTION 且将 *out 置空；结果随 view 和底层 buffer 失效。 */
EZrArtifactExecIrStatus ZrCore_ArtifactExecIr_FindSection(const SZrArtifactExecIrView *v, EZrArtifactExecIrSectionKind kind, const SZrArtifactExecIrSectionView **out, SZrArtifactExecIrDiagnostic *diag) { if(!v||!out) return fail(diag,ZR_ARTIFACT_EXEC_IR_INVALID_ARGUMENT,0,0,0); *out=ZR_NULL; for(TZrUInt32 i=0;i<v->sectionCount;i++) if(v->sections[i].kind==kind){*out=&v->sections[i];return fail(diag,ZR_ARTIFACT_EXEC_IR_OK,0,kind,0);} return fail(diag,ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,0,kind,0); }

/** @brief 先校验全部相对 ExecIR 的 relocation 偏移，再解析本地目标。
 * @note 先写临时数组，全部成功后才复制到调用方 resolved；回调借用 user。 */
EZrArtifactExecIrStatus ZrCore_ArtifactExecIr_ValidateRelocations(const SZrArtifactExecIrView *v, FZrArtifactExecIrResolve resolver, TZrUInt32 *resolved, TZrUInt32 capacity, TZrPtr user, SZrArtifactExecIrDiagnostic *diag) {
    const SZrArtifactExecIrSectionView *s;
    const SZrArtifactExecIrSectionView *codeSection;
    TZrUInt32 *temporary;
    if(!v||!resolved||!resolver) return fail(diag,ZR_ARTIFACT_EXEC_IR_INVALID_ARGUMENT,0,0,0);
    EZrArtifactExecIrStatus st=ZrCore_ArtifactExecIr_FindSection(v,ZR_ARTIFACT_EXEC_IR_SECTION_RELOCATIONS,&s,diag);
    if(st!=ZR_ARTIFACT_EXEC_IR_OK) return st;
    if(s->elementSize!=ZR_ARTIFACT_EXEC_IR_RELOCATION_SIZE || capacity<s->elementCount) return fail(diag,ZR_ARTIFACT_EXEC_IR_INVALID_SECTION,s->byteOffset,s->kind,0);
    st=ZrCore_ArtifactExecIr_FindSection(v,ZR_ARTIFACT_EXEC_IR_SECTION_EXEC_IR,
                                          &codeSection,diag);
    if(st!=ZR_ARTIFACT_EXEC_IR_OK)
        return fail_relocation(diag,ZR_ARTIFACT_EXEC_IR_INVALID_RELOCATION,
                               s->byteOffset,0u,0u);
    for(TZrUInt32 i=0;i<s->elementCount;i++) {
        const TZrByte *p=s->data+i*ZR_ARTIFACT_EXEC_IR_RELOCATION_SIZE;
        TZrUInt32 token=get32(p), code=get32(p+8);
        TZrUInt64 hash=get64(p+16), contract=get64(p+24);
        if(!token || !hash || !contract || code>=codeSection->byteLength)
            return fail_relocation(diag,ZR_ARTIFACT_EXEC_IR_INVALID_RELOCATION,
                                   s->byteOffset+i*s->elementSize,i,token);
    }
    if (s->elementCount > (TZrUInt32)(SIZE_MAX / sizeof(*temporary))) return fail(diag,ZR_ARTIFACT_EXEC_IR_LIMIT,s->byteOffset,s->kind,0);
    /* 临时结果隔离部分成功的回调；任何失败先释放，再保持 resolved 原样。 */
    temporary = (TZrUInt32 *)malloc((TZrSize)s->elementCount * sizeof(*temporary));
    if (temporary == ZR_NULL && s->elementCount != 0u) return fail(diag,ZR_ARTIFACT_EXEC_IR_LIMIT,s->byteOffset,s->kind,0);
    for(TZrUInt32 i=0;i<s->elementCount;i++) {
        const TZrByte *p=s->data+i*ZR_ARTIFACT_EXEC_IR_RELOCATION_SIZE;
        TZrUInt32 token=get32(p), kind=get32(p+4);
        TZrUInt64 hash=get64(p+16), contract=get64(p+24);
        if(!resolver(token,kind,hash,contract,&temporary[i],user)) {
            free(temporary);
            return fail_relocation(diag,ZR_ARTIFACT_EXEC_IR_RESOLUTION_FAILED,
                                   s->byteOffset+i*s->elementSize,i,token);
        }
    }
    if (s->elementCount) memcpy(resolved, temporary, (TZrSize)s->elementCount * sizeof(*temporary));
    free(temporary); return fail(diag,ZR_ARTIFACT_EXEC_IR_OK,0,0,0);
}

/** @brief 将常用状态映射为静态名称；其余状态统一返回 invalid-artifact。 */
const TZrChar *ZrCore_ArtifactExecIr_StatusName(EZrArtifactExecIrStatus s){switch(s){case ZR_ARTIFACT_EXEC_IR_OK:return "ok";case ZR_ARTIFACT_EXEC_IR_BAD_MAGIC:return "bad-magic";case ZR_ARTIFACT_EXEC_IR_UNSUPPORTED_VERSION:return "unsupported-version";case ZR_ARTIFACT_EXEC_IR_OVERLAP:return "overlap";case ZR_ARTIFACT_EXEC_IR_INVALID_RELOCATION:return "invalid-relocation";case ZR_ARTIFACT_EXEC_IR_RESOLUTION_FAILED:return "resolution-failed";default:return "invalid-artifact";}}
