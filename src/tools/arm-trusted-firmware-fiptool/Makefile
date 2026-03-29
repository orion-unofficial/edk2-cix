SHELL := bash
.SHELLFLAGS := -eu -o pipefail -c

HOST_ARCH ?= $(shell uname -m)
PKG_CONFIG ?= $(or $(shell command -v pkg-config 2>/dev/null),$(shell command -v pkgconf 2>/dev/null))
CC ?= cc
V ?= 0

ifeq ($(V),1)
Q :=
define PRINT_STATUS
@:
endef
else
Q := @
define PRINT_STATUS
@printf '  %-7s %s\n' '$(1)' '$(2)'
endef
endif

BUILD_DIR := build/$(HOST_ARCH)
OUTPUT := $(BUILD_DIR)/fiptool
UPSTREAM_TAG := $(shell sed -n 's/^[[:space:]]*"tag":[[:space:]]*"\([^"]*\)".*/\1/p' upstream.json | head -n 1)

ifeq ($(strip $(PKG_CONFIG)),)
$(error pkg-config or pkgconf is required to locate OpenSSL build flags)
endif

OPENSSL_CFLAGS := $(shell $(PKG_CONFIG) --cflags openssl 2>/dev/null)
OPENSSL_LIBS := $(shell $(PKG_CONFIG) --libs openssl 2>/dev/null)

ifeq ($(strip $(OPENSSL_LIBS)),)
$(error unable to resolve OpenSSL build flags via $(PKG_CONFIG) --libs openssl)
endif

CPPFLAGS += -Iinclude/tools_share -Isrc \
	-D_GNU_SOURCE -D_XOPEN_SOURCE=700 -DSTATIC=0 \
	-DVERSION='"$(UPSTREAM_TAG)"'
CPPFLAGS += $(OPENSSL_CFLAGS)
CFLAGS += -Wall -Werror -pedantic -std=c99 -O2
LDLIBS += $(OPENSSL_LIBS)

SOURCES := src/fiptool.c src/tbbr_config.c
OBJECTS := $(patsubst src/%.c,$(BUILD_DIR)/%.o,$(SOURCES))
HEADERS := \
	src/fiptool.h \
	src/fiptool_platform.h \
	src/tbbr_config.h \
	include/tools_share/firmware_image_package.h \
	include/tools_share/uuid.h \
	upstream.json

.PHONY: all clean distclean print-output-path print-upstream-tag

all: $(OUTPUT)

print-output-path:
	printf '%s\n' '$(OUTPUT)'

print-upstream-tag:
	printf '%s\n' '$(UPSTREAM_TAG)'

$(BUILD_DIR):
	$(call PRINT_STATUS,MKDIR,$@)
	$(Q)mkdir -p '$@'

$(BUILD_DIR)/%.o: src/%.c $(HEADERS) | $(BUILD_DIR)
	$(call PRINT_STATUS,CC,$@)
	$(Q)$(CC) $(CPPFLAGS) $(CFLAGS) -c '$<' -o '$@'

$(OUTPUT): $(OBJECTS)
	$(call PRINT_STATUS,LD,$@)
	$(Q)$(CC) $(OBJECTS) $(LDLIBS) -o '$@'

clean:
	$(call PRINT_STATUS,CLEAN,build)
	$(Q)rm -rf build

distclean: clean
