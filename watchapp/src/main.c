/*    
 *  PebbleType for Pebble smartwatches
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

#define KEY_THREAD_LIST 0
#define KEY_SEND_MESSAGE 1
#define KEY_MESSAGE_SENT 2
#define KEY_MESSAGE_NOT_SENT 3

static Window *window;
static TextLayer *text_layer_picker;
static TextLayer *text_layer_input;

static AppTimer *timer;

static const char *sets[] = {"abcdefgh", "ijklmnop", "qrstuvwx", "yz.,?!12", "34567890", "';:@#$%\"", "^&*()-_/", " "};

static int selected_item = 0;
static int selected_set = -1; //-1 for set picker
static bool shift = true;
static bool caps = false;

static char display[9];
static char input[71];

static bool is_up_held = false;

void del_last_char(char* name) {
  int i = 0;
  while(name[i] != '\0') {
    i++;
  }
  name[i-1] = '\0';
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
    input[strlen(input)] = sets[selected_set][selected_item];
    if (shift) {
      input[strlen(input) -1] -= 0x20;
      if (!caps) shift = false;
    }
    text_layer_set_text(text_layer_input, input);
    selected_set = -1;
  }
}

static void up_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Backspace/back to group list
  if (selected_set == -1) {
    del_last_char(input);
    text_layer_set_text(text_layer_input, input);
  } else {
    selected_set = -1;
  }
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Shift/caps
  if (caps) {
    caps = false;
    shift = false;
  } else if (shift) {
    caps = true;
  } else {
    shift = true;
  }
}

void select_long_click_handler(ClickRecognizerRef recognizer, void *context) {
  // Send message!
  text_layer_set_text(text_layer_input, "Sending...");
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_cstring(iter, KEY_SEND_MESSAGE, input);
  app_message_outbox_send();
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
    if (shift && display[0] >= 0x61 && display[0] <= 0x7a) {
      display[0] -= 0x20;
    }
    display[1] = '\0';
  }
  
  text_layer_set_text(text_layer_picker, display);
  
  timer = app_timer_register(100 /* milliseconds */, timer_callback, NULL);
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_bounds(window_layer);
  
  // Input
  text_layer_input = text_layer_create((GRect) { .origin = { 0, 0 }, .size = { bounds.size.w, bounds.size.h / 2 } });
  text_layer_set_text(text_layer_input, "Ready");
  text_layer_set_font(text_layer_input, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  layer_add_child(window_layer, text_layer_get_layer(text_layer_input));
  
  // Picker
  text_layer_picker = text_layer_create((GRect) { .origin = { 0, bounds.size.h / 2 }, .size = { bounds.size.w, bounds.size.h } });
  text_layer_set_text(text_layer_picker, "...");
  text_layer_set_text_alignment(text_layer_picker, GTextAlignmentCenter);
  text_layer_set_font(text_layer_picker, fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK));
  layer_add_child(window_layer, text_layer_get_layer(text_layer_picker));
}

static void window_unload(Window *window) {
  text_layer_destroy(text_layer_picker);
  text_layer_destroy(text_layer_input);
}

static void handle_accel(AccelData *accel_data, uint32_t num_samples) {
  // do nothing
}

void out_sent_handler(DictionaryIterator *sent, void *context) {
  // outgoing message was delivered
}


void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
  // outgoing message failed
}


void in_received_handler(DictionaryIterator *received, void *context) {
  // incoming message received
  Tuple *msg_sent_tuple = dict_find(received, KEY_MESSAGE_SENT);
  if (msg_sent_tuple) {
    text_layer_set_text(text_layer_input, "Message sent!");
  }
}


void in_dropped_handler(AppMessageResult reason, void *context) {
  // incoming message dropped
}

static void init(void) {
  window = window_create();
  window_set_click_config_provider(window, click_config_provider);
  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  const bool animated = true;
  window_stack_push(window, animated);
  
  // Keep the backlight on
  light_enable(true);
  
  // Passing 0 to subscribe sets up the accelerometer for peeking
  accel_data_service_subscribe(0, handle_accel);

  timer = app_timer_register(100 /*milliseconds */, timer_callback, NULL);
  
  app_message_register_inbox_received(in_received_handler);
  app_message_register_inbox_dropped(in_dropped_handler);
  app_message_register_outbox_sent(out_sent_handler);
  app_message_register_outbox_failed(out_failed_handler);

  const uint32_t inbound_size = 64;
  const uint32_t outbound_size = 64;
  app_message_open(inbound_size, outbound_size);
}

static void deinit(void) {
  window_destroy(window);
  
  accel_data_service_unsubscribe();
  
  light_enable(false);
}

int main(void) {
  init();

  APP_LOG(APP_LOG_LEVEL_DEBUG, "Done initializing, pushed window: %p", window);

  app_event_loop();
  deinit();
}
