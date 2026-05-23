/*
 * The MIT License (MIT)
 *
 * Copyright (c) 2019 Ha Thach (tinyusb.org)
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 *
 */

// This is based on pico-examples/usb/device/dev_hid_composite/main.c
//  in https://github.com/raspberrypi/pico-examples.git.

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "pico/stdio.h"
#include "pico/multicore.h"
#include "hardware/clocks.h"

#include "hardware/spi.h"

#include "tusb.h"
#include "usb_descriptors.h"

#include "keyboard.h"

#include "lvgl.h"
#include "DEV_Config.h"
#include "LCD_3in49.h"
#include "Touch.h"
#include "sdc-spi.h"

// if 1, enable quiet mode after QUIET_AFTER sec with no key activities
#define ENABLE_QUIET_MODE 1
#define QUIET_AFTER 60

// if 0, disable macro key function
#define ENABLE_MACRO_KEY 1

// if 1, macro key is enabled at the first time only
#define ENABLE_MACRO_ONETIME 0

// if 1, enable PIN in macro key generation
#define ENABLE_PIN_KEY 1 

const int DEBUG_LED = -1; // 14
#define ALLOW_DEBUG_LED (DEBUG_LED >= 0)

// SDC objects
static bool sd_initialized = false;
static uint8_t secbuf[512];

// macro key data offset in sector 0
// For macro key, sector 0 of sdcard, normally mbr, is used as a "physical key".
// SECTOR_DATA_OFS is the start offset of the key bytes in sector 0.
// Those bytes are xor'ed with another key bytes in the flash page at
// __device_key__[] which is allocated at the end of the flash memory.
#define SECTOR_DATA_OFS 32

// Keyboard hid
uint8_t key_codes[6] = {0};
hid_keyboard_modifier_bm_t key_modifier;
void hid_task(bool quiet);

// Touch screen
static uint16_t ts_x;
static uint16_t ts_y;
static lv_indev_state_t ts_act;
static bool indev_done = false;

static void touch_callback(uint gpio, uint32_t events)
{
  if (gpio == TOUCH_INT_PIN)
    {
        Touch_Read_State();
	ts_x = TOUCH.Point1_x;
        ts_y = TOUCH.Point1_y;
	ts_act = LV_INDEV_STATE_PRESSED;
	indev_done = false;
	if (ALLOW_DEBUG_LED)
	  DEV_Digital_Write(DEBUG_LED, 1);
    }
}

static void touch_screen_init(void) {
    DEV_KEY_Config(TOUCH_INT_PIN);
    DEV_IRQ_SET(TOUCH_INT_PIN, GPIO_IRQ_EDGE_FALL, &touch_callback);
}

// Ten Key and action keys

#define N_KEYS 4
static lv_obj_t *actkey_widgets[N_KEYS];
static lv_obj_t *tenkey_widget = NULL;
static lv_obj_t *macrokey_widget = NULL;

#define KEYAREA(x,y,w,h) (x),(y),((x)+(w)),((y)+(h))

struct TouchKey actkeys[N_KEYS] = {
  {KEYAREA(408,18,66,58), HID_KEY_BACKSPACE, LV_SYMBOL_BACKSPACE},
  {KEYAREA(408,82,66,78), HID_KEY_ENTER, LV_SYMBOL_NEW_LINE},
  {KEYAREA(488,18,66,66), HID_KEY_ARROW_UP, LV_SYMBOL_UP},
  {KEYAREA(488,90,66,66), HID_KEY_ARROW_DOWN, LV_SYMBOL_DOWN},
};

struct TouchKey macrokey = {KEYAREA(568,18,66,66), HID_KEY_NONE, LV_SYMBOL_UPLOAD};

static const char * btnm_map[] = {
  "1", "2", "3", "4", "5", "\n",
  "6", "7", "8", "9", "0", ""
};

static uint32_t tenkey_id;
static bool tenkey_pressed =false;

static void tenkey_event_handler(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t * obj = lv_event_get_target_obj(e);
  if (code == LV_EVENT_VALUE_CHANGED) {
    uint32_t id = lv_buttonmatrix_get_selected_button(obj);
    const char * txt = lv_buttonmatrix_get_button_text(obj, id);
    tenkey_id = *txt - '0';
    tenkey_pressed = true;
  }
}

static uint32_t actkey_id;
static bool actkey_pressed = false;

static void actkey_event_handler(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *obj = lv_event_get_target_obj(e);
  if (code == LV_EVENT_CLICKED) {
    for (int i=0; i < N_KEYS; i++)
      {
	if (obj == actkey_widgets[i])
	  {
	    actkey_id = actkeys[i].key;
	    actkey_pressed = true;
	    return;
	  }
      }
  }
}

static bool macrokey_pressed = false;

static void macrokey_event_handler(lv_event_t *e)
{
  lv_event_code_t code = lv_event_get_code(e);
  lv_obj_t *obj = lv_event_get_target_obj(e);
  if (code == LV_EVENT_CLICKED) {
    if (obj == macrokey_widget)
      {
	macrokey_pressed = true;
	return;
      }
  }
}

#if ENABLE_PIN_KEY
lv_obj_t *pin_prompt_widget;
#define PIN_PROMPT_X 568
#define PIN_PROMPT_Y 113
bool pin_prompt = false;
#endif

void ui_init(lv_obj_t *parent)
{
  lv_obj_t * btnm = lv_buttonmatrix_create(parent);
  tenkey_widget = btnm;
  lv_buttonmatrix_set_map(btnm, btnm_map);
  lv_obj_set_size(btnm, 390, 160);
  lv_obj_align(btnm, LV_ALIGN_TOP_LEFT, 4, 4);
  //lv_obj_set_style_bg_color(btnm, lv_palette_main(LV_PALETTE_BLUE), LV_PART_ITEMS);
  lv_buttonmatrix_set_button_ctrl_all(btnm, LV_BUTTONMATRIX_CTRL_CLICK_TRIG|LV_BUTTONMATRIX_CTRL_NO_REPEAT);
  lv_obj_add_event_cb(btnm, tenkey_event_handler, LV_EVENT_ALL, NULL);

  for (int i = 0; i < N_KEYS; i++)
    {
      struct TouchKey *k = &actkeys[i];
      lv_obj_t *btn = lv_button_create(parent);
      lv_obj_set_size(btn, k->x1-k->x0, k->y1-k->y0);
      lv_obj_align(btn, LV_ALIGN_TOP_LEFT, k->x0, k->y0);
      lv_obj_add_event_cb(btn, actkey_event_handler, LV_EVENT_ALL, NULL);
      lv_obj_t *label = lv_label_create(btn);
      lv_label_set_text(label, k->sym);
      lv_obj_center(label);
      actkey_widgets[i] = btn;
    }

  struct TouchKey *k = &macrokey;
  lv_obj_t *btn = lv_button_create(parent);
  lv_obj_set_size(btn, k->x1-k->x0, k->y1-k->y0);
  lv_obj_align(btn, LV_ALIGN_TOP_LEFT, k->x0, k->y0);
  lv_obj_add_event_cb(btn, macrokey_event_handler, LV_EVENT_ALL, NULL);
  lv_obj_t *label = lv_label_create(btn);
  lv_label_set_text(label, k->sym);
  lv_obj_center(label);
  macrokey_widget = btn;

#if ENABLE_PIN_KEY
  static lv_style_t style_label_bg;
  lv_style_init(&style_label_bg);
  lv_style_set_bg_opa(&style_label_bg, (255 * 100 / 100));
  //lv_style_set_radius(&style_label_bg, 6);
  lv_style_set_text_color(&style_label_bg, lv_palette_main(LV_PALETTE_BLUE_GREY));

  label = lv_label_create(parent);
  lv_obj_set_width(label, 66);
  lv_obj_set_style_text_align(label, LV_TEXT_ALIGN_CENTER, 0);
  lv_obj_align(label, LV_ALIGN_TOP_LEFT, PIN_PROMPT_X, PIN_PROMPT_Y);
  lv_obj_set_style_bg_color(label, lv_color_hex(0xd5f5e3), 0);
  lv_label_set_text(label, "PIN?");
  lv_obj_add_style(label, &style_label_bg, 0);
  lv_obj_add_flag(label, LV_OBJ_FLAG_HIDDEN);
  pin_prompt_widget = label;
#endif
}

void ui_update_all(void)
{
  lv_obj_invalidate(tenkey_widget);
  for (int i=0; i < N_KEYS; i++)
    lv_obj_invalidate(actkey_widgets[i]);
  lv_obj_invalidate(macrokey_widget);
  //lv_obj_invalidate(lv_screen_active());
}

static lv_indev_t *lv_indev;

void touch_input_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
  if (indev == NULL)
    return;

  if (ts_act == LV_INDEV_STATE_PRESSED)
    {
      // Convert LCD coordinate to display cordinate
      data->point.y = LCD_3IN49_WIDTH - ts_x;
      data->point.x = ts_y;
      data->state = LV_INDEV_STATE_PRESSED;
    }
  else
    {
      data->state = LV_INDEV_STATE_RELEASED;
    }
  indev_done = true;
}

extern void lcd_3in49_lvgl_init(void);

static void core1_worker()
{
  lcd_3in49_lvgl_init();

  lv_indev = lv_indev_create();
  lv_indev_set_type(lv_indev, LV_INDEV_TYPE_POINTER);
  lv_indev_set_read_cb(lv_indev, touch_input_read_cb);

  ui_init(lv_screen_active());
  ui_update_all();

  while(1)
    {
      lv_sleep_ms(5);
      lv_task_handler();
      if (indev_done)
	{
	  ts_act = LV_INDEV_STATE_RELEASED;
	  // Sometimes button and button matrix widgets are refreshed only partially.
	  // Make sure that they are updated here anyway.
	  ui_update_all();
	  if (ALLOW_DEBUG_LED)
	    DEV_Digital_Write(DEBUG_LED, 0);
	}
      if (!sd_initialized)
	{
	  if (!lv_obj_has_state(macrokey_widget, LV_STATE_DISABLED))
	    lv_obj_add_state(macrokey_widget, LV_STATE_DISABLED);
	}
      else
	{
	  if (lv_obj_has_state(macrokey_widget, LV_STATE_DISABLED))
	    lv_obj_remove_state(macrokey_widget, LV_STATE_DISABLED);
	}
#if ENABLE_PIN_KEY
      static bool last_pin_prompt = false;
      if (pin_prompt != last_pin_prompt)
	{
	  if (pin_prompt)
	    lv_obj_remove_flag(pin_prompt_widget, LV_OBJ_FLAG_HIDDEN);
	  else
	    lv_obj_add_flag(pin_prompt_widget, LV_OBJ_FLAG_HIDDEN);
	  lv_obj_invalidate(pin_prompt_widget);
	  //printf("hide prompt\n");
	}
      last_pin_prompt = pin_prompt;
#endif
    }
}

// Macro key
#if ENABLE_MACRO_KEY
#define DEVKEY_LENGTH 32
static bool macro_mode = false;
#define MAX_MACRO_LENGTH DEVKEY_LENGTH

static int macro_length = 0;
static uint8_t macro_codes[MAX_MACRO_LENGTH]; // = { 6, 'H', 'e', 'l', 'l', 'o', '\n', };
static int macro_index = 0;

#define MAX_PIN_COUNT 4
static int pin_count = 0;
static size_t pin_value = 0;

// simple ascii to hid key code converter
uint8_t const conv_table[128][2] =  { HID_ASCII_TO_KEYCODE };

static inline uint8_t asc2hidcode(char c, hid_keyboard_modifier_bm_t *m)
{
  if (conv_table[c][0])
    *m = KEYBOARD_MODIFIER_LEFTSHIFT;
  else
    *m = 0;
  return conv_table[c][1];
}

// start address of key code section on flash
extern uint8_t __device_key__[];

static inline uint8_t device_key(size_t index)
{
  size_t ofs, bit_ofs;
  ofs = index + (pin_value >> 3);
  bit_ofs = pin_value & 7;

  return (__device_key__[ofs] >> bit_ofs) | (__device_key__[ofs+1] << (8-bit_ofs));
}
#endif

/*------------- MAIN -------------*/
int main(void)
{
  if (DEV_Module_Init() != 0)
    return -1;

#if 0
  printf("device key address %08x\n", __device_key__);
  for (int i=0; i < 8; i++)
    printf("%02x ", __device_key__[i]);
  printf("\n");
#endif

  if (ALLOW_DEBUG_LED)
    {
      DEV_GPIO_Mode(DEBUG_LED, GPIO_OUT);
      DEV_Digital_Write(DEBUG_LED, 0);
    }

  touch_screen_init();

  tusb_init();

  multicore_launch_core1(core1_worker);

#if ENABLE_MACRO_KEY
    sd_initialized = sd_init_spi_mode();
#endif

  bool quiet_mode = false;
  uint32_t last_time =  to_ms_since_boot(get_absolute_time());
  while (1)
    {
#if ENABLE_QUIET_MODE
      uint32_t now = to_ms_since_boot(get_absolute_time());
      bool timer_expired = (now - last_time > QUIET_AFTER*1000);

      if (tenkey_pressed || actkey_pressed || macrokey_pressed)
	last_time = now;
      if (!quiet_mode && timer_expired)
	{
	  quiet_mode = true;
	  DEV_SET_PWM(40);
	}
       if (DEV_Digital_Read(SYS_OUT) == 0)
	{
	  last_time = now;
	  quiet_mode = false;
	  DEV_SET_PWM(60);
	}
#endif
#if ENABLE_MACRO_KEY
       if (quiet_mode && macrokey_pressed)
	   macrokey_pressed = false;
       if (macrokey_pressed)
	{
	  macrokey_pressed = false;
# if ENABLE_PIN_KEY
	  pin_prompt = true;
	}
       if (pin_prompt)
	 {
	   if (pin_count < MAX_PIN_COUNT)
	     {
	       if (tenkey_pressed)
		 {
		   tenkey_pressed = false;
		   pin_value = pin_value*10 + tenkey_id;
		   pin_count++;
		 }
	       // Ignore act keys
	       if (actkey_pressed)
		 actkey_pressed = false;
	       tud_task();
	       continue;
	     }

	   // pin_value is used as the bit offset in __device_key__ section which
	   // is assumed the last 4096-byte block of flash memory.
	   // Make sure < (4096-32-1)*8.
	   pin_value = pin_value % 10000;
	   pin_prompt = false;
	   //printf("pin value: %d\n", pin_value);
# endif  // ENABLE_PIN_KEY
	  
	  if (sd_initialized && sd_read_block(0, secbuf)) {
	    for (int i = 0; i < DEVKEY_LENGTH; i++)
	      macro_codes[i] = secbuf[SECTOR_DATA_OFS+i] ^ device_key(i);

	    macro_length = macro_codes[0] & 0x1F; // Mask to MAX_MACRO_LENGTH (32)
	    pin_count = 0;
	    pin_value = 0;
	    // Erase sector buffer
	    memset(secbuf, 0, sizeof(secbuf));
	    if (!macro_mode)
	      {
		macro_mode = true;
		macro_index = 1;
	      }
	  }
# if 	ENABLE_MACRO_ONETIME
	  sd_initialized = false;
	  sd_deselect();
# endif
	 }
#endif
      tud_task();
      hid_task(!quiet_mode);
    }

  return 0;
}

//--------------------------------------------------------------------+
// USB HID
//--------------------------------------------------------------------+

static void send_hid_report(bool keys_pressed)
{
    // skip if hid is not ready yet
    if (!tud_hid_ready())
    {
        return;
    }

    // avoid sending multiple zero reports
    static bool send_empty = false;

    if (keys_pressed)
    {
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, key_modifier, key_codes);
        send_empty = true;
    }
    else
    {
        // send empty key report if previously has key pressed
        if (send_empty)
        {
            tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
        }
        send_empty = false;
    }
}

void hid_task(bool genkey)
{
  const uint32_t interval_ms = 100;
  static uint32_t last_ms = 0;
  static bool has_key = false;

  // Ensure the HID interface is ready to send a new report
  if (!tud_hid_ready())
    return;

  //  if (ts_act != LV_INDEV_STATE_PRESSED)
  //    return;
  
  // Remote wakeup
  if (tud_suspended())
    {
      // Wake up host if we are in suspend mode
      // and REMOTE_WAKEUP feature is enabled by host
      tud_remote_wakeup();
      return;
    }

  if (!has_key)
    {
#if ENABLE_MACRO_KEY
      if (macro_mode)
	{
	  if (macro_index <= macro_length)
	    {
	      uint8_t c = macro_codes[macro_index] & 0x7f;
	      key_codes[0] = asc2hidcode(c, &key_modifier);
	      macro_index++;
	      send_hid_report(genkey);
	      has_key = true;
	      //printf("macro key %02x\n", key_codes[0]);
	    }
	  else
	    {
	      macro_mode = false;
	      macro_index = 0;
	      // Erase macro_codes
	      memset(macro_codes, 0, sizeof(macro_codes));
	    }
	}
      else
#endif
      if (tenkey_pressed)
	{
	  key_codes[0] = (tenkey_id == 0)?HID_KEY_0:HID_KEY_1+tenkey_id-1;
	  key_modifier = 0;
	  tenkey_pressed = false;
	  send_hid_report(genkey);
	  has_key = true;
	}
      else if (actkey_pressed)
	{
	  key_codes[0] = actkey_id;
	  key_modifier = 0;
	  actkey_pressed = false;
	  // send a keyboard report
	  send_hid_report(genkey);
	  has_key = true;
	}
    }

  if (to_ms_since_boot(get_absolute_time()) - last_ms > interval_ms)
    {
      last_ms = to_ms_since_boot(get_absolute_time()) ;
      if (has_key)
	{
	  tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, NULL);
	  has_key = false;
	}
    }
}

// Invoked when sent REPORT successfully to host
// Application can use this to send the next report
// Note: For composite reports, report[0] is report ID
void tud_hid_report_complete_cb(uint8_t instance, uint8_t const *report, uint16_t len)
{
    // not implemented, we only send REPORT_ID_KEYBOARD
    (void)instance;
    (void)len;
}

// Invoked when received GET_REPORT control request
// Application must fill buffer report's content and return its length.
// Return zero will cause the stack to STALL request
uint16_t tud_hid_get_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t *buffer, uint16_t reqlen)
{
    // TODO not Implemented
    (void)instance;
    (void)report_id;
    (void)report_type;
    (void)buffer;
    (void)reqlen;

    return 0;
}

// Invoked when received SET_REPORT control request or
// received data on OUT endpoint ( Report ID = 0, Type = 0 )
void tud_hid_set_report_cb(uint8_t instance, uint8_t report_id, hid_report_type_t report_type, uint8_t const *buffer, uint16_t bufsize)
{
    (void)instance;

    if (report_type == HID_REPORT_TYPE_OUTPUT)
    {
        // Set keyboard LED e.g Capslock, Numlock etc...
        if (report_id == REPORT_ID_KEYBOARD)
        {
            // bufsize should be (at least) 1
            if (bufsize < 1)
                return;

	    // Handle keyboard leds
            // uint8_t const kbd_leds = buffer[0];
            // if (kbd_leds & KEYBOARD_LED_CAPSLOCK) ...
        }
    }
}

//--------------------------------------------------------------------+
// Device callbacks
//--------------------------------------------------------------------+

// Invoked when device is mounted
void tud_mount_cb(void)
{
}

// Invoked when device is unmounted
void tud_umount_cb(void)
{
}

// Invoked when usb bus is suspended
// remote_wakeup_en : if host allow us  to perform remote wakeup
// Within 7ms, device must draw an average of current less than 2.5 mA from bus
void tud_suspend_cb(bool remote_wakeup_en)
{
    (void)remote_wakeup_en;
}

// Invoked when usb bus is resumed
void tud_resume_cb(void)
{
}
