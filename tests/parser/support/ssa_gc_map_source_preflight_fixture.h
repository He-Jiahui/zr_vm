#ifndef ZR_TEST_SSA_GC_MAP_SOURCE_PREFLIGHT_FIXTURE_H
#define ZR_TEST_SSA_GC_MAP_SOURCE_PREFLIGHT_FIXTURE_H

/* Metadata-only tests. Every mutation is restored before assertion or owner Free. */
enum ESpEntrance { SP_FRAME, SP_SELECTED, SP_FINALIZED, SP_CANDIDATE };
enum ESpShape {
    SP_ABSENT, SP_EMPTY, SP_MULTI, SP_REAL2_2, SP_REAL1_2,
    SP_NONNULL0_0, SP_NULL1_1, SP_NULL0_1, SP_REAL2_1
};
enum ESpFamily { SP_PF, SP_FM };
typedef struct SSpSummary {
    unsigned attempts, live, errors, faultMeasured, faultCompleted;
} SSpSummary;
void Sp_SetFamily(unsigned family);
void Sp_SetUp(void);
void Sp_TearDown(void);
void Sp_Matrix(unsigned entrance, unsigned shape, unsigned writable);
void Sp_Profile(unsigned entrance, unsigned shape, unsigned writable);
void Sp_PublicMalformed(unsigned entrance, unsigned writable);
void Sp_Priority(unsigned which, unsigned writable);
void Sp_Healthy(void);
void Sp_Grow(unsigned writable);
SSpSummary Sp_Summary(void);
#endif
