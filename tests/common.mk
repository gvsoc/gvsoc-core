# Shared Makefile fragment for GVSoC make-based tests (stub_master /
# stub_target style under gvsoc/core/tests/).
#
# Include from a test Makefile after setting:
#   GVSOC_ROOT — path to the repo root
#   TARGET     — target name, typically "test:case=$(CASE)"
#   CASE       — sub-case name — must be unique per test case; used
#                to isolate the per-case simulation work dir and to
#                uniquify the COVERAGE info file
# Optional overrides:
#   MODULES    — model dir passed to cmake (default $(CURDIR))
#   BUILDDIR   — cmake build dir
#                (default: the root gvsoc build tree —
#                 $(GVSOC_WORKDIR)/build if set, else
#                 $(GVSOC_ROOT)/build)
#   INSTALLDIR — cmake install dir (same default rules as BUILDDIR)
#   WORKDIR    — per-case sim work dir
#                (default $(CURDIR)/build/work/$(CASE))
#
# By default every case of a testset shares BUILDDIR and INSTALLDIR
# with the root gvsoc build, so the platform compiles only once. The
# per-case WORKDIR keeps simulation artifacts isolated so runs can
# execute in parallel. The testset.cfg is expected to serialize the
# `build` phase across cases via a build resource (see
# `TestsetImpl.new_make_test(build_resource=...)` in gvtest) and to
# opt out of the auto-generated `clean` step via `no_clean=True` —
# this Makefile fragment intentionally exposes no `clean` target.
#
# With COVERAGE=1, each `run` drops a uniquely-named lcov .info file
# into $(GVSOC_ROOT)/coverage-report/tests/ so
# `make coverage-report` at the root merges every test's contribution.

MODULES ?= $(CURDIR)
WORKDIR ?= $(CURDIR)/build/work/$(CASE)

# BUILDDIR / INSTALLDIR default to what the root Makefile would pick:
# $(GVSOC_WORKDIR)/{build,install} if GVSOC_WORKDIR is set, else
# $(GVSOC_ROOT)/{build,install}. Leaving them unset here means we do
# NOT pass overrides to the root `make` invocation, so the root
# applies those same defaults itself.
ifdef GVSOC_WORKDIR
_default_builddir   := $(GVSOC_WORKDIR)/build
_default_installdir := $(GVSOC_WORKDIR)/install
else
_default_builddir   := $(GVSOC_ROOT)/build
_default_installdir := $(GVSOC_ROOT)/install
endif

_effective_builddir   := $(if $(BUILDDIR),$(BUILDDIR),$(_default_builddir))
_effective_installdir := $(if $(INSTALLDIR),$(INSTALLDIR),$(_default_installdir))

# Only forward overrides the test Makefile set explicitly, so the
# root Makefile's own defaults apply when nothing was set.
_root_mk_args = $(if $(BUILDDIR),BUILDDIR=$(BUILDDIR)) $(if $(INSTALLDIR),INSTALLDIR=$(INSTALLDIR))

COV_INFO_DIR  ?= $(GVSOC_ROOT)/coverage-report/tests
COV_INFO_NAME  = $(notdir $(CURDIR))_$(CASE).info
COV_CAPTURE    = :
ifdef COVERAGE
COV_CAPTURE = mkdir -p $(COV_INFO_DIR) && \
    lcov --capture --directory $(_effective_builddir) \
         --output-file $(COV_INFO_DIR)/$(COV_INFO_NAME) \
         --ignore-errors mismatch,unused,negative,empty \
         --quiet
endif

build:
	$(MAKE) -C $(GVSOC_ROOT) TARGETS=$(TARGET) MODULES=$(MODULES) $(_root_mk_args) build

all: build

run: $(WORKDIR)
	$(_effective_installdir)/bin/gvrun --target-dir=$(CURDIR) --target=$(TARGET) --work-dir=$(WORKDIR) run $(runner_args)
	@$(COV_CAPTURE)

$(WORKDIR):
	mkdir -p $(WORKDIR)

.PHONY: build run all
