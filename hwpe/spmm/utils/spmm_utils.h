/* SpMM validation helpers. SPDX-License-Identifier: Apache-2.0 */
#ifndef SPMM_UTILS_H
#define SPMM_UTILS_H

#include <stdint.h>
#include <stdio.h>
#include "../inc/tensor_dim.h"

#if SPMM_OUT_BITS != 8
#error "This SpMM test requires INT8 output"
#endif

typedef int8_t spmm_output_t;
typedef int8_t spmm_golden_t;

/* The software-side full CSR uses uint16_t row pointers. */
static inline int spmm_check_csr(const uint16_t *ptr, const uint8_t *col)
{
    unsigned int errors = 0;
    unsigned int r, i;

    if (ptr[0] != 0u || ptr[SPMM_M] != TOTAL_NNZ) {
        printf("[SPMM] Invalid CSR boundaries: first=%u last=%u NNZ=%u\n",
               (unsigned)ptr[0], (unsigned)ptr[SPMM_M], (unsigned)TOTAL_NNZ);
        return 1;
    }

    for (r = 0; r < SPMM_M; ++r) {
        if (ptr[r] > ptr[r + 1] || ptr[r + 1] > TOTAL_NNZ) {
            printf("[SPMM] Invalid row_ptr at row %u\n", r);
            return 1;
        }
    }

    for (i = 0; i < TOTAL_NNZ; ++i) {
        if (col[i] >= SPMM_K) {
            if (errors < 8u)
                printf("[SPMM] col_id[%u]=%u out of bounds\n", i, (unsigned)col[i]);
            ++errors;
        }
    }
    return (int)errors;
}

static inline int spmm_compare(
    const volatile spmm_output_t *actual,
    const spmm_golden_t *expected,
    unsigned int count)
{
    unsigned int errors = 0;
    for (unsigned int i = 0; i < count; ++i) {
        const int32_t got = (int32_t)actual[i];
        const int32_t exp = (int32_t)expected[i];
        if (got != exp) {
            if (errors < 16u) {
                printf("[SPMM] C[%u,%u]: expected=%ld got=%ld\n",
                       (unsigned)(i / SPMM_N), (unsigned)(i % SPMM_N),
                       (long)exp, (long)got);
            }
            ++errors;
        }
    }
    return (int)errors;
}

#endif /* SPMM_UTILS_H */
