/*
 * SpMM PULP cluster regression test, modeled on hwpe/redmule/redmule.c.
 * C[M,N] = A[M,K] (CSR signed INT8) * B[K,N] (dense signed INT8).
 * The SpMM accelerator's C format is selected with SPMM_OUT_BITS.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <string.h>
#include <pulp.h>

#include "archi_spmm.h"
#include "hal_spmm.h"
#include "inc/tensor_dim.h"
#include "inc/row_ptr_input.h"
#include "inc/col_id_input.h"
#include "inc/a_input.h"
#include "inc/b_input.h"
#include "inc/golden.h"
#include "utils/spmm_utils.h"

static void *spmm_l1_alloc(size_t size)
{
  /* The normal L1 allocator can return only word alignment; ensure 16B. */
  const size_t align = (size_t)SPMM_BUFFER_ALIGNMENT;
  void *raw = pi_l1_malloc(0, (int)(size + align - 1u));
  uintptr_t addr;
  if (!raw) return NULL;
  addr = ((uintptr_t)raw + align - 1u) & ~(uintptr_t)(align - 1u);
  return (void *)addr;
}

static void spmm_copy_to_l1(void *dst, const void *src, uint32_t bytes)
{
#ifdef USE_DMA
  const uint32_t id = spmm_dma_alloc();
  spmm_dma_copy_to_l1(bytes, src, dst);
  spmm_dma_wait(id);
#else
  memcpy(dst, src, bytes);
#endif
}

int main(void)
{
  /* The regression can run with multiple cores/clusters; only core 0 drives HWPE. */
  if (rt_cluster_id() != 0)
    return bench_cluster_forward(0);

  int errors = 0;
  if (get_core_id() == 0) {
    uint32_t *row_ptr;
    uint8_t *col_id;
    int8_t *a;
    int8_t *b;
    volatile spmm_output_t *c;
    int job_id;

    /* Generated stimuli live in L2. The HWPE master must access L1/TCDM. */
    row_ptr = (uint32_t *)spmm_l1_alloc((SPMM_M + 1u) * sizeof(uint32_t));
    col_id  = (uint8_t *)spmm_l1_alloc((TOTAL_NNZ ? TOTAL_NNZ : 1u) * sizeof(uint8_t));
    a       = (int8_t *)spmm_l1_alloc((TOTAL_NNZ ? TOTAL_NNZ : 1u) * sizeof(int8_t));
    b       = (int8_t *)spmm_l1_alloc(SPMM_K * SPMM_N * sizeof(int8_t));
    c       = (volatile spmm_output_t *)spmm_l1_alloc(SPMM_M * SPMM_N * sizeof(spmm_output_t));

    if (!row_ptr || !col_id || !a || !b || !c) {
      printf("[SPMM] Not enough L1/TCDM memory\n");
      errors = 1;
    } else {
      spmm_copy_to_l1(row_ptr, row_ptr_inp, sizeof(row_ptr_inp));
      spmm_copy_to_l1(col_id, col_id_inp, sizeof(col_id_inp));
      spmm_copy_to_l1(a, a_inp, sizeof(a_inp));
      spmm_copy_to_l1(b, b_inp, sizeof(b_inp));

      errors = spmm_check_csr(row_ptr, col_id);
      if (errors == 0) {
        /* Sentinel prevents an unmodified output buffer from passing. */
        unsigned int idx;
        for (idx = 0; idx < SPMM_M * SPMM_N; ++idx)
          c[idx] = (spmm_output_t)-91;

        spmm_cg_enable();
        asm volatile("" ::: "memory");
        spmm_soft_clear();
        asm volatile("" ::: "memory");

        job_id = (int)spmm_wait_acquire();
        printf("[SPMM] job=%d M=%u K=%u N=%u nnz=%u out_bits=%u\n",
               job_id, (unsigned)SPMM_M, (unsigned)SPMM_K,
               (unsigned)SPMM_N, (unsigned)TOTAL_NNZ, (unsigned)SPMM_OUT_BITS);

        /* Addresses are 32-bit RV32 cluster addresses, not host pointers. */
        spmm_configure((uint32_t)(uintptr_t)row_ptr,
                       (uint32_t)(uintptr_t)b,
                       (uint32_t)(uintptr_t)a,
                       (uint32_t)(uintptr_t)col_id,
                       (uint32_t)(uintptr_t)c,
                       (uint16_t)SPMM_M, (uint16_t)SPMM_K);
        asm volatile("" ::: "memory");
        spmm_trigger_job();
        if (spmm_wait_done() != 0) {
          printf("[SPMM] HWPE timeout (status=0x%08lx)\n", (unsigned long)spmm_status());
          errors = 1;
        }

        spmm_cg_disable();
        asm volatile("" ::: "memory");
        if (!errors)
          errors = spmm_compare(c, golden, SPMM_M * SPMM_N);
      }
    }

    *(volatile uint32_t *)(uintptr_t)SPMM_TEST_RESULT_ADDR = (uint32_t)errors;
    printf(errors ? "[KO] SPMM TEST FAIL: %d mismatch(es)\n"
                  : "[OK] SPMM TEST SUCCESS: %d mismatch(es)\n", errors);
  }

  synch_barrier();
  return errors != 0;
}
