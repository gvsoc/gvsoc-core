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
	g++ -o launcher launcher.cpp -I../../../../../core/engine/include -L$(build)/install/lib -lpulpvp

prepare:
	cp solution/* .

config:
	$(build)/install/bin/gvrun --target-dir=$(CURDIR) --target=my_system --build-dir=$(build)/work --parameter binary=$(BUILDDIR)/test prepare $(runner_args)

run_launcher:
	LD_LIBRARY_PATH=$(build)/install/lib:$(LD_LIBRARY_PATH) ./launcher --config=$(build)/work/gvsoc_config.json
