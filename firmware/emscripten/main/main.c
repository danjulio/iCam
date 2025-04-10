/*
 * iCamMini LVGL GUI component running in the browser using emscripten
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
#include <stdint.h>
#include <stdbool.h>
#include <stdlib.h>
#include <unistd.h>
#define SDL_MAIN_HANDLED        /*To fix SDL's "undefined reference to WinMain" issue*/
#include <SDL2/SDL.h>
#include <emscripten.h>
#include <emscripten/em_js.h>
#include <emscripten/html5.h>
#include <emscripten/websocket.h>
#include "gui_main.h"
#include "lv_conf.h"
#include "lvgl/lvgl.h"
#include "lv_drivers/sdl/sdl.h"
#include "web_cmd_utilities.h"


//
// Global variables
//

// Browser information
static int browser_w;
static int browser_h;
static bool is_mobile_browser;

// Trigger to reconfigure GUI layout based on orientation changes
static bool reconfig_gui = false;

// Top-level LVGL objects
static lv_disp_t*    disp1;
static lv_indev_t*   mouse_indev;
static lv_indev_t*   kb_indev;
static lv_indev_t*   enc_indev;
static lv_obj_t*     screen;
static lv_task_t*    task_ws_trig;

// Websocket
 EMSCRIPTEN_WEBSOCKET_T ws;
 static bool websocket_open = false;
 


//
// Embedded javascript helpers
//
EM_JS(bool, isMobile, (), {
  return /Android|webOS|iPhone|iPad|iPod|BlackBerry|IEMobile|Opera Mini/i.test(navigator.userAgent);
});

EM_JS(int, get_browser_width, (), {
  return window.innerWidth;
});

EM_JS(int, get_browser_height, (), {
  return window.innerHeight;
});



//
// Forward declarations for internal functions
//
static EM_BOOL cb_orientation(int type, const EmscriptenOrientationChangeEvent* e, void* data);
static void do_loop(void *arg);
static void hal_init(void);
static void gui_init(void);
static void cb_ws_trig_timer(lv_task_t* t);
static void start_websocket();
static void stop_websocket();
static EM_BOOL onopen(int eventType, const EmscriptenWebSocketOpenEvent *websocketEvent, void *userData);
static EM_BOOL onmessage(int eventType, const EmscriptenWebSocketMessageEvent *websocketEvent, void *userData);
static EM_BOOL onerror(int eventType, const EmscriptenWebSocketErrorEvent *websocketEvent, void *userData);
static EM_BOOL onclose(int eventType, const EmscriptenWebSocketCloseEvent *websocketEvent, void *userData);



//
// Application entry point
//
int main()
{
	EMSCRIPTEN_RESULT res;
	
	printf("main start\n");
	
	// Get initial information about the browser we're running in
	browser_w = get_browser_width();
	browser_h = get_browser_height();
	is_mobile_browser = isMobile();
	// Set the canvas size to the one we are configuring LVGL for
	res = emscripten_set_canvas_element_size("#canvas", LV_HOR_RES_MAX, LV_VER_RES_MAX);
	if (res !=  EMSCRIPTEN_RESULT_SUCCESS) {
        printf("Unable to set canvas size - %d\n", res);
        // ??? what to do with this error - can it generate a pop-up error message? maybe not!
    }
    
	// Add a callback to detect mobile orientation changes
	res = emscripten_set_orientationchange_callback(NULL, EM_FALSE, cb_orientation);
    if (res !=  EMSCRIPTEN_RESULT_SUCCESS) {
        printf("Unable to add orientation callback - %d\n", res);
        // ??? what to do with this error - can it generate a pop-up error message?
    }
	
	// Initialize the websocket interface
    if (!web_cmd_init()) {
    	printf("Unable to initialize websocket command interface\n");
    	// ???
    }
    
    // Initialize LVGL
    lv_init();
    
    // Initialize the HAL (display, input devices, tick) for LVGL
    hal_init();
    
    // Initialize the GUI
    gui_init();
    
    // Setup the timer to initiate a websocket connection back to our server once we've handed control over to emscripten
	task_ws_trig = lv_task_create(cb_ws_trig_timer, 100, LV_TASK_PRIO_LOW, NULL);
	lv_task_set_repeat_count(task_ws_trig, 1);

	// Start the main emscripten loop
    emscripten_set_main_loop_arg(do_loop, NULL, -1, true);
    
    return 0;
}


EMSCRIPTEN_KEEPALIVE
void handleBrowserRefresh() {
	// Close any open socket to let the camera know we're going away
	if (websocket_open) {
		emscripten_websocket_close(ws, 1000, "Remote system initiated shutdown");
	}
}


EMSCRIPTEN_KEEPALIVE
void handleBrowserResize() {
	printf("Browser resize\n");
	
	reconfig_gui = true;
}



//
// Internal functions
//

static EM_BOOL cb_orientation(int type, const EmscriptenOrientationChangeEvent* e, void* data) {
    printf("Orientation (C) changed: type:%d, angle: %d\n", e->orientationIndex, e->orientationAngle);
    
    reconfig_gui = true;
    
    return EM_TRUE;
}


static void do_loop(void *arg)
{
	// Evaluate LVGL
    lv_task_handler();
    
    // Look for layout change
    if (reconfig_gui) {
    	reconfig_gui = false;
    	
    	// Update browser info
    	browser_w = get_browser_width();
		browser_h = get_browser_height();
		
		// Configure the GUI layout
		gui_main_reset_browser_dimensions(browser_w, browser_h);
    }
}


static void hal_init(void)
{
    sdl_init();

    // Create display buffers
    static lv_disp_buf_t disp_buf1;
    lv_color_t * buf1_1 = malloc(sizeof(lv_color_t) * LV_HOR_RES_MAX * LV_VER_RES_MAX);
    lv_color_t * buf1_2 = malloc(sizeof(lv_color_t) * LV_HOR_RES_MAX * LV_VER_RES_MAX);
    lv_disp_buf_init(&disp_buf1, buf1_1, buf1_2, LV_HOR_RES_MAX * LV_VER_RES_MAX);

    // Create the display
    static lv_disp_drv_t disp_drv;
    lv_disp_drv_init(&disp_drv);
    disp_drv.buffer = &disp_buf1;
    disp_drv.flush_cb = sdl_display_flush;
    disp_drv.hor_res = LV_HOR_RES_MAX;
    disp_drv.ver_res = LV_VER_RES_MAX;
    disp1 = lv_disp_drv_register(&disp_drv);

    // Add a mouse as input
    static lv_indev_drv_t indev_drv_1;
    lv_indev_drv_init(&indev_drv_1);
    indev_drv_1.type = LV_INDEV_TYPE_POINTER;
    indev_drv_1.read_cb = sdl_mouse_read;
    mouse_indev = lv_indev_drv_register(&indev_drv_1);

	// Add a keyboard as input
    static lv_indev_drv_t indev_drv_2;
    lv_indev_drv_init(&indev_drv_2);
    indev_drv_2.type = LV_INDEV_TYPE_KEYPAD;
    indev_drv_2.read_cb = sdl_keyboard_read;
    kb_indev = lv_indev_drv_register(&indev_drv_2);
    gui_keypad_group = lv_group_create();
/*    
    // Add a mousewheel as input
    static lv_indev_drv_t indev_drv_3;
    lv_indev_drv_init(&indev_drv_3);
    indev_drv_3.type = LV_INDEV_TYPE_ENCODER;
    indev_drv_3.read_cb = sdl_mousewheel_read;
    enc_indev = lv_indev_drv_register(&indev_drv_3);
    gui_encoder_group = lv_group_create();
*/
}


static void gui_init(void)
{
	// Get the top-level screen object to match the underlying web canvas since it is bigger
    // than the objects we'll render and we want a black background
	screen = lv_obj_create(NULL, NULL);
	lv_obj_set_pos(screen, 0, 0);
	lv_obj_set_size(screen, LV_HOR_RES_MAX, LV_VER_RES_MAX);
	lv_obj_set_style_local_bg_color(screen, LV_OBJ_PART_MAIN, LV_STATE_DEFAULT, LV_COLOR_MAKE(0x0,0x0,0x0));
	lv_obj_set_click(screen, false);
	
	// Initialize the GUI manager
	gui_main_init(screen, browser_w, browser_h, is_mobile_browser);
	gui_main_register_socket_connect(start_websocket);
	gui_main_register_socket_disconnect(stop_websocket);
	
	// Afer all LVGL objects have been created and devices desiring keypad/encoder input
	// have added themselves to the appropriate group, assign the groups to the input device
	lv_indev_set_group(kb_indev, gui_keypad_group);
//	lv_indev_set_group(enc_indev, gui_encoder_group);
	
	// Start the display
	lv_scr_load(screen);
}


static void cb_ws_trig_timer(lv_task_t* t)
{
	start_websocket();
}


static void start_websocket()
{
	char ws_url_string[80];
	
	// Upgrade to websocket
	sprintf(ws_url_string, "ws://%s/ws", emscripten_run_script_string("window.location.hostname"));
	printf("Attempt to upgrade: %s\n", ws_url_string);
	
	EmscriptenWebSocketCreateAttributes ws_attrs = {
        ws_url_string,
        NULL,
        EM_TRUE
    };
	ws = emscripten_websocket_new(&ws_attrs);
    
    emscripten_websocket_set_onopen_callback(ws, NULL, onopen);
    emscripten_websocket_set_onerror_callback(ws, NULL, onerror);
    emscripten_websocket_set_onclose_callback(ws, NULL, onclose);
    emscripten_websocket_set_onmessage_callback(ws, NULL, onmessage);
}


static void stop_websocket()
{
	// Close the websocket (normal closure - no longer needed)
	if (websocket_open) {
		emscripten_websocket_close(ws, 1000, "Remote system initiated shutdown");
	}
}


static EM_BOOL onopen(int eventType, const EmscriptenWebSocketOpenEvent *websocketEvent, void *userData)
{
	// Note socket open
	web_cmd_register_socket(websocketEvent->socket);
	gui_main_set_connected(true);
	websocket_open = true;
	
	return EM_TRUE;
}


static EM_BOOL onmessage(int eventType, const EmscriptenWebSocketMessageEvent *websocketEvent, void *userData)
{
	if (!websocketEvent->isText) {
		// Process this packet
		if (!web_cmd_process_socket_rx_data(websocketEvent->numBytes, websocketEvent->data)) {
			printf("web_cmd_process_socket_rx_data failed\n");
		}
	} else {
		printf("unexpected websocket text packet of %d bytes\n", websocketEvent->numBytes);
	}
	
	return true;
}


static EM_BOOL onerror(int eventType, const EmscriptenWebSocketErrorEvent *websocketEvent, void *userData)
{
	printf("onerror: %d\n", eventType);
	return EM_TRUE;
}


static EM_BOOL onclose(int eventType, const EmscriptenWebSocketCloseEvent *websocketEvent, void *userData)
{
	// Note socket closed
	printf("websocket closed: %s\n", websocketEvent->reason);
	web_cmd_register_socket(0);
	gui_main_set_connected(false);
	(void) emscripten_websocket_delete(websocketEvent->socket);
	websocket_open = false;
	return EM_TRUE;
}
