/* SPDX-License-Identifier: MIT */
#include "NativeAdrenoFamily.h"
#include "NativeAdrenoA650HwInit.h"
#include "NativeAdrenoA740HwInit.h"

/* a740: the generated NativeAdrenoA740HwInit.h carries the 0x43050a01 catalog entry and the a740 family branches of
 * a6xx_gpu.c and a6xx_gmu.c. The 16 KiB dummy and 7 page debug objects are a6xx_gmu_init's for a7xx, the iommus (gpu 0
 * and 1, gmu 5) the sm8550 ones the sheng board routes. The kernel buffers sit at 1 MiB (GpuSqeIova), below the 16 MiB
 * GMEM window, as NativeAdrenoGpuContext has always placed them. */
static const NativeAdrenoFamilyInfo kA740={
    NativeAdrenoFamilyA740,0x43050a01u,NATIVE_ADRENO_A740_GMU_CHIP_ID,NATIVE_ADRENO_A740_GMEM_BYTES,
    NATIVE_ADRENO_A740_GMEM_BASE,NATIVE_ADRENO_A740_UCHE_TRAP_BASE,0x100000ull,NATIVE_ADRENO_A740_INT_MASK,
    NATIVE_ADRENO_A740_CP_ERROR_MASK,NATIVE_ADRENO_A740_ZAP_PAS_ID,{0,1},5,0x4000u,0x7000u,
    NATIVE_ADRENO_A740_GMU_FENCE_RANGE,true,true,true,false};
/* a650: the generated NativeAdrenoA650HwInit.h carries the catalog and a6xx_gpu.c values. The GMEM window is the a6xx
 * default of SZ_1M (hw_init's gmem_range_min for every family but the a740, and what msm reports as MSM_PARAM_GMEM_BASE),
 * which the a650 family never reprograms, so the kernel buffers move up to SZ_16M, where adreno_iommu_create_vm starts
 * the GPU address space. a6xx_gmu_init gives the a650 family a 4 KiB dummy and no debug object. The iommus are the
 * kona device tree's (gfx3d_user 0 with mask 0x401, gmu_kernel 5), the same two streams and GMU stream as sheng. */
static const NativeAdrenoFamilyInfo kA650={
    NativeAdrenoFamilyA650,NATIVE_ADRENO_A650_CATALOG_CHIP_ID,0,NATIVE_ADRENO_A650_GMEM_BYTES,0x100000ull,
    NATIVE_ADRENO_A650_UCHE_TRAP_BASE,0x1000000ull,NATIVE_ADRENO_A650_INT_MASK,NATIVE_ADRENO_A650_CP_ERROR_MASK,
    NATIVE_ADRENO_A650_ZAP_PAS_ID,{0,1},5,0x1000u,0,NATIVE_ADRENO_A650_GMU_FENCE_RANGE,false,false,false,false};

const NativeAdrenoFamilyInfo *NativeAdrenoFamilyGet(NativeAdrenoFamily family)
{
    switch(family) {
    case NativeAdrenoFamilyA740:return &kA740;
    case NativeAdrenoFamilyA650:return &kA650;
    }
    return nullptr;
}
const NativeAdrenoFamilyInfo *NativeAdrenoFamilySelect(uint32_t chip_id)
{
    if(chip_id==kA740.chip_id)return &kA740;
    /* core 6 major 5: the kona dts says 0x06050000 for the chip the catalog lists as 0x06050002 */
    if((chip_id&0xffff0000u)==(kA650.chip_id&0xffff0000u))return &kA650;
    return nullptr;
}
uint32_t NativeAdrenoGmuChipId(const NativeAdrenoFamilyInfo *info,uint32_t chip_id)
{
    if(!info)return 0;
    if(info->gmu_chip_id)return info->gmu_chip_id;
    /* a6xx_gmu_fw_start without a catalog gmu_chipid: upper 16 bits kept, minor and patch packed into four bits each */
    return (chip_id&0xffff0000u)|((chip_id<<4)&0xf000u)|((chip_id<<8)&0x0f00u);
}
IOReturn NativeAdrenoUbwcWordsFor(const NativeAdrenoFamilyInfo *info,const NativeAdrenoUbwcInputs *in,
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
