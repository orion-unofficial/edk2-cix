// SPDX-License-Identifier: BSD-2-Clause
/*
 * Copyright (c) 2014, STMicroelectronics International N.V.
 */

#include <assert.h>
#include <config.h>
#include <crypto/crypto.h>
#include <kernel/huk_subkey.h>
#include <kernel/misc.h>
#include <kernel/msg_param.h>
#include <kernel/mutex.h>
#include <kernel/panic.h>
#include <kernel/tee_common.h>
#include <kernel/tee_common_otp.h>
#include <kernel/tee_misc.h>
#include <kernel/thread.h>
#include <mempool.h>
#include <mm/core_memprot.h>
#include <mm/mobj.h>
#include <mm/tee_mm.h>
#include <optee_rpc_cmd.h>
#include <stdlib.h>
#include <string_ext.h>
#include <string.h>
#include <sys/queue.h>
#include <tee/tee_fs.h>
#include <tee/tee_fs_key_manager.h>
#include <tee/tee_pobj.h>
#include <tee/tee_svc_storage.h>
#include <trace.h>
#include <util.h>
#include <drivers/spi_nor.h>
#include <initcall.h>

#define NOR_STORAGE_START_ADDRESS      0
#define NOR_FS_FAT_START_ADDRESS       (0x1000)
#define NOR_BLOCK_SIZE_SHIFT           12
#define NOR_DATA_SIZE                  (0x1000)

#define NOR_FS_MAGIC                   0x4E4F5246
#define FS_VERSION                      2

#define FILE_IS_ACTIVE                  (1u << 0)
#define FILE_IS_LAST_ENTRY              (1u << 1)

#define TEE_NOR_FS_FILENAME_LENGTH 224
#define SERIAL_NUMBER_SIZE_RESERVE      (0x80)

#define TMP_BLOCK_SIZE			4096U

#define NOR_MAX_RETRIES		10

//#define SECURE_DATA_SECTION_INDEX      1

// FLASH START ADDRESS on the microcontroller
#ifndef FLASH_BASE_ADDR
#define FLASH_BASE_ADDR (0x0)
#endif

bool nor_dev_info_synced = false;
struct spi_nor* g_spi_nor;

/**
 * Utilized when caching is enabled, i.e., when CFG_NOR_FS_CACHE_ENTRIES > 0.
 * Cache size + the number of entries that are repeatedly read in and buffered
 * once the cache is full.
 */
#define NOR_BUF_MAX_ENTRIES (CFG_NOR_FS_CACHE_ENTRIES + \
			      CFG_NOR_FS_RD_ENTRIES)

/**
 * FS parameters: Information often used by internal functions.
 * fat_start_address will be set by nor_fs_setup().
 * nor_fs_parameters can be read by any other function.
 */
struct nor_fs_parameters {
	uint32_t fat_start_address;
	uint32_t max_nor_address;
};

/**
 * File entry for a single file in a NOR_FS partition.
 */
struct nor_fat_entry {
	uint32_t start_address;
	uint32_t data_size;
	uint32_t flags;
	uint32_t write_counter;
	uint8_t fek[TEE_FS_KM_FEK_SIZE];
	char filename[TEE_NOR_FS_FILENAME_LENGTH];
	/* Do not use reserved[] for other purpose. */
  	uint8_t reserved[3840];
};

/**
 * Structure that describes buffered/cached FAT FS entries in NOR storage.
 * This structure is used in functions traversing the FAT FS.
 */
struct nor_fat_entry_dir {
	/*
	 * Buffer storing the FAT FS entries read in from NOR storage. It
	 * includes the optional cache entries (CFG_NOR_FS_CACHE_ENTRIES)
	 * and entries temporary read for current FAT FS traversal
	 * (CFG_NOR_FS_RD_ENTRIES) when not found from cached entries.
	 */
	struct nor_fat_entry *nor_fat_entry_buf;
	/* Current index of FAT FS entry to read from buffer. */
	uint32_t idx_curr;
	/* Total number of FAT FS entries in buffer. */
	uint32_t num_buffered;
	/* Total number of FAT FS entries read during traversal. */
	uint32_t num_total_read;
	/* Indicates that last FAT FS entry was read. */
	bool last_reached;
};

/**
 * FAT entry context with reference to a FAT entry and its
 * location in NOR Flash.
 */
struct nor_file_handle {
	struct nor_fat_entry fat_entry;
	const TEE_UUID *uuid;
	char filename[TEE_NOR_FS_FILENAME_LENGTH];
	/* Address for current entry in NOR Flash */
	uint32_t nor_fat_address;
};

/**
 * NOR_FS partition data
 */
struct nor_fs_partition {
	uint32_t nor_fs_magic;
	uint32_t fs_version;
	uint32_t fat_start_address;
	/* Do not use reserved[] for other purpose than partition data. */
	uint8_t reserved[4084];
};

/**
 * A node in a list of directory entries.
 */
struct tee_nor_fs_dirent {
	struct tee_fs_dirent entry;
	SIMPLEQ_ENTRY(tee_nor_fs_dirent) link;
};

/**
 * The NOR Flash directory representation. It contains a queue of
 * NOR Flash directory entries: 'next'.
 * The current pointer points to the last directory entry
 * returned by readdir().
 */
struct tee_fs_dir {
	struct tee_nor_fs_dirent *current;
	/* */
	SIMPLEQ_HEAD(next_head, tee_nor_fs_dirent) next;
};

static struct nor_fs_parameters *fs_par;
static struct nor_fat_entry_dir *fat_entry_dir;

/*
 * Lower interface to NOR device
 */

//#define NOR_DATA_FRAME_SIZE                        512

#define NOR_RESULT_OK                              0x00
#define NOR_RESULT_GENERAL_FAILURE                 0x01
#define NOR_RESULT_AUTH_FAILURE                    0x02
#define NOR_RESULT_ADDRESS_FAILURE                 0x04
#define NOR_RESULT_WRITE_FAILURE                   0x05
#define NOR_RESULT_READ_FAILURE                    0x06
#define NOR_RESULT_AUTH_KEY_NOT_PROGRAMMED         0x07
#define NOR_RESULT_MASK                            0x3F
#define NOR_DEVICE_ID_W77Q                         0x8a
#define NOR_DEVICE_ID_W25R                         0x60
#define NOR_MEMORY_TYPE_W25R_128                   0x18
#define NOR_MEMORY_TYPE_W25R_64                    0x17

/* NOR internal commands */
//#define NOR_CMD_GET_DEV_INFO  0x01

//#define NOR_SIZE_SINGLE (128 * 1024)

/* Error codes for get_dev_info request/response. */
//#define NOR_CMD_GET_DEV_INFO_RET_OK     0x00
//#define NOR_CMD_GET_DEV_INFO_RET_ERROR  0x01

struct nor_data_frame {
	uint8_t data[NOR_DATA_SIZE];
};

/* If set to true, don't try to access NOR until rebooted */
static bool nor_dead;

/*
 * Mutex to serialize the operations exported by this file.
 * It protects nor_ctx and prevents overlapping operations on eMMC devices with
 * different IDs.
 */
static struct mutex nor_mutex = MUTEX_INITIALIZER;

static bool is_zero(const uint8_t *buf, size_t size)
{
	size_t i;

	for (i = 0; i < size; i++)
		if (buf[i])
			return false;
	return true;
}

uint8_t flash_type(uint16_t which_type){
       uint8_t actual_jedec_id[6];
       spi_nor_read_jedec(actual_jedec_id);
       return actual_jedec_id[which_type];
}


static TEE_Result encrypt_block(uint8_t *out, const uint8_t *in,
				uint16_t blk_idx, const uint8_t *fek,
				const TEE_UUID *uuid)
{
	return tee_fs_crypt_block(uuid, out, in, NOR_DATA_SIZE,
				  blk_idx, fek, TEE_MODE_ENCRYPT);
}

static TEE_Result decrypt_block(uint8_t *out, const uint8_t *in,
				uint16_t blk_idx, const uint8_t *fek,
				const TEE_UUID *uuid)
{
	return tee_fs_crypt_block(uuid, out, in, NOR_DATA_SIZE,
				  blk_idx, fek, TEE_MODE_DECRYPT);
}

/* This function must never return TEE_SUCCESS if nor_ctx == NULL */
static TEE_Result tee_nor_init(void)
{
	TEE_Result res = TEE_SUCCESS;
	bool secure_nor = CFG_SECURE_NOR_FLASH;

	if (nor_dead)
		return TEE_ERROR_COMMUNICATION;

	if (true == nor_dev_info_synced)
		return TEE_SUCCESS;

	g_spi_nor = malloc(sizeof(struct spi_nor));
	if (g_spi_nor == NULL)
		return TEE_ERROR_OUT_OF_MEMORY;
	memzero_explicit(g_spi_nor, sizeof(struct spi_nor));

	if (secure_nor)
		spi_nor_register_w77(g_spi_nor);
	else
		spi_nor_register_normal(g_spi_nor);

	res = g_spi_nor->init(g_spi_nor);
	if (res != TEE_SUCCESS)
		panic();

	nor_dev_info_synced = true;
	return res;
}

/*
 * Read NOR Flash data in bytes.
 *
 * @addr       Byte address of data.
 * @data       Pointer to the data.
 * @len        Size of data in bytes.
 * @fek        Encrypted File Encryption Key or NULL.
 */
static TEE_Result tee_nor_read(uint32_t addr, uint8_t *data,
			       uint32_t len, const uint8_t *fek,
			       const TEE_UUID *uuid)
{
	TEE_Result res = TEE_ERROR_GENERIC;
	uint16_t blk_idx;
	uint16_t blkcnt;
	uint8_t byte_offset;
	struct nor_data_frame *datafrm = NULL;

	if (!data || !len)
		return TEE_ERROR_BAD_PARAMETERS;

	blk_idx = addr / NOR_DATA_SIZE;
	byte_offset = addr % NOR_DATA_SIZE;

	if (len + byte_offset + NOR_DATA_SIZE < NOR_DATA_SIZE) {
		/* Overflow */
		return TEE_ERROR_BAD_PARAMETERS;
	}

	blkcnt =
	    ROUNDUP(len + byte_offset, NOR_DATA_SIZE) / NOR_DATA_SIZE;

	if (!nor_dev_info_synced) {
		res = tee_nor_init();
		if (res != TEE_SUCCESS)
			goto func_exit;
	}

	DMSG("Read %u block%s at index %u", blkcnt, ((blkcnt > 1) ? "s" : ""), blk_idx);

	/* Allocate memory for consruct all data packets. */
        datafrm = calloc(blkcnt, NOR_DATA_SIZE);
	if (!datafrm)
		return TEE_ERROR_OUT_OF_MEMORY;

	res = g_spi_nor->read(g_spi_nor, (uint8_t *)datafrm, blk_idx * NOR_DATA_SIZE, blkcnt * NOR_DATA_SIZE);
	if (res != TEE_SUCCESS)
		goto func_exit;

	if (IS_ENABLED(CFG_NOR_FS_DEBUG_DATA)) {
		size_t i = 0;
		for (i = 0; i < blkcnt; i++) {
			DMSG("Dumping reading data by block %zu:", i);
			DHEXDUMP((uint8_t *)&datafrm[i], NOR_DATA_SIZE);
		}
	}
#if 0
	for (int i = 0; i < blkcnt; i++) {
		res = decrypt_block(data + (i * NOR_DATA_SIZE), datafrm[i].data,
				    blk_idx + i, fek, uuid);
		if (res != TEE_SUCCESS)
			goto func_exit;
		blkcnt += 1;
	}
#endif
	memcpy(data, (uint8_t *)datafrm + byte_offset, len);

	if (IS_ENABLED(CFG_NOR_FS_DEBUG_DATA)) {
		DMSG("Dumping databuf after reading:");
		DHEXDUMP(data, len);
	}

	res = TEE_SUCCESS;

func_exit:
	free(datafrm);
	return res;
}

static TEE_Result tee_nor_write_blk(uint16_t blk_idx, const uint8_t *data_blks,
				    uint16_t blkcnt, const uint8_t *fek, const TEE_UUID *uuid)
{
	TEE_Result res;
	struct nor_data_frame *datafrm = NULL;

	DMSG("Write %u block%s at index %u", blkcnt, ((blkcnt > 1) ? "s" : ""),
	     blk_idx);

	if (!data_blks || !blkcnt)
		return TEE_ERROR_BAD_PARAMETERS;

	if (!nor_dev_info_synced) {
		res = tee_nor_init();
		if (res != TEE_SUCCESS)
			goto func_exit;
	}

	/* Allocate memory for consruct all data packets. */
	datafrm = calloc(blkcnt, NOR_DATA_SIZE);
	memcpy(datafrm, data_blks, blkcnt * NOR_DATA_SIZE);
#if 0
	for (int i = 0; i < blkcnt; i++) {
		res = encrypt_block(datafrm[i].data, data_blks + (i * NOR_DATA_SIZE),
				    blk_idx + i, fek, uuid);
		if (res != TEE_SUCCESS)
			goto func_exit;
		blkcnt += 1;
	}
#endif
	res = g_spi_nor->erase(g_spi_nor, blk_idx * NOR_DATA_SIZE, blkcnt * NOR_DATA_SIZE);
	if (res != TEE_SUCCESS)
		goto func_exit;
	res = g_spi_nor->write(g_spi_nor, (uint8_t *)datafrm, blk_idx * NOR_DATA_SIZE, blkcnt * NOR_DATA_SIZE);
	if (res != TEE_SUCCESS)
		goto func_exit;

func_exit:
	free(datafrm);
	return res;
}

static bool tee_nor_write_is_atomic(uint32_t addr, uint32_t len)
{
	return true;
}

/*
 * Write NOR Flash data in bytes.
 *
 * @section    Section ID
 * @addr       Byte address of data.
 * @data       Pointer to the data.
 * @len        Size of data in bytes.
 * @fek        Encrypted File Encryption Key or NULL.
 */
static TEE_Result tee_nor_write(uint32_t addr, const uint8_t *data, uint32_t len,
				const uint8_t *fek, const TEE_UUID *uuid)
{
	TEE_Result res = TEE_ERROR_GENERIC;
	uint8_t *data_tmp = NULL;
	uint16_t blk_idx;
	uint16_t blkcnt;
	uint8_t byte_offset;

	blk_idx = addr / NOR_DATA_SIZE;
	byte_offset = addr % NOR_DATA_SIZE;

	blkcnt =
	    ROUNDUP(len + byte_offset, NOR_DATA_SIZE) / NOR_DATA_SIZE;

	if (byte_offset == 0 && (len % NOR_DATA_SIZE) == 0) {
		res = tee_nor_write_blk(blk_idx, data, blkcnt, fek, uuid);
		if (res != TEE_SUCCESS)
			goto func_exit;
	} else {
		data_tmp = calloc(blkcnt, NOR_DATA_SIZE);
		if (!data_tmp) {
			res = TEE_ERROR_OUT_OF_MEMORY;
			goto func_exit;
		}
		/* Read the complete blocks */
		res = tee_nor_read(blk_idx * NOR_DATA_SIZE, data_tmp,
				   blkcnt * NOR_DATA_SIZE, fek, uuid);
		if (res != TEE_SUCCESS)
			goto func_exit;

		/* Partial update of the data blocks */
		memcpy(data_tmp + byte_offset, data, len);
		res = tee_nor_write_blk(blk_idx, data_tmp, blkcnt, fek, uuid);
		if (res != TEE_SUCCESS)
			goto func_exit;
	}
	res = TEE_SUCCESS;

func_exit:
	free(data_tmp);
	return res;
}

/*
 * Read the NOR Flash max block.
 *
 * @counter    Pointer to receive the max block.
 */
static TEE_Result tee_nor_get_max_block(uint32_t *max_block)
{
	TEE_Result res = TEE_SUCCESS;
	uint32_t max_nor_block = CFG_NOR_FS_MAX_BLOCKS;
	if (!max_block)
		return TEE_ERROR_BAD_PARAMETERS;

	if (nor_dead)
		return TEE_ERROR_COMMUNICATION;
	if (!nor_dev_info_synced) {
		res = tee_nor_init();
		if (res != TEE_SUCCESS)
			goto func_exit;
	}
	       if (NOR_DEVICE_ID_W25R == flash_type(1)/*device id type*/){
               if(NOR_MEMORY_TYPE_W25R_64 == flash_type(2)/*memory type 64B*/)
                       max_nor_block = CFG_NOR_FS_MAX_BLOCKS / 2;
               else if(NOR_MEMORY_TYPE_W25R_128 == flash_type(2)/*memory type 128B*/)
                       max_nor_block = 4 * CFG_NOR_FS_MAX_BLOCKS;
       }

	DMSG("tee_nor_get_max_block successfully");
	*max_block = max_nor_block;

func_exit:
	return res;
}


static TEE_Result update_write_fh(struct nor_file_handle *fh,
	size_t pos, const void *buf,
	size_t size, uintptr_t new_fat,
	size_t new_size)
{
	uintptr_t old_fat = fh->fat_entry.start_address;
	size_t old_size = fh->fat_entry.data_size;
	const uint8_t *rem_buf = buf;
	size_t rem_size = size;
	uint8_t *blk_buf = NULL;
	size_t blk_offset = 0;
	size_t blk_size = 0;
	TEE_Result res = TEE_SUCCESS;

	blk_buf = mempool_alloc(mempool_default, TMP_BLOCK_SIZE);
	if (!blk_buf)
		return TEE_ERROR_OUT_OF_MEMORY;

	while (blk_offset < new_size) {
		uint8_t *copy_dst = blk_buf;
		size_t copy_size = 0;
		size_t rd_size = 0;

		blk_size = MIN(TMP_BLOCK_SIZE, new_size - blk_offset);
		memset(blk_buf, 0, blk_size);

		/* Possibly read old nor data in temporary buffer */
		if (blk_offset < pos && blk_offset < old_size) {
			rd_size = MIN(blk_size, old_size - blk_offset);
			res = tee_nor_read(old_fat + blk_offset, blk_buf,
								rd_size, fh->fat_entry.fek,
								fh->uuid);
			if (res != TEE_SUCCESS)
				break;
		}

		/* Possibly update data in temporary buffer */
		if ((blk_offset + TMP_BLOCK_SIZE > pos) &&
			(blk_offset < pos + size)) {

			size_t offset = 0;
			copy_dst = blk_buf;
			copy_size = TMP_BLOCK_SIZE;

			if (blk_offset < pos) {
				offset = pos - blk_offset;
				copy_dst += offset;
				copy_size -= offset;
			}
			copy_size = MIN(copy_size, rem_size);
			memcpy(copy_dst, rem_buf, copy_size);
			rem_buf += copy_size;
			rem_size -= copy_size;
		}

		/* Write temporary buffer to new nor destination */
		res = tee_nor_write(new_fat + blk_offset,
		blk_buf, blk_size,
		fh->fat_entry.fek, fh->uuid);
		if (res != TEE_SUCCESS)
			break;

		blk_offset += blk_size;
	}

	mempool_free(mempool_default, blk_buf);

	return res;
}

/*
 * End of lower interface to NOR device
 */

static TEE_Result get_fat_start_address(uint32_t *addr);
static TEE_Result nor_fs_setup(void);

/**
 * fat_entry_dir_free: Free the FAT entry dir.
 */
static void fat_entry_dir_free(void)
{
	if (fat_entry_dir) {
		free(fat_entry_dir->nor_fat_entry_buf);
		free(fat_entry_dir);
		fat_entry_dir = NULL;
	}
}

/**
 * fat_entry_dir_init: Initialize the FAT FS entry buffer/cache
 * This function must be called before reading FAT FS entries using the
 * function fat_entry_dir_get_next. This initializes the buffer/cache with the
 * first FAT FS entries.
 */
static TEE_Result fat_entry_dir_init(void)
{
	TEE_Result res = TEE_ERROR_GENERIC;
	struct nor_fat_entry *fe = NULL;
	uint32_t fat_address = 0;
	uint32_t num_elems_read = 0;

	if (fat_entry_dir)
		return TEE_SUCCESS;

	res = nor_fs_setup();
	if (res)
		return res;

	res = get_fat_start_address(&fat_address);
	if (res)
		return res;

	fat_entry_dir = calloc(1, sizeof(struct nor_fat_entry_dir));

	if (!fat_entry_dir)
		return TEE_ERROR_OUT_OF_MEMORY;

	/*
	 * If caching is enabled, read in up to the maximum cache size, but
	 * never more than the single read in size. Otherwise, read in as many
	 * entries fit into the temporary buffer.
	 */
	if (CFG_NOR_FS_CACHE_ENTRIES)
		num_elems_read = MIN(CFG_NOR_FS_CACHE_ENTRIES,
				     CFG_NOR_FS_RD_ENTRIES);
	else
		num_elems_read = CFG_NOR_FS_RD_ENTRIES;

	/*
	 * Allocate memory for the FAT FS entries to read in.
	 */
	fe = calloc(num_elems_read, sizeof(struct nor_fat_entry));
	if (!fe) {
		res = TEE_ERROR_OUT_OF_MEMORY;
		goto out;
	}
//        DHEXDUMP((uint8_t *)fe, num_elems_read * NOR_DATA_SIZE);
	res = tee_nor_read(fat_address, (uint8_t *)fe,
			   num_elems_read * sizeof(*fe), NULL, NULL);
//      DHEXDUMP((uint8_t *)fe, num_elems_read * NOR_DATA_SIZE);
	if (res)
		goto out;

	fat_entry_dir->nor_fat_entry_buf = fe;

	/*
	 * We use this variable when getting next entries from the buffer/cache
	 * to see whether we have to read in more entries from storage.
	 */
	fat_entry_dir->num_buffered = num_elems_read;
	return TEE_SUCCESS;
out:
	fat_entry_dir_free();
	free(fe);
	return res;
}

/**
 * fat_entry_dir_deinit: If caching is enabled, free the temporary buffer for
 * FAT FS entries in case the cache was too small. Keep the elements in the
 * cache. Reset the counter variables to start the next traversal from fresh
 * from the first cached entry. If caching is disabled, just free the
 * temporary buffer by calling fat_entry_dir_free and return.
 */
static void fat_entry_dir_deinit(void)
{
	struct nor_fat_entry *fe = NULL;

	if (!fat_entry_dir)
		return;

	if (!CFG_NOR_FS_CACHE_ENTRIES) {
		fat_entry_dir_free();
		return;
	}

	fe = fat_entry_dir->nor_fat_entry_buf;
	fat_entry_dir->idx_curr = 0;
	fat_entry_dir->num_total_read = 0;
	fat_entry_dir->last_reached = false;

	if (fat_entry_dir->num_buffered > CFG_NOR_FS_CACHE_ENTRIES) {
		fat_entry_dir->num_buffered = CFG_NOR_FS_CACHE_ENTRIES;

		fe = realloc(fe, fat_entry_dir->num_buffered * sizeof(*fe));

		/*
		 * In case realloc fails, we are on the safe side if we destroy
		 * the whole structure. Upon the next init, the cache has to be
		 * re-established, but this case should not happen in practice.
		 */
		if (!fe)
			fat_entry_dir_free();
		else
			fat_entry_dir->nor_fat_entry_buf = fe;
	}
}

/**
 * fat_entry_dir_update: Updates a persisted FAT FS entry in the cache.
 * This function updates the FAT entry fat_entry that was written to address
 * fat_address onto NOR storage in the cache.
 */
static TEE_Result __maybe_unused fat_entry_dir_update
					(struct nor_fat_entry *fat_entry,
					 uint32_t fat_address)
{
	uint32_t fat_entry_buf_idx = 0;
	/* Use a temp var to avoid compiler warning if caching disabled. */
	uint32_t max_cache_entries = CFG_NOR_FS_CACHE_ENTRIES;

	assert(!((fat_address - NOR_FS_FAT_START_ADDRESS) %
	       sizeof(struct nor_fat_entry)));

	/* Nothing to update if the cache is not initialized. */
	if (!fat_entry_dir)
		return TEE_SUCCESS;

	fat_entry_buf_idx = (fat_address - NOR_FS_FAT_START_ADDRESS) /
			     sizeof(struct nor_fat_entry);

	/* Only need to write if index points to an entry in cache. */
	if (fat_entry_buf_idx < fat_entry_dir->num_buffered &&
	    fat_entry_buf_idx < max_cache_entries) {
		memcpy(fat_entry_dir->nor_fat_entry_buf + fat_entry_buf_idx,
		       fat_entry, sizeof(struct nor_fat_entry));
	}

	return TEE_SUCCESS;
}

/**
 * fat_entry_dir_get_next: Get next FAT FS entry.
 * Read either from cache/buffer, or by reading from NOR storage if the
 * elements in the buffer/cache are fully read. When reading in from NOR
 * storage, the buffer is overwritten in case caching is disabled.
 * In case caching is enabled, the cache is either further filled, or a
 * temporary buffer populated if the cache is already full.
 * The FAT FS entry is written to fat_entry. The respective address in NOR
 * storage is written to fat_address, if not NULL. When the last FAT FS entry
 * was previously read, the function indicates this case by writing a NULL
 * pointer to fat_entry.
 * Returns a value different TEE_SUCCESS if the next FAT FS entry could not be
 * retrieved.
 */
static TEE_Result fat_entry_dir_get_next(struct nor_fat_entry **fat_entry,
					 uint32_t *fat_address)
{
	TEE_Result res = TEE_ERROR_GENERIC;
	struct nor_fat_entry *fe = NULL;
	uint32_t num_elems_read = 0;
	uint32_t fat_address_local = 0;

	assert(fat_entry_dir && fat_entry);
	/* Don't read further if we previously read the last FAT FS entry. */
	if (fat_entry_dir->last_reached) {
		*fat_entry = NULL;
		return TEE_SUCCESS;
	}
	fe = fat_entry_dir->nor_fat_entry_buf;

	/* Determine address of FAT FS entry in NOR storage. */
	fat_address_local = NOR_FS_FAT_START_ADDRESS +
			(fat_entry_dir->num_total_read *
			sizeof(struct nor_fat_entry));
	/*
	 * We've read all so-far buffered elements, so we need to
	 * read in more entries from NOR storage.
	 */
	if (fat_entry_dir->idx_curr >= fat_entry_dir->num_buffered) {
		/*
		 * This is the case where we do not cache entries, so just read
		 * in next set of FAT FS entries into the buffer.
		 * Goto the end of the when statement if that is done.
		 */
		if (!CFG_NOR_FS_CACHE_ENTRIES) {
			num_elems_read = CFG_NOR_FS_RD_ENTRIES;
			fat_entry_dir->idx_curr = 0;
			res = tee_nor_read(fat_address_local, (uint8_t *)fe,
					   num_elems_read * sizeof(*fe), NULL, NULL);
			if (res)
				return res;
			goto post_read_in;
		}

		/*
		 * We cache FAT FS entries, and the buffer is not completely
		 * filled. Further keep on extending the buffer up to its max
		 * size by reading in from NOR.
		 */
		if (fat_entry_dir->num_total_read < NOR_BUF_MAX_ENTRIES) {
			/*
			 * Read at most as many elements as fit in the buffer
			 * and no more than the defined number of entries to
			 * read in at once.
			 */
			num_elems_read = MIN(NOR_BUF_MAX_ENTRIES -
					     fat_entry_dir->num_total_read,
					     (uint32_t)CFG_NOR_FS_RD_ENTRIES);

			/*
			 * Expand the buffer to fit in the additional entries.
			 */
			fe = realloc(fe,
				     (fat_entry_dir->num_buffered +
				      num_elems_read) * sizeof(*fe));
			if (!fe)
				return TEE_ERROR_OUT_OF_MEMORY;

			fat_entry_dir->nor_fat_entry_buf = fe;

			/* Read in to the next free slot in the buffer/cache. */
			res = tee_nor_read(fat_address_local,
					   (uint8_t *)(fe + fat_entry_dir->num_total_read),
					   num_elems_read * sizeof(*fe), NULL, NULL);
			if (res)
				return res;

			fat_entry_dir->num_buffered += num_elems_read;
		} else {
			/*
			 * This happens when we have read as many elements as
			 * can possibly fit into the buffer.
			 * As the first part of the buffer serves as our cache,
			 * we only overwrite the last part that serves as our
			 * temporary buffer used to iteratively read in entries
			 * when the cache is full. Read in the temporary buffer
			 * maximum size.
			 */
			num_elems_read = CFG_NOR_FS_RD_ENTRIES;
			/* Reset index to beginning of the temporary buffer. */
			fat_entry_dir->idx_curr = CFG_NOR_FS_CACHE_ENTRIES;

			/* Read in elements after the end of the cache. */
			res = tee_nor_read(fat_address_local,
					   (uint8_t *)(fe + fat_entry_dir->idx_curr),
					   num_elems_read * sizeof(*fe), NULL, NULL);
			if (res)
				return res;
		}
	}

post_read_in:
	if (fat_address)
		*fat_address = fat_address_local;

	*fat_entry = fe + fat_entry_dir->idx_curr;

	fat_entry_dir->idx_curr++;
	fat_entry_dir->num_total_read++;

	/*
	 * Indicate last entry was read.
	 * Ensures we return a zero value for fat_entry on next invocation.
	 */
	if ((*fat_entry)->flags & FILE_IS_LAST_ENTRY)
		fat_entry_dir->last_reached = true;
	return TEE_SUCCESS;
}

#if (TRACE_LEVEL >= TRACE_FLOW)
static void dump_fat(void)
{
	TEE_Result res = TEE_ERROR_SECURITY;
	struct nor_fat_entry *fe = NULL;

	if (!fs_par)
		return;

	if (fat_entry_dir_init())
		return;

	while (true) {
		res = fat_entry_dir_get_next(&fe, NULL);
		if (res || !fe)
			break;

		FMSG("flags %#"PRIx32", size %"PRIu32", address %#"PRIx32
		     ", filename '%s'",
		     fe->flags, fe->data_size, fe->start_address, fe->filename);
	}

	fat_entry_dir_deinit();
}
#else
static void dump_fat(void)
{
}
#endif

#if (TRACE_LEVEL >= TRACE_DEBUG)
static void dump_fh(struct nor_file_handle *fh)
{
	DMSG("fh->filename=%s", fh->filename);
	DMSG("fh->nor_fat_address=%u", fh->nor_fat_address);
	DMSG("fh->fat_entry.start_address=%u", fh->fat_entry.start_address);
	DMSG("fh->fat_entry.data_size=%u", fh->fat_entry.data_size);
}
#else
static void dump_fh(struct nor_file_handle *fh __unused)
{
}
#endif

/* "/TA_uuid/object_id" or "/TA_uuid/.object_id" */
static TEE_Result create_filename(void *buf, size_t blen, struct tee_pobj *po,
				  bool transient)
{
	uint8_t *file = buf;
	uint32_t pos = 0;
	uint32_t hslen = 1 /* Leading slash */
			+ TEE_B2HS_HSBUF_SIZE(sizeof(TEE_UUID) + po->obj_id_len)
			+ 1; /* Intermediate slash */

	/* +1 for the '.' (temporary persistent object) */
	if (transient)
		hslen++;

	if (blen < hslen)
		return TEE_ERROR_SHORT_BUFFER;

	file[pos++] = '/';
	pos += tee_b2hs((uint8_t *)&po->uuid, &file[pos],
			sizeof(TEE_UUID), hslen);
	file[pos++] = '/';

	if (transient)
		file[pos++] = '.';

	tee_b2hs(po->obj_id, file + pos, po->obj_id_len, hslen - pos);

	return TEE_SUCCESS;
}

/* "/TA_uuid" */
static TEE_Result create_dirname(void *buf, size_t blen, const TEE_UUID *uuid)
{
	uint8_t *dir = buf;
	uint32_t hslen = TEE_B2HS_HSBUF_SIZE(sizeof(TEE_UUID)) + 1;

	if (blen < hslen)
		return TEE_ERROR_SHORT_BUFFER;

	dir[0] = '/';
	tee_b2hs((uint8_t *)uuid, dir + 1, sizeof(TEE_UUID), hslen);

	return TEE_SUCCESS;
}

static struct nor_file_handle *alloc_file_handle(struct tee_pobj *po,
						  bool temporary)
{
	struct nor_file_handle *fh = NULL;

	fh = calloc(1, sizeof(struct nor_file_handle));
	if (!fh)
		return NULL;

	if (po)
		create_filename(fh->filename, sizeof(fh->filename), po,
				temporary);

	return fh;
}

/**
 * write_fat_entry: Store info in a fat_entry to NOR.
 */
static TEE_Result write_fat_entry(struct nor_file_handle *fh)
{
	TEE_Result res = TEE_ERROR_GENERIC;

	/* Protect partition data. */
	if (fh->nor_fat_address < sizeof(struct nor_fs_partition)) {
		res = TEE_ERROR_ACCESS_CONFLICT;
		goto out;
	}

	if (fh->nor_fat_address % sizeof(struct nor_fat_entry) != 0) {
		res = TEE_ERROR_BAD_PARAMETERS;
		goto out;
	}

	res = tee_nor_write(fh->nor_fat_address, (uint8_t *)&fh->fat_entry,
			    sizeof(struct nor_fat_entry), NULL, NULL);

	dump_fat();

	/* If caching enabled, update a successfully written entry in cache. */
	if (CFG_NOR_FS_CACHE_ENTRIES && !res)
		res = fat_entry_dir_update(&fh->fat_entry,
					   fh->nor_fat_address);

out:
	return res;
}

/**
 * nor_fs_setup: Setup NOR Flash FS.
 * Set initial partition and FS values and write to NOR Flash.
 * Store frequently used data in RAM.
 */
static TEE_Result nor_fs_setup(void)
{
	TEE_Result res = TEE_ERROR_GENERIC;
	struct nor_fs_partition *partition_data = NULL;
	struct nor_file_handle *fh = NULL;
	uint32_t max_nor_block = 0;

	if (fs_par) {
		res = TEE_SUCCESS;
		goto out;
	}

	res = tee_nor_get_max_block(&max_nor_block);
	if (res != TEE_SUCCESS)
		goto out;

	COMPILE_TIME_ASSERT(sizeof(struct nor_fs_partition) <=
			    NOR_DATA_SIZE);
	partition_data = calloc(1, NOR_DATA_SIZE);
	if (!partition_data) {
		res = TEE_ERROR_OUT_OF_MEMORY;
		goto out;
	}

	res = tee_nor_read(NOR_STORAGE_START_ADDRESS, (uint8_t *)partition_data,
			   NOR_DATA_SIZE, NULL, NULL);
	if (res != TEE_SUCCESS)
		goto out;

#ifndef CFG_NOR_RESET_FAT
	if (partition_data->nor_fs_magic == NOR_FS_MAGIC) {
		if (partition_data->fs_version == FS_VERSION) {
			res = TEE_SUCCESS;
			goto store_fs_par;
		} else {
			EMSG("Wrong software is in use.");
			res = TEE_ERROR_ACCESS_DENIED;
			goto out;
		}
	}
#else
	EMSG("**** Clearing Storage ****");
#endif

	/* Setup new partition data. */
	partition_data->nor_fs_magic = NOR_FS_MAGIC;
	partition_data->fs_version = FS_VERSION;
	partition_data->fat_start_address = NOR_FS_FAT_START_ADDRESS;

	/* Inittee_nor_writeial FAT entry with FILE_IS_LAST_ENTRY flag set. */
	fh = alloc_file_handle(NULL, false);
	if (!fh) {
		res = TEE_ERROR_OUT_OF_MEMORY;
		goto out;
	}
	fh->fat_entry.flags = FILE_IS_LAST_ENTRY;
	fh->nor_fat_address = partition_data->fat_start_address;

	/* Write init FAT entry and partition data to NOR. */
	res = write_fat_entry(fh);
	if (res != TEE_SUCCESS)
		goto out;

	res = tee_nor_write(NOR_STORAGE_START_ADDRESS, (uint8_t *)partition_data,
                            sizeof(struct nor_fs_partition), NULL, NULL);

#ifndef CFG_NOR_RESET_FAT
store_fs_par:
#endif

	/* Store FAT start address. */
	fs_par = calloc(1, sizeof(struct nor_fs_parameters));
	if (!fs_par) {
		res = TEE_ERROR_OUT_OF_MEMORY;
		goto out;
	}

	fs_par->fat_start_address = partition_data->fat_start_address;
	fs_par->max_nor_address = max_nor_block << NOR_BLOCK_SIZE_SHIFT;

	if (NOR_DEVICE_ID_W25R == flash_type(1)/*device id type*/){
               fs_par->max_nor_address -= SERIAL_NUMBER_SIZE_RESERVE;
        }

	dump_fat();

out:
	free(fh);
	free(partition_data);
	return res;
}

/**
 * get_fat_start_address:
 * FAT start_address from fs_par.
 */
static TEE_Result get_fat_start_address(uint32_t *addr)
{
	if (!fs_par)
		return TEE_ERROR_NO_DATA;

	*addr = fs_par->fat_start_address;

	return TEE_SUCCESS;
}

/**
 * read_fat: Read FAT entries
 * Return matching FAT entry for read, rm rename and stat.
 * Build up memory pool and return matching entry for write operation.
 * "Last FAT entry" can be returned during write.
 */
static TEE_Result read_fat(struct nor_file_handle *fh, tee_mm_pool_t *p)
{
	TEE_Result res = TEE_ERROR_GENERIC;
	tee_mm_entry_t *mm = NULL;
	struct nor_fat_entry *fe = NULL;
	uint32_t fat_address;
	bool entry_found = false;
	bool expand_fat = false;
	struct nor_file_handle last_fh;

	DMSG("fat_address %d", fh->nor_fat_address);

	res = fat_entry_dir_init();
	if (res)
		goto out;

	/*
	 * The pool is used to represent the current NOR layout. To find
	 * a slot for the file tee_mm_alloc is called on the pool. Thus
	 * if it is not NULL the entire FAT must be traversed to fill in
	 * the pool.
	 */
	while (true) {
		res = fat_entry_dir_get_next(&fe, &fat_address);
		if (res || !fe)
			break;
		/*
		 * Look for an entry, matching filenames. (read, rm,
		 * rename and stat.). Only store first filename match.
		 */
		if ((!strcmp(fh->filename, fe->filename)) &&
		    (fe->flags & FILE_IS_ACTIVE) && !entry_found) {
			entry_found = true;
			fh->nor_fat_address = fat_address;
			memcpy(&fh->fat_entry, fe, sizeof(*fe));
			if (!p)
				break;
		}
		/* Add existing files to memory pool. (write) */
		if (p) {
			if ((fe->flags & FILE_IS_ACTIVE) && fe->data_size > 0) {

				mm = tee_mm_alloc2(p, fe->start_address,
						   fe->data_size);
				if (!mm) {
					res = TEE_ERROR_OUT_OF_MEMORY;
					goto out;
				}
			}
			/* Unused FAT entries can be reused (write) */
			if (((fe->flags & FILE_IS_ACTIVE) == 0) &&
			    fh->nor_fat_address == 0) {
				fh->nor_fat_address = fat_address;
				memcpy(&fh->fat_entry, fe,
				       sizeof(struct nor_fat_entry));
			}

			if (((fe->flags & FILE_IS_LAST_ENTRY) != 0) &&
			    fh->nor_fat_address == fat_address) {

				/*
				 * If the last entry was reached and was chosen
				 * by the previous check, then the FAT needs to
				 * be expanded.
				 * fh->nor_fat_address is the address chosen
				 * to store the files FAT entry and fat_address
				 * is the current FAT entry address being
				 * compared.
				 */
				expand_fat = true;
			}
		}
	}
	if (res)
		goto out;
	/*
	 * Represent the FAT table in the pool.
	 */
	if (p) {
		/*
		 * Since fat_address is the start of the last entry it needs to
		 * be moved up by an entry.
		 */
		fat_address += sizeof(struct nor_fat_entry);
		/* Make room for yet a FAT entry and add to memory pool. */
		if (expand_fat)
			fat_address += sizeof(struct nor_fat_entry);

		mm = tee_mm_alloc2(p, NOR_STORAGE_START_ADDRESS, fat_address);
		if (!mm) {
			res = TEE_ERROR_OUT_OF_MEMORY;
			goto out;
		}
		if (expand_fat) {
			/*
			 * Point fat_address to the beginning of the new
			 * entry.
			 */
			fat_address -= sizeof(struct nor_fat_entry);
			memset(&last_fh, 0, sizeof(last_fh));
			last_fh.fat_entry.flags = FILE_IS_LAST_ENTRY;
			last_fh.nor_fat_address = fat_address;
			res = write_fat_entry(&last_fh);
			if (res != TEE_SUCCESS)
				goto out;
		}
	}
	if (!fh->nor_fat_address)
		res = TEE_ERROR_ITEM_NOT_FOUND;
out:
	fat_entry_dir_deinit();
	return res;
}

static TEE_Result generate_fek(struct nor_fat_entry *fe, const TEE_UUID *uuid)
{
	TEE_Result res;

again:
	res = tee_fs_generate_fek(uuid, fe->fek, sizeof(fe->fek));
	if (res != TEE_SUCCESS)
		return res;

	if (is_zero(fe->fek, sizeof(fe->fek)))
		goto again;

	return res;
}

static TEE_Result nor_fs_open_internal(struct nor_file_handle *fh,
					const TEE_UUID *uuid, bool create)
{
	tee_mm_pool_t p;
	bool pool_result;
	TEE_Result res = TEE_ERROR_GENERIC;

	/* We need to do setup in order to make sure fs_par is filled in */
	res = nor_fs_setup();
	if (res != TEE_SUCCESS)
		goto out;

	fh->uuid = uuid;
	if (create) {
		/* Upper memory allocation must be used for NOR_FS. */
		pool_result = tee_mm_init(&p,
					  NOR_STORAGE_START_ADDRESS,
					  fs_par->max_nor_address,
					  NOR_BLOCK_SIZE_SHIFT,
					  TEE_MM_POOL_HI_ALLOC);

		if (!pool_result) {
			res = TEE_ERROR_OUT_OF_MEMORY;
			goto out;
		}

		res = read_fat(fh, &p);
		tee_mm_final(&p);
		if (res != TEE_SUCCESS)
			goto out;
	} else {
		res = read_fat(fh, NULL);
		if (res != TEE_SUCCESS)
			goto out;
	}
	/*
	 * If this is opened with create and the entry found was not active
	 * then this is a new file and the FAT entry must be written
	 */
	if (create) {
		if ((fh->fat_entry.flags & FILE_IS_ACTIVE) == 0) {
			memset(&fh->fat_entry, 0,
				sizeof(struct nor_fat_entry));
			memcpy(fh->fat_entry.filename, fh->filename,
				strlen(fh->filename));
			/* Start address and size are 0 */
			fh->fat_entry.flags = FILE_IS_ACTIVE;
//			fh->fat_entry.start_address = fs_par->max_nor_address - fh->rpmb_fat_address;
			res = generate_fek(&fh->fat_entry, uuid);
			if (res != TEE_SUCCESS)
				goto out;
			DMSG("GENERATE FEK key: %p",
			     (void *)fh->fat_entry.fek);
			DHEXDUMP(fh->fat_entry.fek, sizeof(fh->fat_entry.fek));

			res = write_fat_entry(fh);
			if (res != TEE_SUCCESS)
				goto out;
		}
	}

	res = TEE_SUCCESS;

out:
	return res;
}

static void nor_fs_close(struct tee_file_handle **tfh)
{
	struct nor_file_handle *fh = (struct nor_file_handle *)*tfh;
	free(fh);
	*tfh = NULL;
}

static TEE_Result nor_fs_read(struct tee_file_handle *tfh, size_t pos,
			       void *buf, size_t *len)
{
	TEE_Result res;
	struct nor_file_handle *fh = (struct nor_file_handle *)tfh;
	size_t size = *len;

	if (!size)
		return TEE_SUCCESS;

	mutex_lock(&nor_mutex);
	dump_fh(fh);
	res = read_fat(fh, NULL);
	dump_fh(fh);
	if (res != TEE_SUCCESS)
		goto out;
	if (pos >= fh->fat_entry.data_size) {
		*len = 0;
		goto out;
	}
	size = MIN(size, fh->fat_entry.data_size - pos);
	if (size) {
		res = tee_nor_read(fh->fat_entry.start_address + pos, buf,
				   size, fh->fat_entry.fek, fh->uuid);
		if (res != TEE_SUCCESS)
			goto out;
	}
	*len = size;
out:
	mutex_unlock(&nor_mutex);
	return res;
}

static TEE_Result nor_fs_write_primitive(struct nor_file_handle *fh,
					  size_t pos, const void *buf,
					  size_t size)
{
	TEE_Result res = TEE_ERROR_GENERIC;
	tee_mm_pool_t p = { };
	bool pool_result = false;
	size_t end = 0;
	uint32_t start_addr = 0;

	if (!size)
		return TEE_SUCCESS;

	if (!fs_par) {
		res = TEE_ERROR_GENERIC;
		goto out;
	}
	dump_fh(fh);
	/* Upper memory allocation must be used for NOR_FS. */
	pool_result = tee_mm_init(&p,
				  NOR_STORAGE_START_ADDRESS,
				  fs_par->max_nor_address,
				  NOR_BLOCK_SIZE_SHIFT,
				  TEE_MM_POOL_HI_ALLOC);
	if (!pool_result) {
		res = TEE_ERROR_OUT_OF_MEMORY;
		goto out;
	}
	res = read_fat(fh, &p);
	dump_fh(fh);
	if (res != TEE_SUCCESS)
		goto out;
	if (fh->fat_entry.flags & FILE_IS_LAST_ENTRY)
		panic("invalid last entry flag");

	if (ADD_OVERFLOW(pos, size, &end)) {
		res = TEE_ERROR_BAD_PARAMETERS;
		goto out;
	}
	if (ADD_OVERFLOW(fh->fat_entry.start_address, pos, &start_addr)) {
		res = TEE_ERROR_BAD_PARAMETERS;
		goto out;
	}

	if (end <= fh->fat_entry.data_size &&
		tee_nor_write_is_atomic(start_addr, size)) {
		DMSG("Updating data in-place");
		res = tee_nor_write(start_addr, buf, size,
				    fh->fat_entry.fek, fh->uuid);
	} else {
		/*
		 * File must be extended, or update cannot be atomic: allocate,
		 * read, update, write.
		 */
		size_t new_size = MAX(end, fh->fat_entry.data_size);
		tee_mm_entry_t *mm = tee_mm_alloc(&p, new_size);
		uintptr_t new_fat_entry = 0;

		if (!mm) {
			EMSG(" No space left in pool");
			res = TEE_ERROR_STORAGE_NO_SPACE;
			goto out;
		}

		new_fat_entry = tee_mm_get_smem(mm) ;
		DMSG("nor_fs_write_primitive, new_fat_entry = %ld", new_fat_entry);
		res = update_write_fh(fh, pos, buf, size,
			new_fat_entry, new_size);

		if (res == TEE_SUCCESS) {
			fh->fat_entry.data_size = new_size;
			fh->fat_entry.start_address = new_fat_entry ;
			res = write_fat_entry(fh);
		}
	}

out:
	if (pool_result)
		tee_mm_final(&p);

	return res;
}

static TEE_Result nor_fs_write(struct tee_file_handle *tfh, size_t pos,
				const void *buf, size_t size)
{
	TEE_Result res;

	mutex_lock(&nor_mutex);
	res = nor_fs_write_primitive((struct nor_file_handle *)tfh, pos,
				      buf, size);
	mutex_unlock(&nor_mutex);

	return res;
}

static TEE_Result nor_fs_remove_internal(struct nor_file_handle *fh)
{
	TEE_Result res;

	res = read_fat(fh, NULL);

	if (res)
		return res;
	/* Clear this file entry. */
	memset(&fh->fat_entry, 0, sizeof(struct nor_fat_entry));
	return write_fat_entry(fh);
}

static TEE_Result nor_fs_remove(struct tee_pobj *po)
{
	TEE_Result res;
	struct nor_file_handle *fh = alloc_file_handle(po, po->temporary);

	if (!fh)
		return TEE_ERROR_OUT_OF_MEMORY;

	mutex_lock(&nor_mutex);
	res = nor_fs_remove_internal(fh);
	mutex_unlock(&nor_mutex);

	free(fh);
	return res;
}

static  TEE_Result nor_fs_rename_internal(struct tee_pobj *old,
					   struct tee_pobj *new,
					   bool overwrite)
{
	TEE_Result res = TEE_ERROR_GENERIC;
	struct nor_file_handle *fh_old = NULL;
	struct nor_file_handle *fh_new = NULL;
	if (!old) {
		res = TEE_ERROR_BAD_PARAMETERS;
		goto out;
	}
	if (new)
		fh_old = alloc_file_handle(old, old->temporary);
	else
		fh_old = alloc_file_handle(old, true);
	if (!fh_old) {
		res = TEE_ERROR_OUT_OF_MEMORY;
		goto out;
	}
	if (new)
		fh_new = alloc_file_handle(new, new->temporary);
	else
		fh_new = alloc_file_handle(old, false);
	if (!fh_new) {
		res = TEE_ERROR_OUT_OF_MEMORY;
		goto out;
	}
	dump_fh(fh_old);
	res = read_fat(fh_old, NULL);
	dump_fh(fh_old);
	if (res != TEE_SUCCESS)
		goto out;
	dump_fh(fh_new);
	res = read_fat(fh_new, NULL);
	dump_fh(fh_new);
	if (res == TEE_SUCCESS) {
		if (!overwrite) {
			res = TEE_ERROR_ACCESS_CONFLICT;
			goto out;
		}

		/* Clear this file entry. */
		memset(&fh_new->fat_entry, 0, sizeof(struct nor_fat_entry));
		res = write_fat_entry(fh_new);
		if (res != TEE_SUCCESS)
			goto out;
	}
	memset(fh_old->fat_entry.filename, 0, TEE_NOR_FS_FILENAME_LENGTH);
	memcpy(fh_old->fat_entry.filename, fh_new->filename,
	       strlen(fh_new->filename));

	res = write_fat_entry(fh_old);
out:
	free(fh_old);
	free(fh_new);

	return res;
}

static  TEE_Result nor_fs_rename(struct tee_pobj *old, struct tee_pobj *new,
				  bool overwrite)
{
	TEE_Result res;

	mutex_lock(&nor_mutex);
	res = nor_fs_rename_internal(old, new, overwrite);
	mutex_unlock(&nor_mutex);

	return res;
}

static TEE_Result nor_fs_truncate(struct tee_file_handle *tfh, size_t length)
{
	struct nor_file_handle *fh = (struct nor_file_handle *)tfh;
	tee_mm_pool_t p;
	bool pool_result = false;
	tee_mm_entry_t *mm;
	uint32_t newsize;
	uint8_t *newbuf = NULL;
	uintptr_t newaddr;
	TEE_Result res = TEE_ERROR_GENERIC;

	mutex_lock(&nor_mutex);

	if (length > INT32_MAX) {
		res = TEE_ERROR_BAD_PARAMETERS;
		goto out;
	}
	newsize = length;

	res = read_fat(fh, NULL);
	if (res != TEE_SUCCESS)
		goto out;

	if (newsize > fh->fat_entry.data_size) {
		/* Extend file */

		pool_result = tee_mm_init(&p,
					  NOR_STORAGE_START_ADDRESS,
					  fs_par->max_nor_address,
					  NOR_BLOCK_SIZE_SHIFT,
					  TEE_MM_POOL_HI_ALLOC);
		if (!pool_result) {
			res = TEE_ERROR_OUT_OF_MEMORY;
			goto out;
		}
		res = read_fat(fh, &p);
		if (res != TEE_SUCCESS)
			goto out;

		mm = tee_mm_alloc(&p, newsize);
		newbuf = calloc(1, newsize);
		if (!mm || !newbuf) {
			res = TEE_ERROR_OUT_OF_MEMORY;
			goto out;
		}

		if (fh->fat_entry.data_size) {
			res = tee_nor_read(fh->fat_entry.start_address,
					   newbuf, fh->fat_entry.data_size,
					   fh->fat_entry.fek, fh->uuid);
			if (res != TEE_SUCCESS)
				goto out;
		}

		newaddr = tee_mm_get_smem(mm);
		res = tee_nor_write(newaddr, newbuf, newsize,
				    fh->fat_entry.fek, fh->uuid);
		if (res != TEE_SUCCESS)
			goto out;

	} else {
		/* Don't change file location */
		newaddr = fh->fat_entry.start_address;
	}

	/* fh->pos is unchanged */
	fh->fat_entry.data_size = newsize;
	fh->fat_entry.start_address = newaddr;
	res = write_fat_entry(fh);

out:
	mutex_unlock(&nor_mutex);
	if (pool_result)
		tee_mm_final(&p);
	if (newbuf)
		free(newbuf);

	return res;
}

static void nor_fs_dir_free(struct tee_fs_dir *dir)
{
	struct tee_nor_fs_dirent *e;

	if (!dir)
		return;

	free(dir->current);

	while ((e = SIMPLEQ_FIRST(&dir->next))) {
		SIMPLEQ_REMOVE_HEAD(&dir->next, link);
		free(e);
	}
}

static TEE_Result nor_fs_dir_populate(const char *path,
				       struct tee_fs_dir *dir)
{
	struct tee_nor_fs_dirent *current = NULL;
	struct nor_fat_entry *fe = NULL;
	uint32_t fat_address;
	uint32_t filelen;
	char *filename;
	bool matched;
	struct tee_nor_fs_dirent *next = NULL;
	uint32_t pathlen;
	TEE_Result res = TEE_ERROR_GENERIC;
	char temp;

	mutex_lock(&nor_mutex);

	res = fat_entry_dir_init();
	if (res)
		goto out;

	pathlen = strlen(path);

	while (true) {
		res = fat_entry_dir_get_next(&fe, &fat_address);
		if (res || !fe)
			break;

		filename = fe->filename;
		if (fe->flags & FILE_IS_ACTIVE) {
			matched = false;
			filelen = strlen(filename);
			if (filelen > pathlen) {
				temp = filename[pathlen];
				filename[pathlen] = '\0';
				if (strcmp(filename, path) == 0)
					matched = true;

				filename[pathlen] = temp;
			}

			if (matched) {
				next = malloc(sizeof(*next));
				if (!next) {
					res = TEE_ERROR_OUT_OF_MEMORY;
					goto out;
				}

				next->entry.oidlen = tee_hs2b((uint8_t *)
						&filename[pathlen],
						next->entry.oid,
						filelen - pathlen,
						sizeof(next->entry.oid));
				if (next->entry.oidlen) {
					SIMPLEQ_INSERT_TAIL(&dir->next,
							    next, link);
					current = next;
				} else {
					free(next);
					next = NULL;
				}
			}
		}
	}

	if (res)
		goto out;

	if (current)
		res = TEE_SUCCESS;
	else
		res = TEE_ERROR_ITEM_NOT_FOUND; /* No directories were found. */

out:
	mutex_unlock(&nor_mutex);
	fat_entry_dir_deinit();
	if (res)
		nor_fs_dir_free(dir);

	return res;
}

static TEE_Result nor_fs_opendir(const TEE_UUID *uuid, struct tee_fs_dir **dir)
{
	uint32_t len;
	char path_local[TEE_NOR_FS_FILENAME_LENGTH];
	TEE_Result res = TEE_ERROR_GENERIC;
	struct tee_fs_dir *nor_dir = NULL;

	if (!uuid || !dir) {
		res = TEE_ERROR_BAD_PARAMETERS;
		goto out;
	}

	memset(path_local, 0, sizeof(path_local));
	if (create_dirname(path_local, sizeof(path_local) - 1, uuid)) {
		res = TEE_ERROR_BAD_PARAMETERS;
		goto out;
	}
	len = strlen(path_local);

	/* Add a slash to correctly match the full directory name. */
	if (path_local[len - 1] != '/')
		path_local[len] = '/';

	nor_dir = calloc(1, sizeof(*nor_dir));
	if (!nor_dir) {
		res = TEE_ERROR_OUT_OF_MEMORY;
		goto out;
	}
	SIMPLEQ_INIT(&nor_dir->next);

	res = nor_fs_dir_populate(path_local, nor_dir);
	if (res != TEE_SUCCESS) {
		free(nor_dir);
		nor_dir = NULL;
		goto out;
	}

	*dir = nor_dir;

out:
	return res;
}

static TEE_Result nor_fs_readdir(struct tee_fs_dir *dir,
				  struct tee_fs_dirent **ent)
{
	if (!dir)
		return TEE_ERROR_GENERIC;

	free(dir->current);

	dir->current = SIMPLEQ_FIRST(&dir->next);
	if (!dir->current)
		return TEE_ERROR_ITEM_NOT_FOUND;

	SIMPLEQ_REMOVE_HEAD(&dir->next, link);

	*ent = &dir->current->entry;
	return TEE_SUCCESS;
}

static void nor_fs_closedir(struct tee_fs_dir *dir)
{
	if (dir) {
		nor_fs_dir_free(dir);
		free(dir);
	}
}

static TEE_Result nor_fs_open(struct tee_pobj *po, size_t *size,
			       struct tee_file_handle **ret_fh)
{
	TEE_Result res;
	struct nor_file_handle *fh = alloc_file_handle(po, po->temporary);

	if (!fh)
		return TEE_ERROR_OUT_OF_MEMORY;

	mutex_lock(&nor_mutex);
	res = nor_fs_open_internal(fh, &po->uuid, false);
	if (!res && size)
		*size = fh->fat_entry.data_size;
	mutex_unlock(&nor_mutex);

	if (res)
		free(fh);
	else
		*ret_fh = (struct tee_file_handle *)fh;

	return res;
}

static TEE_Result nor_fs_create(struct tee_pobj *po, bool overwrite,
				 const void *head, size_t head_size,
				 const void *attr, size_t attr_size,
				 const void *data, size_t data_size,
				 struct tee_file_handle **ret_fh)
{
	TEE_Result res;
	size_t pos = 0;
	struct nor_file_handle *fh = alloc_file_handle(po, po->temporary);

	if (!fh)
		return TEE_ERROR_OUT_OF_MEMORY;

	mutex_lock(&nor_mutex);
	res = nor_fs_open_internal(fh, &po->uuid, true);
	if (res)
		goto out;

	if (head && head_size) {
		res = nor_fs_write_primitive(fh, pos, head, head_size);
		if (res)
			goto out;
		pos += head_size;
	}

	if (attr && attr_size) {
		res = nor_fs_write_primitive(fh, pos, attr, attr_size);
		if (res)
			goto out;
		pos += attr_size;
	}

	if (data && data_size) {
		res = nor_fs_write_primitive(fh, pos, data, data_size);
		if (res)
			goto out;
	}

	if (po->temporary) {
		/*
		 * If it's a temporary filename (which it normally is)
		 * rename into the final filename now that the file is
		 * fully initialized.
		 */
		po->temporary = false;
		res = nor_fs_rename_internal(po, NULL, overwrite);
		if (res) {
			po->temporary = true;
			goto out;
		}
		/* Update file handle after rename. */
		create_filename(fh->filename, sizeof(fh->filename), po, false);
	}

out:
	if (res) {
		nor_fs_remove_internal(fh);
		free(fh);
	} else {
		*ret_fh = (struct tee_file_handle *)fh;
	}
	mutex_unlock(&nor_mutex);

	return res;
}

const struct tee_file_operations nor_fs_ops = {
	.open = nor_fs_open,
	.create = nor_fs_create,
	.close = nor_fs_close,
	.read = nor_fs_read,
	.write = nor_fs_write,
	.truncate = nor_fs_truncate,
	.rename = nor_fs_rename,
	.remove = nor_fs_remove,
	.opendir = nor_fs_opendir,
	.closedir = nor_fs_closedir,
	.readdir = nor_fs_readdir,
};

TEE_Result tee_nor_fs_raw_open(const char *fname, bool create,
				struct tee_file_handle **ret_fh)
{
	TEE_Result res;
	struct nor_file_handle *fh = calloc(1, sizeof(*fh));
	static const TEE_UUID uuid = { 0 };

	if (!fh)
		return TEE_ERROR_OUT_OF_MEMORY;

	snprintf(fh->filename, sizeof(fh->filename), "/%s", fname);

	mutex_lock(&nor_mutex);

	res = nor_fs_open_internal(fh, &uuid, create);

	mutex_unlock(&nor_mutex);

	if (res) {
		if (create)
			nor_fs_remove_internal(fh);
		free(fh);
	} else {
		*ret_fh = (struct tee_file_handle *)fh;
	}

	return res;
}

service_init_late(tee_nor_init);
