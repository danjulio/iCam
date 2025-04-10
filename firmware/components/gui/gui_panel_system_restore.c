/*
 * GUI system settings restore factory default control panel
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
#include "gui_sub_page_system.h"
#include "gui_panel_system_restore.h"
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


//
// Local variables
//

//
// LVGL Objects
//
static lv_obj_t* my_parent_page;
static lv_obj_t* my_panel;
static lv_obj_t* lbl_name;
static lv_obj_t* btn_restore;
static lv_obj_t* lbl_btn_restore;
static lv_obj_t* lbl_info;



//
// Forward declarations for internal functions
//
static void _cb_btn_restore(lv_obj_t* obj, lv_event_t event);
static void _cb_messagebox(int btn_id);


//
// API
//
void gui_panel_system_restore_init(lv_obj_t* parent_cont)
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
	lv_label_set_static_text(lbl_name, "Factory Defaults");
	
	// Restore Factory Defaults Button
	btn_restore = lv_btn_create(my_panel, NULL);
	lv_obj_set_y(btn_restore, 5);
	lv_obj_add_protect(btn_restore, LV_PROTECT_CLICK_FOCUS);
	lv_obj_set_size(btn_restore, GUIPN_SYSTEM_RESTORE_BTN_W, GUIPN_SYSTEM_RESTORE_BTN_H);
	lv_obj_set_event_cb(btn_restore, _cb_btn_restore);
	
	// Button Label
	lbl_btn_restore = lv_label_create(btn_restore, NULL);
	lv_label_set_align(lbl_btn_restore, LV_LABEL_ALIGN_CENTER);
	lv_label_set_static_text(lbl_btn_restore, "Restore");

	// Information label - width set dynamically
	lbl_info = lv_label_create(my_panel, NULL);
	lv_obj_set_width(lbl_info, lv_obj_get_width(my_panel) - 5);
	lv_label_set_static_text(lbl_info, "System will shutdown after restore");
	
    // Register with our parent page
	gui_sub_page_system_register_panel(my_panel);
}


void gui_panel_system_restore_set_active(bool is_active)
{
	// Nothing to do for this panel
}



//
// Internal functions
//
static void _cb_btn_restore(lv_obj_t* obj, lv_event_t event)
{
	if (event == LV_EVENT_CLICKED) {
		// We check keypad displayed so that this won't trigger if pressed while some
		// other controls keypad is displayed
		if (!gui_keypad_displayed()) {
			// Display the "are you really sure?" messagebox
			gui_display_message_box(my_parent_page, "Restore System Defaults?", GUI_MSG_BOX_2_BTN, _cb_messagebox);
		}
	}
}


static void _cb_messagebox(int btn_id)
{
	if (btn_id == GUI_MSG_BOX_BTN_AFFIRM) {
		// Send command to trigger restore to main controller
		gui_send_activity_command(CMD_CTRL_ACT_RESTORE, 0, my_parent_page, "Restore in progress");
	}
}

#endif /* !CONFIG_BUILD_ICAM_MINI */
