GVSOC_ROOT ?= ../../../..
build ?= $(CURDIR)/build
TARGET = test
CASE ?= single_beat

TARGET := $(TARGET):case=$(CASE)

clean:
	make -C $(GVSOC_ROOT) TARGETS=$(TARGET) MODULES=$(CURDIR) BUILDDIR=$(build)/build INSTALLDIR=$(build)/install clean

build:
	make -C $(GVSOC_ROOT) TARGETS=$(TARGET) MODULES=$(CURDIR) BUILDDIR=$(build)/build INSTALLDIR=$(build)/install build

all: build

run: $(build)/work
	$(build)/install/bin/gvrun --target-dir=$(CURDIR) --target=$(TARGET) --work-dir=$(build)/work run $(runner_args)

$(build)/work:
	mkdir -p $(build)/work

.PHONY: build
