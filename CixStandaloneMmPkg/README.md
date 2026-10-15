# CIX StandaloneMM qualification platform

EDK2 202608 core, CPU, variable and FTW drivers with the exact Radxa 1.3.1
NOR, serial and firmware-version executables. The latter have no matching
public secure-side sources. Vendor/manifest.json pins their recovered bytes.
The source tree includes the dependencies needed by this platform.

Build in the Linux buildbox with GCC_AARCH64_PREFIX=aarch64-linux-gnu-:

    source edksetup.sh
    make -C BaseTools -j8
    build -p CixStandaloneMmPkg/CixStandaloneMm.dsc -a AARCH64 -t GCC -b RELEASE -n 8

The output is Build/StandaloneMm/RELEASE_GCC/FV/BL32_AP_MM.fd. It requires the
maintained CIX OP-TEE FF-A 1.2 handoff (CFG_STMM_FFA_VERSION=12), including the
CIX NVRAM GUID HOB. This is a qualification component, not an approved payload
for STMM=unofficial. The 2 MiB FD is not a flashable board image.

VariablePcd uses the standard FVB GetPhysicalAddress method to bind the source
storage drivers to the vendor RAM shadow. The FVB dependency must be satisfied
before its constructor. VarCheckPolicyLibStandaloneMm must be selected: merely
linking VariablePolicyLib leaves policy disabled and permits authenticated
variable deletion without signature verification.

Known qualification blocker: interruption after erasing the variable FV header
during reclaim causes the retained NOR executable to reformat the NVRAM before
FTW can recover it. The same failure occurs with the recovered vendor variable
and FTW executables. Do not enable this platform for firmware builds until the
early NOR initialization/recovery interaction is fixed and qualified.
