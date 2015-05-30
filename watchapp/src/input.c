/*    
 *  Twist Chat
 *  Copyright (C) 2014 - WinSuk (winsuk@winsuk.net)
 * 
 *  This program is free software: you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation, either version 3 of the License, or
 *  (at your option) any later version.
 * 
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 * 
 *  You should have received a copy of the GNU General Public License
 *  along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include <pebble.h>
#include "keys.h"

static Window *window;
static TextLayer *text_layer_picker;
static TextLayer *text_layer_input;

static ActionBarLayer *action_bar;
static GBitmap *backspace_icon;
static GBitmap *caps_icon;
static GBitmap *confirm_icon;
static GBitmap *cancel_icon;

static AppTimer *timer;

static const char *sets[] = {"abcdefgh", "ijklmnop", "qrstuvwx", "yz.,?!12", "34567890", "';:@#$%\"", "^&*()-_/", " "};

static int selected_item = 0;
static int selected_set = -1; //-1 for set picker

#define CAPS_OFF 0
#define CAPS_SHIFT 1
#define CAPS_ON 2
static int caps = CAPS_SHIFT;

static char display[9];
static char input[71];

static bool is_up_held = false;

static char phone_num[24];

static bool send_confirm = false;

void del_last_char(char* name) {
  int i = 0;
  while(name[i] != '\0') {
    i++;
  }
  name[i-1] = '\0';
}

static void set_caps(int state) {
  int resource;
  switch (state) {
    case CAPS_OFF:
      resource = RESOURCE_ID_ICON_CAPS_OFF;
      break;
    case CAPS_SHIFT:
      resource = RESOURCE_ID_ICON_CAPS_SHIFT;
      break;
    case CAPS_ON:
      resource = RESOURCE_ID_ICON_CAPS_ON;
      break;
    default:
      resource = RESOURCE_ID_ICON_CAPS_OFF;
      break;
  }
  if (caps_icon != NULL) {
    action_bar_layer_clear_icon(action_bar, BUTTON_ID_DOWN);
    gbitmap_destroy(caps_icon);
  }
  caps_icon = gbitmap_create_with_resource(resource);
  action_bar_layer_set_icon(action_bar, BUTTON_ID_DOWN, caps_icon);
  caps = state;
}

static void select_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (selected_set == -1 && selected_item == 7) {
    // Handle Space
    selected_set = 7;
    selected_item = 0;
  }
  
  if (selected_set == -1) {
    selected_set = selected_item;
  } else {
    char ch = sets[selected_set][selected_item];
    if (caps != CAPS_OFF) {
      if (ch >= 0x61 && ch <= 0x7a) {
        ch -= 0x20;
      }
      if (caps == CAPS_SHIFT) set_caps(CAPS_OFF);
    }
    input[strlen(input)] = ch;
    text_layer_set_text(text_layer_input, input);
    selected_set = -1;
  }
}

static void unload_send_confirm(void) {
  action_bar_layer_set_icon(action_bar, BUTTON_ID_UP, backspace_icon);
  set_caps(caps);
  gbitmap_destroy(confirm_icon);
  gbitmap_destroy(cancel_icon);
  confirm_icon = NULL;
  cancel_icon = NULL;
  send_confirm = false;
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (send_confirm) {
    // Send message!
    text_layer_set_text(text_layer_input, "Sending...");
    
    int size = strlen(phone_num) + 1 + strlen(input);
    char output[size];
    strcpy(output, phone_num);
    strcat(output, ";");
    strcat(output, input);
    
    DictionaryIterator *iter;
    app_message_outbox_begin(&iter);
    dict_write_cstring(iter, KEY_SEND_MESSAGE, output);
    app_message_outbox_send();
    
    unload_send_confirm();
  } else {
    // Backspace/back to group list
    if (selected_set == -1) {
      del_last_char(input);
      text_layer_set_text(text_layer_input, input);
      if (input[0] == '\0' && caps == CAPS_OFF) set_caps(CAPS_SHIFT);
    } else {
      selected_set = -1;
    }
  }
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (send_confirm) {
    // Cancel
    unload_send_confirm();
  } else {
    // Shift/caps
    if (caps == CAPS_ON) {
      set_caps(CAPS_OFF);
    } else {
      set_caps(caps + 1);
    }
  }
}

void select_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  send_confirm = true;
  confirm_icon = gbitmap_create_with_resource(RESOURCE_ID_ICON_CONFIRM);
  action_bar_layer_set_icon(action_bar, BUTTON_ID_UP, confirm_icon);
  cancel_icon = gbitmap_create_with_resource(RESOURCE_ID_ICON_CANCEL);
  action_bar_layer_set_icon(action_bar, BUTTON_ID_DOWN, cancel_icon);
  text_layer_set_text(text_layer_picker, "Send?");
}

void select_long_click_release_handler(ClickRecognizerRef recognizer, void *context) {
  //do nothing
}

static void backspace_timer_callback(void *data) {
  del_last_char(input);
  text_layer_set_text(text_layer_input, input);
  if (is_up_held == true) {
    app_timer_register(150 /*milliseconds */, backspace_timer_callback, NULL);
  }
}

void up_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Start deleting stuff
  is_up_held = true;
  app_timer_register(150 /*milliseconds */, backspace_timer_callback, NULL);
}

void up_long_click_release_handler(ClickRecognizerRef recognizer, void *context) {
  // Stop deleting stuff
  is_up_held = false;
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_SELECT, select_click_handler);
  window_single_click_subscribe(BUTTON_ID_UP, up_click_handler);
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
  window_long_click_subscribe(BUTTON_ID_SELECT, 700, select_long_click_handler, select_long_click_release_handler);
  window_long_click_subscribe(BUTTON_ID_UP, 700, up_long_click_handler, up_long_click_release_handler);
}

static void timer_callback(void *data) {
  if (!send_confirm) {
    AccelData accel = (AccelData) { .x = 0, .y = 0, .z = 0 };
    accel_service_peek(&accel);
    
    accel.y = accel.y + 200; //low limit of 200
    accel.y = -accel.y;
    if (accel.y < 0) accel.y = 0;
    if (accel.y > 600) accel.y = 600; //high limit of 800 (600 after low limit)
    
    selected_item = accel.y / 85;
    
    if (selected_set == -1) {
      if (selected_item == 7) {
	strcpy(display, "Space");
      } else {
	strcpy(display, sets[selected_item]);
      }
    } else {
      display[0] = sets[selected_set][selected_item];
      if (caps && display[0] >= 0x61 && display[0] <= 0x7a) {
	display[0] -= 0x20;
      }
      display[1] = '\0';
    }
    
    text_layer_set_text(text_layer_picker, display);
  }
  
  timer = app_timer_register(100 /* milliseconds */, timer_callback, NULL);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  // Action bar
  action_bar = action_bar_layer_create();
  action_bar_layer_add_to_window(action_bar, window);
  action_bar_layer_set_click_config_provider(action_bar, click_config_provider);
  backspace_icon = gbitmap_create_with_resource(RESOURCE_ID_ICON_BACKSPACE);
  action_bar_layer_set_icon(action_bar, BUTTON_ID_UP, backspace_icon);
  // set_caps already does the icon loading for us
  set_caps(CAPS_SHIFT);
  
  // Input
  text_layer_input = text_layer_create((GRect) { .origin = { 0, 0 }, .size = { bounds.size.w - ACTION_BAR_WIDTH, bounds.size.h / 2 } });
  if (input[0] == '\0') {
    text_layer_set_text(text_layer_input, "Ready");
  } else {
    text_layer_set_text(text_layer_input, input);
  }
  text_layer_set_font(text_layer_input, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  layer_add_child(window_layer, text_layer_get_layer(text_layer_input));
  
  // Picker
  text_layer_picker = text_layer_create((GRect) { .origin = { 0, bounds.size.h / 2 }, .size = { bounds.size.w - ACTION_BAR_WIDTH, bounds.size.h } });
  text_layer_set_text(text_layer_picker, "...");
  text_layer_set_text_alignment(text_layer_picker, GTextAlignmentCenter);
  text_layer_set_font(text_layer_picker, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
  layer_add_child(window_layer, text_layer_get_layer(text_layer_picker));
}

static void window_unload(Window *window) {
  text_layer_destroy(text_layer_picker);
  text_layer_destroy(text_layer_input);
  
  action_bar_layer_destroy(action_bar);
  gbitmap_destroy(backspace_icon);
  gbitmap_destroy(caps_icon);
  action_bar = NULL;
  backspace_icon = NULL;
  caps_icon = NULL;
  
  accel_data_service_unsubscribe();
  light_enable(false);
}

static void handle_accel(AccelData *accel_data, uint32_t num_samples) {
  // do nothing
}

void input_message_sent(void) {
  text_layer_set_text(text_layer_input, "Message sent.");
}

void input_message_not_sent(void) {
  text_layer_set_text(text_layer_input, "Failed to send message!");
}


static void init(void) {
  window = window_create();
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  const bool animated = true;
  window_stack_push(window, animated);
  
  // Keep the backlight on
  light_enable(true);
  
  // Reset in case of resume
  selected_set = -1;
  
  // Passing 0 to subscribe sets up the accelerometer for peeking
  accel_data_service_subscribe(0, handle_accel);

  timer = app_timer_register(100 /*milliseconds */, timer_callback, NULL);
}

static void deinit(void) {
  window_destroy(window);
}

void input_main(char* number) {
  strcpy(phone_num, number);
  init();
}
