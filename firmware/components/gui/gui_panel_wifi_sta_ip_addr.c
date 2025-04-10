/*
 * GUI wifi settings STA set static IP address control panel
 *
 * Copyright 2024 Dan Julio
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
#include "esp_system.h"
#ifndef CONFIG_BUILD_ICAM_MINI

#include "cmd_utilities.h"
#include "gui_page_settings.h"
#include "gui_sub_page_wifi.h"
#include "gui_panel_wifi_sta_ip_addr.h"
#include "gui_state.h"
#include "gui_utilities.h"

#ifdef ESP_PLATFORM
	#include "gui_task.h"
#else
	#include "gui_main.h"
#endif


//
// Local constants
//
#define MAX_IP_STRING_LEN  15



//
// Local variables
//
static char val_buf[MAX_IP_STRING_LEN+1];
static char working_val_buf[MAX_IP_STRING_LEN+1];

//
// LVGL Objects
//
static lv_obj_t* my_parent_page;
static lv_obj_t* my_panel;
static lv_obj_t* lbl_name;
static lv_obj_t* btn_ip_addr;
static lv_obj_t* lbl_btn_ip_addr;



//
// Forward declarations for internal functions
//
static void _cb_btn_ip_addr(lv_obj_t* obj, lv_event_t event);
static void _cb_keypad_handler(int kp_event);


//
// API
//
void gui_panel_wifi_sta_ip_addr_init(lv_obj_t* parent_cont)
{
	// Get the top-level displayed page holding us for our pop-ups
	my_parent_page = lv_obj_get_parent(parent_cont);
	
	// Control panel - width fits parent, height fits contents with padding
	my_panel = lv_cont_create(parent_cont, NULL);
	lv_obj_set_click(my_panel, false);
	lv_obj_set_auto_realign(my_panel, true);
	lv_cont_set_fit2(my_panel, LV_FIT_PARENT, LV_FIT_TIGHT);
	lv_cont_set_layout(my_panel, LV_LAYOUT_PRETTY_MID);
	lv_obj_set_style_local_pad_top(my_panel, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, GUIP_SETTINGS_TOP_PAD);
	lv_obj_set_style_local_pad_bottom(my_panel, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, GUIP_SETTINGS_BTM_PAD);
	lv_obj_set_style_local_pad_left(my_panel, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, GUIP_SETTINGS_LEFT_PAD);
	lv_obj_set_style_local_pad_right(my_panel, LV_CONT_PART_MAIN, LV_STATE_DEFAULT, GUIP_SETTINGS_RIGHT_PAD);
	
	// Panel name
	lbl_name = lv_label_create(my_panel, NULL);
	lv_label_set_static_text(lbl_name, "STA Static IP Address");
	
	// IP Address Button
	btn_ip_addr = lv_btn_create(my_panel, NULL);
	lv_obj_set_y(btn_ip_addr, 5);
	lv_obj_add_protect(btn_ip_addr, LV_PROTECT_CLICK_FOCUS);
	lv_obj_set_size(btn_ip_addr, GUIPN_WIFI_STA_IP_ADDR_BTN_W, GUIPN_WIFI_STA_IP_ADDR_BTN_H);
	lv_obj_set_event_cb(btn_ip_addr, _cb_btn_ip_addr);
	
	// Button Label - value set when displayed
	lbl_btn_ip_addr = lv_label_create(btn_ip_addr, NULL);
	lv_label_set_long_mode(lbl_btn_ip_addr, LV_LABEL_LONG_SROLL_CIRC);
	lv_label_set_align(lbl_btn_ip_addr, LV_LABEL_ALIGN_CENTER);
	lv_obj_set_width(lbl_btn_ip_addr, GUIPN_WIFI_STA_IP_ADDR_BTN_W);
	lv_label_set_static_text(lbl_btn_ip_addr, "");
	
    // Register with our parent page
	gui_sub_page_wifi_register_panel(my_panel);
}


void gui_panel_wifi_sta_ip_addr_set_active(bool is_active)
{
	if (is_active) {
		gui_print_ipv4_addr(val_buf, gui_state.sta_ip_addr);
		lv_label_set_static_text(lbl_btn_ip_addr, val_buf);
	}
	
	// We're only enabled for STA mode
	lv_obj_set_state(btn_ip_addr, gui_state.sta_mode ? LV_STATE_DEFAULT : LV_STATE_DISABLED);
}


void gui_panel_wifi_sta_ip_addr_note_updated_mode()
{
	lv_obj_set_state(btn_ip_addr, gui_state.sta_mode ? LV_STATE_DEFAULT : LV_STATE_DISABLED);
}



//
// Internal functions
//
static void _cb_btn_ip_addr(lv_obj_t* obj, lv_event_t event)
{
	if (event == LV_EVENT_CLICKED) {
		// We check keypad displayed so that this won't trigger if pressed while some
		// other controls keypad is displayed
		if (!gui_keypad_displayed()) {
			strcpy(working_val_buf, val_buf);
			gui_display_keypad(my_parent_page, GUI_KEYPAD_TYPE_NUMERIC, "Enter IPV4 address", working_val_buf, MAX_IP_STRING_LEN, _cb_keypad_handler);
		}
	}
}


static void _cb_keypad_handler(int kp_event)
{
	uint8_t temp_ip[4];;
	
	if (kp_event == GUI_KEYPAD_EVENT_CLOSE_ACCEPT) {
		if (!gui_parse_ipv4_addr_string(working_val_buf, temp_ip)) {
			gui_display_message_box(my_parent_page, "IP Address not valid", GUI_MSG_BOX_1_BTN, NULL);
		} else {
			// Update if changed
			if (strcmp(val_buf, working_val_buf) != 0) {
				strcpy(val_buf, working_val_buf);
				for (int i=0; i<4; i++) {
					gui_state.sta_ip_addr[i] = temp_ip[i];
				}
				lv_obj_invalidate(lbl_btn_ip_addr);
				
				gui_sub_page_wifi_note_change(false);
			}
		}
	}
}

#endif /* !CONFIG_BUILD_ICAM_MINI */
