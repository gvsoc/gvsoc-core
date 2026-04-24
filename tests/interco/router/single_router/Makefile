GVSOC_ROOT ?= ../../../../..
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

# Per-case identifier used by common.mk to isolate the work dir and
# uniquify the coverage .info file. Built from the comma-joined
# OPTIONS with all non-alphanumerics normalized to underscores;
# falls back to "default" for the option-less case.
CASE := $(if $(strip $(OPTIONS)),$(subst $(space),_,$(subst =,_,$(subst $(comma),_,$(strip $(OPTIONS))))),default)

include $(GVSOC_ROOT)/gvsoc/core/tests/common.mk
