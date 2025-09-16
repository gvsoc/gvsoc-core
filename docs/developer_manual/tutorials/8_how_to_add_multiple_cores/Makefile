include ../utils/rt.mk

build ?= $(CURDIR)
BUILDDIR = $(CURDIR)/build

clean:
	rm -f *.o $(BUILDDIR)/test
	make -C ../../../../.. TARGETS=my_system MODULES=$(CURDIR) BUILDDIR=$(build)/build INSTALLDIR=$(build)/install $(OPTIONS) clean

all:
	mkdir -p $(BUILDDIR)
	riscv64-unknown-elf-gcc -g -o $(BUILDDIR)/test main.c $(RT_SRCS) $(RT_FLAGS)

gvsoc:
	make -C ../../../../.. TARGETS=my_system MODULES=$(CURDIR) BUILDDIR=$(build)/build INSTALLDIR=$(build)/install $(OPTIONS) build

prepare:
	cp solution/* .

run:
	$(build)/install/bin/gvrun --target-dir=$(CURDIR) --target=my_system --build-dir=$(build)/work --parameter binary=$(BUILDDIR)/test run $(runner_args)
