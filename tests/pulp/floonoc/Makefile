TARGET = test
GVSOC_ROOT?=../../../..

build ?= $(CURDIR)/build

ifdef USE_MEMORY
OPTIONS += use_memory=true
endif

ifdef MEM_BW
OPTIONS += mem_bw=$(MEM_BW)
endif

space := $(subst ,, )
comma := ,

ifneq ($(OPTIONS),)
TARGET := $(TARGET):$(subst $(space),$(comma),$(strip $(OPTIONS)))
endif

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
