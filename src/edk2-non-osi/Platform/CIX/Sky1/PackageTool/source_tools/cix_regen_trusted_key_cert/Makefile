PKG_CONFIG ?= $(or $(shell command -v pkg-config 2>/dev/null),$(shell command -v pkgconf 2>/dev/null))
CC ?= cc
CPPFLAGS ?=
CFLAGS ?= -O2 -Wall -Wextra -Wpedantic -std=c11
LDFLAGS ?=
LDLIBS ?=

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

BUILD_NAME := cix_regen_trusted_key_cert
SRCS := cix_regen_trusted_key_cert.c
OBJS := $(SRCS:.c=.o)

all: $(BUILD_NAME)

$(BUILD_NAME): $(OBJS)
	$(CC) $(CFLAGS) $(LDFLAGS) -o $@ $(OBJS) $(LDLIBS)

$(OBJS): %.o: %.c
	$(CC) $(CPPFLAGS) $(CFLAGS) -c -o $@ $<

clean:
	rm -f $(BUILD_NAME) $(OBJS)

.PHONY: all clean
