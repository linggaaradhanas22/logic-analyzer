#ifndef LA_MEM_GEOM_H
#define LA_MEM_GEOM_H

/*
 * P4.3 Zynq DDR memory geometry for the acquisition pools.
 *
 * Single source for the carve-out of Normal Non-cacheable DDR used by the
 * capture payload and the SG descriptor pool (rtos-design.md, "Cache, MMU
 * and Buffer Contract"). DDR totals 1 GiB on the Zybo Z7-10
 * (0x0010_0000 .. 0x4000_0000, matching the Vitis-generated lscript.ld
 * MEMORY region). The non-cached window sits at the top of DDR, aligned to
 * 1 MiB MMU sections; pools inside it are 64-byte aligned with cache-line-
 * rounded capacities.
 *
 * On target, main.c programmes the window with Xil_SetTlbAttributes(..,
 * NORM_NONCACHE) per 1 MiB section at boot and runs the geometry check
 * against the linker end symbol; this module stays pure so the host tests
 * can exercise every branch.
 */

#include <stdint.h>

#if defined(_WIN32) && defined(LA_NATIVE_BUILD)
#define LA_MEM_API __declspec(dllexport)
#else
#define LA_MEM_API
#endif

#define LA_MEM_DDR_BASE UINT32_C(0x00100000) /* first DDR byte */
#define LA_MEM_DDR_SIZE UINT32_C(0x3FF00000) /* 1 GiB minus the 1 MiB hole */
#define LA_MEM_NC_BASE UINT32_C(0x3F000000)  /* non-cached window base */
#define LA_MEM_NC_SIZE UINT32_C(0x01000000)  /* non-cached window, 16 MiB */
#define LA_MEM_SECTION_SIZE UINT32_C(0x00100000) /* 1 MiB MMU section */
#define LA_MEM_ALIGNMENT UINT32_C(64)        /* allocation alignment */

/* Pools inside the window (offsets from LA_MEM_NC_BASE). */
#define LA_MEM_CAPTURE_OFFSET UINT32_C(0x00000000)
#define LA_MEM_CAPTURE_SIZE UINT32_C(0x00800000) /* burst payload, 8 MiB */
#define LA_MEM_SG_OFFSET UINT32_C(0x00800000)
#define LA_MEM_SG_SIZE UINT32_C(0x00100000)      /* SG descriptors, 1 MiB */

/* U03 direct-USB DMA pool (XUsbPs dQH/dTD/buffers), 64 KiB after the SG
 * pool. The whole window is programmed non-cached at boot, so no cache
 * maintenance is needed for the USB payloads and descriptors. */
#define LA_MEM_USB_OFFSET UINT32_C(0x00900000)
#define LA_MEM_USB_SIZE UINT32_C(0x00010000)

typedef char la_mem_usb_pool_fits[
    ((LA_MEM_USB_OFFSET + LA_MEM_USB_SIZE) <= LA_MEM_NC_SIZE &&
     LA_MEM_USB_OFFSET >= (LA_MEM_SG_OFFSET + LA_MEM_SG_SIZE))
        ? 1
        : -1];

/* P8.5 cached-vs-non-cached experiment. 0 keeps the whole window
 * Non-cacheable. 1 (the measured winner, see rtos-design.md P8.5 results)
 * programs the live-pool prefix (16 x 256 KiB = 4 MiB, the same region
 * la_live_ring uses at LA_MEM_CAPTURE_OFFSET) as Normal Write-Back cacheable;
 * every CPU read of DMA-written data then goes through
 * Xil_DCacheInvalidateRange (la_acq_task live publish, la_tx_task burst
 * read). Override on the compiler command line to rebuild the experimental
 * non-cached variant without editing this header. */
#ifndef LA_MEM_LIVE_CACHED
#define LA_MEM_LIVE_CACHED 1
#endif
#define LA_MEM_LIVE_CACHED_SIZE UINT32_C(0x00400000) /* 16 x 256 KiB */

typedef enum la_mem_check_result {
    LA_MEM_CHECK_OK = 0,
    LA_MEM_CHECK_ERR_FW_OVERLAP_NC = -1,
    LA_MEM_CHECK_ERR_NC_OUT_OF_DDR = -2,
    LA_MEM_CHECK_ERR_NC_MISALIGNED = -3,
    LA_MEM_CHECK_ERR_POOL_MISALIGNED = -4,
    LA_MEM_CHECK_ERR_POOL_OUT_OF_NC = -5
} la_mem_check_result_t;

typedef void (*la_mem_attr_fn)(void *context, uintptr_t section_base);

LA_MEM_API int la_mem_geometry_check(uintptr_t fw_end);

/* Full check with explicit window/pool geometry (host-testable branches).
 * nc_end must equal nc_base + nc_size and must not exceed the DDR end. */
LA_MEM_API int la_mem_geometry_check_full(uintptr_t fw_end, uintptr_t nc_base,
                                          uintptr_t nc_size,
                                          uintptr_t capture_offset,
                                          uintptr_t capture_size,
                                          uintptr_t sg_offset,
                                          uintptr_t sg_size);

/* Applies fn to every 1 MiB section of the non-cached window; returns the
 * number of sections programmed. */
LA_MEM_API uint32_t la_mem_apply_noncached(la_mem_attr_fn fn, void *context);

/* Applies fn to the 1 MiB sections covering [offset, offset+size) inside the
 * window. offset and size must be multiples of LA_MEM_SECTION_SIZE, size must
 * be nonzero, and the range must stay within LA_MEM_NC_SIZE; returns the
 * number of sections programmed, or 0 on invalid arguments. */
LA_MEM_API uint32_t la_mem_apply_range(la_mem_attr_fn fn, void *context,
                                       uintptr_t offset, uintptr_t size);

LA_MEM_API uintptr_t la_mem_nc_base(void);
LA_MEM_API uintptr_t la_mem_capture_base(void);
LA_MEM_API uintptr_t la_mem_sg_base(void);

#endif /* LA_MEM_GEOM_H */
