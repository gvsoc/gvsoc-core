TARGET = test
ROOT_DIR=../../../..

build ?= $(CURDIR)/build

clean:
	make -C $(ROOT_DIR) TARGETS=test MODULES=$(CURDIR) BUILDDIR=$(build)/build INSTALLDIR=$(build)/install clean

build:
	make -C $(ROOT_DIR) TARGETS=test MODULES=$(CURDIR) BUILDDIR=$(build)/build INSTALLDIR=$(build)/install build

all: build

run: $(build)/work
	$(build)/install/bin/gvrun --target-dir=$(CURDIR) --target=$(TARGET) --build-dir=$(build)/work run $(runner_args)

$(build)/work:
	mkdir -p $(build)/work

.PHONY: build
