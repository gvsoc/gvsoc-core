GVSOC_ROOT ?= ../../../..
TARGET = test
CASE ?= untimed
TARGET := $(TARGET):kind=$(CASE)

runner_args = --control-script=$(CURDIR)/control.py

include $(GVSOC_CORE)/tests/common.mk
