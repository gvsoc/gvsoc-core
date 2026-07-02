GVSOC_ROOT ?= ../../..
TARGET = test
CASE ?= default
# Optional extra target qualifier, e.g. TIMING=timing.r_sub=cycle. Part of
# the target string so build and run resolve the same timing levels.
TIMING ?=
TARGET := $(TARGET):case=$(CASE)
ifneq ($(TIMING),)
TARGET := $(TARGET):$(TIMING)
endif

include $(GVSOC_CORE)/tests/common.mk
