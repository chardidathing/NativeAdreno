/* SPDX-License-Identifier: MIT */
/* The hw_init register programs of both families over a software register peer: the a740 one write for write as the
 * sheng board emits it, the a650 one as a6xx_gpu.c hw_init orders its a650 family branches. No hardware. */
#include "XnuMemoryFixture.h"
#include "NativeAdrenoHwInit.h"
#include "NativeAdrenoA650HwInit.h"
#include "NativeAdrenoA740HwInit.h"
#include <cstdio>
#include <vector>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
struct Access {bool gmu,write;uint32_t reg,value;};
struct Peer {
    std::vector<uint32_t> gpu,gmu;
    std::vector<Access> log;
    unsigned fail_at=0;
    Peer():gpu(0x40000,0),gmu(0x40000,0){}
};
static Peer peer;
static int touch(bool gmu,bool write,uint32_t reg,uint32_t *value)
{
    std::vector<uint32_t> &space=gmu?peer.gmu:peer.gpu;
    CHECK(reg<space.size());
    peer.log.push_back({gmu,write,reg,write?*value:space[reg]});
    if(peer.fail_at && peer.log.size()==peer.fail_at)return 1;
    if(write)space[reg]=*value;else *value=space[reg];
    return 0;
}
static int gpuWrite(void *c,uint32_t reg,uint32_t value){CHECK(c==&peer);return touch(false,true,reg,&value);}
static int gpuRead(void *c,uint32_t reg,uint32_t *value){CHECK(c==&peer && value);return touch(false,false,reg,value);}
static int gmuWrite(void *c,uint32_t reg,uint32_t value){CHECK(c==&peer);return touch(true,true,reg,&value);}
static int gmuRead(void *c,uint32_t reg,uint32_t *value){CHECK(c==&peer && value);return touch(true,false,reg,value);}
static const NativeAdrenoHwInitIo io{&peer,gpuWrite,gpuRead,gmuWrite,gmuRead};
static NativeAdrenoHwInitInputs inputs(const NativeAdrenoFamilyInfo *info)
{
    NativeAdrenoHwInitInputs in{};
    in.sqe_iova=info->kernel_iova_base;
    const NativeAdrenoUbwcInputs ubwc{16,32,4,1,1};
    CHECK(NativeAdrenoUbwcWordsFor(info,&ubwc,&in.ubwc)==kIOReturnSuccess);
    return in;
}
/* the next logged access must be this write */
static size_t at;
static void expectGpu(uint32_t reg,uint32_t value)
{
    CHECK(at<peer.log.size());
    const Access &a=peer.log[at++];
    if(a.gmu || !a.write || a.reg!=reg || a.value!=value)
        std::fprintf(stderr,"access %zu: gmu=%d write=%d reg=0x%x value=0x%x, wanted gpu write 0x%x=0x%x\n",at-1,a.gmu,a.write,a.reg,a.value,reg,value);
    CHECK(!a.gmu && a.write && a.reg==reg && a.value==value);
}
static void expectGpu64(uint32_t reg,uint64_t value){expectGpu(reg,uint32_t(value));expectGpu(reg+1,uint32_t(value>>32));}
static void expectGpuRead(uint32_t reg)
{
    CHECK(at<peer.log.size());const Access &a=peer.log[at++];CHECK(!a.gmu && !a.write && a.reg==reg);
}
static void expectGmu(uint32_t reg,uint32_t value)
{
    CHECK(at<peer.log.size());const Access &a=peer.log[at++];CHECK(a.gmu && a.write && a.reg==reg && a.value==value);
}
static void expectGmuRead(uint32_t reg)
{
    CHECK(at<peer.log.size());const Access &a=peer.log[at++];CHECK(a.gmu && !a.write && a.reg==reg);
}
int main()
{
    const NativeAdrenoFamilyInfo *a740=NativeAdrenoFamilyGet(NativeAdrenoFamilyA740),*a650=NativeAdrenoFamilyGet(NativeAdrenoFamilyA650);
    /* a740: the sheng GpuPowerProbe sequence (RBBM_CLOCK_CNTL reads as off, so the table is written) */
    {
        peer=Peer{};at=0;
        const NativeAdrenoHwInitInputs in=inputs(a740);
        NativeAdrenoHwInitResult r{};
        CHECK(NativeAdrenoHwInit(a740,&io,&in,&r)==kIOReturnSuccess && r.step==NativeAdrenoHwInitDone && r.hwcg_programmed);
        expectGpu(A740_GBIF_HALT,0);expectGpuRead(A740_GBIF_HALT);
        expectGpu(A740_RBBM_GBIF_HALT,0);expectGpuRead(A740_RBBM_GBIF_HALT);
        expectGpu(A740_RBBM_SECVID_TSB_CNTL,0);expectGpu64(A740_RBBM_SECVID_TSB_TRUSTED_BASE,0);expectGpu(A740_RBBM_SECVID_TSB_TRUSTED_SIZE,0);
        expectGmu(A740_GPU_GMU_AO_GMU_CGC_MODE_CNTL,0x00020202);expectGmu(A740_GPU_GMU_AO_GMU_CGC_DELAY_CNTL,0x10111);
        expectGmu(A740_GPU_GMU_AO_GMU_CGC_HYST_CNTL,0x5555);
        expectGpuRead(A740_RBBM_CLOCK_CNTL);
        for(unsigned i=0;i<NATIVE_ADRENO_A740_HWCG_COUNT;++i)expectGpu(NativeAdrenoA740Hwcg[i][0],NativeAdrenoA740Hwcg[i][1]);
        expectGpu(A740_RBBM_CLOCK_CNTL,0x8aa8aa82);
        expectGpu(A740_RBBM_GBIF_CLIENT_QOS_CNTL,0x2120212);expectGpu(A740_UCHE_GBIF_GX_CONFIG,0x10240e0);
        expectGpu(A740_RBBM_PERFCTR_GPU_BUSY_MASKED,0xffffffff);
        expectGpu64(A740_UCHE_TRAP_BASE,0x1fffffffff000ull);expectGpu64(A740_UCHE_WRITE_THRU_BASE,0x1fffffffff000ull);
        expectGpu64(A740_UCHE_GMEM_RANGE_MIN,0x1000000);expectGpu64(A740_UCHE_GMEM_RANGE_MAX,0x1000000+3*0x100000-1);
        expectGpu(A740_UCHE_CACHE_WAYS,1u<<23);
        expectGpu(A740_CP_AHB_CNTL,1);expectGpu(A740_RBBM_PERFCTR_CNTL,1);
        expectGpu(A740_RBBM_INTERFACE_HANG_INT_CNTL,(1u<<30)|0xcfffff);expectGpu(A740_UCHE_CLIENT_PF,(1u<<7)|1);
        expectGpu(A740_CP_PROTECT_CNTL,(1u<<0)|(1u<<1)|(1u<<3));
        for(unsigned i=0;i<NATIVE_ADRENO_A740_PROTECT_COUNT;++i)expectGpu(A740_CP_PROTECT+NativeAdrenoA740Protect[i][0],NativeAdrenoA740Protect[i][1]);
        expectGpu(A740_UCHE_CMDQ_CONFIG,(6u<<16)|(6u<<12)|(9u<<8)|(1u<<3)|(1u<<2)|2u);
        expectGpu(A740_CP_APRIV_CNTL,0x6f);expectGpu(A740_CP_BV_APRIV_CNTL,0x0f);expectGpu(A740_CP_LPAC_APRIV_CNTL,0x0f);
        expectGpuRead(A740_RB_CMP_DBG_ECO_CNTL);expectGpu(A740_RB_CMP_DBG_ECO_CNTL,1u<<11);
        expectGpu(A740_RB_NC_MODE_CNTL,(1u<<11)|(1u<<4)|(3u<<1));expectGpu(A740_TPL1_NC_MODE_CNTL,3u<<1);
        expectGpu(A740_SP_NC_MODE_CNTL,(2u<<4)|(3u<<1));
        for(uint32_t pipe=1;pipe<=2;++pipe) {expectGpu(A740_CP_APERTURE_CNTL_HOST,pipe<<12);expectGpu(A740_GRAS_NC_MODE_CNTL,3u<<5);}
        expectGpu(A740_CP_APERTURE_CNTL_HOST,0);expectGpu(A740_UCHE_MODE_CNTL,3u<<21);expectGpu(A740_RBBM_NC_MODE_CNTL,1);
        expectGpu(A740_RBBM_INT_0_MASK,0x33d283c2);
        expectGpu64(A740_CP_SQE_INSTR_BASE,0x100000);
        CHECK(at==peer.log.size());
        CHECK(r.writes+r.reads==peer.log.size());
    }
    /* a740 with clock gating already on: the table is skipped, as the board does */
    {
        peer=Peer{};peer.gpu[A740_RBBM_CLOCK_CNTL]=0x8aa8aa82;
        const NativeAdrenoHwInitInputs in=inputs(a740);
        NativeAdrenoHwInitResult r{};
        CHECK(NativeAdrenoHwInit(a740,&io,&in,&r)==kIOReturnSuccess && !r.hwcg_programmed && r.clock_cntl_before==0x8aa8aa82);
        for(const Access &a:peer.log)CHECK(a.gmu || !a.write || a.reg!=NativeAdrenoA740Hwcg[0][0]);
    }
    /* a650: hw_init's a650 family branches in Linux order */
    {
        peer=Peer{};at=0;
        const NativeAdrenoHwInitInputs in=inputs(a650);
        NativeAdrenoHwInitResult r{};
        CHECK(NativeAdrenoHwInit(a650,&io,&in,&r)==kIOReturnSuccess && r.step==NativeAdrenoHwInitDone && r.hwcg_programmed);
        expectGpu(A650_GBIF_HALT,0);expectGpuRead(A650_GBIF_HALT);
        expectGpu(A650_RBBM_GBIF_HALT,0);expectGpuRead(A650_RBBM_GBIF_HALT);
        expectGpu(A650_RBBM_SECVID_TSB_CNTL,0);expectGpu64(A650_RBBM_SECVID_TSB_TRUSTED_BASE,0);expectGpu(A650_RBBM_SECVID_TSB_TRUSTED_SIZE,0);
        const uint32_t modes[]={A650_CP_ADDR_MODE_CNTL,A650_VSC_ADDR_MODE_CNTL,A650_GRAS_ADDR_MODE_CNTL,A650_RB_ADDR_MODE_CNTL,
            A650_PC_ADDR_MODE_CNTL,A650_HLSQ_ADDR_MODE_CNTL,A650_VFD_ADDR_MODE_CNTL,A650_VPC_ADDR_MODE_CNTL,A650_UCHE_ADDR_MODE_CNTL,
            A650_SP_ADDR_MODE_CNTL,A650_TPL1_ADDR_MODE_CNTL,A650_RBBM_SECVID_TSB_ADDR_MODE_CNTL};
        for(uint32_t reg:modes)expectGpu(reg,1);
        expectGmu(A650_GPU_GMU_AO_GMU_CGC_MODE_CNTL,0x00020202);expectGmu(A650_GPU_GMU_AO_GMU_CGC_DELAY_CNTL,0x10111);
        expectGmu(A650_GPU_GMU_AO_GMU_CGC_HYST_CNTL,0x5555);
        expectGpuRead(A650_RBBM_CLOCK_CNTL);
        expectGmuRead(A650_GPU_GMU_GX_SPTPRAC_CLOCK_CONTROL);expectGmu(A650_GPU_GMU_GX_SPTPRAC_CLOCK_CONTROL,0);
        for(unsigned i=0;i<NATIVE_ADRENO_A650_HWCG_COUNT;++i)expectGpu(NativeAdrenoA650Hwcg[i][0],NativeAdrenoA650Hwcg[i][1]);
        expectGmuRead(A650_GPU_GMU_GX_SPTPRAC_CLOCK_CONTROL);expectGmu(A650_GPU_GMU_GX_SPTPRAC_CLOCK_CONTROL,1);
        expectGpu(A650_RBBM_CLOCK_CNTL,0x8aa8aa82);
        expectGpu(A650_RBBM_GBIF_CLIENT_QOS_CNTL,0x3);expectGpu(A650_RBBM_PERFCTR_GPU_BUSY_MASKED,0xffffffff);
        expectGpu64(A650_UCHE_WRITE_RANGE_MAX,0x1fffffffff000ull+0xfc0);expectGpu64(A650_UCHE_TRAP_BASE,0x1fffffffff000ull);
        expectGpu64(A650_UCHE_WRITE_THRU_BASE,0x1fffffffff000ull);
        expectGpu(A650_UCHE_FILTER_CNTL,0x804);expectGpu(A650_UCHE_CACHE_WAYS,0x4);
        expectGpu(A650_CP_ROQ_THRESHOLDS_2,0x02000140);expectGpu(A650_CP_ROQ_THRESHOLDS_1,0x8040362c);
        expectGpu(A650_CP_MEM_POOL_SIZE,128);expectGpu(A650_PC_DBG_ECO_CNTL,0x00300200);
        expectGpu(A650_CP_AHB_CNTL,1);expectGpu(A650_RBBM_PERFCTR_CNTL,1);expectGpu(A650_CP_PERFCTR_CP_SEL,0);
        expectGpu(A650_RB_NC_MODE_CNTL,0x816);expectGpu(A650_TPL1_NC_MODE_CNTL,6);expectGpu(A650_SP_NC_MODE_CNTL,0x26);
        expectGpu(A650_UCHE_MODE_CNTL,3u<<21);expectGpu(A650_RBBM_NC_MODE_CNTL,1);
        expectGpu(A650_RBBM_INTERFACE_HANG_INT_CNTL,(1u<<30)|0x1fffff);expectGpu(A650_UCHE_CLIENT_PF,(1u<<7)|1);
        for(unsigned i=0;i<5;++i)expectGpu(A650_TPL1_BICUBIC_WEIGHTS_TABLE+i,NativeAdrenoA650BicubicWeights[i]);
        expectGmu(A650_GPU_GMU_AO_GPU_CX_BUSY_MASK,0xff000000);
        expectGmuRead(A650_GMU_CX_GMU_POWER_COUNTER_SELECT_0);expectGmu(A650_GMU_CX_GMU_POWER_COUNTER_SELECT_0,1u<<5);
        expectGmu(A650_GMU_CX_GMU_POWER_COUNTER_ENABLE,1);
        expectGpu(A650_CP_PROTECT_CNTL,0xb);
        for(unsigned i=0;i<NATIVE_ADRENO_A650_PROTECT_COUNT;++i)expectGpu(A650_CP_PROTECT+NativeAdrenoA650Protect[i][0],NativeAdrenoA650Protect[i][1]);
        CHECK(NativeAdrenoA650Protect[NATIVE_ADRENO_A650_PROTECT_COUNT-1][0]==47);
        expectGpu(A650_CP_APRIV_CNTL,0x6e);
        expectGpu(A650_RBBM_INT_0_MASK,0x03d0e242);
        expectGpu64(A650_CP_SQE_INSTR_BASE,0x1000000);
        CHECK(at==peer.log.size());
        /* nothing a7xx-only was touched: no GMEM range, no BV/LPAC apriv, no aperture, no UCHE cmdq */
        for(const Access &a:peer.log) {
            CHECK(a.gmu || a.reg!=A650_UCHE_GMEM_RANGE_MIN);
            CHECK(a.gmu || (a.reg!=A740_CP_BV_APRIV_CNTL && a.reg!=A740_CP_LPAC_APRIV_CNTL && a.reg!=A740_CP_APERTURE_CNTL_HOST && a.reg!=A740_UCHE_CMDQ_CONFIG));
        }
        /* the power counter select keeps the other bits and replaces the low byte */
        peer=Peer{};peer.gmu[A650_GMU_CX_GMU_POWER_COUNTER_SELECT_0]=0xaabbcc07;
        CHECK(NativeAdrenoHwInit(a650,&io,&in,&r)==kIOReturnSuccess && peer.gmu[A650_GMU_CX_GMU_POWER_COUNTER_SELECT_0]==0xaabbcc20);
    }
    /* a failed access stops the program at its step */
    {
        unsigned stops=0;
        for(unsigned fail=1;fail<=40;fail+=3) {
            peer=Peer{};peer.fail_at=fail;
            const NativeAdrenoHwInitInputs in=inputs(a650);
            NativeAdrenoHwInitResult r{};
            CHECK(NativeAdrenoHwInit(a650,&io,&in,&r)==kIOReturnIOError && r.step!=NativeAdrenoHwInitDone && peer.log.size()==fail);
            ++stops;
        }
        peer=Peer{};
        NativeAdrenoHwInitInputs in=inputs(a650);
        NativeAdrenoHwInitResult r{};
        CHECK(NativeAdrenoHwInit(nullptr,&io,&in,&r)==kIOReturnBadArgument && NativeAdrenoHwInit(a650,nullptr,&in,&r)==kIOReturnBadArgument);
        CHECK(NativeAdrenoHwInit(a650,&io,nullptr,&r)==kIOReturnBadArgument && NativeAdrenoHwInit(a650,&io,&in,nullptr)==kIOReturnBadArgument);
        in.sqe_iova=0x1000001;CHECK(NativeAdrenoHwInit(a650,&io,&in,&r)==kIOReturnBadArgument);
        in.sqe_iova=0;CHECK(NativeAdrenoHwInit(a650,&io,&in,&r)==kIOReturnBadArgument);
        const NativeAdrenoHwInitIo broken{&peer,gpuWrite,gpuRead,nullptr,gmuRead};
        in.sqe_iova=0x1000000;CHECK(NativeAdrenoHwInit(a650,&broken,&in,&r)==kIOReturnBadArgument && peer.log.empty());
        std::printf("PASS: a740 and a650 hw_init programs write for write, 1 skipped clock gating table, %u stopped programs and 7 argument refusals over a software register peer\n",stops);
    }
}
