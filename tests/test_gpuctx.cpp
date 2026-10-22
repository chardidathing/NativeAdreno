/* SPDX-License-Identifier: MIT */
/* The GPU context of both families over the software SMMU peer of test_gmuctx: the same bank, streams and table
 * pool, with the kernel buffers placed at the family's addresses (1 MiB on the a740, 16 MiB on the a650, above its
 * GMEM window). No hardware. */
#include "XnuMemoryFixture.h"
#include "NativeAdrenoGpuContext.h"
#include <cstdio>
#include <cstring>
#define CHECK(x) do {if(!(x)){std::fprintf(stderr,"line %d: %s\n",__LINE__,#x);std::exit(1);}}while(0)
IOReturn NativeAdrenoQueryCpu4K(NativeAdrenoCpuState *state)
{CHECK(state);*state={};return kIOReturnSuccess;}
IOReturn NativeAdrenoMaintainOwnedPages(void *mapping,size_t bytes,NativeAdrenoCacheOperation op)
{CHECK(mapping && bytes && op<=NativeAdrenoInvalidate);return kIOReturnSuccess;}
IOReturn NativeAdrenoSetPagesCacheMode(const NativeAdrenoPhysicalSpan *spans,size_t count,bool){return spans && count?kIOReturnSuccess:kIOReturnBadArgument;}
IOReturn NativeAdrenoReassertWriteCombined(const NativeAdrenoPhysicalSpan *spans,size_t count,uint32_t *reset)
{return spans && count && reset?kIOReturnSuccess:kIOReturnBadArgument;}
IOReturn NativeAdrenoMaintainPhysicalPages(uint64_t physical,size_t bytes,NativeAdrenoCacheOperation op)
{
    if(!physical || !bytes || ((physical|bytes)&4095) || op>NativeAdrenoInvalidate)std::abort();
    ++fixture.physical_calls;fixture.physical_bytes+=bytes;return kIOReturnSuccess;
}
/* the board's knobs the context reads: no BO pool, no write combining */
extern "C" {volatile uint32_t native_adreno_wc_enabled=0;}
#define SMMU_BYTES 0x40000
alignas(8) static uint32_t mmio[SMMU_BYTES/4];
static void reset()
{
    std::memset(mmio,0,sizeof(mmio));
    mmio[0]=0x002f0406;mmio[0x20/4]=0x4c017e18;mmio[0x24/4]=0x40000016;mmio[0x28/4]=0x1433;
    for(unsigned i=0;i<24;++i)mmio[(0xc00+4*i)/4]=0x20000;   /* every S2CR FAULT, every SMR invalid */
}
static int read32(void *,size_t at,uint32_t *v){CHECK(at%4==0 && at<=SMMU_BYTES-4);*v=mmio[at/4];return 0;}
static int read64(void *,size_t at,uint64_t *v){CHECK(at%8==0 && at<=SMMU_BYTES-8);std::memcpy(v,mmio+at/4,8);return 0;}
static int write32(void *,size_t at,uint32_t v)
{
    CHECK(at%4==0 && at<=SMMU_BYTES-4);
    if((at&0xfff)==0x58 && at>=0x20000) {mmio[at/4]&=~v;return 0;}   /* context FSR is write-one-to-clear */
    mmio[at/4]=v;return 0;
}
static int write64(void *,size_t at,uint64_t v){CHECK(at%8==0 && at<=SMMU_BYTES-8);std::memcpy(mmio+at/4,&v,8);return 0;}
static int barrier(void *){return 0;}
static int peer;
int main()
{
    const ArmSmmu4KIo io{&peer,SMMU_BYTES,read32,read64,write32,write64,barrier};
    const unsigned slots[2]={0,1};
    const uint16_t sids[2]={0,1};
    uint64_t iovas[GpuBufferCount];
    NativeAdrenoGpuContextIovas(NativeAdrenoFamilyA740,iovas);
    CHECK(iovas[0]==GpuSqeIova && iovas[1]==GpuRingIova && iovas[2]==GpuMemptrsIova && iovas[3]==GpuPwrupIova && iovas[4]==GpuScratchIova);
    NativeAdrenoGpuContextIovas(NativeAdrenoFamilyA650,iovas);
    CHECK(iovas[0]==0x1000000 && iovas[1]==0x1100000 && iovas[2]==0x1200000 && iovas[3]==0x1201000 && iovas[4]==0x1202000);
    /* every a650 kernel buffer sits above the 1 MiB GMEM window the a650 never reprograms */
    const NativeAdrenoFamilyInfo *a650=NativeAdrenoFamilyGet(NativeAdrenoFamilyA650);
    for(uint64_t iova:iovas)CHECK(iova>=a650->gmem_base+a650->gmem_bytes);
    NativeAdrenoGpuContextIovas(NativeAdrenoFamily(7),iovas);
    CHECK(!iovas[0] && !iovas[4]);
    unsigned contexts=0;
    for(NativeAdrenoFamily family:{NativeAdrenoFamilyA740,NativeAdrenoFamilyA650}) {
        reset();fixture=MemoryFixture{};
        NativeAdrenoGpuContext *context=new NativeAdrenoGpuContext;
        NativeAdrenoGmuContextReport report;
        CHECK(context->bringUp(&io,0,2,slots,sids,nullptr,&report,family)==kIOReturnSuccess);
        CHECK(report.step==NativeAdrenoGpuContext::Ready && context->ready() && context->vm());
        CHECK(report.routes_verified==2 && report.spans==GpuBufferCount);
        NativeAdrenoGpuContextIovas(family,iovas);
        for(unsigned i=0;i<GpuBufferCount;++i) {
            CHECK(context->iova(i)==iovas[i]);
            uint64_t iova=0;size_t bytes=0;unsigned permissions=0;
            /* the SQE is privileged read and execute, the rest read and write, at the family's addresses */
            CHECK(context->vm()->describe(context->handle(i),&iova,&bytes,&permissions)==kIOReturnSuccess);
            CHECK(iova==iovas[i] && bytes==context->buffer(i).bytes());
            CHECK(i?permissions==3u:permissions==5u);
        }
        CHECK(context->buffer(0).bytes()==GpuSqeBytes && context->buffer(1).bytes()==GpuRingBytes);
        CHECK(context->bringUp(&io,0,2,slots,sids,nullptr,&report,family)==kIOReturnBusy);
        ++contexts;
        delete context;
    }
    /* an unknown family is refused before anything is allocated */
    {
        reset();fixture=MemoryFixture{};
        NativeAdrenoGpuContext context;
        NativeAdrenoGmuContextReport report;
        CHECK(context.bringUp(&io,0,2,slots,sids,nullptr,&report,NativeAdrenoFamily(9))==kIOReturnBadArgument && fixture.live.empty());
    }
    std::printf("PASS: a740 and a650 GPU context buffer placement, %u contexts brought up over the software SMMU peer, 1 unknown family refusal\n",contexts);
}
