/*
 * WiFi related utilities
 *
 * Contains functions to initialize and query the wifi interface.  Also includes
 * the system event handler for use by the wifi system.
 *
 * Note: Currently only 1 station is allowed to connect at a time.
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
#include "wifi_utilities.h"
#include "ps_utilities.h"
#include "esp_system.h"
#include "esp_app_desc.h"
#include "esp_log.h"
#include "esp_event.h"
#include "esp_mac.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "freertos/FreeRTOS.h"
#include "mdns.h"
#include "nvs_flash.h"
#include <string.h>



//
// Wifi Utilities local constants
//
#define WIFI_INFO_FLAG_INITIALIZED    0x01
#define WIFI_INFO_FLAG_ENABLED        0x02
#define WIFI_INFO_FLAG_CONNECTED      0x04



//
// Wifi Utilities local variables
//
static const char* TAG = "wifi_utilities";

// Wifi netif instance (changed each time wifi is re-started)
static esp_netif_t *wifi_netif;

// Wifi locale configuration
static const wifi_country_t def_country_info = {
	"US",
	1,
	11,
	20,
	WIFI_COUNTRY_POLICY_AUTO
};

// Wifi configuration
static net_config_t wifi_config;

// Internal state
static bool mdns_running = false;
static bool sta_connected = false; // Set when we connect to an AP so we can disconnect if we restart
static int sta_retry_num = 0;
static uint8_t wifi_flags = 0;

// mDNS TXT records
#define NUM_SERVICE_TXT_ITEMS 2
static mdns_txt_item_t service_txt_data[NUM_SERVICE_TXT_ITEMS];
static char* txt_item_keys[NUM_SERVICE_TXT_ITEMS] = {
	"ip4v_address",
	"version"
};



//
// WiFi Utilities Forward Declarations for internal functions
//
static bool init_esp_wifi();
static bool enable_esp_wifi_ap();
static bool enable_esp_wifi_client();
static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data);
static bool start_mdns();



//
// WiFi Utilities API
//

/**
 * Power-on initialization of the WiFi system.  It is enabled based on start-up
 * information from persistent storage.  Returns false if any part of the initialization
 * fails.  Assumes the NVS flash subsystem has already been initialized (by PS).
 */
bool wifi_init()
{
	esp_err_t ret;
	
	// Initialize the TCP/IP stack
	ret = esp_netif_init();
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Could not init netif (%d)", ret);
		return false;
	}
	
	// Setup the default event handlers
	ret = esp_event_loop_create_default();
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Could not create default event loop handler (%d)", ret);
		return false;
	}
	
	ret = esp_event_handler_register(WIFI_EVENT, ESP_EVENT_ANY_ID, &wifi_event_handler, NULL);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Could not register wifi_event_handler (%d)", ret);
		return false;
	}
	
	ret = esp_event_handler_register(IP_EVENT, IP_EVENT_ETH_GOT_IP, &ip_event_handler, NULL);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Could not register ip_event_handler (%d)", ret);
		return false;
	}
	
	// Get our wifi configuration
	ps_get_config(PS_CONFIG_TYPE_NET, &wifi_config);
	
	// Initialize the WiFi interface
	if (init_esp_wifi()) {
		wifi_flags |= WIFI_INFO_FLAG_INITIALIZED;
		ESP_LOGI(TAG, "WiFi initialized");
		
		// Configure the WiFi interface if enabled
		if (wifi_config.sta_mode) {
			if (enable_esp_wifi_client()) {
				wifi_flags |= WIFI_INFO_FLAG_ENABLED;
				ESP_LOGI(TAG, "WiFi Station starting");
			} else {
				return false;
			}
		} else {
			if (enable_esp_wifi_ap()) {
				wifi_flags |= WIFI_INFO_FLAG_ENABLED;
				ESP_LOGI(TAG, "WiFi AP %s enabled", wifi_config.ap_ssid);
			} else {
				return false;
			}
		}
	} else {
		ESP_LOGE(TAG, "WiFi Initialization failed");
		return false;
	}
	
	// Start advertising our name and address for others to find us if enabled
	if (wifi_config.mdns_en) {
		if (start_mdns()) {
			ESP_LOGI(TAG, "mDNS enabled");
		}
	}
	
	return true;
}


/**
 * Re-initialize the WiFi system when information such as the SSID, password or enable-
 * state have changed.  Returns false if anything fails.
 */
bool wifi_reinit()
{
	// Disable mDNS
	if (wifi_config.mdns_en) {
		mdns_free();
		mdns_running = false;
	}
	
	// Attempt to disconnect from an AP if we were previously connected
	if (sta_connected) {
		ESP_LOGI(TAG, "Attempting to disconnect from AP");
		esp_wifi_disconnect();
		sta_connected = false;
	}
	
	// Shut down the old configuration
	if ((wifi_flags & WIFI_INFO_FLAG_ENABLED) != 0) {
		ESP_LOGI(TAG, "WiFi stopping");
		esp_wifi_stop();
		wifi_flags &= ~WIFI_INFO_FLAG_ENABLED;
	}
	
	// Destroy the associated esp_netif object
	esp_netif_destroy_default_wifi(wifi_netif);
	wifi_netif = NULL;
	
	// Nothing should be connected now
	wifi_flags &= ~WIFI_INFO_FLAG_CONNECTED;

	if ((wifi_flags & WIFI_INFO_FLAG_INITIALIZED) == 0) {
		// Attempt to initialize the wifi interface again
		if (!init_esp_wifi()) {
			wifi_flags &= ~WIFI_INFO_FLAG_INITIALIZED;
			return false;
		}
		wifi_flags |= WIFI_INFO_FLAG_INITIALIZED;
	}
	
	// Update the wifi info because we're called when it's updated
	ps_get_config(PS_CONFIG_TYPE_NET, &wifi_config);
	
	// Reconfigure the interface if enabled
	if (wifi_config.sta_mode) {
		if (enable_esp_wifi_client()) {
			wifi_flags |= WIFI_INFO_FLAG_ENABLED;
			ESP_LOGI(TAG, "WiFi Station starting");
		} else {
			return false;
		}
	} else {
		if (enable_esp_wifi_ap()) {
			wifi_flags |= WIFI_INFO_FLAG_ENABLED;
			ESP_LOGI(TAG, "WiFi AP %s enabled", wifi_config.ap_ssid);
		} else {
			return false;
		}
	}
	
	// Re-enabled mDNS
	if (wifi_config.mdns_en) {
		if (start_mdns()) {
			ESP_LOGI(TAG, "mDNS enabled");
		}
	}
	
	return true;
}


/**
 * Controls mDNS during runtime
 */
bool wifi_en_mdns(bool en)
{
	if (en) {
		if (!mdns_running) {
			if (start_mdns()) {
				ESP_LOGI(TAG, "mDNS enabled");
			} else {
				return false;
			}
		}
	} else {
		if (mdns_running) {
			ESP_LOGI(TAG, "mDNS disabled");
			mdns_free();
			mdns_running = false;
		}
	}
	
	return true;
}


/**
 * Returns enabled status
 */
bool wifi_is_enabled()
{
	return ((wifi_flags & WIFI_INFO_FLAG_ENABLED) != 0);
}



/**
 * Return connected to network status
 */
bool wifi_is_connected()
{
	return ((wifi_flags & WIFI_INFO_FLAG_CONNECTED) != 0);
}


void wifi_get_ipv4_addr(char* s)
{
	esp_netif_ip_info_t ip_info;
	
	esp_netif_get_ip_info(wifi_netif, &ip_info);
	sprintf(s, IPSTR, IP2STR(&ip_info.ip));
}



//
// WiFi Utilities internal functions
//

/**
 * Initialize the WiFi interface resources
 */
static bool init_esp_wifi()
{
	esp_err_t ret;
	wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
	
	ret = esp_wifi_init(&cfg);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Could not allocate wifi resources (%d)", ret);
		return false;
	}
	
	// We don't need the NVS configuration storage for the WiFi configuration since we
	// are managing persistent storage ourselves
	ret = esp_wifi_set_storage(WIFI_STORAGE_RAM);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Could not set RAM storage for configuration (%d)", ret);
		return false;
	}
		
	// Setup WiFi country restrictions to US/AUTO
	ret = esp_wifi_set_country(&def_country_info);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Could not set default country configuration - %x", ret);
		return false;
	}
	
	return true;
}


/**
 * Enable this device as a Soft AP
 */
static bool enable_esp_wifi_ap()
{
	esp_err_t ret;
	
	// Create the esp_netif object
	wifi_netif = esp_netif_create_default_wifi_ap();
	
	// Enable the AP
	wifi_config_t wifi_driver_config = {
        .ap = {
            .ssid_len = strlen(wifi_config.ap_ssid),
            .max_connection = 1,
            .authmode = WIFI_AUTH_WPA_WPA2_PSK
        }
    };
    strcpy((char*) wifi_driver_config.ap.ssid, wifi_config.ap_ssid);
    strcpy((char*) wifi_driver_config.ap.password, wifi_config.ap_pw);
    if (strlen(wifi_config.ap_pw) == 0) {
        wifi_driver_config.ap.authmode = WIFI_AUTH_OPEN;
    }
    
    ret = esp_wifi_set_mode(WIFI_MODE_AP);
    if (ret != ESP_OK) {
    	ESP_LOGE(TAG, "Could not set Soft AP mode (%d)", ret);
    	return false;
    }
    
    ret = esp_wifi_set_config(ESP_IF_WIFI_AP, &wifi_driver_config);
    if (ret != ESP_OK) {
    	ESP_LOGE(TAG, "Could not set Soft AP configuration (%d)", ret);
    	return false;
    }
    
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
    	ESP_LOGE(TAG, "Could not start Soft AP (%d)", ret);
    	return false;
    }
    	
    return true;
}


/**
 * Enable this device as a Client
 */
static bool enable_esp_wifi_client()
{
	esp_err_t ret;
	esp_netif_ip_info_t ipInfo;
	
	
	// Configure the IP address mechanism
	if (wifi_config.sta_static_ip) {
		// Static IP
		//
		// Create the esp_netif object
		wifi_netif = esp_netif_create_default_wifi_sta();
		
		// Stop the DHCP client
		ret = esp_netif_dhcpc_stop(wifi_netif);
		if ((ret != ESP_OK) && (ret != ESP_ERR_ESP_NETIF_DHCP_ALREADY_STOPPED)) {
    		ESP_LOGE(TAG, "Stop Station DHCP returned %d", ret);
    		return false;
    	}
    	
    	// Set the Static IP address
		ipInfo.ip.addr = wifi_config.sta_ip_addr[3] |
						 (wifi_config.sta_ip_addr[2] << 8) |
						 (wifi_config.sta_ip_addr[1] << 16) |
						 (wifi_config.sta_ip_addr[0] << 24);
		ipInfo.gw.addr = esp_netif_ip4_makeu32(0, 0, 0, 0);
  		ipInfo.netmask.addr = wifi_config.sta_netmask[3] |
						     (wifi_config.sta_netmask[2] << 8) |
						     (wifi_config.sta_netmask[1] << 16) |
						     (wifi_config.sta_netmask[0] << 24);
		ret = esp_netif_set_ip_info(wifi_netif, &ipInfo);
		if (ret != ESP_OK) {
			ESP_LOGE(TAG, "Set IP info returned %d", ret);
		}
	} else {
		// DHCP served address
		//
		// Create the esp_netif object
		wifi_netif = esp_netif_create_default_wifi_sta();
		
		ret = esp_netif_dhcpc_start(wifi_netif);
		if (ret != ESP_OK) {
    		ESP_LOGE(TAG, "Start Station DHCP returned %d", ret);
    		return false;
    	}
	}
	
	// Enable the Client
	wifi_config_t wifi_driver_config = {
		.sta = {
			.scan_method = WIFI_FAST_SCAN,
			.bssid_set = 0,
			.channel = 0,
			.listen_interval = 0,
			.sort_method = WIFI_CONNECT_AP_BY_SIGNAL			
		}
	};	
    strcpy((char*) wifi_driver_config.sta.ssid, wifi_config.sta_ssid);
    if (strlen(wifi_config.sta_pw) == 0) {
        strcpy((char*) wifi_driver_config.sta.password, "");
    } else {
    	strcpy((char*) wifi_driver_config.sta.password, wifi_config.sta_pw);
    }
    
    ret = esp_wifi_set_mode(WIFI_MODE_STA);
    if (ret != ESP_OK) {
    	ESP_LOGE(TAG, "Could not set Station mode (%d)", ret);
    	return false;
    }
    
    ret = esp_wifi_set_config(ESP_IF_WIFI_STA, &wifi_driver_config);
    if (ret != ESP_OK) {
    	ESP_LOGE(TAG, "Could not set Station configuration (%d)", ret);
    	return false;
    }
    
    ret = esp_wifi_start();
    if (ret != ESP_OK) {
    	ESP_LOGE(TAG, "Could not start Station (%d)", ret);
    	return false;
    }
    
    return true;
}


/*
 * Handle events from the WiFi stack
 */

static void wifi_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	wifi_event_ap_staconnected_t *con_event;
	wifi_event_ap_stadisconnected_t *dis_event;
	
	switch (event_id) {
		case WIFI_EVENT_AP_STACONNECTED:
			con_event = (wifi_event_ap_staconnected_t *) event_data;
			ESP_LOGI(TAG, "Station:"MACSTR" join, AID=%d", MAC2STR(con_event->mac), con_event->aid);
			wifi_flags |= WIFI_INFO_FLAG_CONNECTED;
			break;
		
		case WIFI_EVENT_AP_STADISCONNECTED:
			dis_event = (wifi_event_ap_stadisconnected_t *) event_data;
			ESP_LOGI(TAG, "Station:"MACSTR" leave, AID=%d", MAC2STR(dis_event->mac), dis_event->aid);
			wifi_flags &= ~WIFI_INFO_FLAG_CONNECTED;
			break;
			
		case WIFI_EVENT_STA_START:
			ESP_LOGI(TAG, "Station started, trying to connect to %s", wifi_config.sta_ssid);
			esp_wifi_connect();
			sta_retry_num = 0;
        	break;
        
        case WIFI_EVENT_STA_STOP:
        	ESP_LOGI(TAG, "Station stopped");
        	break;
        	
        case WIFI_EVENT_STA_CONNECTED:
        	ESP_LOGI(TAG, "Station connected");
        	wifi_flags |= WIFI_INFO_FLAG_CONNECTED;
        	break;
        	
        case WIFI_EVENT_STA_DISCONNECTED:
        	wifi_flags &= ~WIFI_INFO_FLAG_CONNECTED;
        	if (sta_retry_num > WIFI_FAST_RECONNECT_ATTEMPTS) {
        		vTaskDelay(pdMS_TO_TICKS(1000));
        	} else {
        		sta_retry_num++;
        	}
            esp_wifi_connect();
            ESP_LOGI(TAG, "Retry connection to %s", wifi_config.sta_ssid);
        	break;
	}
}


/*
 * Handle events from the TCP/IP stack
 */

static void ip_event_handler(void *arg, esp_event_base_t event_base, int32_t event_id, void *event_data)
{
	ip_event_got_ip_t *event = (ip_event_got_ip_t *) event_data;
	const esp_netif_ip_info_t *ip_info = &event->ip_info;
	
    sta_connected = true;
    sta_retry_num = 0;
    
	ESP_LOGI(TAG, "Got IP Address: " IPSTR, IP2STR(&ip_info->ip));
}


/**
 * Start the mDNS responder
 */
static bool start_mdns()
{
	char s[16];
	const esp_app_desc_t* app_desc;
	esp_err_t ret;
	esp_netif_ip_info_t ip_info;
	
	// Attempt to initialize mDNS
	ret = mdns_init();
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Could not initialize mDNS (%d)", ret);
		return false;
	}
	
	// Set our hostname
	ret = mdns_hostname_set("icam");
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Could not set mDNS hostname (%d)", ret);
		return false;
	}
	
	// Set our default instance
	ret = mdns_instance_name_set(wifi_config.ap_ssid);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Could not set mDNS instance %s (%d)", wifi_config.ap_ssid, ret);
		return false;
	}
	
	// Get info for our TXT records
	app_desc = esp_app_get_description();  // Get version info
	esp_netif_get_ip_info(wifi_netif, &ip_info);  // Get IP address
	sprintf(s, IPSTR, IP2STR(&ip_info.ip));
	
	service_txt_data[0].key = txt_item_keys[0];
	service_txt_data[0].value = s;
	service_txt_data[1].key = txt_item_keys[1];
	service_txt_data[1].value = app_desc->version;
	
	// Initialize service
	ret = mdns_service_add(NULL, "_http", "_tcp", 80, service_txt_data, NUM_SERVICE_TXT_ITEMS);
	if (ret != ESP_OK) {
		ESP_LOGE(TAG, "Could not initialize mDNS service (%d)", ret);
		return false;
	}
	
	mdns_running = true;
	return true;
}
	