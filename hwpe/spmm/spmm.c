
/*
 * PULP cluster regression test for the fixed 16x16 CSR SpMM instance.
 * C[16x16] = A[16x16] (CSR INT8) x B[16x16] (dense INT8).
 * The current RTL has 16 MAC lanes and an 8-bit wrapping accumulator/output.
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

#if (SPMM_M != 16u) || (SPMM_K != 16u) || (SPMM_N != 16u)
#error "This regression matches the current RTL instance: M=K=N=16"
#endif

#if SPMM_OUT_BITS != 8
#error "The current SpMM RTL instance exports 8-bit accumulator results"
#endif

#if TOTAL_NNZ > 255u
#error "The current RTL stores CSR row pointers in 8 bits (TOTAL_NNZ <= 255)"
#endif


/* ================================================================
 * L1 allocation with alignment
 * ================================================================ */

static void *spmm_l1_alloc(size_t size)
{
    const size_t align = (size_t)SPMM_BUFFER_ALIGNMENT;

    void *raw = pi_l1_malloc(0, (int)(size + align - 1u));

    uintptr_t addr;

    if (!raw)
        return NULL;

    addr = ((uintptr_t)raw + align - 1u) &
           ~(uintptr_t)(align - 1u);

    return (void *)addr;
}


/* ================================================================
 * Copy data to L1
 * ================================================================ */

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


/* ================================================================
 * Main
 * ================================================================ */

int main(void)
{
    /* Execute the test only on cluster 0 */
    if (rt_cluster_id() != 0)
        return bench_cluster_forward(0);

    int errors = 0;

    if (get_core_id() == 0)
    {
        /* --------------------------------------------------------
         * Buffer declarations
         * -------------------------------------------------------- */

        uint8_t *row_ptr = NULL;
        uint8_t *col_id  = NULL;

        int8_t *a = NULL;
        int8_t *b = NULL;

        volatile spmm_output_t *c = NULL;

        int32_t job_id = -1;

        int completed = 0;


        /* --------------------------------------------------------
         * Allocate buffers in L1/TCDM
         *
         * Buffers are aligned because the streamer fetches
         * complete 128-bit beats.
         * -------------------------------------------------------- */

        row_ptr = (uint8_t *)spmm_l1_alloc(sizeof(row_ptr_inp));

        col_id = (uint8_t *)spmm_l1_alloc(sizeof(col_id_inp));

        a = (int8_t *)spmm_l1_alloc(sizeof(a_inp));

        b = (int8_t *)spmm_l1_alloc(sizeof(b_inp));

        c = (volatile spmm_output_t *)spmm_l1_alloc(
                SPMM_M * SPMM_N * sizeof(spmm_output_t)
            );


        /* --------------------------------------------------------
         * Check allocation
         * -------------------------------------------------------- */

        if (!row_ptr || !col_id || !a || !b || !c)
        {
            printf("[SPMM] Not enough L1/TCDM memory\n");

            errors = 1;
        }
        else
        {
            /* ----------------------------------------------------
             * Copy input data to L1
             * ---------------------------------------------------- */

            spmm_copy_to_l1(
                row_ptr,
                row_ptr_inp,
                sizeof(row_ptr_inp)
            );

            spmm_copy_to_l1(
                col_id,
                col_id_inp,
                sizeof(col_id_inp)
            );

            spmm_copy_to_l1(
                a,
                a_inp,
                sizeof(a_inp)
            );

            spmm_copy_to_l1(
                b,
                b_inp,
                sizeof(b_inp)
            );


            /* ----------------------------------------------------
             * Check CSR structure
             * ---------------------------------------------------- */

            errors = spmm_check_csr(row_ptr, col_id);


            if (errors == 0)
            {
                /* ------------------------------------------------
                 * Initialize output matrix
                 * ------------------------------------------------ */

                for (unsigned int i = 0;
                     i < SPMM_M * SPMM_N;
                     ++i)
                {
                    c[i] = (spmm_output_t)-91;
                }


                /* ------------------------------------------------
                 * Enable and select HWPE
                 * ------------------------------------------------ */

                printf("[SPMM] Selecting HWPE index %u\n",
                       (unsigned)SPMM_HWPE_INDEX);

                spmm_cg_enable();


                /* Read cluster control register */

                uint32_t cluster_ctrl =
                    *(volatile uint32_t *)(uintptr_t)
                    (
                        SPMM_CLUSTER_CTRL_BASE +
                        SPMM_CLUST_CTRL_HWPE_EN_OFFSET
                    );


                printf("[SPMM] CLUSTER_CTRL = 0x%08lx\n",
                       (unsigned long)cluster_ctrl);

                printf("[SPMM] HWPE_EN = %lu, HWPE_SEL = %lu\n",
                       (unsigned long)((cluster_ctrl >> 11) & 0x1u),
                       (unsigned long)((cluster_ctrl >> 13) & 0x7u));


                __asm__ volatile("" ::: "memory");


                /* ------------------------------------------------
                 * Soft clear HWPE
                 * ------------------------------------------------ */

                spmm_soft_clear();

                __asm__ volatile("" ::: "memory");


                /* ------------------------------------------------
                 * Acquire HWPE job
                 * ------------------------------------------------ */

                printf("[SPMM] Waiting for ACQUIRE\n");

                job_id = spmm_wait_acquire();


                if (job_id < 0)
                {
                    printf("[SPMM] ACQUIRE timeout (status=0x%08lx)\n",
                           (unsigned long)spmm_status());

                    errors = 1;
                }
                else
                {
                    printf("[SPMM] job=%ld M=%u K=%u N=%u nnz=%u "
                           "row_ptr_words=%u b_words=%u\n",

                           (long)job_id,

                           (unsigned)SPMM_M,
                           (unsigned)SPMM_K,
                           (unsigned)SPMM_N,

                           (unsigned)TOTAL_NNZ,

                           (unsigned)SPMM_ROW_PTR_WORDS,
                           (unsigned)SPMM_B_WORDS);


                    /* ------------------------------------------------
                     * Print actual L1 buffer addresses
                     * ------------------------------------------------ */

                    printf("[SPMM] PTR ROW_PTR = 0x%08lx\n",
                           (unsigned long)(uintptr_t)row_ptr);

                    printf("[SPMM] PTR B       = 0x%08lx\n",
                           (unsigned long)(uintptr_t)b);

                    printf("[SPMM] PTR A       = 0x%08lx\n",
                           (unsigned long)(uintptr_t)a);

                    printf("[SPMM] PTR COL_ID  = 0x%08lx\n",
                           (unsigned long)(uintptr_t)col_id);

                    printf("[SPMM] PTR C       = 0x%08lx\n",
                           (unsigned long)(uintptr_t)c);


                    /* ------------------------------------------------
                     * Configure SpMM registers
                     *
                     * 0x20 = ROW_PTR_ADDR
                     * 0x24 = B_ADDR
                     * 0x28 = A_ADDR
                     * 0x2C = COL_ID_ADDR
                     * 0x30 = C_ADDR
                     * 0x34 = LENGTHS
                     *
                     * LENGTHS[15:0]  = row_ptr words
                     * LENGTHS[31:16] = B words
                     * ------------------------------------------------ */

                    printf("[SPMM] Configuring HWPE registers\n");

                    spmm_configure(
                        row_ptr,
                        b,
                        a,
                        col_id,
                        c,
                        SPMM_ROW_PTR_WORDS,
                        SPMM_B_WORDS
                    );


                    __asm__ volatile("" ::: "memory");


                    /* ------------------------------------------------
                     * Read back configured registers
                     * ------------------------------------------------ */

                    printf("[SPMM] REG ROW_PTR = 0x%08lx\n",
                           (unsigned long)spmm_reg_read(0x20));

                    printf("[SPMM] REG B       = 0x%08lx\n",
                           (unsigned long)spmm_reg_read(0x24));

                    printf("[SPMM] REG A       = 0x%08lx\n",
                           (unsigned long)spmm_reg_read(0x28));

                    printf("[SPMM] REG COL_ID  = 0x%08lx\n",
                           (unsigned long)spmm_reg_read(0x2C));

                    printf("[SPMM] REG C       = 0x%08lx\n",
                           (unsigned long)spmm_reg_read(0x30));

                    printf("[SPMM] REG LENGTHS = 0x%08lx\n",
                           (unsigned long)spmm_reg_read(0x34));


                    __asm__ volatile("" ::: "memory");


                    /* ------------------------------------------------
                     * Trigger SpMM job
                     * ------------------------------------------------ */

                    printf("[SPMM] Triggering job\n");

                    spmm_trigger_job();


                    /*printf("[SPMM] Trigger write returned: "
                           "STATUS=0x%08lx RUNNING_JOB=0x%08lx\n",

                           (unsigned long)spmm_status(),

                           (unsigned long)spmm_running_job());


                    /* ------------------------------------------------
                     * Wait for completion
                     * ------------------------------------------------ */

                    if (spmm_wait_done() != 0)
                    {
                        printf("[SPMM] HWPE timeout "
                               "(status=0x%08lx running_job=0x%08lx)\n",

                               (unsigned long)spmm_status(),

                               (unsigned long)spmm_running_job());

                        errors = 1;
                    }
                    else
                    {
                        completed = 1;

                        printf("[SPMM] HWPE completed\n");
                    }
                }


                /* ------------------------------------------------
                 * Disable clock and compare results
                 *
                 * Do not gate the clock on timeout because the
                 * accelerator may still be accessing TCDM.
                 * ------------------------------------------------ */

                if (completed)
                {
                    spmm_cg_disable();

                    __asm__ volatile("" ::: "memory");

                    errors += spmm_compare(
                        c,
                        golden,
                        SPMM_M * SPMM_N
                    );
                }
            }
        }


        /* --------------------------------------------------------
         * Communicate test result to testbench
         * -------------------------------------------------------- */

        *(volatile uint32_t *)(uintptr_t)SPMM_TEST_RESULT_ADDR =
            (uint32_t)errors;


        if (errors)
        {
            printf("[KO] SPMM TEST FAIL: %d mismatch(es)\n",
                   errors);
        }
        else
        {
            printf("[OK] SPMM TEST SUCCESS: %d mismatch(es)\n",
                   errors);
        }
    }


    /* ------------------------------------------------------------
     * Synchronize cluster cores
     * ------------------------------------------------------------ */

    synch_barrier();

    return errors != 0;
}
