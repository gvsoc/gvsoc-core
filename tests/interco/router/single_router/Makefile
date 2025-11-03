build ?= $(CURDIR)/build
TARGET = test
OPTIONS :=

ifdef ACCESS_RW
OPTIONS += access_type=rw
endif

ifdef SHARED_RW
OPTIONS += shared_rw=true
endif

ifdef ASYNC
OPTIONS += synchronous=false
endif

ifdef BW
OPTIONS += bandwidth=${BW}
endif

ifdef PACKET_SIZE
OPTIONS += packet_size=${PACKET_SIZE}
endif

ifdef INPUT_FIFO_SIZE
OPTIONS += input_fifo_size=${INPUT_FIFO_SIZE}
endif

ifdef RCV
OPTIONS += use_memory=false
endif

ifdef TARGET_BW
OPTIONS += target_bw=${TARGET_BW}
endif

space := $(subst ,, )
comma := ,

ifneq ($(OPTIONS),)
TARGET := $(TARGET):$(subst $(space),$(comma),$(strip $(OPTIONS)))
endif

clean:
	make -C ../../../../.. TARGETS=$(TARGET) MODULES=$(CURDIR) BUILDDIR=$(build)/build INSTALLDIR=$(build)/install clean

build:
	make -C ../../../../.. TARGETS=$(TARGET) MODULES=$(CURDIR) BUILDDIR=$(build)/build INSTALLDIR=$(build)/install build

all: build

run: $(build)/work
	$(build)/install/bin/gvrun --target-dir=$(CURDIR) --target=$(TARGET) --work-dir=$(build)/work run $(runner_args)

$(build)/work:
	mkdir -p $(build)/work

.PHONY: build
