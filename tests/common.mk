# Shared Makefile fragment for GVSoC make-based tests (stub_master /
# stub_target style under gvsoc/core/tests/).
#
# Include from a test Makefile after setting:
#   GVSOC_ROOT — path to the repo root
#   TARGET     — target name, typically "test:case=$(CASE)"
#   CASE       — sub-case name (used to uniquify the COVERAGE info file)
# Optional overrides:
#   build      — build dir  (default $(CURDIR)/build)
#   MODULES    — model dir passed to cmake (default $(CURDIR))
#
# With COVERAGE=1, each `run` drops a uniquely-named lcov .info file
# into $(GVSOC_ROOT)/coverage-report/tests/ so `make coverage-report`
# at the root merges every test's contribution.

build   ?= $(CURDIR)/build
MODULES ?= $(CURDIR)

COV_INFO_DIR  ?= $(GVSOC_ROOT)/coverage-report/tests
COV_INFO_NAME  = $(notdir $(CURDIR))_$(CASE).info
COV_CAPTURE    = :
ifdef COVERAGE
COV_CAPTURE = mkdir -p $(COV_INFO_DIR) && \
    lcov --capture --directory $(build)/build \
         --output-file $(COV_INFO_DIR)/$(COV_INFO_NAME) \
         --ignore-errors mismatch,unused,negative,empty \
         --quiet
endif

clean:
	make -C $(GVSOC_ROOT) TARGETS=$(TARGET) MODULES=$(MODULES) BUILDDIR=$(build)/build INSTALLDIR=$(build)/install clean

build:
	make -C $(GVSOC_ROOT) TARGETS=$(TARGET) MODULES=$(MODULES) BUILDDIR=$(build)/build INSTALLDIR=$(build)/install build

all: build

run: $(build)/work
	$(build)/install/bin/gvrun --target-dir=$(CURDIR) --target=$(TARGET) --work-dir=$(build)/work run $(runner_args)
	@$(COV_CAPTURE)

$(build)/work:
	mkdir -p $(build)/work

.PHONY: build clean run all
