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

#include "pico/multicore.h"
#include "hardware/clocks.h"

#include "tusb.h"
#include "usb_descriptors.h"

#include "keyboard.h"

#include "lvgl.h"
#include "DEV_Config.h"
#include "LCD_3in49.h"
#include "Touch.h"

const int DEBUG_LED = 12;
#define ALLOW_DEBUG_LED (DEBUG_LED >= 0)

uint8_t key_codes[6] = {0};
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

#define KEYAREA(x,y,w,h) (x),(y),((x)+(w)),((y)+(h))

struct TouchKey actkeys[N_KEYS] = {
  {KEYAREA(412,18,66,58), HID_KEY_BACKSPACE, LV_SYMBOL_BACKSPACE},
  {KEYAREA(412,82,66,78), HID_KEY_ENTER, LV_SYMBOL_NEW_LINE},
  {KEYAREA(500,18,66,66), HID_KEY_ARROW_UP, LV_SYMBOL_UP},
  {KEYAREA(500,90,66,66), HID_KEY_ARROW_DOWN, LV_SYMBOL_DOWN},
};

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

void ui_init(lv_obj_t *parent)
{
  lv_obj_t * btnm = lv_buttonmatrix_create(parent);
  tenkey_widget = btnm;
  lv_buttonmatrix_set_map(btnm, btnm_map);
  lv_obj_set_size(btnm, 390, 160);
  lv_obj_align(btnm, LV_ALIGN_TOP_LEFT, 4, 4);
  //lv_obj_set_style_bg_color(btnm, lv_palette_main(LV_PALETTE_BLUE), LV_PART_ITEMS);
  lv_obj_add_event_cb(btnm, tenkey_event_handler, LV_EVENT_ALL, NULL);

  for (int i = 0; i < N_KEYS; i++)
    {
      struct TouchKey *k = &actkeys[i];
      lv_obj_t *btn = lv_button_create(parent);
      lv_obj_set_size(btn, k->x1-k->x0, k->y1-k->y0);
      lv_obj_align(btn, LV_ALIGN_TOP_LEFT, k->x0, k->y0);
      lv_obj_add_event_cb(btn, actkey_event_handler, LV_EVENT_ALL, NULL);
      lv_obj_t * label = lv_label_create(btn);
      lv_label_set_text(label, k->sym);
      lv_obj_center(label);
      actkey_widgets[i] = btn;
    }
}

void ui_update_all(void)
{
  lv_obj_invalidate(tenkey_widget);
  for (int i=0; i < N_KEYS; i++)
    lv_obj_invalidate(actkey_widgets[i]);
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
  lv_sleep_ms(100);
  ui_update_all();
  lv_refr_now(NULL);
  lv_task_handler();

  while(1)
    {
      lv_sleep_ms(5);
      lv_task_handler();
      if (indev_done)
	{
	  ts_act = LV_INDEV_STATE_RELEASED;
	  if (ALLOW_DEBUG_LED)
	    DEV_Digital_Write(DEBUG_LED, 0);
	}
    }
}

/*------------- MAIN -------------*/
int main(void)
{
  if (DEV_Module_Init() != 0)
    return -1;

  if (ALLOW_DEBUG_LED)
    {
      DEV_GPIO_Mode(12, GPIO_OUT);
      DEV_Digital_Write(12, 0);
    }

  touch_screen_init();

  tusb_init();

  multicore_launch_core1(core1_worker);

  bool quiet_mode = false;
  uint32_t last_time =  to_ms_since_boot(get_absolute_time());
  while (1)
    {
      uint32_t now = to_ms_since_boot(get_absolute_time());
      bool timer_expired = (now - last_time > 60*1000); // 60sec

      if (tenkey_pressed || actkey_pressed)
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
        tud_hid_keyboard_report(REPORT_ID_KEYBOARD, 0, key_codes);
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

void hid_task(bool quiet)
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
      if (tenkey_pressed)
	{
	  key_codes[0] = (tenkey_id == 0)?HID_KEY_0:HID_KEY_1+tenkey_id-1;
	  tenkey_pressed = false;
	  send_hid_report(quiet);
	  has_key = true;
	}
      else if (actkey_pressed)
	{
	  key_codes[0] = actkey_id;
	  actkey_pressed = false;
	  // send a keyboard report
	  send_hid_report(quiet);
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
