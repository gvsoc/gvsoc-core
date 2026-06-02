# SPDX-FileCopyrightText: 2026 ETH Zurich, University of Bologna and EssilorLuxottica SAS
#
# SPDX-License-Identifier: Apache-2.0
#
# Authors: Germain Haugou (germain.haugou@gmail.com)
#
# Verilator plugin wrapping common_cells `cdc_2phase`, paired forward +
# reverse via the cdc_2phase_pair.sv top-level. Used by the GVSoC bridge
# component `utils.io_v2_cdc_2phase_rtl` for cycle-accurate timing
# comparison against the behavioural `utils.io_v2_clock_bridge`.
#
# Two flavours:
#   obj_dir_plugin_fast/vlt_cdc_2phase_fast.so   no trace, default
#   obj_dir_plugin/vlt_cdc_2phase.so             FST file output (+trace=<path>)

DUT_NAME := cdc_2phase_pair
SDK_HOME := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../../../../../../../../)
CC_HOME  := $(SDK_HOME)/hw/audio_subsystem_v2/IPS/common_cells/src
EXCHANGE_INC := $(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..)

DUT_SRCS := \
    $(CC_HOME)/cdc_2phase.sv \
    cdc_2phase_pair.sv

VERILATOR_ROOT ?= $(shell verilator --getenv VERILATOR_ROOT 2>/dev/null)
PLUGIN_INC     := $(SDK_HOME)/gvsoc/core/models/utils
PLUGIN_SHIM    := plugin_shim.cpp

PLUGIN_DIR_FAST    := obj_dir_plugin_fast
PLUGIN_DIR_FST     := obj_dir_plugin
PLUGIN_SO_FAST     := $(PLUGIN_DIR_FAST)/vlt_cdc_2phase_fast.so
PLUGIN_SO_FST      := $(PLUGIN_DIR_FST)/vlt_cdc_2phase.so

INSTALL_LIB_DIR    := $(SDK_HOME)/install/lib

PLUGIN_VLT_COMMON := \
    --cc --build \
    --public-flat-rw \
    --timescale 1ps/1ps \
    -CFLAGS "-fPIC -O2" \
    -j 8 \
    --top-module $(DUT_NAME) \
    -Wno-DECLFILENAME \
    -Wno-UNUSEDSIGNAL \
    -Wno-UNOPTFLAT \
    -Wno-WIDTH

PLUGIN_CXXFLAGS_BASE := -std=c++17 -fPIC -O2 \
    -I$(VERILATOR_ROOT)/include \
    -I$(VERILATOR_ROOT)/include/vltstd \
    -I$(PLUGIN_INC) \
    -I$(abspath $(dir $(lastword $(MAKEFILE_LIST)))/../..) \
    -I.

.PHONY: all plugin plugin-fast plugin-fst install clean

all: plugin install

plugin: plugin-fast plugin-fst
plugin-fast: $(PLUGIN_SO_FAST)
plugin-fst:  $(PLUGIN_SO_FST)

# ---------------------------------------------------------------------------
# Fast flavour β€” no trace instrumentation. The .so the bridge loads by default.
# ---------------------------------------------------------------------------
$(PLUGIN_DIR_FAST)/V$(DUT_NAME)__ALL.a $(PLUGIN_DIR_FAST)/libverilated.a &: $(DUT_SRCS)
	verilator $(PLUGIN_VLT_COMMON) --Mdir $(PLUGIN_DIR_FAST) $(DUT_SRCS)

$(PLUGIN_DIR_FAST)/plugin_shim.o: $(PLUGIN_SHIM) $(EXCHANGE_INC)/cdc_bridge_exchange.h $(PLUGIN_INC)/verilator_plugin.h $(PLUGIN_INC)/verilator_plugin_helpers.hpp | $(PLUGIN_DIR_FAST)/V$(DUT_NAME)__ALL.a
	$(CXX) -c $(PLUGIN_CXXFLAGS_BASE) -DGV_NO_TRACE -I$(PLUGIN_DIR_FAST) $< -o $@

$(PLUGIN_SO_FAST): $(PLUGIN_DIR_FAST)/plugin_shim.o $(PLUGIN_DIR_FAST)/V$(DUT_NAME)__ALL.a $(PLUGIN_DIR_FAST)/libverilated.a
	$(CXX) -shared -fPIC -o $@ \
	    $(PLUGIN_DIR_FAST)/plugin_shim.o \
	    -Wl,--whole-archive \
	        $(PLUGIN_DIR_FAST)/V$(DUT_NAME)__ALL.a \
	        $(PLUGIN_DIR_FAST)/libverilated.a \
	    -Wl,--no-whole-archive \
	    -pthread

# ---------------------------------------------------------------------------
# FST flavour β€” for waveform debug. Built on demand.
# ---------------------------------------------------------------------------
$(PLUGIN_DIR_FST)/V$(DUT_NAME)__ALL.a $(PLUGIN_DIR_FST)/libverilated.a &: $(DUT_SRCS)
	verilator $(PLUGIN_VLT_COMMON) --trace-fst --Mdir $(PLUGIN_DIR_FST) $(DUT_SRCS)

$(PLUGIN_DIR_FST)/plugin_shim.o: $(PLUGIN_SHIM) $(EXCHANGE_INC)/cdc_bridge_exchange.h $(PLUGIN_INC)/verilator_plugin.h $(PLUGIN_INC)/verilator_plugin_helpers.hpp | $(PLUGIN_DIR_FST)/V$(DUT_NAME)__ALL.a
	$(CXX) -c $(PLUGIN_CXXFLAGS_BASE) -DGV_TRACE_FST -I$(PLUGIN_DIR_FST) $< -o $@

$(PLUGIN_SO_FST): $(PLUGIN_DIR_FST)/plugin_shim.o $(PLUGIN_DIR_FST)/V$(DUT_NAME)__ALL.a $(PLUGIN_DIR_FST)/libverilated.a
	$(CXX) -shared -fPIC -o $@ \
	    $(PLUGIN_DIR_FST)/plugin_shim.o \
	    -Wl,--whole-archive \
	        $(PLUGIN_DIR_FST)/V$(DUT_NAME)__ALL.a \
	        $(PLUGIN_DIR_FST)/libverilated.a \
	    -Wl,--no-whole-archive \
	    -pthread

# ---------------------------------------------------------------------------
# Install β€” the bridge component dlopens this path. Drop both flavours into
# install/lib/ alongside other GVSoC artifacts.
# ---------------------------------------------------------------------------
install: $(PLUGIN_SO_FAST)
	mkdir -p $(INSTALL_LIB_DIR)
	cp $(PLUGIN_SO_FAST) $(INSTALL_LIB_DIR)/vlt_cdc_2phase_fast.so
	@if [ -f $(PLUGIN_SO_FST) ]; then \
	    cp $(PLUGIN_SO_FST) $(INSTALL_LIB_DIR)/vlt_cdc_2phase.so ; \
	fi

clean:
	rm -rf $(PLUGIN_DIR_FAST) $(PLUGIN_DIR_FST)
