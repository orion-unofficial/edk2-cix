SHELL := bash
.SHELLFLAGS := -eu -o pipefail -c

HOST_ARCH_RAW ?= $(shell uname -m)
HOST_ARCH := $(if $(filter arm64,$(HOST_ARCH_RAW)),aarch64,$(HOST_ARCH_RAW))
PKG_CONFIG ?= $(or $(shell command -v pkg-config 2>/dev/null),$(shell command -v pkgconf 2>/dev/null))
CC ?= cc
CPPFLAGS ?=
CFLAGS ?= -O2 -Wall -Wextra -Wpedantic -std=c11
LDFLAGS ?=
LDLIBS ?=
V ?= 0

ifeq ($(V),1)
Q :=
PRINT_CC = @:
PRINT_LD = @:
PRINT_CLEAN = @:
else
Q := @
PRINT_CC = @printf '  CC      %s\n' '$@'
PRINT_LD = @printf '  LD      %s\n' '$@'
PRINT_CLEAN = @printf '  CLEAN   %s\n' 'cert_uefi_create_rsa'
endif

ifeq ($(strip $(PKG_CONFIG)),)
$(error pkg-config or pkgconf is required to locate OpenSSL build flags)
endif

OPENSSL_CFLAGS := $(shell $(PKG_CONFIG) --cflags openssl 2>/dev/null)
OPENSSL_LIBS := $(shell $(PKG_CONFIG) --libs openssl 2>/dev/null)

ifeq ($(strip $(OPENSSL_LIBS)),)
$(error unable to resolve OpenSSL build flags via $(PKG_CONFIG) --libs openssl)
endif

CPPFLAGS += $(OPENSSL_CFLAGS)
LDLIBS += $(OPENSSL_LIBS)

BUILD_NAME := cert_uefi_create_rsa
SRCS := cert_uefi_create_rsa.c
BUILD_DIR := build/$(HOST_ARCH)
OUTPUT := $(BUILD_DIR)/$(BUILD_NAME)
OBJS := $(patsubst %.c,$(BUILD_DIR)/%.o,$(SRCS))

.PHONY: all clean print-output-path

all: $(OUTPUT)

print-output-path:
	printf '%s\n' '$(OUTPUT)'

$(BUILD_DIR):
	$(Q)mkdir -p '$@'

$(OUTPUT): $(OBJS)
	$(PRINT_LD)
	$(Q)$(CC) $(CFLAGS) $(LDFLAGS) -o '$@' $(OBJS) $(LDLIBS)

$(BUILD_DIR)/%.o: %.c | $(BUILD_DIR)
	$(PRINT_CC)
	$(Q)$(CC) $(CPPFLAGS) $(CFLAGS) -c -o '$@' '$<'

clean:
	$(PRINT_CLEAN)
	$(Q)rm -rf build
