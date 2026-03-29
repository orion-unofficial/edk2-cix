V ?= 0
CC:=gcc
CFLAGS:=-Wall -Wno-overflow

ifeq ($(V),1)
Q :=
PRINT_CC = @:
PRINT_GEN = @:
RUN_GEN = ./$@
else
Q := @
PRINT_CC = @printf '  CC      %s\n' '$@'
PRINT_GEN = @printf '  GEN     %s\n' 'csu_pm_config.bin'
RUN_GEN = ./$@ >/dev/null
endif

all: csupm_bin_config

.PHONY: csupm_bin_config
csupm_bin_config: csupm_bin_config.c $(wildcard *.h) clean
	$(PRINT_CC)
	$(Q)$(CC) $(CFLAGS) -o $@ $(firstword $^)
	$(PRINT_GEN)
	$(Q)$(RUN_GEN)

clean:
	$(Q)rm -f csupm_bin_config csu_pm_config.bin

.PHONY: all clean
