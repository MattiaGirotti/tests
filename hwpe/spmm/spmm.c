// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// SpMM Software Driver / Main Application

#include <stdint.h>
#include <stdio.h>

#include "archi_spmm.h"
#include "hal_spmm.h"
#include "spmm_utils.h"
#include "tinyprintf.h"

#include "tensor_dim.h"
#include "golden.h"
#include "row_ptr_input.h"
#include "col_id_input.h"
#include "a_input.h"
#include "b_input.h"

#define ERR 0x0000

// Buffer di destinazione in memoria dove l'acceleratore scriverà la matrice C
// (La dimensione copre il numero di righe per il numero di colonne/canali di B)
#define C_BUFFER_SIZE (NUM_ROWS * 4)
volatile int c_result[C_BUFFER_SIZE];

int main() {
  uint16_t num_rows = NUM_ROWS;
  uint16_t num_cols = NUM_COLS;

  // Puntatori ai vettori CSR e matrici in memoria
  uint32_t row_ptr_addr = (uint32_t)row_ptr_inp;
  uint32_t b_addr       = (uint32_t)b_inp;
  uint32_t a_addr       = (uint32_t)a_inp;
  uint32_t col_id_addr  = (uint32_t)col_id_inp;
  uint32_t c_addr       = (uint32_t)c_result; // Indirizzo del buffer di destinazione C

  volatile int errors = 0;
  int offload_id_tmp;

  // 1. Abilitazione del clock gate dell'acceleratore HWPE
  hwpe_cg_enable();

  // 2. Pulizia software dell'acceleratore
  hwpe_soft_clear();

  // 3. Acquisizione del job dall'infrastruttura HWPE/HCI
  while ((offload_id_tmp = hwpe_acquire_job()) < 0);

  // 4. Configurazione dei registri dell'acceleratore SpMM
  spmm_cfg(row_ptr_addr, b_addr, a_addr, col_id_addr, c_addr, num_rows, num_cols);

  // 5. Trigger dell'acceleratore e attesa (wfi) della fine calcolo
  printf("Triggering SpMM accelerator and going to sleep...\n");
  hwpe_trigger_job();

  asm volatile("wfi" ::: "memory");

  // Al risveglio, la computazione è completata
  printf("Resumed! Accelerator execution finished.\n");

  // 6. Disabilitazione del clock gate
  hwpe_cg_disable();

  // 7. Validazione dei risultati confrontando il buffer C calcolato con il golden model
  errors = spmm_compare_int((uint32_t *)c_addr, golden, C_BUFFER_SIZE, ERR);

  // Scrittura degli errori sul registro di terminazione monitorato dal testbench
  *(int *)0x80000000 = errors;

  tfp_printf("Terminated SpMM test with %d errors. See you!\n", errors);

  return errors;
}