// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// SpMM Architectural Definitions (basato sul mapping hwpe-ctrl e top_module_tb.sv)

#ifndef ARCHI_SPMM_H
#define ARCHI_SPMM_H

// --- 1. INDIRIZZI BASE DELLA PERIFERICA HWPE ---
// (L'indirizzo base standard assegnato alla periferica di controllo nel cluster PULP)
#ifndef SPMM_PERIPH_BASE
#define SPMM_PERIPH_BASE 0x00100000
#endif

// --- 2. REGISTRI STANDARD HWPE-CTRL ---
#define SPMM_HWPE_COMMIT_OFFSET     0x00
#define SPMM_HWPE_TRIGGER_OFFSET    0x04
#define SPMM_HWPE_JOB_START_OFFSET  0x08
#define SPMM_HWPE_STATUS_OFFSET     0x0C  // Usato per il polling di fine elaborazione
#define SPMM_HWPE_SOFT_CLEAR_OFFSET 0x10
#define SPMM_HWPE_CG_OFFSET         0x14

// --- 3. REGISTRI CUSTOM DELLO SPMM (Offset basati su top_module_tb.sv) ---
#define SPMM_ROW_PTR_ADDR_OFFSET    0x20  // Indirizzo base in TCDM per row_ptr[cite: 6]
#define SPMM_B_ADDR_OFFSET          0x24  // Indirizzo base in TCDM per la matrice densa B[cite: 6]
#define SPMM_A_ADDR_OFFSET          0x28  // Indirizzo base in TCDM per i valori non-zero A[cite: 6]
#define SPMM_COL_ID_ADDR_OFFSET     0x2C  // Indirizzo base in TCDM per gli indici di colonna[cite: 6]
#define SPMM_C_ADDR_OFFSET          0x30  // Indirizzo base in TCDM per il risultato C[cite: 6]
#define SPMM_LENGTHS_OFFSET         0x34  // Parametri dimensionali (es. num_rows, num_cols)[cite: 6]

// --- 4. MACRO DI COMODO PER IL PUNTAMENTO DEI REGISTRI ---
#define SPMM_REG_ADDR(base, offset) ((volatile uint32_t *)((base) + (offset)))

#endif // ARCHI_SPMM_H