WORK_DIR ?= work
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

space := $(subst ,, )
comma := ,

ifneq ($(OPTIONS),)
TARGET := $(TARGET):$(subst $(space),$(comma),$(strip $(OPTIONS)))
endif

clean:
	make -C ../../../../../.. TARGETS=$(TARGET) MODULES=$(CURDIR) BUILDDIR=$(CURDIR)/build clean

build:
	make -C ../../../../../.. TARGETS=$(TARGET) MODULES=$(CURDIR) BUILDDIR=$(CURDIR)/build build

all: build

run: $(WORK_DIR)
	gvrun --target-dir=$(CURDIR) --target=$(TARGET) --build-dir=$(WORK_DIR) run $(runner_args)

$(WORK_DIR):
	mkdir -p $(WORK_DIR)

.PHONY: build
