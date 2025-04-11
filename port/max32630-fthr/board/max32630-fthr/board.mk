ifeq "$(BOARD_DIR)" ""
$(error BOARD_DIR must be set)
endif

PROJ_CFLAGS+=-DRO_FREQ=96000000

# Source files for this board (add path to VPATH below)
SRCS += board.c
SRCS += stdio.c
SRCS += led.c
SRCS += pb.c
SRCS += max14690n.c
# TODO: Check can we drop the
# variant and use the common part
# SRCS += max14690.c

# Where to find BSP source files
VPATH += $(BOARD_DIR)/Source
VPATH += $(BOARD_DIR)/../Source

# Where to find BSP header files
IPATH += $(BOARD_DIR)/Include
IPATH += $(BOARD_DIR)/../Include