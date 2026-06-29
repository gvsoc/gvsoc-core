GVSOC_ROOT ?= ../../../..
TARGET = test
CASE ?= r1
TARGET := $(TARGET):case=$(CASE)

# Run the asserts variant so the adapter / stub traces.assert() protocol checks
# are live (they are no-ops in the default optimized build).
runner_args = --asserts-mode

include $(GVSOC_CORE)/tests/common.mk
