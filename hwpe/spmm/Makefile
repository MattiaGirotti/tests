# PULP cluster HWPE regression test (same SDK invocation as hwpe/redmule).
PULP_APP = test
PULP_APP_SRCS = spmm.c
PULP_CFLAGS += -O3

ifeq ($(use_dma),1)
  PULP_CFLAGS += -DUSE_DMA
endif
ifeq ($(no_ecc),1)
  PULP_CFLAGS += -DNO_ECC
endif
ifeq ($(polling),1)
  PULP_CFLAGS += -DSPMM_USE_POLLING -DSPMM_POLL_TIMEOUT=$(poll_timeout)
endif
poll_timeout ?= 10000000

ifeq ($(fault_inject),1)
  export FAULT_INJECTION=1
  export FAULT_INJECTION_SCRIPT=$(CURDIR)/pulp_inject_fault.tcl
endif
ifeq ($(multi_bit_upset),1)
  export MULTI_BIT_UPSET=1
else
  export MULTI_BIT_UPSET=0
endif

# make golden / check-golden can be run on any host without a PULP SDK.
ifneq ($(filter-out golden check-golden,$(MAKECMDGOALS)),)
  include $(PULP_SDK_HOME)/install/rules/pulp_rt.mk
else ifeq ($(MAKECMDGOALS),)
  include $(PULP_SDK_HOME)/install/rules/pulp_rt.mk
endif

PYTHON ?= python3
M ?= 16
K ?= 16
N ?= 4
SPARSITY ?= 0.60
SEED ?= 42
OUT_BITS ?= 32
EMPTY_ROW ?= -1

.PHONY: golden check-golden
# M = rows of A, K = columns of A / rows of B, N = columns of B.
golden:
	$(PYTHON) spmm-golden-model/golden_model_spmm.py --rows $(M) --cols $(K) --b-cols $(N) --sparsity $(SPARSITY) --seed $(SEED) --out-bits $(OUT_BITS) --empty-row $(EMPTY_ROW) --output-dir inc

check-golden:
	$(PYTHON) spmm-golden-model/golden_model_spmm.py --self-test
