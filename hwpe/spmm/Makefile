# PULP cluster HWPE regression: square 16/32/64 SpMM with RTL-managed tiling and a 16x16 engine.
PULP_APP = test
PULP_APP_SRCS = spmm.c
PULP_CFLAGS += -O3

ifeq ($(use_dma),1)
  PULP_CFLAGS += -DUSE_DMA
endif
ifeq ($(no_ecc),1)
  PULP_CFLAGS += -DNO_ECC
endif
polling ?= 1
poll_timeout ?= 1000000
ifeq ($(polling),1)
  PULP_CFLAGS += -DSPMM_USE_POLLING -DSPMM_POLL_TIMEOUT=$(poll_timeout)
endif

ifeq ($(fault_inject),1)
  export FAULT_INJECTION=1
  export FAULT_INJECTION_SCRIPT=$(CURDIR)/pulp_inject_fault.tcl
endif
ifeq ($(multi_bit_upset),1)
  export MULTI_BIT_UPSET=1
else
  export MULTI_BIT_UPSET=0
endif

ifneq ($(filter-out golden check-golden,$(MAKECMDGOALS)),)
  include $(PULP_SDK_HOME)/install/rules/pulp_rt.mk
else ifeq ($(MAKECMDGOALS),)
  include $(PULP_SDK_HOME)/install/rules/pulp_rt.mk
endif

PYTHON ?= python3
M ?= 32
K ?= 32
N ?= 32
SPARSITY ?= 0.60
SEED ?= 42
OUT_BITS ?= 8
EMPTY_ROW ?= -1

.PHONY: golden check-golden all
# For this integrated square-matrix HWPE test, use M=K=N=16, 32, or 64.
# The matching hardware parameter SPMM_MATRIX_SIZE in hwpe_subsystem.sv must
# be set to the same value before rebuilding the cluster.
golden:
	$(PYTHON) spmm-golden-model/golden_model_spmm.py --rows $(M) --cols $(K) --b-cols $(N) --sparsity $(SPARSITY) --seed $(SEED) --out-bits $(OUT_BITS) --empty-row $(EMPTY_ROW) --output-dir inc

check-golden:
	$(PYTHON) spmm-golden-model/golden_model_spmm.py --self-test

all: golden
spmm.c: golden
