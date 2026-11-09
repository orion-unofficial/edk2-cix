# Independent secure-world EDK2 202608, with FF-A 1.2.
CIX_RELEASE_STMM_HELPER := $(if $(filter unofficial,$(STMM)),scripts/build_hybrid_stmm.py,scripts/stage_vendor_stmm.py)
CIX_RELEASE_STMM_SOURCE := $(abspath $(CIX_RELEASE_SOURCE_DIR)/stmm-unofficial)
CIX_RELEASE_STMM_FFA_VERSION := $(if $(filter unofficial,$(STMM)),12,10)
CIX_RELEASE_STMM_MANIFEST := $(CIX_RELEASE_V12_PAYLOAD_DIR)/standalone-mm/manifest.json
CIX_RELEASE_STMM_ARGS = $(if $(filter unofficial,$(STMM)),--source "$(CIX_RELEASE_STMM_SOURCE)" --edk2-source "$(CIX_RELEASE_STMM_SOURCE)" --native-dsc "$(abspath stmm-integration-v1/PlatformStandaloneMmRpmb.dsc)" --edk2-release 202608 --cache-root "$(CIX_RELEASE_CACHE_ROOT)/stmm" --cross-compile "$(GCC_AARCH64_REAL_PREFIX)$(GCC5_AARCH64_REAL_PREFIX)" --jobs "$(HELPER_BUILD_JOBS)")
