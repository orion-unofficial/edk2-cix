ifeq (,$(shlibuuid))
$(error SHLIBUUID not set)
endif
link-out-dir = $(out-dir)

SIGN ?= $(TA_DEV_KIT_DIR)/scripts/sign_encrypt.py
TA_SIGN_KEY ?= $(TA_DEV_KIT_DIR)/keys/oem_privatekey.pem

ifeq ($(CFG_ENCRYPT_TA),y)
# Default TA encryption key is a dummy key derived from default
# hardware unique key (an array of 16 zero bytes) to demonstrate
# usage of REE-FS TAs encryption feature.
#
# Note that a user of this TA encryption feature needs to provide
# encryption key and its handling corresponding to their security
# requirements.
#TA_ENC_KEY ?= 'b64d239b1f3c7d3b06506229cd8ff7c8af2bb4db2168621ac62c84948468c4f4'
TA_ENC_KEY ?= $(TA_DEV_KIT_DIR)/keys/ta_enc.key
endif

ifeq ($(CFG_ENCRYPT_TA),y)
crypt-args-shlib := --enc-key $(TA_ENC_KEY)
cmd-echo-shlib := SIGNENC
else
cmd-echo-shlib := SIGN
endif

all: $(link-out-dir)/$(shlibname).so $(link-out-dir)/$(shlibname).dmp \
	$(link-out-dir)/$(shlibname).stripped.so \
	$(link-out-dir)/$(shlibuuid).elf \
	$(link-out-dir)/$(shlibuuid).ta

cleanfiles += $(link-out-dir)/$(shlibname).so
cleanfiles += $(link-out-dir)/$(shlibname).dmp
cleanfiles += $(link-out-dir)/$(shlibname).stripped.so
cleanfiles += $(link-out-dir)/$(shlibuuid).elf
cleanfiles += $(link-out-dir)/$(shlibuuid).ta

shlink-ldflags  = $(LDFLAGS)
shlink-ldflags += -shared -z max-page-size=4096
shlink-ldflags += $(call ld-option,-z separate-loadable-segments)
ifeq ($(sm)-$(CFG_TA_BTI),ta_arm64-y)
shlink-ldflags += $(call ld-option,-z force-bti) --fatal-warnings
endif
shlink-ldflags += --as-needed # Do not add dependency on unused shlib

shlink-ldadd  = $(LDADD)
shlink-ldadd += $(addprefix -L,$(libdirs))
shlink-ldadd += --start-group $(addprefix -l,$(libnames)) --end-group
ldargs-$(shlibname).so := $(shlink-ldflags) $(objs) $(shlink-ldadd) $(libgcc$(sm))


$(link-out-dir)/$(shlibname).so: $(objs) $(libdeps)
	@$(cmd-echo-silent) '  LD      $@'
	$(q)$(LD$(sm)) $(ldargs-$(shlibname).so) --soname=$(shlibuuid) -o $@

$(link-out-dir)/$(shlibname).dmp: $(link-out-dir)/$(shlibname).so
	@$(cmd-echo-silent) '  OBJDUMP $@'
	$(q)$(OBJDUMP$(sm)) -l -x -d $< > $@

$(link-out-dir)/$(shlibname).stripped.so: $(link-out-dir)/$(shlibname).so
	@$(cmd-echo-silent) '  OBJCOPY $@'
	$(q)$(OBJCOPY$(sm)) --strip-unneeded $< $@

$(link-out-dir)/$(shlibuuid).elf: $(link-out-dir)/$(shlibname).so
	@$(cmd-echo-silent) '  LN      $@'
	$(q)ln -sf $(<F) $@

$(link-out-dir)/$(shlibuuid).ta: $(link-out-dir)/$(shlibname).stripped.so \
				$(TA_SIGN_KEY)
	@$(cmd-echo-silent) '  SIGN    $@'
	@$(cmd-echo-silent) '  $(cmd-echo-shlib) $@'
	$(q)$(PYTHON3) $(SIGN) --key $(TA_SIGN_KEY) $(crypt-args-shlib) \
		--uuid $(shlibuuid) \
		--in $< --out $@
