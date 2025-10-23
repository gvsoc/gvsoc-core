include ../utils/rt.mk

GVSOC_WORKDIR ?= $(CURDIR)/build
ifdef BUILDDIR
FULL_BUILDDIR := $(GVSOC_WORKDIR)/$(BUILDDIR)
else
FULL_BUILDDIR := $(GVSOC_WORKDIR)
endif

clean:
	rm -rf $(FULL_BUILDDIR)/test
	make -C ../../../../.. TARGETS=my_system MODULES=$(CURDIR) BUILDDIR=$(FULL_BUILDDIR)/build INSTALLDIR=$(FULL_BUILDDIR)/install $(OPTIONS) clean

all:
	mkdir -p $(FULL_BUILDDIR)/test
	riscv64-unknown-elf-gcc -g -o $(FULL_BUILDDIR)/test/test main.c $(RT_SRCS) $(RT_FLAGS)

gvsoc:
	make -C ../../../../.. TARGETS=my_system MODULES=$(CURDIR) BUILDDIR=$(FULL_BUILDDIR)/build INSTALLDIR=$(FULL_BUILDDIR)/install $(OPTIONS) build
	g++ -o launcher launcher.cpp -I../../../../../core/engine/include -L$(FULL_BUILDDIR)/install/lib -lpulpvp

prepare:
	cp solution/* .

config:
	$(FULL_BUILDDIR)/install/bin/gvrun --target-dir=$(CURDIR) --target=my_system --build-dir=$(FULL_BUILDDIR)/work --parameter binary=$(FULL_BUILDDIR)/test/test prepare $(runner_args)

run_launcher:
	LD_LIBRARY_PATH=$(FULL_BUILDDIR)/install/lib:$(LD_LIBRARY_PATH) ./launcher --config=$(FULL_BUILDDIR)/work/gvsoc_config.json
