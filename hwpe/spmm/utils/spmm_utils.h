// Copyright 2023 ETH Zurich and University of Bologna.
// Licensed under the Apache License, Version 2.0, see LICENSE for details.
// SPDX-License-Identifier: Apache-2.0
//
// SpMM Utilities (Funzioni di supporto e confronto risultati)

#ifndef SPMM_UTILS_H
#define SPMM_UTILS_H

#include <stdint.h>
#include <stdio.h>

// Funzione di utilità per confrontare i risultati calcolati dall'SpMM (matrice C) 
// rispetto al vettore golden generato dal modello Python.
// Restituisce il numero totale di errori riscontrati.
static inline int spmm_compare_int(const uint32_t *computed, const int *golden, int size, int threshold) {
    int errors = 0;
    
    for (int i = 0; i < size; i++) {
        uint32_t comp_val = computed[i];
        uint32_t gold_val = (uint32_t)golden[i];
        
        // Esempio di confronto a livello di parola o elemento impacchettato
        if (comp_val != gold_val) {
            errors++;
            // Se necessario, è possibile abilitare un log di dettaglio:
            // printf("Errore a indice %d: calcolato = 0x%08x, atteso = 0x%08x\n", i, comp_val, gold_val);
        }
    }
    
    return errors;
}

#endif // SPMM_UTILS_H