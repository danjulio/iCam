/*
 * File related utilities
 *
 * Contains functions to initialize the sdmmc interface, detect and format SD Cards,
 * create directories and write image files.
 *
 * Copyright 2020-2024 Dan Julio
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <https://www.gnu.org/licenses/>.
 *
 */
#include "file_utilities.h"
#include "sys_utilities.h"
#include "esp_system.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "ff.h"
#include "diskio_sdmmc.h"
#include "driver/sdmmc_host.h"
#include "driver/sdspi_host.h"
#include "driver/spi_master.h"
#include "sdmmc_cmd.h"
#include "vfs_fat_internal.h"
#include <errno.h>
#include <fcntl.h>
#include <string.h>
#include <sys/unistd.h>
#include <sys/stat.h>



//
// File Utilities internal constants
//
//

// Uncomment to debug SD Card related activities
//#define DEBUG_SD_CARD

// Uncomment to debug filesystem information structure (generates a lot of data)
//#define DEBUG_FS_INFO_STRUCT

//
// File Utilities internal variables
//
static const char* TAG = "file_utilities";

static const char base_path[] = "/sdcard";

#ifdef FILE_USE_SPI_IF
// SPI Host Driver
static sdmmc_host_t host_driver = SDSPI_HOST_DEFAULT();
static sdspi_device_config_t slot_config = {
	FILE_SDSPI_HOST,
	FILE_SDSPI_CS,
	SDSPI_SLOT_NO_CD,
	SDSPI_SLOT_NO_WP,
	GPIO_NUM_NC,
	SDSPI_IO_ACTIVE_LOW
};
#else
// SDMMC Host Driver
static sdmmc_host_t host_driver = SDMMC_HOST_DEFAULT();               // 4-bit, 20 MHz
static sdmmc_slot_config_t slot_config = SDMMC_SLOT_CONFIG_DEFAULT(); // No CD, WP
#endif

static sdmmc_card_t sd_card;

static bool card_mounted = false;


// Options for mounting the filesystem.
esp_vfs_fat_sdmmc_mount_config_t mount_config = {
    .format_if_mount_failed = false,
    .max_files = 5,
    .allocation_unit_size = 16 * 1024
};

static FATFS *fat_fs;     // Pointer to the filesystem object

// Static allocations for directory and file names
static bool write_dir_is_new;
static char write_dir_name[DIR_NAME_LEN];
static char write_file_name[FILE_NAME_LEN];

// Pointer to indexed storage
static void* file_info_cur_bufferP;
static directory_node_t* indexed_fs_rootP;

// Mutex to protect access to the indexed storage data structure
static SemaphoreHandle_t catalog_mutex;

// Card Info
static uint64_t card_total_bytes = 0;
static uint64_t card_free_bytes = 0;



//
// File Utilities Forward Declarations for internal functions
//
#ifdef FILE_USE_SPI_IF
static bool file_init_sdspi_driver();
#else
static bool file_init_sdmmc_driver();
#endif
static void file_get_card_stats();
static bool file_create_directory(char* dir_name);
static directory_node_t* file_insert_directory_info(char* name);
static file_node_t* file_insert_file_info(directory_node_t* dirP, char* name);
static directory_node_t* file_allocate_dir_entry();
static file_node_t* file_allocate_file_entry();
static char* file_allocate_name_entry(char* name);
static bool file_is_valid_dir(char* name);
static bool file_is_valid_name(char* name);
static FRESULT delete_node (TCHAR* path, UINT sz_buff, FILINFO* fno);
static int parse_filename_for_number(const char* name);

#ifdef DEBUG_FS_INFO_STRUCT
static void dump_filesystem_info();
#endif



//
// File Utilities API
//

/**
 * Connect the SDMCC or SDSPI driver to FATFS and initialize the host driver.  Designed to
 * be called once during startup.
 */
bool file_init_driver()
{
	// Create the indexed storage data structure protection mutex
	catalog_mutex = xSemaphoreCreateMutex();

#ifdef FILE_USE_SPI_IF
	return file_init_sdspi_driver();
#else
	return file_init_sdmmc_driver();
#endif
}


/**
 * Getter for card_mounted variable for use by file_task
 */
bool file_get_card_mounted()
{
	return card_mounted;
}


/**
 * Format the attached storage device
 */
bool file_format_card()
{
	FRESULT ret;
	const size_t workbuf_size = 4096;
	void* workbuf = NULL;
	
	// Malloc a workbuf
	workbuf = malloc(workbuf_size);
	if (workbuf == NULL) {
		ESP_LOGE(TAG, "Could not allocate work buffer for sd card format");
		return false;
	}
		
	// Partition into one partion
	DWORD plist[] = {100, 0, 0, 0};
	ESP_LOGI(TAG, "partitioning card");
	ret = f_fdisk(0, plist, workbuf);
	if (ret != FR_OK) {
		free(workbuf);
		ESP_LOGE(TAG, "Could not partition sd card");
		return false;
	}
		
	// Format the partition
	size_t alloc_unit_size = esp_vfs_fat_get_allocation_unit_size(
            sd_card.csd.sector_size,
            mount_config.allocation_unit_size);
    MKFS_PARM opt = {(BYTE)FM_ANY, 1, 0, 0, alloc_unit_size};
    ESP_LOGI(TAG, "formatting card, allocation unit size=%d", alloc_unit_size);
    ret = f_mkfs("", &opt, workbuf, workbuf_size);
    if (ret != FR_OK) {
        free(workbuf);
        ESP_LOGE(TAG, "Could not format sd card");
        return false;
    }
        
    // Need to set FF_USE_LABEL in ESP IDF components fatfs/src/ffconf.h to use this
    // Name it
    //f_setlabel(DEF_SD_CARD_LABEL);
        
    free(workbuf);
    
    return true;
}


/**
 * Probe and initialize the SD card.
 */
bool file_init_card()
{
	if (sdmmc_card_init(&host_driver, &sd_card) != ESP_OK) {
 		return false;
 	}

 	return true;
}


bool file_reinit_card()
{
	// Reconfigure the SD Slot (necessary before initializing card)
#ifndef FILE_USE_SPI_IF
	if (sdmmc_host_init_slot(host_driver.slot, &slot_config) != ESP_OK) {
		ESP_LOGE(TAG, "Could not re-initialize SD Slot");
		return false;
	}
#endif
	if (sdmmc_card_init(&host_driver, &sd_card) != ESP_OK) {
		ESP_LOGE(TAG, "Could not re-initialize SD Card");
		return false;
	}
	
	return true;
}


/**
 * Attempt to mount the SD Card
 */
bool file_mount_sdcard()
{
	FILINFO fno;
	FRESULT ret;
	
	// Attempt to mount the default drive immediately to verify it's still present
	ret = f_mount(fat_fs, "", 1);
	if (ret == FR_NO_FILESYSTEM) {
		// Card mounted but we have to put a filesystem on it
		if (!file_format_card()) {
			return false;
		}
        
        // Attempt to mount the new filesystem
        ret = f_mount(fat_fs, "", 1);
        if (ret != FR_OK) {
        	ESP_LOGE(TAG, "Could not mount sd card (%d)", ret);
        	return false;
        }
	} else if (ret != FR_OK) {
		ESP_LOGE(TAG, "Could not mount sd card (%d)", ret);
		return false;
	}
	
	// Make sure that the top-level DCIM folder exists
	ret = f_stat("/DCIM", &fno);
	if (ret == FR_NO_FILE) {
		// Need to create it
		ret = f_mkdir("/DCIM");
		if (ret == FR_OK) {
			ESP_LOGI(TAG, "Created DCIM directory");
		} else {
			ESP_LOGI(TAG, "Failed to create DCIM directory (%d)", ret);
			return false;
		}
	} else if (ret == FR_OK) {
		// Found it, make sure it's a directory and writable
		if (!(fno.fattrib & AM_DIR)) {
			ESP_LOGE(TAG, "DCIM is not a directory");
			return false;
		} else if (fno.fattrib & AM_RDO) {
			ESP_LOGE(TAG, "DCIM is not writable");
			return false;
		}
	} else {
		ESP_LOGE(TAG, "Unexpected result checking for DCIM (%d)", ret);
		return false;
	}
	
	// Update usage information every time we mount a card
	file_get_card_stats();
	
	card_mounted = true;

	return true;
}


/**
 * Delete a directory.  The directory must be empty of all tcam files before calling this
 * so the filesystem information structure is correct.  This routine will delete non-tcam
 * files (and other subdirectories).  The filesystem should be mounted.
 */
bool file_delete_directory(char* dir_name)
{
	char full_name[DIR_NAME_LEN + FILE_NAME_LEN + 32];        // include room for full file pathname + extra for delete_node work buffer
	FRESULT ret;
    FILINFO fno;
	
	// Fill a buffer with the full file name
	sprintf(full_name, "/DCIM/%s", dir_name);
	
	// Attempt to delete the directory
	ret = delete_node(full_name, sizeof(full_name), &fno);
	if (ret != 0) {
		ESP_LOGE(TAG, "Delete %s failed (%d)", full_name, ret);
		return false;
	}
	
	ESP_LOGI(TAG, "Delete directory %s", full_name);
	
	return true;
}


/**
 * Delete a file.  The filesystem should be mounted.
 */
bool file_delete_file(char* dir_name, char* file_name)
{
	char full_name[DIR_NAME_LEN + FILE_NAME_LEN + 8]; // include room for "DCIM" + '/' characters
	int ret;
	
	// Fill a buffer with the full file name
	sprintf(full_name, "/DCIM/%s/%s", dir_name, file_name);
	
	// Attempt to delete the file
	ret = f_unlink(full_name);
	if (ret != 0) {
		ESP_LOGE(TAG, "Delete %s failed (%d)", full_name, ret);
		return false;
	}
	
	ESP_LOGI(TAG, "Delete %s", full_name);
	
	return true;
}


/**
 * Open a file for writing an image or movie to and return a file pointer to it
 */
bool file_open_image_write_file(FILE** fp)
{
	char full_name[sizeof(base_path) + DIR_NAME_LEN + FILE_NAME_LEN + 9]; // include room for "DCIM" + '/' characters
	directory_node_t* dirP;
	file_node_t* fileP = NULL;
	int dir_num;
	int num_dirs = 0;
	int num_files = 0;
	int new_file_num;
	
	// Get the total number of directories, a pointer to the last directory if it exists,
	// and the number of files in it.  Then get a pointer to the last file in that directory
	// if it exists.
	dirP = indexed_fs_rootP;
	if (dirP != NULL) {
		num_dirs = 1;
		num_files = dirP->num_files;
		while (dirP->nextP != NULL) {
			dirP = dirP->nextP;
			num_dirs += 1;
			num_files = dirP->num_files;
		}
		
		fileP = dirP->fileP;
		if (fileP != NULL) {
			while (fileP->nextP != NULL) {
				fileP = fileP->nextP;
			}
		}
	}
	
	// Get the current highest numbered file from its name if possible
	if (fileP != NULL) {
		new_file_num = parse_filename_for_number(fileP->nameP) + 1;
	} else {
		new_file_num = 1;
	}
	num_files += 1;
	
	// Determine the directory number to use and if a new directory is needed
	if ((dirP == NULL) || (num_files > MAX_FILES_PER_DIR)) {
		// Need a new directory
		dir_num = DIR_NAME_START_NUM + num_dirs;
	} else {
		// Use the existing directory
		dir_num = DIR_NAME_START_NUM + (num_dirs - 1);
	}
	
	// Error if too many dirs
	if ((dir_num - DIR_NAME_START_NUM) >= MAX_NUM_DIRS) {
		ESP_LOGE(TAG, "Too many directories for writing");
		return false;
	}
	
	// Create the full directory and file names
	sprintf(write_dir_name, "%03dICAMF", dir_num);
	sprintf(write_file_name, "ICAM_%04d.JPG", new_file_num);
	
	// Create the directory if necessary (will set write_dir_is_new if necessary)
	sprintf(full_name, "DCIM/%s", write_dir_name);
	if (!file_create_directory(full_name)) {
		return false;
	}
	
	// Fill a buffer with the full VFS file name for Posix functions
	sprintf(full_name, "%s/DCIM/%s/%s", base_path, write_dir_name, write_file_name);

	// Attempt to open the file
	*fp = fopen(full_name, "w");
	if (*fp == NULL) {
		ESP_LOGE(TAG, "Could not open %s for writing", full_name);
		return false;
	} else {
		// Increase the buffer size for newlib to speed up access
		if (setvbuf(*fp, NULL, _IOFBF, STREAM_BUF_SIZE) != 0) {
  			ESP_LOGE(TAG, "write setvbuf failed");
		}
	}
	
	return true;
}


/**
 * Open a file for reading and return a file pointer to it
 */
bool file_open_image_read_file(char* dir_plus_file_name, FILE** fp)
{
	char full_name[sizeof(base_path) + DIR_NAME_LEN + FILE_NAME_LEN + 9];
	
	// Fill a buffer with the full VFS file name for Posix functions
	sprintf(full_name, "%s/DCIM/%s", base_path, dir_plus_file_name);

	// Attempt to open the file
	*fp = fopen(full_name, "r");
	if (*fp == NULL) {
		ESP_LOGE(TAG, "Could not open %s for reading", full_name);
		return false;
	} else {
		// Increase the buffer size for newlib to speed up access
		if (setvbuf(*fp, NULL, _IOFBF, STREAM_BUF_SIZE) != 0) {
  			ESP_LOGE(TAG, "read setvbuf failed");
		}
	}
	
	return true;
}


/**
 * Return a pointer to the last directory name and update a boolean flag if
 * a new directory had to be created.  Should only be called after
 * file_open_image_write_file
 */
char* file_get_open_write_dirname(bool* new)
{
	*new = write_dir_is_new;
	return write_dir_name;
}


/**
 * Return a pointer to the last filename - should only be called after
 * file_open_image_write_file
 */
char* file_get_open_write_filename()
{
	return write_file_name;
}


/**
 * Return the length of an open file.  File stream is positioned at beginning on return.
 */
int file_get_open_filelength(FILE* fp)
{
	int len;
	
	if (fseek(fp, 0, SEEK_END) == 0) {
		len = ftell(fp);
		(void) fseek(fp, 0, SEEK_SET);
		return len;
	} else {
		return 0;
	}
}


/**
 * Attempt to read a specified length of an open file into buf.
 * File stream is positioned at beginning on return.
 */
bool file_read_open_section(FILE* fp, char* buf, int start_pos, int len)
{
	int read_len;
	
	if (fseek(fp, start_pos, SEEK_SET) == 0) {
		read_len = fread(buf, 1, len, fp);
		(void) fseek(fp, 0, SEEK_SET);
		return (read_len == len);
	} else {
		return false;
	}
}


/**
 * Close a file
 */
void file_close_file(FILE* fp)
{
	fclose(fp);
}


/**
 * Unmount the sd card
 */
void file_unmount_sdcard()
{
	f_mount(0, "", 0);
	card_mounted = false;
}


/**
 * Traverse the storage medium finding tCam related directories and files and
 * create a filesystem information structure.  Should only be called on a mounted
 * filesystem.
 */
bool file_create_filesystem_info()
{
	bool success = true;
    FF_DIR top_dir;
    FF_DIR file_dir;
    FRESULT res;
    static FILINFO dir_fno;
    static FILINFO file_fno;
    char dir_name[sizeof(dir_fno.fname) + 8];
    directory_node_t* cur_dirP;
    
#ifdef DEBUG_FS_INFO_STRUCT
	ESP_LOGI(TAG, "file_create_filesystem_info()");
#endif

    // Start allocating at the start of our buffer
    file_info_cur_bufferP = file_info_bufferP;
    indexed_fs_rootP = NULL;
    
	// Open the top-level DCIM directory
	res = f_opendir(&top_dir, "/DCIM");
	if (res == FR_OK) {
		// Scan through all directories at the top level
		for (;;) {
			res = f_readdir(&top_dir, &dir_fno);
			if ((res != FR_OK) || (dir_fno.fname[0] == 0)) {
				// Break on error or end of dir
				break;
			}
			// Look for valid directories
			if ((dir_fno.fattrib & AM_DIR) && file_is_valid_dir(dir_fno.fname)) {
				// Add the directory to the filesystem information structure
				cur_dirP = file_insert_directory_info(dir_fno.fname);
				
				// Open the directory
				sprintf(dir_name, "/DCIM/%s", dir_fno.fname);
				res = f_opendir(&file_dir, dir_name);
				if (res == FR_OK) {
					// Scan through the directory
					for (;;) {
						res = f_readdir(&file_dir, &file_fno);
						if ((res != FR_OK) || (file_fno.fname[0] == 0)) {
							// Break on error or end of dir
							break;
						}
						// Look for valid files
						if (((file_fno.fattrib & AM_DIR) == 0) && (file_fno.fsize != 0) && file_is_valid_name(file_fno.fname)) {
							// Add the file to the filesystem information structure
							(void) file_insert_file_info(cur_dirP, file_fno.fname);
						}
					}
					f_closedir(&file_dir);
				}
			}
		}
		f_closedir(&top_dir);
	} else {
		success = false;
	}
#ifdef DEBUG_FS_INFO_STRUCT
	dump_filesystem_info();
#endif
	return success;
}


/**
 * Delete the filesystem information structure
 */
void file_delete_filesystem_info()
{
#ifdef DEBUG_FS_INFO_STRUCT
	ESP_LOGI(TAG, "file_delete_filesystem_info()");
#endif
	
	indexed_fs_rootP = NULL;
}


/**
 * Create a new directory information record and append it to the end of the list.
 */
directory_node_t* file_add_directory_info(char* name)
{
	char* nameP;
	directory_node_t* dirP;
	directory_node_t* newP;
	directory_node_t* prevP = NULL;
	
	xSemaphoreTake(catalog_mutex, portMAX_DELAY);
	
#ifdef DEBUG_FS_INFO_STRUCT
	ESP_LOGI(TAG, "file_add_directory_info(%s)", name);
#endif
	
	// Create a new directory record
	newP = file_allocate_dir_entry();
	if (newP != NULL) {
		newP->nextP = NULL;
		newP->prevP = NULL;
		newP->fileP = NULL;
		newP->num_files = 0;
		
		// Add the name
		nameP = file_allocate_name_entry(name);
		if (nameP != NULL) {
			strcpy(nameP, name);
		}
		newP->nameP = nameP;
	
		// Then either add it as the first record or append it to the end of the list
		if (indexed_fs_rootP == NULL) {
			indexed_fs_rootP = newP;
		} else {
			// Find the end of the directory list
			dirP = indexed_fs_rootP;
			while (dirP != NULL) {
				prevP = dirP;
				dirP = dirP->nextP;
			}
			prevP->nextP = newP;
			newP->prevP = prevP;
		}
	}
	
#ifdef DEBUG_FS_INFO_STRUCT
	dump_filesystem_info();
#endif
	
	xSemaphoreGive(catalog_mutex);
	
	return newP;
}


/**
 * Create a new file information record and append it to the end of the file list
 * for the specified dirP directory record. 
 */
file_node_t* file_add_file_info(directory_node_t* dirP, char* name)
{
	file_node_t* newP;
	
	xSemaphoreTake(catalog_mutex, portMAX_DELAY);
	
#ifdef DEBUG_FS_INFO_STRUCT
	if (dirP != NULL) {
		ESP_LOGI(TAG, "file_add_file_info(%s, %s)", dirP->nameP, name);
	} else {
		ESP_LOGI(TAG, "file_add_file_info(NULL, %s)", name);
	}
#endif
	
	newP = file_insert_file_info(dirP, name);
	
#ifdef DEBUG_FS_INFO_STRUCT
	dump_filesystem_info();
#endif
	
	xSemaphoreGive(catalog_mutex);
	
	return newP;
}


/**
 * Delete the specified directory record, connecting subsequent records to previous
 * records.  All file records pointed to by this record should have previously been
 * deleted.
 */
void file_delete_directory_info(int n)
{
	directory_node_t* dirP;
	
	xSemaphoreTake(catalog_mutex, portMAX_DELAY);
	
#ifdef DEBUG_FS_INFO_STRUCT
	ESP_LOGI(TAG, "file_delete_directory_info(%d)", n);
#endif
	
	// Find the nth directory record
	dirP = indexed_fs_rootP;
	while ((dirP != NULL) && (n != 0)) {
		dirP = dirP->nextP;
		n -= 1;
	}
	
	if (dirP != NULL) {
		// Link any subsequent record to the previous record if it exists
		if (dirP->nextP != NULL) {
			if (dirP != indexed_fs_rootP) {
				// Directory record exists previous to this record
				dirP->nextP->prevP = dirP->prevP;
				dirP->prevP->nextP = dirP->nextP;
			} else {
				// This record is first directory record pointed to by indexed_fs_rootP
				dirP->nextP->prevP = NULL;
				indexed_fs_rootP = dirP->nextP;
			}
		} else {
			if (dirP != indexed_fs_rootP) {
				// Set previous directory record as end
				dirP->prevP->nextP = NULL;
			} else {
				// No directories
				indexed_fs_rootP = NULL;
			}
		}
	}
	
#ifdef DEBUG_FS_INFO_STRUCT
	dump_filesystem_info();
#endif
	
	xSemaphoreGive(catalog_mutex);
}


/**
 * Delete the specified file record for dirP, connecting subsequent records to previous records.
 */
void file_delete_file_info(directory_node_t* dirP, int n)
{
	file_node_t* fileP;
	
	xSemaphoreTake(catalog_mutex, portMAX_DELAY);
	
#ifdef DEBUG_FS_INFO_STRUCT
	if (dirP != NULL) {
		ESP_LOGI(TAG, "file_delete_file_info(%s, %d)", dirP->nameP, n);
	} else {
		ESP_LOGI(TAG, "file_delete_file_info(NULL, %d)", n);
	}
#endif
	
	// Find the nth file record pointer for dirP
	fileP = dirP->fileP;
	while ((fileP != NULL) && (n != 0)) {
		fileP = fileP->nextP;
		n -= 1;
	}
	
	if (fileP != NULL) {
		// Link any subsequent record to the previous record if it exists
		if (fileP->nextP != NULL) {
			// File record exists beyond this record
			if (fileP != dirP->fileP) {
				// File record exists previous to this record
				fileP->nextP->prevP = fileP->prevP;
				fileP->prevP->nextP = fileP->nextP;
			} else {
				// This record is first file record pointed to by directory record
				fileP->nextP->prevP = NULL;
				dirP->fileP = fileP->nextP;
			}
		} else {
			// This is last file record
			if (fileP != dirP->fileP) {
				// Set previous file record as end
				fileP->prevP->nextP = NULL;
			} else {
				// No files
				dirP->fileP = NULL;
			}
		}
		
		// Decrement the count of files
		dirP->num_files = dirP->num_files - 1;
	}
	
#ifdef DEBUG_FS_INFO_STRUCT
	dump_filesystem_info();
#endif
	
	xSemaphoreGive(catalog_mutex);
}


/**
 * Generate a list of comma separated names.
 *   type - specify the list type (-1 for list of directory names, 0-n for list
 *          of files for the specified directory index)
 *   list - pointer to large-enough string to hold the list of comma separated names
 */
int file_get_name_list(int type, char* list)
{
	int cnt = 0;
	int n;
	directory_node_t* dirP;
	file_node_t* fileP;
	
	if (type < 0) {
		// Generate a comma separated list of directory names
		dirP = indexed_fs_rootP;
		while ((dirP != NULL) && (cnt < FILE_MAX_CATALOG_NAMES)) {
			// Copy the name into the list
			n = strlen(dirP->nameP);
			memcpy(list, dirP->nameP, n);
			list += n;
			*(list++) = ',';
			cnt++;
			// Next record
			dirP = dirP->nextP;
		}
	} else {
		// Generate a comma separated list of files for the specified directory
		dirP = file_get_indexed_directory(type);
		if (dirP != NULL) {
			// Generate a comma separated list of file names
			fileP = dirP->fileP;
			while ((fileP != NULL) && (cnt < FILE_MAX_CATALOG_NAMES)) {
				// Copy the name into the list
				n = strlen(fileP->nameP);
				memcpy(list, fileP->nameP, n);
				list += n;
				*(list++) = ',';
				cnt++;
				// Next record
				fileP = fileP->nextP;
			}
		}
	}
	
	// Null terminate the list
	*list = 0;
	
#ifdef DEBUG_FS_INFO_STRUCT
	ESP_LOGI(TAG, "%d <- file_get_name_list(%d, %s)", cnt, type, list);
#endif
	
	return cnt;
}



/**
 * Find and return the nth directory record pointer (n = 0 returns the indexed_fs_rootP).
 */
directory_node_t* file_get_indexed_directory(int n)
{
	directory_node_t* dirP;
#ifdef DEBUG_FS_INFO_STRUCT
	int sav_n = n;
#endif
	
	xSemaphoreTake(catalog_mutex, portMAX_DELAY);
	
	dirP = indexed_fs_rootP;
	
	while ((dirP != NULL) && (n-- != 0)) {
		dirP = dirP->nextP;
	}
	
#ifdef DEBUG_FS_INFO_STRUCT
	if (dirP != NULL) {
		ESP_LOGI(TAG, "%s <- file_get_indexed_directory(%d)", dirP->nameP, sav_n);
	} else {
		ESP_LOGI(TAG, "NULL <- file_get_indexed_directory(%d)", sav_n);
	}
#endif
	
	xSemaphoreGive(catalog_mutex);
	
	return dirP;
}


/**
 * Find and return the index of first directory entry matching the specified directory name.
 * Return -1 if not found.
 */
int file_get_named_directory_index(char* name)
{
	bool done = false;
	directory_node_t* dirP;
	int n = 0;
	int ret;
	
	xSemaphoreTake(catalog_mutex, portMAX_DELAY);
	
	dirP = indexed_fs_rootP;
	
	while ((dirP != NULL) && !done) {
		if (strcmp(name, dirP->nameP) == 0) {
			done = true;
		} else {
			dirP = dirP->nextP;
			n++;
		}
	}
	
	if (dirP == NULL) {
		ret = -1;
	} else {
		ret = n;
	}
	
#ifdef DEBUG_FS_INFO_STRUCT
	ESP_LOGI(TAG, "%d <- file_get_named_directory_index(%s)", ret, name);
#endif
	
	xSemaphoreGive(catalog_mutex);
	
	return ret;
}


/**
 * Starting with the dirP directory record, find and return the nth file
 * record pointer (n = 0 returns the first entry).
 */
file_node_t* file_get_indexed_file(directory_node_t* dirP, int n)
{
	file_node_t* cur_fileP;
#ifdef DEBUG_FS_INFO_STRUCT
	int sav_n = n;
#endif
	
	xSemaphoreTake(catalog_mutex, portMAX_DELAY);
	
	cur_fileP = dirP->fileP;
	while ((cur_fileP != NULL) && (n-- != 0)) {
		cur_fileP = cur_fileP->nextP;
	}
	
#ifdef DEBUG_FS_INFO_STRUCT
	if (cur_fileP != NULL) {
		ESP_LOGI(TAG, "%s <- file_get_indexed_file(%s, %d)", cur_fileP->nameP, dirP->nameP, sav_n);
	} else {
		ESP_LOGI(TAG, "NULL <- file_get_indexed_file(%s, %d)", dirP->nameP, sav_n);
	}
#endif
	
	xSemaphoreGive(catalog_mutex);
	
	return cur_fileP;
}


/**
 * Find and return the index of first file entry matching the specified file name.
 * Return -1 if not found.
 */
int file_get_named_file_index(directory_node_t* dirP, char* name)
{
	bool done = false;
	file_node_t* cur_fileP;
	int n = 0;
	int ret;
	
	xSemaphoreTake(catalog_mutex, portMAX_DELAY);
	
	cur_fileP = dirP->fileP;
	while ((cur_fileP != NULL) && !done) {
		if (strcmp(name, cur_fileP->nameP) == 0) {
			done = true;
		} else {
			cur_fileP = cur_fileP->nextP;
			n++;
		}
	}
	
	if (cur_fileP == NULL) {
		ret = -1;
	} else {
		ret = n;
	}
	
#ifdef DEBUG_FS_INFO_STRUCT
	if (dirP != NULL) {
		ESP_LOGI(TAG, "%d <- file_get_named_file_index(%s, %s)", ret, dirP->nameP, name);
	} else {
		ESP_LOGI(TAG, "%d <- file_get_named_file_index(NULL, %s)", ret, name);
	}
#endif
	
	xSemaphoreGive(catalog_mutex);
	
	return ret;
}


/**
 * Return the number of directories
 */
int file_get_num_directories()
{
	int n = 0;
	directory_node_t* dirP;
	
	xSemaphoreTake(catalog_mutex, portMAX_DELAY);
	
	dirP = indexed_fs_rootP;
	
	while (dirP != NULL) {
		dirP = dirP->nextP;
		n += 1;
	}
	
#ifdef DEBUG_FS_INFO_STRUCT
	ESP_LOGI(TAG, "%d <- file_get_num_directories()", n);
#endif
	
	xSemaphoreGive(catalog_mutex);
	
	return n;
}


/**
 * Return the total number of files
 */
int file_get_num_files()
{
	int n = 0;
	directory_node_t* dirP;
	
	xSemaphoreTake(catalog_mutex, portMAX_DELAY);
	
	dirP = indexed_fs_rootP;
	
	while (dirP != NULL) {
		n += dirP->num_files;
		dirP = dirP->nextP;
	}
	
#ifdef DEBUG_FS_INFO_STRUCT
	ESP_LOGI(TAG, "%d <- file_get_num_files()", n);
#endif
	
	xSemaphoreGive(catalog_mutex);
	
	return n;
}


/**
 * Return the absolute file index for the given dir_index and file_index in that directory node.
 * Return -1 if it does not exist.
 */
int file_get_abs_file_index(int dir_index, int file_index)
{
	int abs_file_index = 0;
	int n;
	directory_node_t* dirP;
	
	xSemaphoreTake(catalog_mutex, portMAX_DELAY);

	dirP = indexed_fs_rootP;
	
	// Include all file counts up to the specified dir_index
	n = dir_index - 1;
	while ((dirP != NULL) && (n-- >= 0)) {
		abs_file_index += dirP->num_files;
		dirP = dirP->nextP;
	}
	
	if (dirP == NULL) {
		// Can't find the directory
		abs_file_index = -1;
	} else {
		// Add the file_index in the current directory if it exists
		if (file_index < dirP->num_files) {
			abs_file_index += file_index;
		} else {
			// File doesn't exist
			abs_file_index = -1;
		}
	}
	
#ifdef DEBUG_FS_INFO_STRUCT
	ESP_LOGI(TAG, "%d <- file_get_abs_file_index(%d, %d)", n, dir_index, file_index);
#endif
	
	xSemaphoreGive(catalog_mutex);
	
	return abs_file_index;
}


/**
 * Set the dir_index and file_index from an absolute file index.  Returns false if the
 * file does not exist.
 */
bool file_get_indexes_from_abs(int abs_index, int* dir_index, int* file_index)
{
	bool ret = true;
	int cur_file_total = 0;
	int dir_i = 0;
	
	xSemaphoreTake(catalog_mutex, portMAX_DELAY);
	
	directory_node_t* dirP;
	
	dirP = indexed_fs_rootP;
	
	// Scan through directory nodes until we find the total number of files greater than abs_index
	while ((dirP != NULL) && (cur_file_total <= abs_index)) {
		cur_file_total += dirP->num_files;
		if (cur_file_total <= abs_index) {
			// Next directory
			dirP = dirP->nextP;
			dir_i++;
		}
	}
	
	// If the abs_index is in the last directory then compute the diretory and file_index in the directory
	if (abs_index < cur_file_total) {
		// Found directory that has file
		*dir_index = dir_i;
		*file_index = dirP->num_files - (cur_file_total - abs_index);
	} else {
		// Did not find abs_index
		ret = false;
	}
	
#ifdef DEBUG_FS_INFO_STRUCT
	ESP_LOGI(TAG, "%d <- file_get_indexes_from_abs(%d, * %d, * %d)", ret, abs_index, *dir_index, *file_index);
#endif
	
	xSemaphoreGive(catalog_mutex);
	
	return ret;
}


/**
 * Return storage utilization information
 */
uint64_t file_get_storage_len()
{
	return card_total_bytes;
}


uint64_t file_get_storage_free()
{
	return card_free_bytes;
}



//
// File Utilities internal functions
//
/**
 * Interface-specific initialization.
 */
#ifdef FILE_USE_SPI_IF
static bool file_init_sdspi_driver()
{
	esp_err_t ret;
	
	// Initialize the SPI driver
	spi_bus_config_t spi_buscfg = {
		.miso_io_num=FILE_SDSPI_MISO,
		.mosi_io_num=FILE_SDSPI_MOSI,
		.sclk_io_num=FILE_SDSPI_CLK,
		.max_transfer_sz=STREAM_BUF_SIZE,
		.quadwp_io_num=-1,
		.quadhd_io_num=-1
	};
	if (spi_bus_initialize(FILE_SDSPI_HOST, &spi_buscfg, FILE_SDSPI_DMA) != ESP_OK) {
		ESP_LOGE(TAG, "SD SPI Master initialization failed");
		return false;
	}
	
	ret = sdspi_host_init_device((const sdspi_device_config_t*) &slot_config, &host_driver.slot);
	if (ret != ESP_OK) {
		ESP_LOGI(TAG, "Could not initialize SDSPI device (%d)", ret);
		return false;
	}
	
	// Reconfigure speed if necessary
	ret = sdspi_host_set_card_clk(host_driver.slot, FILE_SD_CLK_HZ / 1000);
	if (ret != ESP_OK) {
		ESP_LOGI(TAG, "Could not set SDSPI speed (%d)", ret);
		return false;
	}
	
	// Register FATFS with the VFS component
	// Since we only have one drive, use the default drive number (0)
	ret = esp_vfs_fat_register(base_path, "", mount_config.max_files, &fat_fs);
	if (ret != ESP_OK) {
		ESP_LOGI(TAG, "Could not register FATFS (%d)", ret);
		return false;
	}
	
	// Register FATFS with our card (which will point to the driver when initialized)
 	ff_diskio_register_sdmmc(0, &sd_card);
 	
 	return true;
}
#else
static bool file_init_sdmmc_driver()
{
	esp_err_t ret;
	
	// Set driver clock rate
	host_driver.max_freq_khz = FILE_SD_CLK_HZ / 1000;
		
	// Initialize the driver
	ret = host_driver.init();
	if (ret != ESP_OK) {
		ESP_LOGI(TAG, "Could not initialize SDMMC driver (%d)", ret);
		return false;
	}
	
	// Configure the SD Slot
	ret = sdmmc_host_init_slot(host_driver.slot, &slot_config);
	if (ret != ESP_OK) {
		ESP_LOGI(TAG, "Could not initialize SD Slot (%d)", ret);
		return false;
	}
	
	// Register FATFS with the VFS component
	// Since we only have one drive, use the default drive number (0)
	ret = esp_vfs_fat_register(base_path, "", mount_config.max_files, &fat_fs);
	if (ret != ESP_OK) {
		ESP_LOGI(TAG, "Could not register FATFS (%d)", ret);
		return false;
	}
	
	// Register FATFS with our card (which will point to the driver when initialized)
 	ff_diskio_register_sdmmc(0, &sd_card);
	
	return true;
}
#endif


/**
 * Update card size and free space information.  Call after FS mounted.
 */
static void file_get_card_stats()
{
	FRESULT ret;
	FATFS *fs;
    DWORD fre_clust;

	ret = f_getfree("0:", &fre_clust, &fs);
	if (ret != FR_OK) {
		ESP_LOGE(TAG, "Could not get cluster info - %d", ret);
		card_total_bytes = 0;
		card_free_bytes = 0;
	} else {
    	card_total_bytes = (uint64_t) sd_card.csd.sector_size * (fs->n_fatent - 2) * fs->csize;
		card_free_bytes = (uint64_t) sd_card.csd.sector_size * fre_clust * fs->csize;
#ifdef DEBUG_SD_CARD
		ESP_LOGI(TAG, "SD Card sector size = %d", sd_card.csd.sector_size);
#endif
	}
}


/**
 * Create a directory, do nothing if the directory already exists.  Update write_dir_is_new.
 */
static bool file_create_directory(char* dir_name)
{
	FRESULT ret;
	
	// Assume directory exists
	write_dir_is_new = false;
	
	// Check if the directory already exists
	ret = f_stat(dir_name, NULL);
	if (ret == FR_NO_FILE) {
		// Create the directory
		ret = f_mkdir(dir_name);
		if (ret != FR_OK) {
			ESP_LOGE(TAG, "Could not create directory %s (%d)", dir_name, ret);
			return false;
		}
		write_dir_is_new = true;
		return true;
	} else if (ret != FR_OK) {
		// Something went wrong
		ESP_LOGE(TAG, "Could not stat directory %s (%d)", dir_name, ret);
		return false;
	} else {
		// Directory exists already - no need to do anything
		return true;
	}
}


// If not maybe it should and store # in entry.  And for writing we could have a way of 
// getting last entry.  Then we could see how many files and act accordingly
static directory_node_t* file_insert_directory_info(char* name)
{
	bool done = false;
	char* nameP;
	directory_node_t* dirP;
	directory_node_t* newP;
	
	// Create a new directory record
	newP = file_allocate_dir_entry();
	if (newP != NULL) {
		newP->nextP = NULL;
		newP->prevP = NULL;
		newP->fileP = NULL;
		newP->num_files = 0;
		
		// Add the name
		nameP = file_allocate_name_entry(name);
		if (nameP != NULL) {
			strcpy(nameP, name);
		}
		newP->nameP = nameP;
		
		// Then either add it as the first record or insert it alphabetically in list
		if (indexed_fs_rootP == NULL) {
			indexed_fs_rootP = newP;
		} else {
			// Insert it before the first entry that is greater than it
			dirP = indexed_fs_rootP;
			while (!done) {
				if (strcmp(dirP->nameP, name) > 0) {
					// Insert before dirP
					if (dirP == indexed_fs_rootP) {
						// Insert at head of list
						newP->nextP = indexed_fs_rootP;
						indexed_fs_rootP = newP;
						dirP->prevP = newP;
					} else {
						// Insert in the middle of list
						newP->nextP = dirP;
						newP->prevP = dirP->prevP;
						dirP->prevP->nextP = newP;
						dirP->prevP = newP;
					}
					done = true;
				} else if (dirP->nextP == NULL) {
					// Insert at end of list
					dirP->nextP = newP;
					newP->prevP = dirP;
					done = true;
				} else {
					// Move to next record
					dirP = dirP->nextP;
				}
			}
		}
	}
	
	return newP;
}


// It might have to change with different .types JPG, MJPG
// or maybe not, we always key on number of files in a dir to figure out next one
static file_node_t* file_insert_file_info(directory_node_t* dirP, char* name)
{
	bool done = false;
	char* nameP;
	file_node_t* fileP;
	file_node_t* newP;
	
	// Create a new file record
	newP = file_allocate_file_entry();
	if (newP != NULL) {
		newP->nextP = NULL;
		newP->prevP = NULL;
	
		// Add the name
		nameP = file_allocate_name_entry(name);
		if (nameP != NULL) {
			strcpy(nameP, name);
		}
		newP->nameP = nameP;
	
		// Then ether add it as the first record or insert it alphabetically in list
		if (dirP->fileP == NULL) {
			dirP->fileP = newP;
		} else {
			// Insert it before the first entry that is greater than it
			fileP = dirP->fileP;
			while (!done) {
				if (strcmp(fileP->nameP, name) > 0) {
					// Insert before fileP
					if (fileP == dirP->fileP) {
						// Insert at head of list
						newP->nextP = fileP;
						dirP->fileP = newP;
						fileP->prevP = newP;
					} else {
						// Insert in the middle of list
						newP->nextP = fileP;
						newP->prevP = fileP->prevP;
						fileP->prevP->nextP = newP;
						fileP->prevP = newP;
					}
					done = true;
				} else if (fileP->nextP == NULL) {
					// Insert at end of list
					fileP->nextP = newP;
					newP->prevP = fileP;
					done = true;
				} else {
					// Move to next record
					fileP = fileP->nextP;
				}
			}
		}
	
		// Increment the file count
		dirP->num_files = dirP->num_files+1;
	}
	
	return newP;
}


static directory_node_t* file_allocate_dir_entry()
{
	int len = sizeof(directory_node_t);
	directory_node_t* dirP;
	
	if (file_info_cur_bufferP >= file_info_bufferP + FILE_INFO_BUFFER_LEN) {
		dirP = NULL;
		ESP_LOGE(TAG, "filesystem information structure directory allocate failed");
	} else {
		dirP = (directory_node_t*) file_info_cur_bufferP;
		file_info_cur_bufferP += len;	
	}
	
	return dirP;
}


static file_node_t* file_allocate_file_entry()
{
	int len = sizeof(file_node_t);
	file_node_t* fileP;
	
	if (file_info_cur_bufferP >= file_info_bufferP + FILE_INFO_BUFFER_LEN) {
		fileP = NULL;
		ESP_LOGE(TAG, "filesystem information structure file allocate failed");
	} else {
		fileP = (file_node_t*) file_info_cur_bufferP;
		file_info_cur_bufferP += len;	
	}
	
	return fileP;
}


static char* file_allocate_name_entry(char* name)
{
	char* nameP;
	int len = strlen(name) + 1;
	
	if (file_info_cur_bufferP >= file_info_bufferP + FILE_INFO_BUFFER_LEN) {
		nameP = NULL;
		ESP_LOGE(TAG, "filesystem information structure name allocate failed");
	} else {
		// Make sure length is 32-bit aligned
		if (len & 0x3) {
			len = (len & 0xFFFFFFFC) + 4;
		}
		nameP = (char*) file_info_cur_bufferP;
		file_info_cur_bufferP += len;	
	}
	
	return nameP;
}


// Looking for "N...ICAMF" where N is a number
static bool file_is_valid_dir(char* name)
{
	int n;
	
	// First check that the first digit is a number
	// Note: This will filter out Mac OS X ".NNNICAMF" names
	if ((*name < '0') || (*name > '9')) {
		return false;
	}
	
	// Then make sure the final five characters match "ICAMF"
	n = strlen(name);
	if (n < 6) {
		return false;
	}
	if (strncmp((name + (n - 5)), "ICAMF", 5) != 0) {
		return false;
	}
	
	return true;
}


// Looking for "ICAM...JPG" or "ICAM...MJPG"
static bool file_is_valid_name(char* name)
{
	int n;
	
	n = strlen(name);
	if (n < 9) {
		return false;
	}
	
	// Look for "ICAM" in the first four locations
	if (strncmp(name, "ICAM", 4) != 0) {
		return false;
	}
	
	// Look for either ".JPG" or ".MJPG" in the last locations
	if (strncmp((name + (n - 4)), ".JPG", 4) != 0) {
		if (strncmp((name + (n - 5)), ".MJPG", 5) != 0) {
			return false;
		}
	}
	
	return true;
}


/**
 * Recursive directory deletion routine from elmchan.org (http://elm-chan.org/fsw/ff/res/app2.c)
 */
FRESULT delete_node (
    TCHAR* path,    /* Path name buffer with the sub-directory to delete */
    UINT sz_buff,   /* Size of path name buffer (items) */
    FILINFO* fno    /* Name read buffer */
)
{
	UINT i, j;
    FRESULT fr;
    FF_DIR dir;


    fr = f_opendir(&dir, path); /* Open the sub-directory to make it empty */
    if (fr != FR_OK) return fr;

    for (i = 0; path[i]; i++) ; /* Get current path length */
    path[i++] = _T('/');

    for (;;) {
        fr = f_readdir(&dir, fno);  /* Get a directory item */
        if (fr != FR_OK || !fno->fname[0]) break;   /* End of directory? */
        j = 0;
        do {    /* Make a path name */
            if (i + j >= sz_buff) { /* Buffer over flow? */
                fr = 100; break;    /* Fails with 100 when buffer overflow */
            }
            path[i + j] = fno->fname[j];
        } while (fno->fname[j++]);
        if (fno->fattrib & AM_DIR) {    /* Item is a sub-directory */
            fr = delete_node(path, sz_buff, fno);
        } else {                        /* Item is a file */
            fr = f_unlink(path);
        }
        if (fr != FR_OK) break;
    }

    path[--i] = 0;  /* Restore the path name */
    f_closedir(&dir);

    if (fr == FR_OK) fr = f_unlink(path);  /* Delete the empty sub-directory */
    return fr;
}


/**
 * Get the numeric portion of an image filename and return it as an integer
 */
static int parse_filename_for_number(const char* name)
{
	char c;
	const char* cP = name;
	int n = 0;
	
	while ((c = *cP++) != 0) {
		if (c == '.') {
			break;
		} else if ((c >= '0') && (c <= '9')) {
			n = n*10 + (c - '0');
		}
	}
	
	return n;
}


/**
 * Dump the filesystem information structure
 */
#ifdef DEBUG_FS_INFO_STRUCT
static void dump_filesystem_info()
{
	directory_node_t* cur_dirP;
	file_node_t* cur_fileP;
	
	ESP_LOGI(TAG, "filesystem information structure requires %d bytes", (int) (file_info_cur_bufferP - file_info_bufferP));
	
	cur_dirP = indexed_fs_rootP;
	while (cur_dirP != NULL) {
		if (cur_dirP->nextP == NULL) {
			ESP_LOGI(TAG, "Directory: %s (%d files): (next NULL)", cur_dirP->nameP, cur_dirP->num_files);
		} else {
			ESP_LOGI(TAG, "Directory: %s (%d files):", cur_dirP->nameP, cur_dirP->num_files);
		}
		cur_fileP = cur_dirP->fileP;
		cur_dirP = cur_dirP->nextP;
		while (cur_fileP != NULL) {
			if (cur_fileP->prevP == NULL) {
				if (cur_fileP->nextP == NULL) {
					ESP_LOGI(TAG, "  File: %s (prev NULL, next NULL)", cur_fileP->nameP);
				} else {
					ESP_LOGI(TAG, "  File: %s (prev NULL)", cur_fileP->nameP);
				}
			} else {
				if (cur_fileP->nextP == NULL) {
					ESP_LOGI(TAG, "  File: %s (next NULL)", cur_fileP->nameP);
				} else {
					ESP_LOGI(TAG, "  File: %s", cur_fileP->nameP);
				}
			}
			cur_fileP = cur_fileP->nextP;
		}
	}
}
#endif
