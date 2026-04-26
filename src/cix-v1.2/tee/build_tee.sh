#! /bin/bash

#Contact author: email to abel.shuai@cixcomputing.com
#
CURRENT_DIR=$(cd $(dirname $0); pwd)
OUTPUT_PATH=${CURRENT_DIR}/out


#TOOLCHAINS_PATH=/home/icyshuai/share/cix_code/toolchains/gcc/gcc-arm-10.2-2020.11-x86_64-aarch64-none-linux-gnu/bin

CROSS=aarch64-none-linux-gnu
CROSS_COMPILE=${TOOLCHAINS_PATH}/${CROSS}-


CFG_TEE_CORE_LOG_LEVEL=1
DEBUG=1
PLATFORM=cix
PLATFORM_FLAVOR=sky1
V=0


BUILD_OPTION=
BUILD_LOG_OPTION=


#-----------------------------------------------------------------------
# FUNCTION: usage
# DESCRIPTION:  Display usage information.
#-----------------------------------------------------------------------
function usage() {
    cat << EOT

You can use below command to use this tool, it support long & short options:
./cix_image_tool.sh [Options]...

Options:
  -h, --help                   Display this message
  -c, --clean                  Clean up build object
  -d, --debug                  Build with debug flag
  -r, --release                Build with release flag

Exit status:
  0   if OK,
  !=0 if serious problems.

Example:
  1) Use short options to clean up all object files:
    $./build_tee.sh -c

  2) Use long options to build a debug TEE os:
    $./build_tee.sh -d

  3) Use below command to build a release TEE os:
    ./build_tee.sh -r

  4) Use below command to get help info:
    ./build_tee.sh -h
EOT
}



#-----------------------------------------------------------------------
# FUNCTION: check_path
# DESCRIPTION:  Check if path was exist, if not, create it.
#-----------------------------------------------------------------------
function check_path() {
	if [ ! -d "$1" ]; then
		mkdir $1
	fi
}



#-----------------------------------------------------------------------
# FUNCTION: clean_target
# DESCRIPTION:  Clean up all targets.
#-----------------------------------------------------------------------
function clean_target() {
	make clean
	rm -rf ${OUTPUT_PATH}
}



#-----------------------------------------------------------------------
# FUNCTION: build
# DESCRIPTION:  Build TEE OS.
#-----------------------------------------------------------------------
function build ()
{
	echo "start build TEE os"

	check_path ${OUTPUT_PATH}

	if [ "$BUILD_OPTION" = "debug" ];then
		DEBUG=1
		CFG_TEE_CORE_LOG_LEVEL=4
	fi

	if [ "$BUILD_OPTION" = "release" ];then
		DEBUG=0
		CFG_TEE_CORE_LOG_LEVEL=1
	fi

	if [ "$BUILD_OPTION" = "clean" ];then
		clean_target
	elif [ "$BUILD_LOG_OPTION" = "y" ];then
		make -j4 ARCH=arm CROSS_COMPILE64=$CROSS_COMPILE CFG_ARM64_core=y CFG_USER_TA_TARGETS=ta_arm64 all >> make.log
		cp out/arm-plat-cix/core/tee.bin out/
	else
		make -j4 ARCH=arm CROSS_COMPILE64=$CROSS_COMPILE CFG_ARM64_core=y CFG_USER_TA_TARGETS=ta_arm64 all
		cp out/arm-plat-cix/core/tee.bin out/
	fi
	echo "build optee os over"

}



# parse options:
RET=`getopt -o hcdrl \
--long help,clean,debug,release,log \
-n ' * ERROR' -- "$@"`

if [ $? != 0 ] ; then echo "build_tee.sh exited with doing nothing." >&2 ; exit 1 ; fi

# Note the quotes around $RET: they are essential!
eval set -- "$RET"

# set option values
while true; do
	case "$1" in
		-h | --help )
			usage;
			exit 1;;
		-c | --clean )
			BUILD_OPTION=clean;
			shift 1 ;;
		-d | --debug )
			BUILD_OPTION=debug;
			shift 1 ;;
		-r | --release )
			BUILD_OPTION=release;
			shift 1 ;;
		-l | --log )
			BUILD_LOG_OPTION=y;
			shift 1 ;;
		-- )
			shift;
			break ;;
		* )
			echo "internal error!" ;
			exit 1 ;;
	esac
done


export PLATFORM
export CROSS_COMPILE
export V

build
