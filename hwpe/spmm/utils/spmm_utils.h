/* SpMM validation helpers. SPDX-License-Identifier: Apache-2.0 */
#ifndef SPMM_UTILS_H
#define SPMM_UTILS_H

#include <stdint.h>
#include <stdio.h>
#include "../inc/tensor_dim.h"

#if SPMM_OUT_BITS == 32
typedef int32_t spmm_output_t;
#elif SPMM_OUT_BITS == 8
typedef int8_t spmm_output_t;
#else
#error "Unsupported SPMM_OUT_BITS: expected 8 or 32"
#endif

/* Check every CSR index and boundary before submitting addresses to HW. */
static inline int spmm_check_csr(const uint8_t *ptr, const uint8_t *col)
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
      if (errors < 8u) printf("[SPMM] col_id[%u]=%u out of bounds\n",
                              i, (unsigned)col[i]);
      ++errors;
    }
  }
  return (int)errors;
}

/* The golden reference is int32, even when the HW produces signed INT8. */
static inline int spmm_compare(const volatile spmm_output_t *actual,
                               const int32_t *expected, unsigned int count)
{
  unsigned int errors = 0;
  unsigned int i;
  for (i = 0; i < count; ++i) {
    const int32_t got = (int32_t)actual[i];
    const int32_t exp = (SPMM_OUT_BITS == 8) ? (int32_t)(int8_t)expected[i]
                                             : expected[i];
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
