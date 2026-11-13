/*
 * SpMM accelerator register map for the PULP cluster HWPE port.
 *
 * The standard HWPE-CTRL registers below follow hwpe/redmule.
 * The job-dependent offsets (0x20..0x38) match rtl/spmm_regif.rdl.
 * The selector field matches cluster_control_unit (HWPE_SEL at [15:13]).
 * All SpMM-specific fields can be overridden with -D... if necessary.
 *
 * SPDX-License-Identifier: Apache-2.0
 */
#ifndef ARCHI_SPMM_H
#define ARCHI_SPMM_H

/* ARCHI_HWCE_ADDR and ARCHI_CLUSTER_CTRL_ADDR come from <pulp.h>. */
#ifndef SPMM_HWPE_BASE
#define SPMM_HWPE_BASE ARCHI_HWCE_ADDR
#endif
#ifndef SPMM_CLUSTER_CTRL_BASE
#define SPMM_CLUSTER_CTRL_BASE ARCHI_CLUSTER_CTRL_ADDR
#endif

#define SPMM_HWPE_TRIGGER_OFFSET       0x00
#define SPMM_HWPE_ACQUIRE_OFFSET       0x04
#define SPMM_HWPE_AUTOTRIGGER_OFFSET   0x08
#define SPMM_HWPE_STATUS_OFFSET        0x0c
#define SPMM_HWPE_RUNNING_JOB_OFFSET   0x10
#define SPMM_HWPE_SOFT_CLEAR_OFFSET    0x14

#ifndef SPMM_JOB_REG_OFFSET
#define SPMM_JOB_REG_OFFSET           0x20
#endif
#define SPMM_ROW_PTR_ADDR_OFFSET       (SPMM_JOB_REG_OFFSET + 0x00)
#define SPMM_B_ADDR_OFFSET             (SPMM_JOB_REG_OFFSET + 0x04)
#define SPMM_A_ADDR_OFFSET             (SPMM_JOB_REG_OFFSET + 0x08)
#define SPMM_COL_ID_ADDR_OFFSET        (SPMM_JOB_REG_OFFSET + 0x0c)
#define SPMM_C_ADDR_OFFSET             (SPMM_JOB_REG_OFFSET + 0x10)
#define SPMM_LENGTHS_OFFSET            (SPMM_JOB_REG_OFFSET + 0x14)

/* SystemRDL LENGTHS: [15:0] row_ptr 128-bit words,
 * [31:16] dense-B 128-bit words. */
#ifndef SPMM_PACK_LENGTHS
#define SPMM_PACK_LENGTHS(row_ptr_words, b_words) \
  ((((uint32_t)(b_words)) << 16) | (uint32_t)(row_ptr_words))
#endif

#ifndef SPMM_CLUST_CTRL_HWPE_EN_OFFSET
#define SPMM_CLUST_CTRL_HWPE_EN_OFFSET 0x18
#endif
#ifndef SPMM_CLUST_CTRL_HWPE_EN_MASK
#define SPMM_CLUST_CTRL_HWPE_EN_MASK   0x800u
#endif
/* The current PULP cluster control register places the 3-bit HWPE_SEL field at bits [15:13].
 * The preserved HWPE order is SoftEx=0, Neureka=1, RedMulE=2, SpMM=3. */
#ifndef SPMM_HWPE_INDEX
#define SPMM_HWPE_INDEX                3u
#endif
#ifndef SPMM_CLUST_CTRL_HWPE_SEL_SHIFT
#define SPMM_CLUST_CTRL_HWPE_SEL_SHIFT 13u
#endif
#ifndef SPMM_CLUST_CTRL_HWPE_SEL_MASK
#define SPMM_CLUST_CTRL_HWPE_SEL_MASK  (0x7u << SPMM_CLUST_CTRL_HWPE_SEL_SHIFT)
#endif
#ifndef SPMM_HWPE_EVENT
#define SPMM_HWPE_EVENT                 12
#endif

#ifndef SPMM_TEST_RESULT_ADDR
#define SPMM_TEST_RESULT_ADDR         0x1A1040A0u
#endif

/* PULP HWPE traffic normally accesses L1/TCDM. */
#ifndef SPMM_BUFFER_ALIGNMENT
#define SPMM_BUFFER_ALIGNMENT         16u
#endif

/* Optional polling timeout (number of MMIO reads). */
#ifndef SPMM_POLL_TIMEOUT
#define SPMM_POLL_TIMEOUT             1000000u
#endif

#endif /* ARCHI_SPMM_H */
