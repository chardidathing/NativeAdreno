/* SPDX-License-Identifier: MIT */
#ifndef NATIVE_ADRENO_FAMILY_H
#define NATIVE_ADRENO_FAMILY_H
#include <IOKit/IOTypes.h>
#include <stddef.h>
#include <stdint.h>

/* Which Adreno the driver is talking to. The board reads the chip id out of its device tree node (qcom,chipid on the
 * kgsl node, the compatible on a mainline one) and selects once; every table that differs between the A740 (sheng,
 * a7xx) and the A650 (pdx203, a6xx gen 3) hangs off the selected info. Zero is the A740 so the existing aggregate
 * initialisers of the sheng board keep their meaning. */
enum NativeAdrenoFamily : uint32_t {NativeAdrenoFamilyA740=0,NativeAdrenoFamilyA650=1};
struct NativeAdrenoFamilyInfo {
    NativeAdrenoFamily family;
    uint32_t chip_id;              /* the pinned a6xx_catalog.c chip id of the entry the tables come from */
    uint32_t gmu_chip_id;          /* what a6xx_gmu_fw_start writes for the firmware (GMU_GENERAL_10 or GMU_HFI_SFR_ADDR) */
    uint32_t gmem_bytes;           /* catalog .gmem */
    uint64_t gmem_base;            /* UCHE GMEM window start as the GPU sees it */
    uint64_t uche_trap_base;       /* a6xx_gpu_init */
    uint64_t kernel_iova_base;     /* first GPU address of the kernel buffers (sqe, ring, memptrs, pwrup, scratch) */
    uint32_t irq_mask;             /* RBBM_INT_0_MASK enable set of hw_init */
    uint32_t cp_error_mask;        /* CP_INTERRUPT_STATUS bits that are faults */
    uint32_t zap_pas_id;           /* GPU_PAS_ID of a6xx_gpu.c, the TrustZone peripheral the zap shader loads into */
    uint16_t gpu_sids[2];          /* the GPU node's iommus at the pinned DT, routed to one context bank */
    uint16_t gmu_sid;              /* the GMU node's iommus */
    uint32_t gmu_dummy_bytes;      /* a6xx_gmu_init: the dummy page at 0x60000000 */
    uint32_t gmu_debug_bytes;      /* a6xx_gmu_init: the debug object at 0x60400000, zero when the family has none */
    uint32_t gmu_fence_range;      /* GMU_AHB_FENCE_RANGE_0 of a6xx_gmu_fw_start */
    bool concurrent_binning;       /* BV thread: CP_THREAD_CONTROL, CP_EVENT_WRITE7 framing, the a7xx context entry */
    bool uses_pwrup_list;          /* CP_ME_INIT carries the power-up register list */
    bool gmem_range_programmed;    /* hw_init writes UCHE_GMEM_RANGE_MIN/MAX (not for the a650 family) */
    bool legacy_hfi;               /* a6xx_gmu.c gmu->legacy (HFI v1), false for both families here */
};
/* The info for a chip id, or null for one the driver has no tables for. The A740 is matched exactly (0x43050a01); the
 * A650 by core and major (0x0605xxxx), since the kona device tree says 0x06050000 and the catalog 0x06050002 for the
 * same silicon. */
const NativeAdrenoFamilyInfo *NativeAdrenoFamilySelect(uint32_t chip_id);
const NativeAdrenoFamilyInfo *NativeAdrenoFamilyGet(NativeAdrenoFamily family);
/* What a6xx_gmu_fw_start hands the GMU firmware for this chip: the catalog gmu_chipid when the entry has one (a740),
 * else the chip id with minor and patch packed into four bits each. */
uint32_t NativeAdrenoGmuChipId(const NativeAdrenoFamilyInfo *,uint32_t chip_id);

/* The UBWC configuration a6xx_set_ubwc_config derives from the SoC's qcom_ubwc_cfg_data: the board supplies what its
 * device tree says (kgsl: qcom,highest-bank-bit, qcom,min-access-length, qcom,ubwc-mode) and the words come out exactly
 * as hw_init writes them. */
struct NativeAdrenoUbwcInputs {
    uint32_t highest_bank_bit;     /* 13..16 */
    uint32_t min_access_length;    /* 32 or 64 bytes */
    uint32_t ubwc_version;         /* encoder version, 2, 3 or 4 */
    uint32_t level2_swizzle;       /* 1 when UBWC_SWIZZLE_ENABLE_LVL2 is set */
    uint32_t macrotile_mode;       /* 0 four channel, 1 eight channel */
};
struct NativeAdrenoUbwcWords {
    uint32_t rb_nc_mode_cntl,tpl1_nc_mode_cntl,sp_nc_mode_cntl,uche_mode_cntl,rbbm_nc_mode_cntl;
    uint32_t gras_nc_mode_cntl;    /* a7xx only, per pipe through CP_APERTURE_CNTL_HOST */
};
IOReturn NativeAdrenoUbwcWordsFor(const NativeAdrenoFamilyInfo *,const NativeAdrenoUbwcInputs *,NativeAdrenoUbwcWords *);
#endif
