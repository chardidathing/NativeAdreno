/* SPDX-License-Identifier: MIT */
#ifndef NATIVE_ADRENO_HWINIT_H
#define NATIVE_ADRENO_HWINIT_H
#include "NativeAdrenoFamily.h"

/* The register program of a6xx_gpu.c hw_init between the GMU's OOB GPU_SET and the ring: GBIF halt release, SECVID,
 * clock gating, QoS, UCHE, CP thresholds, UBWC, hang detection, CP protection, APRIV, the interrupt mask and the SQE
 * base. Nothing before it (power, GMU, SMMU) and nothing after it (ring, SQE start, CP init) is done here; those stay
 * with the board and the ring objects. Register indices are dword offsets as the pinned XML numbers them, the GMU ones
 * relative to the GPU base as a6xx_gmu.xml lists them; the board's adapter owns the windows and their deltas. The A740
 * program is the sheng board's sequence (hw_init's a740 branches as it runs them on hardware), the A650 program follows
 * hw_init's a650 family branches in Linux order. A failed access stops the program at that write and reports where. */
struct NativeAdrenoHwInitIo {
    void *cookie;
    int (*gpu_write)(void *,uint32_t reg,uint32_t value);
    int (*gpu_read)(void *,uint32_t reg,uint32_t *value);
    int (*gmu_write)(void *,uint32_t reg,uint32_t value);
    int (*gmu_read)(void *,uint32_t reg,uint32_t *value);
};
struct NativeAdrenoHwInitInputs {
    uint64_t sqe_iova;                 /* CP_SQE_INSTR_BASE, page aligned, nonzero */
    NativeAdrenoUbwcWords ubwc;        /* NativeAdrenoUbwcWordsFor */
};
struct NativeAdrenoHwInitResult {
    uint32_t writes,reads;
    uint32_t step;                     /* NativeAdrenoHwInitStep of the last access */
    uint32_t hwcg_programmed;          /* 0 when RBBM_CLOCK_CNTL already read as on */
    uint32_t clock_cntl_before;
};
enum NativeAdrenoHwInitStep : uint32_t {
    NativeAdrenoHwInitGbif=1,NativeAdrenoHwInitSecvid,NativeAdrenoHwInitAddressMode,NativeAdrenoHwInitHwcg,
    NativeAdrenoHwInitQos,NativeAdrenoHwInitUche,NativeAdrenoHwInitCp,NativeAdrenoHwInitUbwc,
    NativeAdrenoHwInitFaults,NativeAdrenoHwInitCounters,NativeAdrenoHwInitProtect,NativeAdrenoHwInitApriv,
    NativeAdrenoHwInitInterrupts,NativeAdrenoHwInitSqe,NativeAdrenoHwInitDone
};
IOReturn NativeAdrenoHwInit(const NativeAdrenoFamilyInfo *,const NativeAdrenoHwInitIo *,
    const NativeAdrenoHwInitInputs *,NativeAdrenoHwInitResult *);
#endif
