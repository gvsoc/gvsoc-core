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
	make -C ../../../../.. TARGETS=my_system MODULES=$(CURDIR) $(OPTIONS) clean

all:

gvsoc:
	make -C ../../../../.. TARGETS=my_system MODULES=$(CURDIR) $(OPTIONS) build

prepare:
	cp solution/* .

run:
    mkdir -p $(BUILDDIR)
	$(GVSOC) --target-dir=$(CURDIR) --target=my_system --work-dir=$(BUILDDIR) --binary=$(BUILDDIR)/test run $(runner_args)
