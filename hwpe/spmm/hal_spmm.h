// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// SpMM Hardware Abstraction Layer (HAL)

#ifndef HAL_SPMM_H
#define HAL_SPMM_H

#include <stdint.h>
#include "archi_spmm.h"

// Funzione di utilità inline per scrivere su un registro della periferica
static inline void spmm_write_reg(uint32_t base, uint32_t offset, uint32_t value) {
    *((volatile uint32_t *)(base + offset)) = value;
}
/*static inline void spmm_write_reg(uint32_t base, uint32_t offset, uint32_t value) {
    // Forza il bit 20 a 1 sull'indirizzo base per essere immuni a configurazioni errate
    uint32_t absolute_addr = (base | 0x00100000) + offset;
    *((volatile uint32_t *)absolute_addr) = value;
}*/

// Funzione di utilità inline per leggere da un registro della periferica
static inline uint32_t spmm_read_reg(uint32_t base, uint32_t offset) {
    return *((volatile uint32_t *)(base + offset));
}

// Abilitazione del clock gating dell'acceleratore HWPE
static inline void hwpe_cg_enable(void) {
    spmm_write_reg(SPMM_PERIPH_BASE, SPMM_HWPE_CG_OFFSET, 1);
}

// Disabilitazione del clock gating
static inline void hwpe_cg_disable(void) {
    spmm_write_reg(SPMM_PERIPH_BASE, SPMM_HWPE_CG_OFFSET, 0);
}

// Reset software dell'acceleratore
static inline void hwpe_soft_clear(void) {
    spmm_write_reg(SPMM_PERIPH_BASE, SPMM_HWPE_SOFT_CLEAR_OFFSET, 1);
}

// Acquisizione del job tramite l'infrastruttura HWPE/HCI
static inline int hwpe_acquire_job(void) {
    // Ritorna 0 per indicare che il job è pronto/acquisito nel contesto di simulazione
    return 0; 
}

/*// Trigger dell'acceleratore per avviare l'elaborazione (scrittura su commit/trigger)
static inline void hwpe_trigger_job(void) {
    spmm_write_reg(SPMM_PERIPH_BASE, SPMM_HWPE_TRIGGER_OFFSET, 0);
}*/
// Trigger dell'acceleratore per avviare l'elaborazione (scrittura su commit_trigger @ 0x00)
static inline void hwpe_trigger_job(void) {
    // Scrive 0 su commit_trigger (offset 0x00) per committare il job e far partire l'esecuzione
    spmm_write_reg(0x00, 0x00, 0);
}

// Funzione di configurazione principale dello SpMM
// Mappa i puntatori TCDM e i parametri dimensionali esattamente come atteso dall'hardware
static inline void spmm_cfg(uint32_t row_ptr_addr, 
                            uint32_t b_addr, 
                            uint32_t a_addr, 
                            uint32_t col_id_addr, 
                            uint32_t c_addr, 
                            uint16_t num_rows, 
                            uint16_t num_cols) {
    
    // Scrittura dei puntatori base dei buffer in memoria TCDM
    spmm_write_reg(SPMM_PERIPH_BASE, SPMM_ROW_PTR_ADDR_OFFSET, row_ptr_addr);
    spmm_write_reg(SPMM_PERIPH_BASE, SPMM_B_ADDR_OFFSET,       b_addr);
    spmm_write_reg(SPMM_PERIPH_BASE, SPMM_A_ADDR_OFFSET,       a_addr);
    spmm_write_reg(SPMM_PERIPH_BASE, SPMM_COL_ID_ADDR_OFFSET,  col_id_addr);
    spmm_write_reg(SPMM_PERIPH_BASE, SPMM_C_ADDR_OFFSET,       c_addr);
    
    // Scrittura dei parametri dimensionali (es. pack di num_rows e num_cols nel registro lengths)
    uint32_t lengths = ((uint32_t)num_rows << 16) | (num_cols & 0xFFFF);
    spmm_write_reg(SPMM_PERIPH_BASE, SPMM_LENGTHS_OFFSET,      lengths);
}

#endif // HAL_SPMM_H