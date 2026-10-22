/* SPDX-License-Identifier: MIT */
#ifndef NATIVE_ADRENO_FAMILY_H
#define NATIVE_ADRENO_FAMILY_H
#include <IOKit/IOTypes.h>
#include <stddef.h>
#include <stdint.h>
#include "NativeAdrenoA650HwInit.h"
#include "NativeAdrenoA740HwInit.h"

/* Which Adreno the driver is talking to. The board reads the chip id out of its device tree node (qcom,chipid on the
 * kgsl node, the compatible on a mainline one) and selects once; every table that differs between the A740 (sheng,
 * a7xx) and the A650 (pdx203, a6xx gen 3) hangs off the selected info. Zero is the A740 so the existing aggregate
 * initialisers of the sheng board keep their meaning. Header only, so the objects that consult it (ring, irq, cp init,
 * the gmu objects) link as they did before the family existed. */
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
/* a740: the generated NativeAdrenoA740HwInit.h carries the 0x43050a01 catalog entry and the a740 family branches of
 * a6xx_gpu.c and a6xx_gmu.c. The 16 KiB dummy and 7 page debug objects are a6xx_gmu_init's for a7xx, the iommus (gpu 0
 * and 1, gmu 5) the sm8550 ones the sheng board routes. The kernel buffers sit at 1 MiB (GpuSqeIova), below the 16 MiB
 * GMEM window, as NativeAdrenoGpuContext has always placed them. */
inline const NativeAdrenoFamilyInfo NativeAdrenoFamilyInfoA740={
    NativeAdrenoFamilyA740,0x43050a01u,NATIVE_ADRENO_A740_GMU_CHIP_ID,NATIVE_ADRENO_A740_GMEM_BYTES,
    NATIVE_ADRENO_A740_GMEM_BASE,NATIVE_ADRENO_A740_UCHE_TRAP_BASE,0x100000ull,NATIVE_ADRENO_A740_INT_MASK,
    NATIVE_ADRENO_A740_CP_ERROR_MASK,NATIVE_ADRENO_A740_ZAP_PAS_ID,{0,1},5,0x4000u,0x7000u,
    NATIVE_ADRENO_A740_GMU_FENCE_RANGE,true,true,true,false};
/* a650: the generated NativeAdrenoA650HwInit.h carries the catalog and a6xx_gpu.c values. The GMEM window is the a6xx
 * default of SZ_1M (hw_init's gmem_range_min for every family but the a740, and what msm reports as MSM_PARAM_GMEM_BASE),
 * which the a650 family never reprograms, so the kernel buffers move up to SZ_16M, where adreno_iommu_create_vm starts
 * the GPU address space. a6xx_gmu_init gives the a650 family a 4 KiB dummy and no debug object. The iommus are the
 * kona device tree's (gfx3d_user 0 with mask 0x401, gmu_kernel 5), the same two streams and GMU stream as sheng. */
inline const NativeAdrenoFamilyInfo NativeAdrenoFamilyInfoA650={
    NativeAdrenoFamilyA650,NATIVE_ADRENO_A650_CATALOG_CHIP_ID,0,NATIVE_ADRENO_A650_GMEM_BYTES,0x100000ull,
    NATIVE_ADRENO_A650_UCHE_TRAP_BASE,0x1000000ull,NATIVE_ADRENO_A650_INT_MASK,NATIVE_ADRENO_A650_CP_ERROR_MASK,
    NATIVE_ADRENO_A650_ZAP_PAS_ID,{0,1},5,0x1000u,0,NATIVE_ADRENO_A650_GMU_FENCE_RANGE,false,false,false,false};
inline const NativeAdrenoFamilyInfo *NativeAdrenoFamilyGet(NativeAdrenoFamily family)
{
    switch(family) {
    case NativeAdrenoFamilyA740:return &NativeAdrenoFamilyInfoA740;
    case NativeAdrenoFamilyA650:return &NativeAdrenoFamilyInfoA650;
    }
    return nullptr;
}
/* The info for a chip id, or null for one the driver has no tables for. The A740 is matched exactly (0x43050a01); the
 * A650 by core and major (0x0605xxxx), since the kona device tree says 0x06050000 and the catalog 0x06050002 for the
 * same silicon. */
inline const NativeAdrenoFamilyInfo *NativeAdrenoFamilySelect(uint32_t chip_id)
{
    if(chip_id==NativeAdrenoFamilyInfoA740.chip_id)return &NativeAdrenoFamilyInfoA740;
    if((chip_id&0xffff0000u)==(NativeAdrenoFamilyInfoA650.chip_id&0xffff0000u))return &NativeAdrenoFamilyInfoA650;
    return nullptr;
}
/* What a6xx_gmu_fw_start hands the GMU firmware for this chip: the catalog gmu_chipid when the entry has one (a740),
 * else the chip id with minor and patch packed into four bits each. */
inline uint32_t NativeAdrenoGmuChipId(const NativeAdrenoFamilyInfo *info,uint32_t chip_id)
{
    if(!info)return 0;
    if(info->gmu_chip_id)return info->gmu_chip_id;
    return (chip_id&0xffff0000u)|((chip_id<<4)&0xf000u)|((chip_id<<8)&0x0f00u);
}

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
inline IOReturn NativeAdrenoUbwcWordsFor(const NativeAdrenoFamilyInfo *info,const NativeAdrenoUbwcInputs *in,
    NativeAdrenoUbwcWords *out)
{
    if(!out)return kIOReturnBadArgument;
    *out={};
    if(!info || !in || in->highest_bank_bit<13 || in->highest_bank_bit>16 ||
        (in->min_access_length!=32 && in->min_access_length!=64) || in->ubwc_version<1 || in->ubwc_version>4 ||
        in->level2_swizzle>1 || in->macrotile_mode>1)return kIOReturnBadArgument;
    /* a6xx_set_ubwc_config: hbb is the bank bit above 13, split as hbb_lo (two bits) and hbb_hi; the predicator and amsbc
     * follow the encoder version (rgb565 from 4.0, amsbc from 3.0), ubwc_mode is 1 only for UBWC 1.0 */
    const uint32_t hbb=in->highest_bank_bit-13,hbb_hi=hbb>>2,hbb_lo=hbb&3;
    const uint32_t rgb565=in->ubwc_version>=4,amsbc=in->ubwc_version>=3,mode=in->ubwc_version==1;
    const uint32_t l2dis=in->level2_swizzle?0:1,mal64=in->min_access_length==64;
    const uint32_t uav=info->family==NativeAdrenoFamilyA650?NATIVE_ADRENO_A650_UAVFLAGPRD_INV:NATIVE_ADRENO_A740_UAVFLAGPRD_INV;
    out->rb_nc_mode_cntl=l2dis<<12|rgb565<<11|hbb_hi<<10|amsbc<<4|mal64<<3|hbb_lo<<1|mode;
    out->tpl1_nc_mode_cntl=l2dis<<6|hbb_hi<<4|mal64<<3|hbb_lo<<1|mode;
    out->sp_nc_mode_cntl=l2dis<<12|hbb_hi<<10|uav<<4|mal64<<3|hbb_lo<<1|mode;
    out->uche_mode_cntl=mal64<<23|hbb_lo<<21;
    out->rbbm_nc_mode_cntl=in->macrotile_mode;
    out->gras_nc_mode_cntl=info->concurrent_binning?hbb_lo<<5:0;
    return kIOReturnSuccess;
}
#endif
