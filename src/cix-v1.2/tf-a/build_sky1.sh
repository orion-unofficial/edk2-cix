#!/bin/bash
CURRENT_DIR=`pwd`
export WORKSPACE=$CURRENT_DIR/../..
export CROSS_COMPILE=$WORKSPACE/bsp/downloads/gcc-arm-10.3-2021.07-x86_64-aarch64-none-elf/bin/aarch64-none-elf-
export WORKDIR=$WORKSPACE/tc0_source/arm-trusted-firmware
export BUILD_DIR=$WORKDIR/build/sky1/debug/
export DEPOLY_DIR=$WORKSPACE/bsp/build-poky/tmp-poky/deploy/images/sky1/

echo "build TC0 ATF start"

make PLAT=sky1 BL33=$WORKSPACE/bsp/build-poky/tmp-poky/deploy/images/tc0/u-boot.bin \
SCP_BL2=$WORKSPACE/bsp/build-poky/tmp-poky/deploy/images/tc0/scp_ramfw.bin TARGET_PLATFORM=0 DEBUG=1 \
MBEDTLS_DIR=$WORKSPACE/tc0_source/mbedtls  \
TRUSTED_BOARD_BOOT=1 GENERATE_COT=1 ARM_ROTPK_LOCATION=devel_rsa \
ROT_KEY=plat/arm/board/common/rotpk/arm_rotprivk_rsa.pem \
ARM_GPT_SUPPORT=1 all fip
echo "build TC0 ATF done"

do_generate_gpt() {
    gpt_image="${BUILD_DIR}/fip_gpt.bin"
    fip_bin="${BUILD_DIR}/fip.bin"
    # the FIP partition type is not standardized, so generate one
    fip_type_uuid=`uuidgen --sha1 --namespace @dns --name "fip_type_uuid"`
    # metadata partition type UUID, specified by the document:
    # Platform Security Firmware Update for the A-profile Arm Architecture
    # version: 1.0BET0
    metadata_type_uuid="8a7a84a0-8387-40f6-ab41-a8b9a5a60d23"
    location_uuid=`uuidgen`
    FIP_A_uuid=`uuidgen`
    FIP_B_uuid=`uuidgen`

    # maximum FIP size 4MB. This is the current size of the FIP rounded up to an integer number of MB.
    fip_max_size=4194304
    fip_bin_size=$(stat -c %s $fip_bin)
    if [ $fip_max_size -lt $fip_bin_size ]; then
        bberror "FIP binary ($fip_bin_size bytes) is larger than the GPT partition ($fip_max_size bytes)"
    fi

    # maximum metadata size 512B. This is the current size of the metadata rounded up to an integer number of sectors.
    metadata_max_size=512
    metadata_file="${BUILD_DIR}/metadata.bin"
    python3 ${WORKDIR}/generate_metadata.py --metadata_file $metadata_file \
                                            --img_type_uuids $fip_type_uuid \
                                            --location_uuids $location_uuid \
                                            --img_uuids $FIP_A_uuid $FIP_B_uuid

    # create GPT image. The GPT contains 2 FIP partitions: FIP_A and FIP_B, and 2 metadata partitions: FWU-Metadata and Bkup-FWU-Metadata.
    # the GPT layout is the following:
    # -----------------------
    # Protective MBR
    # -----------------------
    # Primary GPT Header
    # -----------------------
    # FIP_A
    # -----------------------
    # FIP_B
    # -----------------------
    # FWU-Metadata
    # -----------------------
    # Bkup-FWU-Metadata
    # -----------------------
    # Secondary GPT Header
    # -----------------------

    sector_size=512
    gpt_header_size=33 # valid only for 512-byte sectors
    num_sectors_fip=`expr $fip_max_size / $sector_size`
    num_sectors_metadata=`expr $metadata_max_size / $sector_size`
    start_sector_1=`expr 1 + $gpt_header_size` # size of MBR is 1 sector
    start_sector_2=`expr $start_sector_1 + $num_sectors_fip`
    start_sector_3=`expr $start_sector_2 + $num_sectors_fip`
    start_sector_4=`expr $start_sector_3 + $num_sectors_metadata`
    num_sectors_gpt=`expr $start_sector_4 + $num_sectors_metadata + $gpt_header_size`
    gpt_size=`expr $num_sectors_gpt \* $sector_size`

    # create raw image
    dd if=/dev/zero of=$gpt_image bs=$gpt_size count=1

    # create the GPT layout
    sgdisk $gpt_image \
           --set-alignment 1 \
           --disk-guid $location_uuid \
           \
           --new 1:$start_sector_1:+$num_sectors_fip \
           --change-name 1:FIP_A \
           --typecode 1:$fip_type_uuid \
           --partition-guid 1:$FIP_A_uuid \
           \
           --new 2:$start_sector_2:+$num_sectors_fip \
           --change-name 2:FIP_B \
           --typecode 2:$fip_type_uuid \
           --partition-guid 2:$FIP_B_uuid \
           \
           --new 3:$start_sector_3:+$num_sectors_metadata \
           --change-name 3:FWU-Metadata \
           --typecode 3:$metadata_type_uuid \
           \
           --new 4:$start_sector_4:+$num_sectors_metadata \
           --change-name 4:Bkup-FWU-Metadata \
           --typecode 4:$metadata_type_uuid

    # populate the GPT partitions
    dd if=$fip_bin of=$gpt_image bs=$sector_size seek=$start_sector_1 count=$num_sectors_fip conv=notrunc
    dd if=$fip_bin of=$gpt_image bs=$sector_size seek=$start_sector_2 count=$num_sectors_fip conv=notrunc
    dd if=$metadata_file of=$gpt_image bs=$sector_size seek=$start_sector_3 count=$num_sectors_metadata conv=notrunc
    dd if=$metadata_file of=$gpt_image bs=$sector_size seek=$start_sector_4 count=$num_sectors_metadata conv=notrunc
}

do_generate_gpt
install -m 0644 ${BUILD_DIR}/fip_gpt.bin ${DEPOLY_DIR}/fip_gpt-tc.bin
ln -sf fip_gpt-tc.bin ${DEPOLY_DIR}/fip_gpt.bin

