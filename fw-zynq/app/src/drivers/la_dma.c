#include "la_dma.h"

#include "xparameters.h"

#define LA_DMA_RESET_TIMEOUT 100000u

int la_dma_init(la_dma_t *dma, uintptr_t bd_space, uint32_t bd_space_size,
                uint32_t bd_max_bytes)
{
    XAxiDma_Config *config;

    if (dma == NULL || bd_space == 0u || bd_space_size == 0u ||
        bd_max_bytes == 0u) {
        return -1;
    }
    config = XAxiDma_LookupConfig(XPAR_XAXIDMA_0_BASEADDR);
    if (config == NULL) {
        return -1;
    }
    if (XAxiDma_CfgInitialize(&dma->dma, config) != XST_SUCCESS) {
        return -2;
    }
    if (!XAxiDma_HasSg(&dma->dma)) {
        return -3;
    }
    dma->ring = XAxiDma_GetRxRing(&dma->dma);
    if (dma->ring == NULL) {
        return -3;
    }
    dma->bd_space = bd_space;
    dma->bd_space_size = bd_space_size;
    dma->bd_max_bytes = bd_max_bytes;
    dma->error = 0u;
    dma->ioc_count = 0u;
    dma->last_bd_count = 0u;
    dma->last_actual_bytes = 0u;
    dma->ready = 1;
    return 0;
}

void la_dma_stop(la_dma_t *dma)
{
    uint32_t timeout = LA_DMA_RESET_TIMEOUT;

    if (dma == NULL || !dma->ready) {
        return;
    }
    XAxiDma_Reset(&dma->dma);
    while (timeout != 0u) {
        if (XAxiDma_ResetIsDone(&dma->dma)) {
            break;
        }
        timeout--;
    }
}

int la_dma_prepare(la_dma_t *dma, const la_burst_plan_t *plan,
                   uintptr_t buffer_base)
{
    XAxiDma_BdRing *ring;
    XAxiDma_Bd bd_template;
    XAxiDma_Bd *first_bd;
    XAxiDma_Bd *bd;
    uint32_t index;
    uint32_t max_bds;
    int status;

    if (dma == NULL || plan == NULL || !dma->ready || plan->bd_count == 0u) {
        return -1;
    }
    ring = dma->ring;
    max_bds = XAxiDma_BdRingCntCalc(XAXIDMA_BD_MINIMUM_ALIGNMENT,
                                    dma->bd_space_size);
    if (plan->bd_count > max_bds) {
        return -2;
    }

    la_dma_stop(dma);
    XAxiDma_BdRingIntDisable(ring, XAXIDMA_IRQ_ALL_MASK);

    status = XAxiDma_BdRingCreate(ring, dma->bd_space, dma->bd_space,
                                  XAXIDMA_BD_MINIMUM_ALIGNMENT,
                                  (int)plan->bd_count);
    if (status != XST_SUCCESS) {
        return -3;
    }
    XAxiDma_BdClear(&bd_template);
    status = XAxiDma_BdRingClone(ring, &bd_template);
    if (status != XST_SUCCESS) {
        return -4;
    }
    status = XAxiDma_BdRingAlloc(ring, (int)plan->bd_count, &first_bd);
    if (status != XST_SUCCESS) {
        return -5;
    }

    bd = first_bd;
    for (index = 0u; index < plan->bd_count; ++index) {
        UINTPTR address = buffer_base + plan->bd_offset[index];

        status = XAxiDma_BdSetBufAddr(bd, address);
        if (status != XST_SUCCESS) {
            return -6;
        }
        status = XAxiDma_BdSetLength(bd, plan->bd_length[index],
                                     ring->MaxTransferLen);
        if (status != XST_SUCCESS) {
            return -7;
        }
        XAxiDma_BdSetId(bd, address);
        bd = (XAxiDma_Bd *)XAxiDma_BdRingNext(ring, bd);
    }

    status = XAxiDma_BdRingToHw(ring, (int)plan->bd_count, first_bd);
    if (status != XST_SUCCESS) {
        return -8;
    }
    dma->error = 0u;
    dma->ioc_count = 0u;
    XAxiDma_BdRingIntEnable(ring, XAXIDMA_IRQ_ALL_MASK);
    status = XAxiDma_BdRingStart(ring);
    if (status != XST_SUCCESS) {
        return -9;
    }
    return 0;
}

int la_dma_collect(la_dma_t *dma, uint32_t *bytes_out, int *eof_out)
{
    XAxiDma_BdRing *ring;
    XAxiDma_Bd *first_bd;
    XAxiDma_Bd *bd;
    int count;
    int index;
    uint32_t total = 0u;
    int eof = 0;

    if (dma == NULL || !dma->ready || bytes_out == NULL || eof_out == NULL) {
        return -1;
    }
    ring = dma->ring;
    count = XAxiDma_BdRingFromHw(ring, XAXIDMA_ALL_BDS, &first_bd);
    bd = first_bd;
    for (index = 0; index < count; ++index) {
        uint32_t status_word = XAxiDma_BdGetSts(bd);

        if ((status_word & XAXIDMA_BD_STS_ALL_ERR_MASK) != 0u) {
            dma->error = 1u;
        }
        if ((status_word & XAXIDMA_BD_STS_RXEOF_MASK) != 0u) {
            eof = 1;
        }
        total += XAxiDma_BdGetActualLength(bd, ring->MaxTransferLen);
        bd = (XAxiDma_Bd *)XAxiDma_BdRingNext(ring, bd);
    }
    if (count > 0) {
        (void)XAxiDma_BdRingFree(ring, count, first_bd);
    }
    dma->last_bd_count = (uint32_t)count;
    dma->last_actual_bytes = total;
    *bytes_out = total;
    *eof_out = eof;
    return count;
}

int la_dma_prepare_live(la_dma_t *dma, const uintptr_t *addresses,
                        uint32_t count, uint32_t segment_bytes)
{
    XAxiDma_BdRing *ring;
    XAxiDma_Bd bd_template;
    XAxiDma_Bd *first_bd;
    XAxiDma_Bd *bd;
    uint32_t index;
    uint32_t max_bds;
    int status;

    if (dma == NULL || addresses == NULL || !dma->ready || count == 0u ||
        segment_bytes == 0u) {
        return -1;
    }
    ring = dma->ring;
    max_bds = XAxiDma_BdRingCntCalc(XAXIDMA_BD_MINIMUM_ALIGNMENT,
                                    dma->bd_space_size);
    if (count > max_bds) {
        return -2;
    }

    la_dma_stop(dma);
    XAxiDma_BdRingIntDisable(ring, XAXIDMA_IRQ_ALL_MASK);

    status = XAxiDma_BdRingCreate(ring, dma->bd_space, dma->bd_space,
                                  XAXIDMA_BD_MINIMUM_ALIGNMENT, (int)count);
    if (status != XST_SUCCESS) {
        return -3;
    }
    XAxiDma_BdClear(&bd_template);
    status = XAxiDma_BdRingClone(ring, &bd_template);
    if (status != XST_SUCCESS) {
        return -4;
    }
    status = XAxiDma_BdRingAlloc(ring, (int)count, &first_bd);
    if (status != XST_SUCCESS) {
        return -5;
    }
    bd = first_bd;
    for (index = 0u; index < count; ++index) {
        status = XAxiDma_BdSetBufAddr(bd, addresses[index]);
        if (status != XST_SUCCESS) {
            return -6;
        }
        status = XAxiDma_BdSetLength(bd, segment_bytes, ring->MaxTransferLen);
        if (status != XST_SUCCESS) {
            return -7;
        }
        XAxiDma_BdSetId(bd, index);
        bd = (XAxiDma_Bd *)XAxiDma_BdRingNext(ring, bd);
    }
    status = XAxiDma_BdRingToHw(ring, (int)count, first_bd);
    if (status != XST_SUCCESS) {
        return -8;
    }
    dma->error = 0u;
    dma->ioc_count = 0u;
    XAxiDma_BdRingIntEnable(ring, XAXIDMA_IRQ_ALL_MASK);
    status = XAxiDma_BdRingStart(ring);
    if (status != XST_SUCCESS) {
        return -9;
    }
    return 0;
}

int la_dma_collect_live(la_dma_t *dma, la_dma_live_bd_t *entries,
                        uint32_t max_entries)
{
    XAxiDma_BdRing *ring;
    XAxiDma_Bd *first_bd;
    XAxiDma_Bd *bd;
    int count;
    int index;
    int overflow = 0;

    if (dma == NULL || !dma->ready || entries == NULL || max_entries == 0u) {
        return -1;
    }
    ring = dma->ring;
    count = XAxiDma_BdRingFromHw(ring, XAXIDMA_ALL_BDS, &first_bd);
    if (count < 0) {
        dma->error = 1u;
        return -1;
    }
    if ((uint32_t)count > max_entries) {
        overflow = 1;
    }
    bd = first_bd;
    for (index = 0; index < count; ++index) {
        uint32_t status_word = XAxiDma_BdGetSts(bd);

        if ((status_word & XAXIDMA_BD_STS_ALL_ERR_MASK) != 0u) {
            dma->error = 1u;
        }
        if ((uint32_t)index < max_entries) {
            entries[index].id = XAxiDma_BdGetId(bd);
            entries[index].length = XAxiDma_BdGetActualLength(bd, ring->MaxTransferLen);
            entries[index].status = status_word;
        }
        bd = (XAxiDma_Bd *)XAxiDma_BdRingNext(ring, bd);
    }
    if (count > 0) {
        (void)XAxiDma_BdRingFree(ring, count, first_bd);
    }
    dma->last_bd_count = (uint32_t)count;
    if (overflow != 0) {
        return -2;
    }
    return count;
}

int la_dma_post_live(la_dma_t *dma, uint32_t id, uintptr_t address,
                     uint32_t length)
{
    XAxiDma_BdRing *ring;
    XAxiDma_Bd *bd;
    int status;

    if (dma == NULL || !dma->ready) {
        return -1;
    }
    ring = dma->ring;
    status = XAxiDma_BdRingAlloc(ring, 1, &bd);
    if (status != XST_SUCCESS) {
        return -2;
    }
    if (XAxiDma_BdSetBufAddr(bd, address) != XST_SUCCESS ||
        XAxiDma_BdSetLength(bd, length, ring->MaxTransferLen) != XST_SUCCESS) {
        (void)XAxiDma_BdRingUnAlloc(ring, 1, bd);
        return -1;
    }
    XAxiDma_BdSetId(bd, id);
    if (XAxiDma_BdRingToHw(ring, 1, bd) != XST_SUCCESS) {
        return -1;
    }
    return 0;
}

uint32_t la_dma_free_bd_count(const la_dma_t *dma)
{
    if (dma == NULL || !dma->ready || dma->ring == NULL) {
        return 0u;
    }
    return (uint32_t)XAxiDma_BdRingGetFreeCnt(dma->ring);
}

void la_dma_isr(la_dma_t *dma)
{
    uint32_t irq_status;

    if (dma == NULL || !dma->ready || dma->ring == NULL) {
        return;
    }
    irq_status = XAxiDma_BdRingGetIrq(dma->ring);
    XAxiDma_BdRingAckIrq(dma->ring, irq_status);
    if ((irq_status & XAXIDMA_IRQ_ERROR_MASK) != 0u) {
        dma->error = 1u;
    }
    if ((irq_status & (XAXIDMA_IRQ_IOC_MASK | XAXIDMA_IRQ_DELAY_MASK)) != 0u) {
        dma->ioc_count++;
    }
}
