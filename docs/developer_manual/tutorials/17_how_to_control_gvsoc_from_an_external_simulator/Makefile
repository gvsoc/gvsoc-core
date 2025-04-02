include ../utils/rt.mk

BUILDDIR = $(CURDIR)/build
INSTALLDIR = $(CURDIR)/install

ifdef LOCAL_BUILD
OPTIONS=BUILDDIR=$(BUILDDIR) INSTALLDIR=$(INSTALLDIR)
GVSOC=$(INSTALLDIR)/bin/gvsoc
else
GVSOC=../../../../../install/bin/gvsoc
endif

clean:
	rm -f *.o $(BUILDDIR)/test
	make -C ../../../../.. TARGETS=my_system MODULES=$(CURDIR) $(OPTIONS) clean

all:
	mkdir -p $(BUILDDIR)
	riscv64-unknown-elf-gcc -g -o $(BUILDDIR)/test main.c $(RT_SRCS) $(RT_FLAGS)

gvsoc:
	make -C ../../../../.. TARGETS=my_system MODULES=$(CURDIR) $(OPTIONS) build
	g++ -o launcher launcher.cpp -I../../../../core/engine/include -L$(INSTALLDIR)/lib -lpulpvp

prepare:
	cp solution/* .

config:
	$(GVSOC) --target-dir=$(CURDIR) --target=my_system --work-dir=$(BUILDDIR) --binary=$(BUILDDIR)/test prepare $(runner_args)

run_launcher:
	LD_LIBRARY_PATH=$(INSTALLDIR)/lib:$(LD_LIBRARY_PATH) ./launcher --config=build/gvsoc_config.json
