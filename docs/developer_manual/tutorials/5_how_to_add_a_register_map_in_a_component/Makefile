include ../utils/rt.mk

build ?= $(CURDIR)
BUILDDIR = $(CURDIR)/build
INSTALLDIR = $(CURDIR)/install

export PYTHONPATH:=$(CURDIR)/../../../../../core/models:$(PYTHONPATH)

clean:
	rm -f *.o $(BUILDDIR)/test
	make -C ../../../../.. TARGETS=my_system MODULES=$(CURDIR) BUILDDIR=$(build)/build INSTALLDIR=$(build)/install $(OPTIONS) clean

all:
	mkdir -p $(BUILDDIR)
	riscv64-unknown-elf-gcc -o $(BUILDDIR)/test main.c $(RT_SRCS) $(RT_FLAGS)

gvsoc:
	make -C ../../../../.. TARGETS=my_system MODULES=$(CURDIR) BUILDDIR=$(build)/build INSTALLDIR=$(build)/install $(OPTIONS) build

run:
	$(build)/install/bin/gvrun --target-dir=$(CURDIR) --target=my_system --build-dir=$(build)/work --parameter binary=$(BUILDDIR)/test run $(runner_args)

prepare:
	cp solution/* .

regmap:
	../../../../../core/bin/regmap-gen --input-md regmap.md --header headers/mycomp
