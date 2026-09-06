#include "la_mem_geom.h"

static int aligned64(uintptr_t value)
{
    return value % LA_MEM_ALIGNMENT == 0u;
}

int la_mem_geometry_check_full(uintptr_t fw_end, uintptr_t nc_base,
                               uintptr_t nc_size, uintptr_t capture_offset,
                               uintptr_t capture_size, uintptr_t sg_offset,
                               uintptr_t sg_size)
{
    uintptr_t ddr_end = LA_MEM_DDR_BASE + LA_MEM_DDR_SIZE;
    uintptr_t nc_end = nc_base + nc_size;

    if (fw_end > nc_base) {
        return LA_MEM_CHECK_ERR_FW_OVERLAP_NC;
    }
    if (nc_base < LA_MEM_DDR_BASE || nc_end > ddr_end) {
        return LA_MEM_CHECK_ERR_NC_OUT_OF_DDR;
    }
    if (nc_base % LA_MEM_SECTION_SIZE != 0u || nc_size % LA_MEM_SECTION_SIZE != 0u) {
        return LA_MEM_CHECK_ERR_NC_MISALIGNED;
    }
    if (capture_offset % LA_MEM_ALIGNMENT != 0u || capture_size % LA_MEM_ALIGNMENT != 0u ||
        sg_offset % LA_MEM_ALIGNMENT != 0u || sg_size % LA_MEM_ALIGNMENT != 0u) {
        return LA_MEM_CHECK_ERR_POOL_MISALIGNED;
    }
    if (capture_offset + capture_size > nc_size || sg_offset + sg_size > nc_size) {
        return LA_MEM_CHECK_ERR_POOL_OUT_OF_NC;
    }
    return LA_MEM_CHECK_OK;
}

int la_mem_geometry_check(uintptr_t fw_end)
{
    return la_mem_geometry_check_full(fw_end, LA_MEM_NC_BASE, LA_MEM_NC_SIZE,
                                      LA_MEM_CAPTURE_OFFSET, LA_MEM_CAPTURE_SIZE,
                                      LA_MEM_SG_OFFSET, LA_MEM_SG_SIZE);
}

uint32_t la_mem_apply_noncached(la_mem_attr_fn fn, void *context)
{
    uintptr_t section;
    uint32_t count = 0u;

    for (section = LA_MEM_NC_BASE;
         section < LA_MEM_NC_BASE + LA_MEM_NC_SIZE;
         section += LA_MEM_SECTION_SIZE) {
        fn(context, section);
        count += 1u;
    }
    return count;
}

uintptr_t la_mem_nc_base(void)
{
    return LA_MEM_NC_BASE;
}

uintptr_t la_mem_capture_base(void)
{
    return LA_MEM_NC_BASE + LA_MEM_CAPTURE_OFFSET;
}

uintptr_t la_mem_sg_base(void)
{
    return LA_MEM_NC_BASE + LA_MEM_SG_OFFSET;
}
