/* SPDX-License-Identifier: MIT */
#include "NativeAdrenoGmuConfig.h"
#include <IOKit/IOLib.h>
static bool matches(const NativeAdrenoMemory &memory,uint64_t bytes,uint64_t iova,
    ShengGmuMemory *out)
{
    if(!memory.mapping() || memory.bytes()!=size_t(bytes) || memory.spanCount()!=1)
        return false;
    out->cpu=static_cast<volatile uint32_t *>(memory.mapping());
    out->bytes=size_t(bytes);
    out->iova=iova;
    return true;
}
IOReturn NativeAdrenoAssembleGmuConfig(const NativeAdrenoFirmwareView *firmware,
    const NativeAdrenoGmuAllocator *allocator,uint32_t chip_id,unsigned poll_budget,
    ShengGmuConfig *config)
{
    if(!config)return kIOReturnBadArgument;
    *config={};
    if(!firmware || !firmware->bytes || !firmware->length || !allocator ||
        !allocator->ready() || !chip_id || !poll_budget || poll_budget>1024)
        return kIOReturnBadArgument;
    /* the chip id names the family, and the family the payload sizes the startup consumes */
    const NativeAdrenoFamilyInfo *info=NativeAdrenoFamilySelect(chip_id);
    if(!info)return kIOReturnBadArgument;
    const NativeAdrenoGmuSizes sizes=NativeAdrenoGmuFamilySizes(info->family,0x1000);
    const NativeAdrenoGmuMemoryLayout &layout=allocator->layout();
    ShengGmuConfig built{};
    if(!matches(allocator->icache(),sizes.icache,layout.icache_iova,&built.icache) ||
        !matches(allocator->dummy(),sizes.dummy,layout.dummy_iova,&built.dummy) ||
        !matches(allocator->log(),sizes.log,layout.log_iova,&built.log) ||
        !matches(allocator->hfi(),sizes.hfi,layout.hfi_iova,&built.hfi))
        return kIOReturnBadArgument;
    if(sizes.debug) {
        if(!matches(allocator->debug(),sizes.debug,layout.debug_iova,&built.debug))return kIOReturnBadArgument;
    } else if(allocator->hasDebug())return kIOReturnBadArgument;   /* a650: no debug object may be mapped */
    built.chip_id=chip_id;
    built.firmware=firmware->bytes;
    built.firmware_bytes=firmware->length;
    built.poll_budget=poll_budget;
    *config=built;
    return kIOReturnSuccess;
}
