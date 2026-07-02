# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)
GVSOC_ROOT ?= ../../../../..
TARGET = test
CASE ?= equal
BRIDGE ?= sync_only
PIPELINE_BURST ?= 1
BRIDGE_DEPTH ?= 0
TARGET := $(TARGET):case=$(CASE):bridge=$(BRIDGE):pipeline_burst=$(PIPELINE_BURST):bridge_depth=$(BRIDGE_DEPTH)

# Add the calibration-only RTL bridge subdir to gapy's target-dir search
# so add_sources() / Python imports under rtl_calibration/ resolve.
MODULES := $(CURDIR);$(CURDIR)/rtl_calibration

include $(GVSOC_CORE)/tests/common.mk

# RTL calibration plugins β€” build whichever Verilator plugin the chosen
# BRIDGE needs before the simulator dlopens it. The cdc_*_beh kinds need
# none of these; the cdc_*_rtl kinds each need their own plugin .so.
RTL_PLUGIN_DIR := rtl_calibration/sim
ifneq (,$(findstring _rtl,$(BRIDGE)))
build: rtl-plugin
endif

rtl-plugin:
	@case $(BRIDGE) in \
	    cdc_2phase_rtl)      sub=cdc_2phase ;; \
	    cdc_fifo_gray_rtl)   sub=cdc_fifo_gray ;; \
	    cdc_fifo_2phase_rtl) sub=cdc_fifo_2phase ;; \
	    *) exit 0 ;; \
	esac ; \
	$(MAKE) -C $(RTL_PLUGIN_DIR)/$$sub plugin-fast install

.PHONY: rtl-plugin
