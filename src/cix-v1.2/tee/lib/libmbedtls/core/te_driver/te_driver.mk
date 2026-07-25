################################################################################
# Input variables
################################################################################

# TOP_DIR      - Build system top director
# TE_NAME      - TE code name
# TE_SW_TOP    - TE driver software top directory
# TE_UNIT_TEST - TE unit test flag: y|n
# TE_HWA_DEBUG - TE HWA debug flag: y|n
# TE_OTP_PUF   - TE OTP PUF flag: y|n
# TE_PRINT     - printf equivalent function

################################################################################
# Output variables
################################################################################

# TE_SOURCES  - list of te driver source files
# TE_CFLAGS   - te driver include options and flags

################################################################################
# TE build variables
################################################################################

include $(TE_SW_TOP)/te_common.mk
#-include $(TE_SW_TOP)/hwa/res.mk

ifeq ($(TE_HWA_DEBUG),y)
TE_CFLAGS    += -DTE_PRINT=$(TE_PRINT)
endif

#
# cleanup generated files and directories
.PHONY: distclean realclean
distclean realclean::
	$(Q)rm -f $(TEGENFILES)
	$(Q)set -e;                                         \
	    $(cmd-echo-silent) "  CLEANUP $(TE_GEN_TOP)";   \
	    dirs="$(call cleandirs-for-rmdir,$(TE_GEN_TOP), \
	                 $(TEGENFILES))";                   \
	    if [ "$$dirs" ]; then $(RMDIR) $$dirs; fi

