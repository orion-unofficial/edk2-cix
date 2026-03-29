## @file
# Makefile
#
# Copyright (c) 2007 - 2016, Intel Corporation. All rights reserved.<BR>
# SPDX-License-Identifier: BSD-2-Clause-Patent
#

DEPFILES = $(OBJECTS:%.o=%.d)

$(MAKEROOT)/libs-$(HOST_ARCH):
	$(call PRINT_STATUS,MKDIR,$@)
	$(Q)mkdir -p $(MAKEROOT)/libs-$(HOST_ARCH)

.PHONY: install
install: $(MAKEROOT)/libs-$(HOST_ARCH) $(LIBRARY)
	$(call PRINT_STATUS,INSTALL,$(notdir $(LIBRARY)))
	$(Q)cp $(LIBRARY) $(MAKEROOT)/libs-$(HOST_ARCH)

$(LIBRARY): $(OBJECTS)
	$(call PRINT_STATUS,AR,$@)
	$(Q)$(BUILD_AR) crs $@ $^

%.o : %.c
	$(call PRINT_STATUS,CC,$@)
	$(Q)$(BUILD_CC)  -c $(BUILD_CPPFLAGS) $(BUILD_CFLAGS) $< -o $@

%.o : %.cpp
	$(call PRINT_STATUS,CXX,$@)
	$(Q)$(BUILD_CXX) -c $(BUILD_CPPFLAGS) $(BUILD_CXXFLAGS) $< -o $@

.PHONY: clean
clean:
	$(call PRINT_STATUS,CLEAN,$(CURDIR))
	$(Q)rm -f $(OBJECTS) $(LIBRARY) $(DEPFILES)

-include $(DEPFILES)
