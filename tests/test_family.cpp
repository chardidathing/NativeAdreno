/* SPDX-License-Identifier: MIT */
/* Family selection and the per-family facts against the generated headers and the a740 code paths. No hardware. */
#include "XnuMemoryFixture.h"
#include "NativeAdrenoFamily.h"
#include "NativeAdrenoA650HwInit.h"
#include "NativeAdrenoA740HwInit.h"
#include <cstdio>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
int main()
{
    /* the sheng chip id selects the a740, the kona dts and catalog ids both select the a650, anything else nothing */
    const NativeAdrenoFamilyInfo *a740=NativeAdrenoFamilySelect(0x43050a01u);
    const NativeAdrenoFamilyInfo *a650=NativeAdrenoFamilySelect(0x06050000u);
    CHECK(a740 && a740->family==NativeAdrenoFamilyA740 && a740==NativeAdrenoFamilyGet(NativeAdrenoFamilyA740));
    CHECK(a650 && a650->family==NativeAdrenoFamilyA650 && a650==NativeAdrenoFamilyGet(NativeAdrenoFamilyA650));
    CHECK(NativeAdrenoFamilySelect(0x06050002u)==a650 && NativeAdrenoFamilySelect(0x06050001u)==a650);
    CHECK(!NativeAdrenoFamilySelect(0) && !NativeAdrenoFamilySelect(0x06030500u) && !NativeAdrenoFamilySelect(0x43050a00u));
    CHECK(!NativeAdrenoFamilyGet(NativeAdrenoFamily(2)));
    /* the a740 facts are the ones the sheng board and NativeAdrenoIrq carry */
    CHECK(a740->chip_id==0x43050a01u && a740->gmu_chip_id==0x07020100u && NativeAdrenoGmuChipId(a740,0x43050a01u)==0x07020100u);
    CHECK(a740->gmem_bytes==(3u<<20) && a740->gmem_base==0x1000000ull && a740->uche_trap_base==0x1fffffffff000ull);
    CHECK(a740->kernel_iova_base==0x100000ull && a740->irq_mask==0x33d283c2u && a740->cp_error_mask==0x3fff7u);
    CHECK(a740->zap_pas_id==13 && a740->gpu_sids[0]==0 && a740->gpu_sids[1]==1 && a740->gmu_sid==5);
    CHECK(a740->gmu_dummy_bytes==0x4000 && a740->gmu_debug_bytes==0x7000 && a740->gmu_fence_range==0x80c808a0u);
    CHECK(a740->concurrent_binning && a740->uses_pwrup_list && a740->gmem_range_programmed && !a740->legacy_hfi);
    /* the a650 facts are the generated header's, the GMU chip id the a6xx_gmu_fw_start packing of the board's id */
    CHECK(a650->chip_id==0x06050002u && a650->gmu_chip_id==0);
    CHECK(NativeAdrenoGmuChipId(a650,0x06050000u)==0x06050000u && NativeAdrenoGmuChipId(a650,0x06050002u)==0x06050200u);
    CHECK(NativeAdrenoGmuChipId(a650,0x06050113u)==0x06051300u);
    CHECK(a650->gmem_bytes==0x120000u && a650->gmem_bytes==NATIVE_ADRENO_A650_GMEM_BYTES && a650->gmem_base==0x100000ull);
    CHECK(a650->uche_trap_base==0x1fffffffff000ull && a650->kernel_iova_base==0x1000000ull);
    CHECK(a650->irq_mask==0x03d0e242u && a650->cp_error_mask==0xf7u && a650->zap_pas_id==13);
    CHECK(a650->gpu_sids[0]==0 && a650->gpu_sids[1]==1 && a650->gmu_sid==5);
    CHECK(a650->gmu_dummy_bytes==0x1000 && a650->gmu_debug_bytes==0 && a650->gmu_fence_range==0x802800a0u);
    CHECK(!a650->concurrent_binning && !a650->uses_pwrup_list && !a650->gmem_range_programmed && !a650->legacy_hfi);
    CHECK(!NativeAdrenoGmuChipId(nullptr,1));
    /* UBWC words: the sheng board's sm8550 values from the same inputs the kona dts gives the a650 (hbb 16, 32 byte
     * minimum access, UBWC 4.0, level 2 swizzle, eight channel macrotile) */
    const NativeAdrenoUbwcInputs in{16,32,4,1,1};
    NativeAdrenoUbwcWords w{};
    CHECK(NativeAdrenoUbwcWordsFor(a740,&in,&w)==kIOReturnSuccess);
    CHECK(w.rb_nc_mode_cntl==((1u<<11)|(1u<<4)|(3u<<1)) && w.tpl1_nc_mode_cntl==(3u<<1) && w.sp_nc_mode_cntl==((2u<<4)|(3u<<1)));
    CHECK(w.uche_mode_cntl==(3u<<21) && w.rbbm_nc_mode_cntl==1 && w.gras_nc_mode_cntl==(3u<<5));
    CHECK(NativeAdrenoUbwcWordsFor(a650,&in,&w)==kIOReturnSuccess);
    CHECK(w.rb_nc_mode_cntl==0x816u && w.tpl1_nc_mode_cntl==6u && w.sp_nc_mode_cntl==0x26u && w.uche_mode_cntl==(3u<<21));
    CHECK(w.rbbm_nc_mode_cntl==1 && w.gras_nc_mode_cntl==0);
    /* the LPDDR4X variant kgsl selects for ddr type 7 (hbb 15) and a 64 byte minimum access length */
    const NativeAdrenoUbwcInputs lp4{15,64,4,1,1};
    CHECK(NativeAdrenoUbwcWordsFor(a650,&lp4,&w)==kIOReturnSuccess);
    CHECK(w.rb_nc_mode_cntl==((1u<<11)|(1u<<4)|(1u<<3)|(2u<<1)) && w.uche_mode_cntl==((1u<<23)|(2u<<21)));
    const NativeAdrenoUbwcInputs bad[]={{12,32,4,1,1},{17,32,4,1,1},{16,48,4,1,1},{16,32,0,1,1},{16,32,5,1,1},{16,32,4,2,1},{16,32,4,1,2}};
    for(const NativeAdrenoUbwcInputs &b:bad) {
        w.rb_nc_mode_cntl=0xffffffffu;
        CHECK(NativeAdrenoUbwcWordsFor(a650,&b,&w)==kIOReturnBadArgument && !w.rb_nc_mode_cntl);
    }
    CHECK(NativeAdrenoUbwcWordsFor(nullptr,&in,&w)==kIOReturnBadArgument && NativeAdrenoUbwcWordsFor(a650,nullptr,&w)==kIOReturnBadArgument);
    CHECK(NativeAdrenoUbwcWordsFor(a650,&in,nullptr)==kIOReturnBadArgument);
    /* the generated a740 scalars agree with what the sheng board types */
    CHECK(NATIVE_ADRENO_A740_INT_MASK==0x33d283c2u && NATIVE_ADRENO_A740_GBIF_CLIENT_QOS==0x2120212u);
    CHECK(NATIVE_ADRENO_A740_UCHE_GBIF_GX_CONFIG==0x10240e0u && NATIVE_ADRENO_A740_UCHE_CMDQ_CONFIG==0x6690eu);
    CHECK(NATIVE_ADRENO_A740_HANG_INT_CNTL==((1u<<30)|0xcfffffu) && NATIVE_ADRENO_A740_BR_APRIV_MASK==0x6fu && NATIVE_ADRENO_A740_APRIV_MASK==0x0fu);
    CHECK(NATIVE_ADRENO_A740_CP_PROTECT_CNTL==((1u<<0)|(1u<<1)|(1u<<3)) && NATIVE_ADRENO_A740_RB_CMP_DBG_ECO_BITS==(1u<<11));
    std::printf("PASS: 2 families selected by chip id, 3 refused ids, the a740 facts of the sheng board, the a650 facts of the generated header, 3 UBWC profiles and 9 UBWC refusals\n");
}
