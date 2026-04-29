__build: main

V ?= 0
BUILD_NAME := MemConfigBinTool
ROOT_DIR   := $(shell pwd)
SRC_FILE   := $(wildcard *.c ./*/*.c)
OBJS       := $(SRC_FILE:.c=.o)
INC_DIR    := -I$(ROOT_DIR)/include
CFLAG      :=

ifeq ($(V),1)
Q :=
PRINT_CC = @:
PRINT_LD = @:
PRINT_GEN = @:
else
Q := @
PRINT_CC = @printf '  CC      %s\n' '$@'
PRINT_LD = @printf '  LD      %s\n' '$(BUILD_NAME)'
PRINT_GEN = @printf '  GEN     %s\n' 'memory_config.bin'
endif

main: $(OBJS)
	$(PRINT_LD)
	$(Q)$(CC) $^ -o $(BUILD_NAME)

	$(PRINT_GEN)
	$(Q)./$(BUILD_NAME)
	$(Q)rm -f $(OBJS) $(BUILD_NAME)

$(OBJS): %.o: %.c
	$(PRINT_CC)
	$(Q)$(CC) $(INC_DIR) -c $< -o $@ $(CFLAG)

.PHONY: clean
clean:
	$(Q)rm -f $(OBJS) $(BUILD_NAME)
