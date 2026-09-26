# OP-TEE Trusted OS
This git contains source code for the secure side implementation of OP-TEE
project.

All official OP-TEE documentation has moved to http://optee.readthedocs.io.

// OP-TEE core maintainers

How to build it:
   Before run script of build_tee.sh, you must set right toolchain in this
script, please change value of "TOOLCHAINS_PATH" to your locatiion of toolchain
in your workstation. then you can run below command to build it:

./build_tee.sh -d or ./build_tee.sh -r

More detail info, you can check this script
