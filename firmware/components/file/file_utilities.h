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
 */
#ifndef FILE_UTILITIES_H
#define FILE_UTILITIES_H

#include <stdio.h>
#include <stdbool.h>
#include <stdint.h>
#include "system_config.h"


// Notes - for above
// file_info_bufferP must be allocated elsewhere
// FILE_MAX_FILES_PER_DIR
// FILE_MAX_CATALOG_NAMES

//
// File Utilities Constants
//

// Note define FILE_USE_SPI_IF to use a SPI connected SD Card.  Otherwise the default 4-bit
// SDMMC driver will be used.  When using SPI the following defines must be configured.
// When using SPI the SPI interface itself must be initialized before calling file_driver_init()
//   FILE_SDSPI_HOST
//   FILE_SDSPI_DMA
//   FILE_SDSPI_CS
//   FILE_SDSPI_CLK
//   FILE_SDSPI_MOSI
//   FILE_SDSPI_MISO
//   FILE_SD_CLK_HZ
// When using the SDMMC interface the following defines must be configured
//   FILE_SD_CLK_HZ


#define DEF_SD_CARD_LABEL "ICAM"

// Filesystem layout
//    ROOT
//      DCIM folder - created automatically if it does not exist
//        NNNICAMF sub-folders, each containing up to MAX_FILES_PER_DIR files, NNN is 100-999
//          ICAM_NNNN.JPG files where NNNN is 0001-9999
//          ICAM_NNNN.MJPG files where NNNN is 0001-9999
//      FW folder - created automatically if it does not exist (todo??? maybe this module doesn't deal with this)
//        esp32_M_N_fw.bin file - ESP32 firmware update file where M, N are major/minor (optional)
//        tiny1c_M_N_fw.bin file - Tiny1C firmware update file where M, N are major/minor (optional)
//
// Filesystem catalog includes only image directories.
//
// Starting sub-folder number
#define DIR_NAME_START_NUM 100

// Maximum files per directory
#define MAX_FILES_PER_DIR  FILE_MAX_FILES_PER_DIR

// Maximum number of directories
#define MAX_NUM_DIRS       FILE_MAX_DIRS

// Directory and file string lengths (including null)
//  DIR_NAME has room for "NNNICAMF" + extra to make the compiler happy
//  FILE_NAME has room for "ICAM_NNNN.JPG" + extra to make the compiler happy
#define DIR_NAME_LEN       16
#define FILE_NAME_LEN      21

// Newlib buffer size increase (see https://blog.drorgluska.com/2022/06/esp32-sd-card-optimization.html)
// Through experimentation it was discovered 8192 bytes is the largest that can be
// taken from the heap during runtime without causing memory allocation problems.
#define STREAM_BUF_SIZE    8192


//
// File System local data structure
//
typedef struct file_node_t file_node_t;

struct file_node_t {
	char* nameP;
	file_node_t* nextP;
	file_node_t* prevP;
};

typedef struct directory_node_t directory_node_t;

struct directory_node_t {
	char* nameP;
	directory_node_t* nextP;
	directory_node_t* prevP;
	file_node_t* fileP;
	int num_files;
};


//
// File Utilities API
//

// File access (file_task only)
bool file_init_driver();
bool file_get_card_mounted();
bool file_format_card();
bool file_init_card();
bool file_reinit_card();
bool file_mount_sdcard();
bool file_delete_directory(char* dir_name);
bool file_delete_file(char* dir_name, char* file_name);
bool file_open_image_write_file(FILE** fp);
bool file_open_image_read_file(char* dir_plus_file_name, FILE** fp);
char* file_get_open_write_dirname(bool* new);
char* file_get_open_write_filename();
int file_get_open_filelength(FILE* fp);
bool file_read_open_section(FILE* fp, char* buf, int start_pos, int len);
void file_close_file(FILE* fp);
void file_unmount_sdcard();

// Local filesystem info management (file_task only)
bool file_create_filesystem_info();
void file_delete_filesystem_info();
directory_node_t* file_add_directory_info(char* name);
file_node_t* file_add_file_info(directory_node_t* dirP, char* name);
void file_delete_directory_info(int n);
void file_delete_file_info(directory_node_t* dirP, int n);
int file_get_name_list(int type, char* list);

// Local filesystem info management (mutex protected for multiple task access)
directory_node_t* file_get_indexed_directory(int n);
int file_get_named_directory_index(char* name);
file_node_t* file_get_indexed_file(directory_node_t* dirP, int n);
int file_get_named_file_index(directory_node_t* dirP, char* name);
int file_get_num_directories();
int file_get_num_files();
int file_get_abs_file_index(int dir_index, int file_index);
bool file_get_indexes_from_abs(int abs_index, int* dir_index, int* file_index);
uint64_t file_get_storage_len();
uint64_t file_get_storage_free();


#endif /* FILE_UTILITIES_H */