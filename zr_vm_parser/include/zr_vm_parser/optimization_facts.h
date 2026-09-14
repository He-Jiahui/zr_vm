#ifndef ZR_VM_PARSER_OPTIMIZATION_FACTS_H
#define ZR_VM_PARSER_OPTIMIZATION_FACTS_H

/*
 * Parser-owned, pointer-free optimization facts.
 *
 * This header is intentionally independent from AST and type-name storage.
 * Producers publish stable scalar identities (tokens, hashes, layout ids and
 * generations); consumers can therefore ask whether an optimization is
 * proven without rediscovering source spelling or inspecting a runtime
 * object.  A missing proof is represented by UNKNOWN and must retain the
 * baseline implementation.  Only an explicitly invalid language use is an
 * error.
 */

#include "zr_vm_core/execution_contract.h"
#include "zr_vm_parser/conf.h"

/* The fields added to this contract are append-only.  A producer must use the
 * initializer below instead of relying on C struct zero-initialization. */
#define ZR_OPTIMIZATION_FACTS_SCHEMA_VERSION ((TZrUInt32)1u)

typedef enum EZrOptimizationFactValidity {
    ZR_OPTIMIZATION_FACTS_PROVEN = 0,
    ZR_OPTIMIZATION_FACTS_UNKNOWN = 1,
    ZR_OPTIMIZATION_FACTS_INVALID_LANGUAGE_USE = 2
} EZrOptimizationFactValidity;
typedef EZrOptimizationFactValidity EZrOptimizationFactsValidity;

#define ZR_OPTIMIZATION_FACT_VALIDITY_PROVEN ZR_OPTIMIZATION_FACTS_PROVEN
#define ZR_OPTIMIZATION_FACT_VALIDITY_UNKNOWN ZR_OPTIMIZATION_FACTS_UNKNOWN
#define ZR_OPTIMIZATION_FACT_VALIDITY_INVALID_LANGUAGE_USE \
    ZR_OPTIMIZATION_FACTS_INVALID_LANGUAGE_USE
#define ZR_OPTIMIZATION_VALIDITY_PROVEN ZR_OPTIMIZATION_FACTS_PROVEN
#define ZR_OPTIMIZATION_VALIDITY_UNKNOWN ZR_OPTIMIZATION_FACTS_UNKNOWN
#define ZR_OPTIMIZATION_VALIDITY_INVALID_LANGUAGE_USE \
    ZR_OPTIMIZATION_FACTS_INVALID_LANGUAGE_USE

typedef enum EZrOptimizationFactUnknownReason {
    ZR_OPTIMIZATION_UNKNOWN_NONE = 0,
    ZR_OPTIMIZATION_UNKNOWN_EXTERNAL_EFFECTS,
    ZR_OPTIMIZATION_UNKNOWN_ALIAS,
    ZR_OPTIMIZATION_UNKNOWN_ESCAPE,
    ZR_OPTIMIZATION_UNKNOWN_LAYOUT,
    ZR_OPTIMIZATION_UNKNOWN_OWNERSHIP,
    ZR_OPTIMIZATION_UNKNOWN_PROTOCOL,
    ZR_OPTIMIZATION_UNKNOWN_STALE_GENERATION,
    ZR_OPTIMIZATION_UNKNOWN_UNSUPPORTED,
    /* These distinctions are useful to remarks and are intentionally kept
     * separate from INVALID_LANGUAGE_USE: uncertainty is not a compile
     * error. */
    ZR_OPTIMIZATION_UNKNOWN_BORROW,
    ZR_OPTIMIZATION_UNKNOWN_RECEIVER,
    ZR_OPTIMIZATION_UNKNOWN_TASK_EFFECT,
    ZR_OPTIMIZATION_UNKNOWN_IMMUTABILITY,
    ZR_OPTIMIZATION_UNKNOWN_REASON_COUNT
} EZrOptimizationFactUnknownReason;
typedef EZrOptimizationFactUnknownReason EZrOptimizationUnknownReason;

/* Compatibility spellings used by producers and generated tests. */
#define ZR_OPTIMIZATION_UNKNOWN_EFFECTS ZR_OPTIMIZATION_UNKNOWN_EXTERNAL_EFFECTS
#define ZR_OPTIMIZATION_UNKNOWN_EFFECT ZR_OPTIMIZATION_UNKNOWN_EXTERNAL_EFFECTS
#define ZR_OPTIMIZATION_UNKNOWN_EXTERNAL ZR_OPTIMIZATION_UNKNOWN_EXTERNAL_EFFECTS
#define ZR_OPTIMIZATION_UNKNOWN_BORROW_SAFETY ZR_OPTIMIZATION_UNKNOWN_BORROW
#define ZR_OPTIMIZATION_UNKNOWN_RECEIVER_EFFECT ZR_OPTIMIZATION_UNKNOWN_RECEIVER
#define ZR_OPTIMIZATION_UNKNOWN_TASK_EFFECTS ZR_OPTIMIZATION_UNKNOWN_TASK_EFFECT
#define ZR_OPTIMIZATION_UNKNOWN_TASK ZR_OPTIMIZATION_UNKNOWN_TASK_EFFECT
#define ZR_OPTIMIZATION_UNKNOWN_IMMUTABLE ZR_OPTIMIZATION_UNKNOWN_IMMUTABILITY
#define ZR_OPTIMIZATION_UNKNOWN_PERSISTENT ZR_OPTIMIZATION_UNKNOWN_IMMUTABILITY
#define ZR_OPTIMIZATION_UNKNOWN_MODULE ZR_OPTIMIZATION_UNKNOWN_UNSUPPORTED

/* Fact bits are serialized capability bits.  Existing values are preserved;
 * the two appended bits carry proofs that cannot safely be inferred from a
 * generic effect mask alone.  SEND_SYNC is the type/ownership publication
 * proof; TASK_SEND_SYNC is the separate proof that a task boundary has no
 * published task effects. */
#define ZR_OPTIMIZATION_FACT_PURITY ((TZrUInt32)1u << 0u)
#define ZR_OPTIMIZATION_FACT_RECEIVER_READONLY ((TZrUInt32)1u << 1u)
#define ZR_OPTIMIZATION_FACT_NO_ALLOC ((TZrUInt32)1u << 2u)
#define ZR_OPTIMIZATION_FACT_NO_THROW ((TZrUInt32)1u << 3u)
#define ZR_OPTIMIZATION_FACT_NO_SUSPEND ((TZrUInt32)1u << 4u)
#define ZR_OPTIMIZATION_FACT_NO_ESCAPE ((TZrUInt32)1u << 5u)
#define ZR_OPTIMIZATION_FACT_SEND_SYNC ((TZrUInt32)1u << 6u)
#define ZR_OPTIMIZATION_FACT_CONTIGUOUS ((TZrUInt32)1u << 7u)
#define ZR_OPTIMIZATION_FACT_ALIGNED ((TZrUInt32)1u << 8u)
#define ZR_OPTIMIZATION_FACT_IMMUTABLE ((TZrUInt32)1u << 9u)
#define ZR_OPTIMIZATION_FACT_PERSISTENT ((TZrUInt32)1u << 10u)
#define ZR_OPTIMIZATION_FACT_BORROW_SAFE ((TZrUInt32)1u << 11u)
#define ZR_OPTIMIZATION_FACT_TASK_SEND_SYNC ((TZrUInt32)1u << 12u)
#define ZR_OPTIMIZATION_FACT_KNOWN_MASK ((TZrUInt32)0x1fffu)

#define ZR_OPTIMIZATION_FACT_BORROW_STABLE ZR_OPTIMIZATION_FACT_BORROW_SAFE
#define ZR_OPTIMIZATION_FACT_NO_BORROW_ESCAPE ZR_OPTIMIZATION_FACT_BORROW_SAFE
#define ZR_OPTIMIZATION_FACT_BORROW ZR_OPTIMIZATION_FACT_BORROW_SAFE
#define ZR_OPTIMIZATION_FACT_RECEIVER_READ_ONLY ZR_OPTIMIZATION_FACT_RECEIVER_READONLY
#define ZR_OPTIMIZATION_FACT_READONLY_RECEIVER ZR_OPTIMIZATION_FACT_RECEIVER_READONLY
#define ZR_OPTIMIZATION_FACT_READ_ONLY_RECEIVER \
    ZR_OPTIMIZATION_FACT_RECEIVER_READONLY
#define ZR_OPTIMIZATION_FACT_READONLY ZR_OPTIMIZATION_FACT_RECEIVER_READONLY
#define ZR_OPTIMIZATION_FACT_RECEIVER_CONST ZR_OPTIMIZATION_FACT_RECEIVER_READONLY
#define ZR_OPTIMIZATION_FACT_IMMUTABLE_CONTENT ZR_OPTIMIZATION_FACT_IMMUTABLE
#define ZR_OPTIMIZATION_FACT_PERSISTENT_UPDATE ZR_OPTIMIZATION_FACT_PERSISTENT
#define ZR_OPTIMIZATION_FACT_IMMUTABLE_CONTAINER ZR_OPTIMIZATION_FACT_IMMUTABLE
#define ZR_OPTIMIZATION_FACT_PERSISTENT_CONTAINER ZR_OPTIMIZATION_FACT_PERSISTENT
#define ZR_OPTIMIZATION_FACT_CONTIGUOUS_STORAGE ZR_OPTIMIZATION_FACT_CONTIGUOUS
#define ZR_OPTIMIZATION_FACT_TASK_SAFE ZR_OPTIMIZATION_FACT_TASK_SEND_SYNC
#define ZR_OPTIMIZATION_FACT_TASK_EFFECTS_SAFE ZR_OPTIMIZATION_FACT_TASK_SEND_SYNC
#define ZR_OPTIMIZATION_FACT_NO_ALLOCATION ZR_OPTIMIZATION_FACT_NO_ALLOC
#define ZR_OPTIMIZATION_FACT_NO_EXCEPTIONS ZR_OPTIMIZATION_FACT_NO_THROW
#define ZR_OPTIMIZATION_FACT_NO_AWAIT ZR_OPTIMIZATION_FACT_NO_SUSPEND

/* Canonical type definitions publish Send/Sync capability bits in the
 * ownership mask.  Keep the values local here so this summary header stays
 * free of AST/type-definition includes; the values intentionally mirror
 * ZR_CANONICAL_TYPE_CAPABILITY_SEND and ..._SYNC. */
#define ZR_OPTIMIZATION_OWNERSHIP_CAPABILITY_SEND ((TZrUInt32)1u << 10u)
#define ZR_OPTIMIZATION_OWNERSHIP_CAPABILITY_SYNC ((TZrUInt32)1u << 11u)
#define ZR_OPTIMIZATION_OWNERSHIP_CAPABILITY_SEND_SYNC \
    (ZR_OPTIMIZATION_OWNERSHIP_CAPABILITY_SEND | \
     ZR_OPTIMIZATION_OWNERSHIP_CAPABILITY_SYNC)
#define ZR_OPTIMIZATION_OWNERSHIP_CAPABILITY_KNOWN_MASK ((TZrUInt32)0xfffu)
#define ZR_OPTIMIZATION_OWNERSHIP_SEND ZR_OPTIMIZATION_OWNERSHIP_CAPABILITY_SEND
#define ZR_OPTIMIZATION_OWNERSHIP_SYNC ZR_OPTIMIZATION_OWNERSHIP_CAPABILITY_SYNC

/* Canonical receiver and borrow states are scalar summaries of the existing
 * parser receiver/ownership analyses.  ZERO means that the producer did not
 * prove a state; it is never treated as a positive capability. */
typedef enum EZrOptimizationReceiverEffect {
    ZR_OPTIMIZATION_RECEIVER_UNKNOWN = 0,
    ZR_OPTIMIZATION_RECEIVER_READONLY,
    ZR_OPTIMIZATION_RECEIVER_MUTABLE,
    ZR_OPTIMIZATION_RECEIVER_EFFECT_COUNT
} EZrOptimizationReceiverEffect;
typedef EZrOptimizationReceiverEffect EZrOptimizationReceiverState;

#define ZR_OPTIMIZATION_RECEIVER_NONE ZR_OPTIMIZATION_RECEIVER_UNKNOWN
#define ZR_OPTIMIZATION_RECEIVER_READ_ONLY ZR_OPTIMIZATION_RECEIVER_READONLY
#define ZR_OPTIMIZATION_RECEIVER_MUTATING ZR_OPTIMIZATION_RECEIVER_MUTABLE

typedef enum EZrOptimizationBorrowState {
    ZR_OPTIMIZATION_BORROW_UNKNOWN = 0,
    ZR_OPTIMIZATION_BORROW_STABLE,
    ZR_OPTIMIZATION_BORROW_ESCAPES,
    ZR_OPTIMIZATION_BORROW_ACROSS_SUSPEND,
    ZR_OPTIMIZATION_BORROW_STATE_COUNT
} EZrOptimizationBorrowState;
typedef EZrOptimizationBorrowState EZrOptimizationBorrowSafety;

#define ZR_OPTIMIZATION_BORROW_NONE ZR_OPTIMIZATION_BORROW_UNKNOWN
#define ZR_OPTIMIZATION_BORROW_SAFE ZR_OPTIMIZATION_BORROW_STABLE
#define ZR_OPTIMIZATION_BORROW_ESCAPING ZR_OPTIMIZATION_BORROW_ESCAPES
#define ZR_OPTIMIZATION_BORROW_ACROSS_AWAIT ZR_OPTIMIZATION_BORROW_ACROSS_SUSPEND

/* Task effects are intentionally separate from execution effects.  For
 * example, a synchronous read may have READ_MEMORY but no task effect; an
 * external/native task with no published contract is represented by UNKNOWN,
 * not by an empty mask. */
#define ZR_OPTIMIZATION_TASK_EFFECT_NONE ((TZrUInt32)0u)
#define ZR_OPTIMIZATION_TASK_EFFECT_SUSPEND ((TZrUInt32)1u << 0u)
#define ZR_OPTIMIZATION_TASK_EFFECT_SPAWN ((TZrUInt32)1u << 1u)
#define ZR_OPTIMIZATION_TASK_EFFECT_CANCEL ((TZrUInt32)1u << 2u)
#define ZR_OPTIMIZATION_TASK_EFFECT_DETACH ((TZrUInt32)1u << 3u)
#define ZR_OPTIMIZATION_TASK_EFFECT_EXTERNAL ((TZrUInt32)1u << 4u)
#define ZR_OPTIMIZATION_TASK_EFFECT_BORROW_ESCAPE ((TZrUInt32)1u << 5u)
#define ZR_OPTIMIZATION_TASK_EFFECT_UNKNOWN ((TZrUInt32)1u << 6u)
#define ZR_OPTIMIZATION_TASK_EFFECT_KNOWN_MASK ((TZrUInt32)0x7fu)

#define ZR_OPTIMIZATION_TASK_EFFECT_AWAIT ZR_OPTIMIZATION_TASK_EFFECT_SUSPEND
#define ZR_OPTIMIZATION_TASK_EFFECT_ASYNC ZR_OPTIMIZATION_TASK_EFFECT_SUSPEND
#define ZR_OPTIMIZATION_TASK_EFFECTS_NONE ZR_OPTIMIZATION_TASK_EFFECT_NONE
#define ZR_OPTIMIZATION_TASK_EFFECTS_KNOWN_MASK \
    ZR_OPTIMIZATION_TASK_EFFECT_KNOWN_MASK

/* Effect aliases keep the facts API readable while preserving the shared
 * execution-contract bit assignments. */
#define ZR_OPTIMIZATION_EFFECT_NONE ((TZrUInt32)0u)
#define ZR_OPTIMIZATION_EFFECT_READ_MEMORY ZR_EXECUTION_EFFECT_READ_MEMORY
#define ZR_OPTIMIZATION_EFFECT_WRITE_MEMORY ZR_EXECUTION_EFFECT_WRITE_MEMORY
#define ZR_OPTIMIZATION_EFFECT_ALLOCATE ZR_EXECUTION_EFFECT_ALLOCATE
#define ZR_OPTIMIZATION_EFFECT_THROW ZR_EXECUTION_EFFECT_THROW
#define ZR_OPTIMIZATION_EFFECT_SUSPEND ZR_EXECUTION_EFFECT_SUSPEND
#define ZR_OPTIMIZATION_EFFECT_KNOWN_MASK ZR_EXECUTION_EFFECT_KNOWN_MASK

/* A protocol's identity is token/signature/capability based; source type
 * names and descriptive element names are never consulted. */
typedef enum EZrOptimizationProtocolKind {
    ZR_OPTIMIZATION_PROTOCOL_NONE = 0,
    ZR_OPTIMIZATION_PROTOCOL_CONTIGUOUS_VIEW,
    ZR_OPTIMIZATION_PROTOCOL_TYPED_ARRAY,
    ZR_OPTIMIZATION_PROTOCOL_BATCH,
    ZR_OPTIMIZATION_PROTOCOL_ITERATION,
    ZR_OPTIMIZATION_PROTOCOL_IMMUTABLE_UPDATE,
    ZR_OPTIMIZATION_PROTOCOL_PERSISTENT_UPDATE,
    ZR_OPTIMIZATION_PROTOCOL_KIND_COUNT
} EZrOptimizationProtocolKind;

#define ZR_OPTIMIZATION_PROTOCOL_ITERATOR ZR_OPTIMIZATION_PROTOCOL_ITERATION
#define ZR_OPTIMIZATION_PROTOCOL_ITERABLE ZR_OPTIMIZATION_PROTOCOL_ITERATION
#define ZR_OPTIMIZATION_PROTOCOL_CONTIGUOUS ZR_OPTIMIZATION_PROTOCOL_CONTIGUOUS_VIEW
#define ZR_OPTIMIZATION_PROTOCOL_TYPED ZR_OPTIMIZATION_PROTOCOL_TYPED_ARRAY
#define ZR_OPTIMIZATION_PROTOCOL_BATCHING ZR_OPTIMIZATION_PROTOCOL_BATCH
#define ZR_OPTIMIZATION_PROTOCOL_IMMUTABLE ZR_OPTIMIZATION_PROTOCOL_IMMUTABLE_UPDATE
#define ZR_OPTIMIZATION_PROTOCOL_PERSISTENT ZR_OPTIMIZATION_PROTOCOL_PERSISTENT_UPDATE

#define ZR_OPTIMIZATION_PROTOCOL_OP_LOAD ((TZrUInt32)1u << 0u)
#define ZR_OPTIMIZATION_PROTOCOL_OP_STORE ((TZrUInt32)1u << 1u)
#define ZR_OPTIMIZATION_PROTOCOL_OP_SLICE ((TZrUInt32)1u << 2u)
#define ZR_OPTIMIZATION_PROTOCOL_OP_ITERATE ((TZrUInt32)1u << 3u)
#define ZR_OPTIMIZATION_PROTOCOL_OP_BATCH ((TZrUInt32)1u << 4u)
#define ZR_OPTIMIZATION_PROTOCOL_OP_UPDATE ((TZrUInt32)1u << 5u)
#define ZR_OPTIMIZATION_PROTOCOL_OP_NEXT ((TZrUInt32)1u << 6u)
#define ZR_OPTIMIZATION_PROTOCOL_OP_ITERATOR_NEXT ZR_OPTIMIZATION_PROTOCOL_OP_NEXT
#define ZR_OPTIMIZATION_PROTOCOL_OP_BORROW ((TZrUInt32)1u << 7u)
#define ZR_OPTIMIZATION_PROTOCOL_OP_READONLY ((TZrUInt32)1u << 8u)
#define ZR_OPTIMIZATION_PROTOCOL_OP_KNOWN_MASK ((TZrUInt32)0x1ffu)

typedef struct SZrOptimizationProtocol {
    TZrUInt32 schemaVersion;
    union {
        TZrMetadataToken protocolToken;
        TZrMetadataToken metadataToken;
    };
    TZrUInt32 kind;
    union {
        TZrUInt32 operationMask;
        TZrUInt32 operations;
    };
    TZrUInt32 requiredFacts;
    TZrUInt32 declaredEffects;
    TZrMetadataToken elementTypeToken;
    TZrUInt32 layoutId;
    TZrUInt64 signatureHash;
    TZrUInt64 layoutHash;
    TZrUInt64 moduleHash;
    TZrUInt64 generation;
} SZrOptimizationProtocol;

typedef struct SZrOptimizationFacts {
    TZrUInt32 schemaVersion;
    TZrUInt32 validity;
    TZrUInt32 unknownReason;
    TZrUInt32 factMask;
    TZrMetadataToken typeToken;
    TZrUInt32 layoutId;
    TZrUInt32 ownershipMask;
    TZrUInt32 declaredEffects;
    TZrUInt64 moduleHash;
    TZrUInt64 generation;
    /* proofHash is a witness over all fields except proofHash itself. */
    TZrUInt64 proofHash;
    TZrUInt64 protocolHash;
    union {
        TZrUInt32 taskEffects;
        TZrUInt32 taskEffectMask;
    };
    union {
        TZrUInt32 receiverEffect;
        TZrUInt32 receiver;
    };
    union {
        TZrUInt32 borrowState;
        TZrUInt32 borrowSafety;
    };
    TZrUInt32 protocolKind;
    union {
        TZrUInt32 protocolOperations;
        TZrUInt32 protocolOps;
    };
    /* Optional proof scope.  These ids identify the parser/IR region that
     * produced the summary; they are scalar witnesses, never AST pointers. */
    union {
        TZrMetadataToken proofFunctionToken;
        TZrUInt32 proofFunction;
        TZrUInt32 functionToken;
    };
    union {
        TZrUInt32 proofBlockId;
        TZrUInt32 proofBlock;
        TZrUInt32 blockId;
    };
    union {
        TZrUInt32 proofInstructionId;
        TZrUInt32 proofInstruction;
        TZrUInt32 instructionId;
    };
    union {
        TZrUInt32 proofSourceId;
        TZrUInt32 proofSource;
        TZrUInt32 sourceId;
    };
} SZrOptimizationFacts;

typedef struct SZrOptimizationFactQuery {
    TZrUInt32 schemaVersion;
    TZrUInt32 requiredFacts;
    TZrUInt32 forbiddenEffects;
    TZrMetadataToken expectedTypeToken;
    TZrUInt32 expectedLayoutId;
    TZrUInt64 expectedModuleHash;
    TZrUInt64 expectedGeneration;
    TZrUInt64 expectedProtocolHash;
    TZrUInt32 observedFactMask;
    TZrUInt32 observedEffects;
    TZrMetadataToken observedTypeToken;
    TZrUInt32 observedLayoutId;
    TZrUInt64 observedModuleHash;
    TZrUInt64 observedGeneration;
    TZrUInt64 observedProtocolHash;
    TZrUInt32 invalidLanguageUse;
    /* Optional scalar evidence.  Zero means unknown unless a corresponding
     * *_Known flag is present. */
    TZrUInt32 observedOwnershipMask;
    union {
        TZrUInt32 observedTaskEffects;
        TZrUInt32 observedTaskEffectMask;
    };
    union {
        TZrUInt32 observedTaskEffectsKnown;
        TZrUInt32 taskEffectsKnown;
    };
    union {
        TZrUInt32 observedTaskEffectsUnknown;
        TZrUInt32 taskEffectsUnknown;
    };
    union {
        TZrUInt32 observedExternalEffectsUnknown;
        TZrUInt32 observedEffectsUnknown;
        TZrUInt32 effectsUnknown;
    };
    union {
        TZrUInt32 observedExternalEffectsKnown;
        TZrUInt32 observedEffectsKnown;
        TZrUInt32 effectsKnown;
    };
    union {
        TZrUInt32 observedReceiverEffect;
        TZrUInt32 observedReceiver;
    };
    union {
        TZrUInt32 observedReceiverMayMutate;
        TZrUInt32 receiverMayMutate;
    };
    union {
        TZrUInt32 observedBorrowState;
        TZrUInt32 observedBorrowSafety;
    };
    union {
        TZrUInt32 observedBorrowAcrossSuspend;
        TZrUInt32 borrowAcrossSuspend;
    };
    TZrUInt32 observedProtocolKind;
    union {
        TZrUInt32 observedProtocolOperations;
        TZrUInt32 observedProtocolOps;
    };
    union {
        TZrUInt32 expectedTaskEffects;
        TZrUInt32 expectedTaskEffectMask;
    };
    union {
        TZrUInt32 forbiddenTaskEffects;
        TZrUInt32 forbiddenTaskEffectMask;
    };
    union {
        TZrUInt32 expectedReceiverEffect;
        TZrUInt32 expectedReceiver;
    };
    union {
        TZrUInt32 expectedBorrowState;
        TZrUInt32 expectedBorrowSafety;
    };
    TZrUInt32 expectedProtocolKind;
    union {
        TZrUInt32 expectedProtocolOperations;
        TZrUInt32 expectedProtocolOps;
    };
    /* Optional source/IR provenance copied into diagnostics. */
    TZrUInt32 functionToken;
    TZrUInt32 blockId;
    TZrUInt32 instructionId;
    TZrUInt32 sourceId;
} SZrOptimizationFactQuery;

ZR_PARSER_API void ZrParser_OptimizationFacts_Init(SZrOptimizationFacts *facts);
ZR_PARSER_API void ZrParser_OptimizationFactQuery_Init(SZrOptimizationFactQuery *query);
ZR_PARSER_API void ZrParser_OptimizationProtocol_Init(SZrOptimizationProtocol *protocol);
ZR_PARSER_API TZrUInt64 ZrParser_OptimizationFacts_Hash(const SZrOptimizationFacts *facts);
ZR_PARSER_API TZrUInt64 ZrParser_OptimizationProtocol_Hash(const SZrOptimizationProtocol *protocol);
ZR_PARSER_API TZrBool ZrParser_OptimizationFacts_Validate(
        const SZrOptimizationFacts *facts, SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_OptimizationProtocol_Validate(
        const SZrOptimizationProtocol *protocol, SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_Optimization_QueryFacts(
        const SZrOptimizationFactQuery *query,
        SZrOptimizationFacts *facts,
        SZrExecIrDiagnostic *diagnostic);
ZR_PARSER_API TZrBool ZrParser_Optimization_RecognizeProtocol(
        const SZrOptimizationProtocol *provided,
        const SZrOptimizationProtocol *required,
        SZrExecIrDiagnostic *diagnostic);

/* Noun-first aliases keep generated parser clients consistent with the
 * other parser-owned contracts without introducing duplicate entry points. */
#define ZrParser_Optimization_Facts_Init ZrParser_OptimizationFacts_Init
#define ZrParser_Optimization_FactQuery_Init ZrParser_OptimizationFactQuery_Init
#define ZrParser_Optimization_Protocol_Init ZrParser_OptimizationProtocol_Init
#define ZrParser_Optimization_Facts_Hash ZrParser_OptimizationFacts_Hash
#define ZrParser_Optimization_Protocol_Hash ZrParser_OptimizationProtocol_Hash
#define ZrParser_Optimization_Facts_Validate ZrParser_OptimizationFacts_Validate
#define ZrParser_Optimization_Protocol_Validate \
    ZrParser_OptimizationProtocol_Validate
#define ZrParser_Optimization_Facts_Query ZrParser_Optimization_QueryFacts
#define ZrParser_Optimization_Facts_RecognizeProtocol \
    ZrParser_Optimization_RecognizeProtocol

#endif /* ZR_VM_PARSER_OPTIMIZATION_FACTS_H */
