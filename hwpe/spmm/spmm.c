/*
 * Single-command parametric CSR SpMM regression for an RTL-tiled 16x16 HWPE.
 * Supported square matrix sizes in this delivery: 16, 32, and 64.
 * The CPU supplies the complete matrices once; RTL iterates over the output
 * tile grid and writes the complete C matrix back to TCDM.
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

#if (SPMM_M != SPMM_K) || (SPMM_M != SPMM_N)
#error "This test configuration currently expects square M=K=N matrices"
#endif
#if ((SPMM_M != 16u) && (SPMM_M != 32u) && (SPMM_M != 64u))
#error "This RTL delivery supports square matrix sizes 16, 32, and 64"
#endif
#if ((SPMM_M % SPMM_ENGINE_LANES) != 0) || ((SPMM_N % SPMM_ENGINE_MACS) != 0)
#error "Matrix dimensions must be multiples of the fixed 16x16 engine tile"
#endif
#if SPMM_OUT_BITS != 8
#error "The current SpMM RTL instance exports 8-bit accumulator results"
#endif
#if (SPMM_ENGINE_LANES != 16u) || (SPMM_ENGINE_MACS != 16u)
#error "The RTL engine is fixed to 16 lanes x 16 MACs"
#endif

/* L1/TCDM allocations must be aligned to the 256-bit streamer beat. */
static void *spmm_l1_alloc(size_t size)
{
    const size_t align = (size_t)SPMM_BUFFER_ALIGNMENT;
    void *raw = pi_l1_malloc(0, (int)(size + align - 1u));
    uintptr_t addr;

    if (!raw)
        return NULL;

    addr = ((uintptr_t)raw + align - 1u) & ~(uintptr_t)(align - 1u);
    return (void *)addr;
}

int main(void)
{
    if (rt_cluster_id() != 0)
        return bench_cluster_forward(0);

    int errors = 0;

    if (get_core_id() == 0) {
        uint16_t *row_ptr = NULL;
        uint8_t *col_id = NULL;
        int8_t *a = NULL;
        int8_t *b = NULL;
        volatile spmm_output_t *c = NULL;

        const size_t row_ptr_bytes = (size_t)SPMM_ROW_PTR_WORDS * SPMM_STREAM_WORD_BYTES;
        const size_t nnz_bytes = (size_t)SPMM_NNZ_WORDS * SPMM_STREAM_WORD_BYTES;
        const size_t b_bytes = (size_t)SPMM_B_WORDS * SPMM_STREAM_WORD_BYTES;
        const size_t c_bytes = (size_t)SPMM_M * SPMM_N * sizeof(spmm_output_t);
        int job_timeout = 0;

        row_ptr = (uint16_t *)spmm_l1_alloc(row_ptr_bytes);
        col_id = (uint8_t *)spmm_l1_alloc(nnz_bytes);
        a = (int8_t *)spmm_l1_alloc(nnz_bytes);
        b = (int8_t *)spmm_l1_alloc(b_bytes);
        c = (volatile spmm_output_t *)spmm_l1_alloc(c_bytes);

        if (!row_ptr || !col_id || !a || !b || !c) {
            printf("[SPMM] Not enough L1/TCDM memory for %ux%u hardware-tiled test\n",
                   (unsigned)SPMM_M, (unsigned)SPMM_N);
            errors = 1;
        } else {
            errors = spmm_check_csr(row_ptr_inp, col_id_inp);

            if (errors == 0) {
                /* Copy full, standard-layout matrices to aligned TCDM buffers. */
                memcpy(row_ptr, row_ptr_inp, row_ptr_bytes);
                memcpy(a, a_inp, nnz_bytes);
                memcpy(col_id, col_id_inp, nnz_bytes);
                memcpy(b, b_inp, b_bytes);
                memset((void *)c, 0, c_bytes);

                printf("[SPMM] Hardware tiled test M=%u K=%u N=%u, NNZ=%u\n",
                       (unsigned)SPMM_M, (unsigned)SPMM_K, (unsigned)SPMM_N,
                       (unsigned)TOTAL_NNZ);
                printf("[SPMM] One HWPE job; engine=%u lanes x %u MACs; row_ptr words=%u, B words=%u\n",
                       (unsigned)SPMM_ENGINE_LANES, (unsigned)SPMM_ENGINE_MACS,
                       (unsigned)SPMM_ROW_PTR_WORDS, (unsigned)SPMM_B_WORDS);

                spmm_cg_enable();
                spmm_soft_clear();
                __asm__ volatile("" ::: "memory");

                {
                    const int32_t job_id = spmm_wait_acquire();
                    if (job_id < 0) {
                        printf("[SPMM] ACQUIRE timeout, status=0x%08lx\n",
                               (unsigned long)spmm_status());
                        errors = 1;
                    } else {
                        printf("[SPMM] Submit complete matrices in one job, job=%ld\n",
                               (long)job_id);

                        spmm_configure(
                            row_ptr,
                            b,
                            a,
                            col_id,
                            (const volatile void *)c,
                            (uint16_t)SPMM_ROW_PTR_WORDS,
                            (uint16_t)SPMM_B_WORDS
                        );
                        __asm__ volatile("" ::: "memory");

                        spmm_trigger_job();

                        if (spmm_wait_done() != 0) {
                            printf("[SPMM] HWPE timeout: status=0x%08lx running_job=0x%08lx\n",
                                   (unsigned long)spmm_status(),
                                   (unsigned long)spmm_running_job());
                            errors = 1;
                            job_timeout = 1;
                        }
                    }
                }

                /* Keep the HWPE clock running if the job may still be active. */
                if (!job_timeout)
                    spmm_cg_disable();

                __asm__ volatile("" ::: "memory");

                if (errors == 0) {
                    printf("[SPMM] Hardware completed the %ux%u tile loop; checking C[%ux%u]\n",
                           (unsigned)(SPMM_M / SPMM_TILE_M),
                           (unsigned)(SPMM_N / SPMM_TILE_N),
                           (unsigned)SPMM_M, (unsigned)SPMM_N);
                    errors += spmm_compare(c, golden, SPMM_M * SPMM_N);
                }
            }
        }

        *(volatile uint32_t *)(uintptr_t)SPMM_TEST_RESULT_ADDR = (uint32_t)errors;
        if (errors)
            printf("[KO] SPMM %ux%u HARDWARE-TILED TEST FAIL: %d mismatch/error(s)\n",
                   (unsigned)SPMM_M, (unsigned)SPMM_N, errors);
        else
            printf("[OK] SPMM %ux%u HARDWARE-TILED TEST SUCCESS: 0 mismatch(es)\n",
                   (unsigned)SPMM_M, (unsigned)SPMM_N);
    }

    synch_barrier();
    return errors != 0;
}
