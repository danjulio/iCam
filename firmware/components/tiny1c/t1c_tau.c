/*
 * Utility functions for computing the TAU value based on gain-specific correction table
 * information and environmental conditions.
 *
 * Copyright 2024 Dan Julio
 *
 * This is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * It is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this software.  If not, see <https://www.gnu.org/licenses/>.
 */
#include "esp_system.h"
#include "esp_log.h"
#include "esp_spiffs.h"
#include "t1c_tau.h"
#include <math.h>


//
// Local constants
//
#define SUCCESS 0
#define FAIL   -1

// Undefine to print some additional diagnostic info
#define DIAG_PRINT_VALS

// Undefine to dump tables
//#define DIAG_DUMP_TABLES

// Number of temp_array entries to search.  This is one less than the length of
// the temperature axis because Infiray reserved the last one for customer data
// but I've put nothing in there
#define SEARCHABLE_TEMP_ENTRIES (TAU_TABLE_NUM_TEMP-1)
#define SEARCHABLE_DIST_ENTRIES (TAU_TABLE_NUM_DIST)

//
// Variables
//
static const char* TAG = "t1c_tau";

static uint16_t correct_table[TAU_TABLE_SIZE]={0};

// Lookup tables to find indexes into correct_table
// (from Tiny1C Ambient Correction Document - note it doesn't use humidity at this time)
static float temp_array[] = {-5, 0, 5, 10, 15, 20, 25, 30, 35, 40, 45, 50, 55, 55};
static float dist_array[] = {0.25, 0.30, 0.35, 0.40, 0.45, 0.50, 0.55, 0.60, 0.65, 0.70, \
                             0.75, 0.80, 0.85, 0.90, 0.95, 1.00, 1.05, 1.10, 1.15, 1.20, \
                             1.30, 1.40, 1.50, 1.60, 1.70, 1.80, 1.90, 2.00, 2.20, 2.40, \
                             2.60, 2.80, 3.00, 3.20, 3.40, 3.60, 3.80, 4.00, 4.50, 5.00, \
                             5.50, 6.00, 6.50, 7.00, 7.50, 8.00, 9.00, 10.00, 11.00, 12.00, \
                             13.00, 14.00, 16.00, 18.00, 20.00, 22.00, 24.00, 26.00, 28.00, \
                             30.00, 35.00, 40.00, 45.00, 50.00};



//
// Forward declarations for internal functions
//
static void _lookup_temp_indices(float t, int* i1, float* i1_fit, int* i2, float* i2_fit);
static void _lookup_dist_indices(float d, int* i1, float* i1_fit, int* i2, float* i2_fit);
static float _lookup_tau_value(int hi, int ti, int di);
#ifdef DIAG_DUMP_TABLES
static void _dump_table(uint16_t* table, int len);
#endif


//
// API
//
int read_correct_table(int gain)
{
	FILE* f;
	esp_err_t ret;
	int n;
	int f_ret = SUCCESS;
	
	esp_vfs_spiffs_conf_t conf = {
		.base_path = "/spiffs",
		.partition_label = NULL,
		.max_files = 1,
		.format_if_mount_failed = false
	};
	
	// Initialize and mount SPIFFS filesystem.
    ret = esp_vfs_spiffs_register(&conf);
	if (ret != ESP_OK) {
        if (ret == ESP_FAIL) {
            ESP_LOGE(TAG, "Failed to mount or format filesystem");
        } else if (ret == ESP_ERR_NOT_FOUND) {
            ESP_LOGE(TAG, "Failed to find SPIFFS partition");
        } else {
            ESP_LOGE(TAG, "Failed to initialize SPIFFS (%s)", esp_err_to_name(ret));
        }
        return FAIL;
    }
    
    // Open correction file
    if (gain == HIGH_GAIN) {
    	f = fopen("/spiffs/tau_H.bin", "r");
    } else {
    	f = fopen("/spiffs/tau_L.bin", "r");
    }
    if (f == NULL) {
        ESP_LOGE(TAG, "Failed to open tau file");
        f_ret = FAIL;
        goto clean;
    }
    
    // Read the file into correct_table
    n = fread(correct_table, 1, sizeof(correct_table), f);
    if (n != sizeof(correct_table)) {
       ESP_LOGE(TAG, "Read %d bytes in tau data, expected %d", n, sizeof(correct_table));
        f_ret = FAIL;
        goto clean;
    }
    
    if (gain == HIGH_GAIN) {
    	ESP_LOGI(TAG, "Read tau table: tau_H.bin");
    } else {
    	ESP_LOGI(TAG, "Read tau table: tau_L.bin");
    }
    
#ifdef DIAG_DUMP_TABLES
    _dump_table(correct_table, TAU_TABLE_SIZE);
#endif

clean:
	if (f != NULL) {
		fclose(f);
	}
	esp_vfs_spiffs_unregister(NULL);
	return f_ret;
}


uint16_t estimate_tau(float ta, float dist, float hum)
{
	int hi;
	int ti1, ti2, di1, di2;
	float ti1_fit, ti2_fit, di1_fit, di2_fit;
	float p11, p21, p12, p22;
	float r1, r2, tau_f;
	uint16_t t1c_tau;
	
	// Bilinear Fit
	//
	//     d
	//     ^
	//     |
	// di2 |    P12       R2     P22
	//     |              P
	//     |
	// di1 |    P11       R1     P21
	//     |
	//     +---------------------------> t
	//           |                |
	//          ti1              ti2
	//
	
	// For now we just ignore the humidity value
	hi = 0;
	
	// Look up the four indices (at the one humidity level) bracketing the desired tau value
	_lookup_temp_indices(ta, &ti1, &ti1_fit, &ti2, &ti2_fit);
	_lookup_dist_indices(dist, &di1, &di1_fit, &di2, &di2_fit);
	
	// Get the integer tau values at the four points
	p11 = _lookup_tau_value(hi, ti1, di1);
	p21 = _lookup_tau_value(hi, ti2, di1);
	p12 = _lookup_tau_value(hi, ti1, di2);
	p22 = _lookup_tau_value(hi, ti2, di2);
	
	// Compute the intermediate values in the t axis
	r1 = p11 * ti1_fit + p21 * ti2_fit;
	r2 = p12 * ti1_fit + p22 * ti2_fit;
	
	// Finally compute P in the d axis
	tau_f = r1 * di1_fit + r2 * di2_fit;
	
#ifdef DIAG_PRINT_VALS
	ESP_LOGI(TAG, "estimate_tau(%1.1f, %1.1f, %1.1f) = %1.2f", ta, dist, hum, tau_f);
#endif

	t1c_tau = round(tau_f * 127) + 1;
	return t1c_tau;
}    



//
// Internal routines
//
static void _lookup_temp_indices(float t, int* i1, float* i1_fit, int* i2, float* i2_fit)
{
	int n;
	
	for (n=0; n<SEARCHABLE_TEMP_ENTRIES; n++) {
		if (t < temp_array[n]) {
			if (n == 0) {
				// Before first entry - we return both points with equal values
				*i1 = n;
				*i1_fit = 0.5;
				*i2 = n;
				*i2_fit = 0.5;
			} else {
				// Between two entries
				*i1 = n - 1;
				*i2 = n;
				*i2_fit = (t - temp_array[*i1]) / (temp_array[*i2] - temp_array[*i1]);
				*i1_fit = 1.0 - *i2_fit;
			}
			return;
		}
	}
	
	// After final entry
	*i1 = SEARCHABLE_TEMP_ENTRIES - 1;
	*i1_fit = 0.5;
	*i2 = SEARCHABLE_TEMP_ENTRIES - 1;
	*i2_fit = 0.5;
}


static void _lookup_dist_indices(float d, int* i1, float* i1_fit, int* i2, float* i2_fit)
{
	int n;
	
	for (n=0; n<SEARCHABLE_DIST_ENTRIES; n++) {
		if (d < dist_array[n]) {
			if (n == 0) {
				// Before first entry
				*i1 = n;
				*i1_fit = 0.5;
				*i2 = n;
				*i2_fit = 0.5;
			} else {
				// Between two entries
				*i1 = n - 1;
				*i2 = n;
				*i2_fit = (d - dist_array[*i1]) / (dist_array[*i2] - dist_array[*i1]);
				*i1_fit = 1.0 - *i2_fit;
			}
			return;
		}
	}
	
	// After final entry
	*i1 = SEARCHABLE_DIST_ENTRIES - 1;
	*i1_fit = 0.5;
	*i2 = SEARCHABLE_DIST_ENTRIES - 1;
	*i2_fit = 0.5;
}


static float _lookup_tau_value(int hi, int ti, int di)
{
	int index;
	
	// From the documentation
	//    "If Tau[i][j][k] represent the value of the i-th humidity point, the j-th Atmospheric
	//    temperature point and the k-th distance point (counting from 0). This value is the
	//    number of (i *64*14+j*64+k) in bin file. Read two bytes in the positions of
	//    (i*64*14+j*64+k)*2 and (i *64*14+j*64+ k)*2+1 respectively. The value resolved
	//    according to the little-end storage mode is the Tau after 14-bit quantization."
	index = hi*TAU_TABLE_NUM_DIST*TAU_TABLE_NUM_TEMP + ti*TAU_TABLE_NUM_DIST + di;
	
	if (index > TAU_TABLE_SIZE) {
		ESP_LOGE(TAG, "Tau lookup (%d) exceeded table limit [%d, %d, %d]", index, hi, ti, di);
		return 1.0;
	} else {
		return ((float) correct_table[index] / 16384.0);
	}
}


#ifdef DIAG_DUMP_TABLES
static void _dump_table(uint16_t* table, int len)
{
	for (int i=0; i<len; i++) {
		if ((i%16) == 0) {
			printf("%04X: ", i);
		}
		
		printf("%04X ", *(table+i));
		
		if ((i%16) == 15) {
			printf("\n");
		}
	}
	printf("\n");
}
#endif
