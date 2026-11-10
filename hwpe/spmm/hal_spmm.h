/*
 * SpMM HWPE-CTRL HAL, following the protocol used by hwpe/redmule.
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef HAL_SPMM_H
#define HAL_SPMM_H

#include <stdint.h>
#include <pulp.h>
#include "archi_spmm.h"

/* Each access is MMIO relative to the PULP cluster HWPE base. */
static inline void spmm_reg_write(uint32_t offset, uint32_t value)
{
  *(volatile uint32_t *)(uintptr_t)(SPMM_HWPE_BASE + offset) = value;
}

static inline uint32_t spmm_reg_read(uint32_t offset)
{
  return *(volatile uint32_t *)(uintptr_t)(SPMM_HWPE_BASE + offset);
}

static inline void spmm_cg_enable(void)
{
  volatile uint32_t *ctrl = (volatile uint32_t *)(uintptr_t)
      (SPMM_CLUSTER_CTRL_BASE + SPMM_CLUST_CTRL_HWPE_EN_OFFSET);
  *ctrl |= SPMM_CLUST_CTRL_HWPE_EN_MASK;
}

static inline void spmm_cg_disable(void)
{
  volatile uint32_t *ctrl = (volatile uint32_t *)(uintptr_t)
      (SPMM_CLUSTER_CTRL_BASE + SPMM_CLUST_CTRL_HWPE_EN_OFFSET);
  *ctrl &= ~SPMM_CLUST_CTRL_HWPE_EN_MASK;
}

static inline void spmm_soft_clear(void)
{
  /* HWPE-CTRL performs the clear on a write of 0 (as in RedMulE). */
  spmm_reg_write(SPMM_HWPE_SOFT_CLEAR_OFFSET, 0);
}

static inline int32_t spmm_acquire_job(void)
{
  /* Negative means that no job context is currently available. */
  return (int32_t)spmm_reg_read(SPMM_HWPE_ACQUIRE_OFFSET);
}

static inline int32_t spmm_wait_acquire(void)
{
  int32_t job_id;
  do {
    job_id = spmm_acquire_job();
    if (job_id < 0) {
#ifndef SPMM_USE_POLLING
      eu_evt_maskWaitAndClr(1u << SPMM_HWPE_EVENT);
#endif
    }
  } while (job_id < 0);
  return job_id;
}

static inline void spmm_configure(uint32_t row_ptr, uint32_t b,
                                   uint32_t a, uint32_t col_id,
                                   uint32_t c, uint16_t m, uint16_t k)
{
  spmm_reg_write(SPMM_ROW_PTR_ADDR_OFFSET, row_ptr);
  spmm_reg_write(SPMM_B_ADDR_OFFSET, b);
  spmm_reg_write(SPMM_A_ADDR_OFFSET, a);
  spmm_reg_write(SPMM_COL_ID_ADDR_OFFSET, col_id);
  spmm_reg_write(SPMM_C_ADDR_OFFSET, c);
  spmm_reg_write(SPMM_LENGTHS_OFFSET, SPMM_PACK_LENGTHS(m, k));
}

static inline void spmm_trigger_job(void)
{
  spmm_reg_write(SPMM_HWPE_TRIGGER_OFFSET, 0);
}

static inline uint32_t spmm_status(void)
{
  return spmm_reg_read(SPMM_HWPE_STATUS_OFFSET);
}

static inline int spmm_wait_done(void)
{
#ifdef SPMM_USE_POLLING
  /* Polling is useful during bring-up when IRQ/event routing is not ready. */
  uint32_t timeout = (uint32_t)SPMM_POLL_TIMEOUT;
  while (spmm_status() != 0u) {
    if (timeout-- == 0u) return -1;
  }
#else
  /* RedMulE-style event wait. Event 12 must be routed from SpMM. */
  do {
    eu_evt_maskWaitAndClr(1u << SPMM_HWPE_EVENT);
  } while (spmm_status() != 0u);
#endif
  return 0;
}

/* Optional MCHAN copy from L2 to TCDM, adapted from RedMulE. */
#ifdef USE_DMA
#define SPMM_DMA_COMMAND_QUEUE    ARCHI_MCHAN_DEMUX_ADDR
#define SPMM_DMA_STATUS_REGISTER (ARCHI_MCHAN_DEMUX_ADDR + 4)
static inline uint32_t spmm_dma_alloc(void)
{
  return *(volatile uint32_t *)(uintptr_t)SPMM_DMA_COMMAND_QUEUE;
}

static inline void spmm_dma_copy_to_l1(uint32_t bytes,
                                        const void *src, void *dst)
{
  /* RX=bit17, increment=bit18, interrupt=bits20/21; like RedMulE. */
  const uint32_t cmd = bytes | (1u << 17) | (1u << 18) | (1u << 20) | (1u << 21);
  *(volatile uint32_t *)(uintptr_t)SPMM_DMA_COMMAND_QUEUE = cmd;
  *(volatile uint32_t *)(uintptr_t)SPMM_DMA_COMMAND_QUEUE = (uint32_t)(uintptr_t)dst;
  *(volatile uint32_t *)(uintptr_t)SPMM_DMA_COMMAND_QUEUE = (uint32_t)(uintptr_t)src;
}

static inline void spmm_dma_wait(uint32_t id)
{
  while ((*(volatile uint32_t *)(uintptr_t)SPMM_DMA_STATUS_REGISTER >> id) & 1u) {
    /* Poll rather than relying on a DMA event mask specific to the cluster. */
  }
  *(volatile uint32_t *)(uintptr_t)SPMM_DMA_STATUS_REGISTER = 1u << id;
}
#endif

#endif /* HAL_SPMM_H */
