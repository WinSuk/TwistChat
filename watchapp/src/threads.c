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

#include "pebble.h"
#include "input.h"
#include "keys.h"

#define NUM_THREADS 5

static Window *window;

static TextLayer *text_layer_main;
static TextLayer *text_layer_demo;

static SimpleMenuLayer *simple_menu_layer;
static SimpleMenuSection menu_sections[1];
static SimpleMenuItem menu_items[NUM_THREADS];

//TODO: Are these big enough?
static char addresses[NUM_THREADS][24];
static char names[NUM_THREADS][48];

bool show_demo_mode = false;

static void menu_select_callback(int index, void *ctx) {
  input_main(addresses[index]);
}

static void load_threads(void) {
  int mi = 0;
  
  for (int i = 0; i < NUM_THREADS; i++) {
    if (addresses[i][0] != '\0') {
      menu_items[mi] = (SimpleMenuItem) {
        .title = names[i],
        .subtitle = addresses[i],
        .callback = menu_select_callback,
      };
      mi++;
    }
  }

  menu_sections[0] = (SimpleMenuSection){
    .num_items = mi,
    .items = menu_items,
  };

  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_frame(window_layer);

  simple_menu_layer = simple_menu_layer_create(bounds, window, menu_sections, 1, NULL);
  
  layer_remove_child_layers(window_layer);

  layer_add_child(window_layer, simple_menu_layer_get_layer(simple_menu_layer));
}

static void window_load(Window *window) {
  Layer *window_layer = window_get_root_layer(window);
  GRect bounds = layer_get_frame(window_layer);
  
  // Main text layer
  text_layer_main = text_layer_create((GRect) { .origin = { 4, 0 }, .size = { bounds.size.w -10, bounds.size.h / 2 } });
  text_layer_set_text(text_layer_main, "Loading threads...");
  text_layer_set_font(text_layer_main, fonts_get_system_font(FONT_KEY_GOTHIC_24_BOLD));
  layer_add_child(window_layer, text_layer_get_layer(text_layer_main));
  
  // Demo mode text layer
  text_layer_demo = text_layer_create((GRect) { .origin = { 4, 126 }, .size = { bounds.size.w -10, bounds.size.h / 2 } });
  text_layer_set_font(text_layer_demo, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD));
  text_layer_set_text_alignment(text_layer_demo, GTextAlignmentRight);
  layer_add_child(window_layer, text_layer_get_layer(text_layer_demo));
}

static void window_unload(Window *window) {
  text_layer_destroy(text_layer_main);
  text_layer_destroy(text_layer_demo);
  if (simple_menu_layer != NULL) simple_menu_layer_destroy(simple_menu_layer);
}

void parse_list(char *threads) {
  // Currently, pattern for each thread is "address;name\n"
  int i = 0;
  uint pos = 0;
  uint last_pos = 0;
  while (pos < strlen(threads) && i < NUM_THREADS) {
    if (threads[pos] == ';') {
      strncpy(addresses[i], &threads[last_pos], pos - last_pos);
      last_pos = pos + 1;
    }
    if (threads[pos] == '\n') {
      strncpy(names[i], &threads[last_pos], pos - last_pos);
      last_pos = pos + 1;
      i++;
    }
    pos++;
  }
}

void threads_demo(void) {
  strcpy(names[0], "John Smith");
  strcpy(names[1], "Joe Dirt");
  strcpy(names[2], "Bill");
  strcpy(names[3], "Lee");
  strcpy(names[4], "Mr President");
  for (int i = 0; i < 5; i++) {
    strcpy(addresses[i], "");
  }
}

static void down_click_handler(ClickRecognizerRef recognizer, void *context) {
  if (show_demo_mode) {
    threads_demo();
    load_threads();
  }
}

static void click_config_provider(void *context) {
  window_single_click_subscribe(BUTTON_ID_DOWN, down_click_handler);
}

void out_sent_handler(DictionaryIterator *sent, void *context) {
  // outgoing message was delivered
}

void out_failed_handler(DictionaryIterator *failed, AppMessageResult reason, void *context) {
  // outgoing message failed
  text_layer_set_text(text_layer_main, "Companion app not found.");
  text_layer_set_text(text_layer_demo, "Demo Mode");
  show_demo_mode = true;
}

void in_received_handler(DictionaryIterator *received, void *context) {
  // incoming message received
  Tuple *thread_list_tuple = dict_find(received, KEY_THREAD_LIST);
  if (thread_list_tuple) {
    parse_list(thread_list_tuple->value->cstring);
    load_threads();
  }
  Tuple *msg_sent_tuple = dict_find(received, KEY_MESSAGE_SENT);
  if (msg_sent_tuple) {
    input_message_sent();
  }
  Tuple *msg_not_sent_tuple = dict_find(received, KEY_MESSAGE_NOT_SENT);
  if (msg_not_sent_tuple) {
    input_message_not_sent();
  }
}

void in_dropped_handler(AppMessageResult reason, void *context) {
  // incoming message dropped
}

int main(void) {
  app_message_register_inbox_received(in_received_handler);
  app_message_register_inbox_dropped(in_dropped_handler);
  app_message_register_outbox_sent(out_sent_handler);
  app_message_register_outbox_failed(out_failed_handler);
  //TODO: Good sizes?
  const uint32_t inbound_size = 350;
  const uint32_t outbound_size = 64;
  app_message_open(inbound_size, outbound_size);
  
  // Get threads
  DictionaryIterator *iter;
  app_message_outbox_begin(&iter);
  dict_write_cstring(iter, KEY_REQUEST_THREAD_LIST, "");
  app_message_outbox_send();
  
  window = window_create();
  
  window_set_click_config_provider(window, click_config_provider);

  window_set_window_handlers(window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });

  window_stack_push(window, true/*animated*/);

  app_event_loop();

  window_destroy(window);
}
