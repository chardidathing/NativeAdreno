/* SPDX-License-Identifier: MIT */
#include "NativeAdrenoHwInit.h"
#include "NativeAdrenoA650HwInit.h"
#include "NativeAdrenoA740HwInit.h"
#include <IOKit/IOLib.h>

namespace {
/* Counts every access and stops the program at the first refused one. */
struct Program {
    const NativeAdrenoHwInitIo *io;
    NativeAdrenoHwInitResult *r;
    bool failed=false;
    void step(NativeAdrenoHwInitStep s) {r->step=s;}
    void gpu(uint32_t reg,uint32_t value)
    {
        if(failed)return;
        ++r->writes;
        if(io->gpu_write(io->cookie,reg,value))failed=true;
    }
    void gpu64(uint32_t reg,uint64_t value) {gpu(reg,uint32_t(value));gpu(reg+1,uint32_t(value>>32));}
    bool gpuRead(uint32_t reg,uint32_t *value)
    {
        if(failed)return false;
        ++r->reads;
        if(io->gpu_read(io->cookie,reg,value)) {failed=true;return false;}
        return true;
    }
    void gmu(uint32_t reg,uint32_t value)
    {
        if(failed)return;
        ++r->writes;
        if(io->gmu_write(io->cookie,reg,value))failed=true;
    }
    bool gmuRead(uint32_t reg,uint32_t *value)
    {
        if(failed)return false;
        ++r->reads;
        if(io->gmu_read(io->cookie,reg,value)) {failed=true;return false;}
        return true;
    }
    void gmuRmw(uint32_t reg,uint32_t clear,uint32_t set)
    {
        uint32_t value=0;
        if(gmuRead(reg,&value))gmu(reg,(value&~clear)|set);
    }
};
/* The sheng board's a740 sequence, write for write. */
void programA740(Program &p,const NativeAdrenoHwInitInputs *in)
{
    p.step(NativeAdrenoHwInitGbif);
    uint32_t value=0;
    p.gpu(A740_GBIF_HALT,0);p.gpuRead(A740_GBIF_HALT,&value);
    p.gpu(A740_RBBM_GBIF_HALT,0);p.gpuRead(A740_RBBM_GBIF_HALT,&value);
    p.step(NativeAdrenoHwInitSecvid);
    p.gpu(A740_RBBM_SECVID_TSB_CNTL,0);
    p.gpu64(A740_RBBM_SECVID_TSB_TRUSTED_BASE,0);
    p.gpu(A740_RBBM_SECVID_TSB_TRUSTED_SIZE,0);
    /* a7xx has no ADDR_MODE_CNTL writes */
    p.step(NativeAdrenoHwInitHwcg);
    p.gmu(A740_GPU_GMU_AO_GMU_CGC_MODE_CNTL,NATIVE_ADRENO_A740_GMU_CGC_MODE);
    p.gmu(A740_GPU_GMU_AO_GMU_CGC_DELAY_CNTL,NATIVE_ADRENO_A740_CGC_DELAY);
    p.gmu(A740_GPU_GMU_AO_GMU_CGC_HYST_CNTL,NATIVE_ADRENO_A740_CGC_HYST);
    if(p.gpuRead(A740_RBBM_CLOCK_CNTL,&value)) {
        p.r->clock_cntl_before=value;
        if(value!=NATIVE_ADRENO_A740_CLOCK_CNTL_ON) {
            p.r->hwcg_programmed=1;
            for(unsigned i=0;i<NATIVE_ADRENO_A740_HWCG_COUNT;++i)p.gpu(NativeAdrenoA740Hwcg[i][0],NativeAdrenoA740Hwcg[i][1]);
            p.gpu(A740_RBBM_CLOCK_CNTL,NATIVE_ADRENO_A740_CLOCK_CNTL_ON);
        }
    }
    p.step(NativeAdrenoHwInitQos);
    p.gpu(A740_RBBM_GBIF_CLIENT_QOS_CNTL,NATIVE_ADRENO_A740_GBIF_CLIENT_QOS);
    p.gpu(A740_UCHE_GBIF_GX_CONFIG,NATIVE_ADRENO_A740_UCHE_GBIF_GX_CONFIG);
    p.gpu(A740_RBBM_PERFCTR_GPU_BUSY_MASKED,0xffffffffu);
    p.step(NativeAdrenoHwInitUche);
    p.gpu64(A740_UCHE_TRAP_BASE,NATIVE_ADRENO_A740_UCHE_TRAP_BASE);
    p.gpu64(A740_UCHE_WRITE_THRU_BASE,NATIVE_ADRENO_A740_UCHE_TRAP_BASE);
    p.gpu64(A740_UCHE_GMEM_RANGE_MIN,NATIVE_ADRENO_A740_GMEM_BASE);
    p.gpu64(A740_UCHE_GMEM_RANGE_MAX,NATIVE_ADRENO_A740_GMEM_BASE+NATIVE_ADRENO_A740_GMEM_BYTES-1);
    p.gpu(A740_UCHE_CACHE_WAYS,NATIVE_ADRENO_A740_UCHE_CACHE_WAYS);
    p.step(NativeAdrenoHwInitCp);
    p.gpu(A740_CP_AHB_CNTL,NATIVE_ADRENO_A740_CP_AHB_CNTL);
    p.gpu(A740_RBBM_PERFCTR_CNTL,NATIVE_ADRENO_A740_RBBM_PERFCTR_CNTL);
    p.step(NativeAdrenoHwInitFaults);
    p.gpu(A740_RBBM_INTERFACE_HANG_INT_CNTL,NATIVE_ADRENO_A740_HANG_INT_CNTL);
    p.gpu(A740_UCHE_CLIENT_PF,NATIVE_ADRENO_A740_UCHE_CLIENT_PF);
    p.step(NativeAdrenoHwInitProtect);
    p.gpu(A740_CP_PROTECT_CNTL,NATIVE_ADRENO_A740_CP_PROTECT_CNTL);
    for(unsigned i=0;i<NATIVE_ADRENO_A740_PROTECT_COUNT;++i)
        p.gpu(A740_CP_PROTECT+NativeAdrenoA740Protect[i][0],NativeAdrenoA740Protect[i][1]);
    p.gpu(A740_UCHE_CMDQ_CONFIG,NATIVE_ADRENO_A740_UCHE_CMDQ_CONFIG);
    p.step(NativeAdrenoHwInitApriv);
    p.gpu(A740_CP_APRIV_CNTL,NATIVE_ADRENO_A740_BR_APRIV_MASK);
    p.gpu(A740_CP_BV_APRIV_CNTL,NATIVE_ADRENO_A740_APRIV_MASK);
    p.gpu(A740_CP_LPAC_APRIV_CNTL,NATIVE_ADRENO_A740_APRIV_MASK);
    if(p.gpuRead(A740_RB_CMP_DBG_ECO_CNTL,&value))p.gpu(A740_RB_CMP_DBG_ECO_CNTL,value|NATIVE_ADRENO_A740_RB_CMP_DBG_ECO_BITS);
    p.step(NativeAdrenoHwInitUbwc);
    p.gpu(A740_RB_NC_MODE_CNTL,in->ubwc.rb_nc_mode_cntl);
    p.gpu(A740_TPL1_NC_MODE_CNTL,in->ubwc.tpl1_nc_mode_cntl);
    p.gpu(A740_SP_NC_MODE_CNTL,in->ubwc.sp_nc_mode_cntl);
    /* PIPE_BR and PIPE_BV through the host aperture, then PIPE_NONE */
    for(uint32_t pipe=NATIVE_ADRENO_A740_PIPE_BR;pipe<=NATIVE_ADRENO_A740_PIPE_BV;++pipe) {
        p.gpu(A740_CP_APERTURE_CNTL_HOST,pipe<<NATIVE_ADRENO_A740_APERTURE_PIPE_SHIFT);
        p.gpu(A740_GRAS_NC_MODE_CNTL,in->ubwc.gras_nc_mode_cntl);
    }
    p.gpu(A740_CP_APERTURE_CNTL_HOST,0);
    p.gpu(A740_UCHE_MODE_CNTL,in->ubwc.uche_mode_cntl);
    p.gpu(A740_RBBM_NC_MODE_CNTL,in->ubwc.rbbm_nc_mode_cntl);
    p.step(NativeAdrenoHwInitInterrupts);
    p.gpu(A740_RBBM_INT_0_MASK,NATIVE_ADRENO_A740_INT_MASK);
    p.step(NativeAdrenoHwInitSqe);
    p.gpu64(A740_CP_SQE_INSTR_BASE,in->sqe_iova);
}
/* hw_init's a650 family branches, in Linux order. */
void programA650(Program &p,const NativeAdrenoHwInitInputs *in)
{
    p.step(NativeAdrenoHwInitGbif);
    uint32_t value=0;
    p.gpu(A650_GBIF_HALT,0);p.gpuRead(A650_GBIF_HALT,&value);
    p.gpu(A650_RBBM_GBIF_HALT,0);p.gpuRead(A650_RBBM_GBIF_HALT,&value);
    p.step(NativeAdrenoHwInitSecvid);
    p.gpu(A650_RBBM_SECVID_TSB_CNTL,0);
    p.gpu64(A650_RBBM_SECVID_TSB_TRUSTED_BASE,0);
    p.gpu(A650_RBBM_SECVID_TSB_TRUSTED_SIZE,0);
    p.step(NativeAdrenoHwInitAddressMode);
    static const uint32_t address_mode[]={A650_CP_ADDR_MODE_CNTL,A650_VSC_ADDR_MODE_CNTL,A650_GRAS_ADDR_MODE_CNTL,
        A650_RB_ADDR_MODE_CNTL,A650_PC_ADDR_MODE_CNTL,A650_HLSQ_ADDR_MODE_CNTL,A650_VFD_ADDR_MODE_CNTL,
        A650_VPC_ADDR_MODE_CNTL,A650_UCHE_ADDR_MODE_CNTL,A650_SP_ADDR_MODE_CNTL,A650_TPL1_ADDR_MODE_CNTL,
        A650_RBBM_SECVID_TSB_ADDR_MODE_CNTL};
    for(uint32_t reg:address_mode)p.gpu(reg,1);   /* 64-bit addressing in every block */
    p.step(NativeAdrenoHwInitHwcg);
    p.gmu(A650_GPU_GMU_AO_GMU_CGC_MODE_CNTL,NATIVE_ADRENO_A650_GMU_CGC_MODE);
    p.gmu(A650_GPU_GMU_AO_GMU_CGC_DELAY_CNTL,NATIVE_ADRENO_A650_CGC_DELAY);
    p.gmu(A650_GPU_GMU_AO_GMU_CGC_HYST_CNTL,NATIVE_ADRENO_A650_CGC_HYST);
    if(p.gpuRead(A650_RBBM_CLOCK_CNTL,&value)) {
        p.r->clock_cntl_before=value;
        if(value!=NATIVE_ADRENO_A650_CLOCK_CNTL_ON) {
            p.r->hwcg_programmed=1;
            /* the SP clock is held while the table is written, a6xx only */
            p.gmuRmw(A650_GPU_GMU_GX_SPTPRAC_CLOCK_CONTROL,1,0);
            for(unsigned i=0;i<NATIVE_ADRENO_A650_HWCG_COUNT;++i)p.gpu(NativeAdrenoA650Hwcg[i][0],NativeAdrenoA650Hwcg[i][1]);
            p.gmuRmw(A650_GPU_GMU_GX_SPTPRAC_CLOCK_CONTROL,0,1);
            p.gpu(A650_RBBM_CLOCK_CNTL,NATIVE_ADRENO_A650_CLOCK_CNTL_ON);
        }
    }
    p.step(NativeAdrenoHwInitQos);
    p.gpu(A650_RBBM_GBIF_CLIENT_QOS_CNTL,NATIVE_ADRENO_A650_GBIF_CLIENT_QOS);
    p.gpu(A650_RBBM_PERFCTR_GPU_BUSY_MASKED,0xffffffffu);
    p.step(NativeAdrenoHwInitUche);
    p.gpu64(A650_UCHE_WRITE_RANGE_MAX,NATIVE_ADRENO_A650_UCHE_TRAP_BASE+NATIVE_ADRENO_A650_UCHE_WRITE_RANGE_SPAN);
    p.gpu64(A650_UCHE_TRAP_BASE,NATIVE_ADRENO_A650_UCHE_TRAP_BASE);
    p.gpu64(A650_UCHE_WRITE_THRU_BASE,NATIVE_ADRENO_A650_UCHE_TRAP_BASE);
    /* no UCHE_GMEM_RANGE writes: hw_init skips them for the a650 family */
    p.gpu(A650_UCHE_FILTER_CNTL,NATIVE_ADRENO_A650_UCHE_FILTER_CNTL);
    p.gpu(A650_UCHE_CACHE_WAYS,NATIVE_ADRENO_A650_UCHE_CACHE_WAYS);
    p.step(NativeAdrenoHwInitCp);
    p.gpu(A650_CP_ROQ_THRESHOLDS_2,NATIVE_ADRENO_A650_CP_ROQ_THRESHOLDS_2);
    p.gpu(A650_CP_ROQ_THRESHOLDS_1,NATIVE_ADRENO_A650_CP_ROQ_THRESHOLDS_1);
    p.gpu(A650_CP_MEM_POOL_SIZE,NATIVE_ADRENO_A650_CP_MEM_POOL_SIZE);
    p.gpu(A650_PC_DBG_ECO_CNTL,NATIVE_ADRENO_A650_PRIM_FIFO_THRESHOLD);
    p.gpu(A650_CP_AHB_CNTL,NATIVE_ADRENO_A650_CP_AHB_CNTL);
    p.gpu(A650_RBBM_PERFCTR_CNTL,NATIVE_ADRENO_A650_RBBM_PERFCTR_CNTL);
    p.gpu(A650_CP_PERFCTR_CP_SEL,NATIVE_ADRENO_A650_PERF_CP_ALWAYS_COUNT);
    p.step(NativeAdrenoHwInitUbwc);
    p.gpu(A650_RB_NC_MODE_CNTL,in->ubwc.rb_nc_mode_cntl);
    p.gpu(A650_TPL1_NC_MODE_CNTL,in->ubwc.tpl1_nc_mode_cntl);
    p.gpu(A650_SP_NC_MODE_CNTL,in->ubwc.sp_nc_mode_cntl);
    p.gpu(A650_UCHE_MODE_CNTL,in->ubwc.uche_mode_cntl);
    p.gpu(A650_RBBM_NC_MODE_CNTL,in->ubwc.rbbm_nc_mode_cntl);
    p.step(NativeAdrenoHwInitFaults);
    p.gpu(A650_RBBM_INTERFACE_HANG_INT_CNTL,NATIVE_ADRENO_A650_HANG_INT_CNTL);
    p.gpu(A650_UCHE_CLIENT_PF,NATIVE_ADRENO_A650_UCHE_CLIENT_PF);
    for(unsigned i=0;i<5;++i)p.gpu(A650_TPL1_BICUBIC_WEIGHTS_TABLE+i,NativeAdrenoA650BicubicWeights[i]);
    p.step(NativeAdrenoHwInitCounters);
    p.gmu(A650_GPU_GMU_AO_GPU_CX_BUSY_MASK,NATIVE_ADRENO_A650_CX_BUSY_MASK);
    p.gmuRmw(A650_GMU_CX_GMU_POWER_COUNTER_SELECT_0,NATIVE_ADRENO_A650_POWER_COUNTER_SELECT_MASK,
        NATIVE_ADRENO_A650_POWER_COUNTER_SELECT_VALUE);
    p.gmu(A650_GMU_CX_GMU_POWER_COUNTER_ENABLE,1);
    p.step(NativeAdrenoHwInitProtect);
    p.gpu(A650_CP_PROTECT_CNTL,NATIVE_ADRENO_A650_CP_PROTECT_CNTL);
    for(unsigned i=0;i<NATIVE_ADRENO_A650_PROTECT_COUNT;++i)
        p.gpu(A650_CP_PROTECT+NativeAdrenoA650Protect[i][0],NativeAdrenoA650Protect[i][1]);
    p.step(NativeAdrenoHwInitApriv);
    p.gpu(A650_CP_APRIV_CNTL,NATIVE_ADRENO_A650_CP_APRIV_CNTL);
    p.step(NativeAdrenoHwInitInterrupts);
    p.gpu(A650_RBBM_INT_0_MASK,NATIVE_ADRENO_A650_INT_MASK);
    p.step(NativeAdrenoHwInitSqe);
    p.gpu64(A650_CP_SQE_INSTR_BASE,in->sqe_iova);
}
}
IOReturn NativeAdrenoHwInit(const NativeAdrenoFamilyInfo *info,const NativeAdrenoHwInitIo *io,
    const NativeAdrenoHwInitInputs *in,NativeAdrenoHwInitResult *result)
{
    if(!result)return kIOReturnBadArgument;
    *result={};
    if(!info || !io || !io->gpu_write || !io->gpu_read || !io->gmu_write || !io->gmu_read || !in ||
        !in->sqe_iova || (in->sqe_iova&4095) || in->sqe_iova>=(UINT64_C(1)<<48))return kIOReturnBadArgument;
    Program p{io,result};
    switch(info->family) {
    case NativeAdrenoFamilyA740:programA740(p,in);break;
    case NativeAdrenoFamilyA650:programA650(p,in);break;
    default:return kIOReturnUnsupported;
    }
    if(p.failed)return kIOReturnIOError;
    result->step=NativeAdrenoHwInitDone;
    return kIOReturnSuccess;
}
