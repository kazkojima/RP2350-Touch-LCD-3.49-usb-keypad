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
#include "Touch.h"


uint8_t key_codes[6] = {0};
void hid_task(void);

// Touch screen
static uint16_t ts_x;
static uint16_t ts_y;
static lv_indev_state_t ts_act;
static int ts_timer = 0;

bool mouse_p(uint16_t x, uint16_t y)
{
  return y > 320;
}

static void touch_callback(uint gpio, uint32_t events)
{
  if (gpio == TOUCH_INT_PIN)
    {
        Touch_Read_State();
	ts_x = TOUCH.Point1_x;
        ts_y = TOUCH.Point1_y;
	//key_codes[0] = pos2key(ts_y, ts_x);
	ts_act = LV_INDEV_STATE_PRESSED;
	ts_timer = 10;
	DEV_Digital_Write(12, 1);
    }
}

static void touch_screen_init(void) {
    DEV_KEY_Config(TOUCH_INT_PIN);
    DEV_IRQ_SET(TOUCH_INT_PIN, GPIO_IRQ_EDGE_FALL, &touch_callback);
}

static lv_obj_t *pad_widget = NULL;
static lv_obj_t *keys_widget[N_KEYS];

#define PAD_WIDTH 40
#define PAD_HEIGHT 20

void ui_init(lv_obj_t *parent)
{
#if 0
  pad_widget = lv_obj_create(parent);
  lv_obj_set_size(pad_widget, PAD_WIDTH, PAD_HEIGHT);
  lv_obj_align(pad_widget, LV_ALIGN_TOP_LEFT, 0, 0);
  lv_obj_update_layout(pad_widget);
  lv_obj_set_style_bg_color(pad_widget, lv_palette_main(LV_PALETTE_GREY), 0);
#endif
  for (int i = 0; i < N_KEYS; i++)
    {
      struct TouchKey *k = &touch_keys[i];
      lv_obj_t *btn = lv_button_create(parent);
      lv_obj_set_size(btn, k->x1-k->x0, k->y1-k->y0);
      lv_obj_align(btn, LV_ALIGN_TOP_LEFT, k->x0, k->y0);
      lv_obj_t * label = lv_label_create(btn);
      lv_label_set_text(label, k->sym);
#if 0
      lv_obj_set_style_transform_pivot_x(label, lv_pct(50), LV_PART_MAIN);
      lv_obj_set_style_transform_pivot_y(label, lv_pct(50), LV_PART_MAIN);
      lv_obj_set_style_transform_rotation(label, 900, LV_PART_MAIN);
#endif
      lv_obj_center(label);
      keys_widget[i] = btn;
    }
}

void ui_update_all(void)
{
  //lv_obj_invalidate(pad_widget);
  for (int i=0; i < N_KEYS; i++)
    lv_obj_invalidate(keys_widget[i]);
  //lv_obj_invalidate(lv_screen_active());
}

static lv_indev_t *lv_indev;

void touch_input_read_cb(lv_indev_t *indev, lv_indev_data_t *data)
{
  if (indev == NULL)
    return;

  if (ts_timer > 0)
    {
      data->point.y = 172-ts_x;
      data->point.x = ts_y;
      data->state = LV_INDEV_STATE_PRESSED;
    }
  else
    {
      data->state = LV_INDEV_STATE_RELEASED;
    }
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
    }
}

/*------------- MAIN -------------*/
int main(void)
{
  if (DEV_Module_Init() != 0)
    return -1;

  DEV_GPIO_Mode(12, GPIO_OUT);
  DEV_Digital_Write(12, 0);
  DEV_GPIO_Mode(13, GPIO_OUT);
  DEV_Digital_Write(13, 0);

  touch_screen_init();

  keyboard_init();

  tusb_init();

  multicore_launch_core1(core1_worker);

  while (1)
    {
      //DEV_Digital_Write(12, 1);
      tud_task();
      //DEV_Digital_Write(12, 0);
      hid_task();
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

void hid_task(void)
{
  const uint32_t interval_ms = 10;
  static uint32_t last_ms = 0;
  static bool has_key = false;
  static int16_t prev_x = -1;
  static int16_t prev_y = -1;

  // Ensure the HID interface is ready to send a new report
  if (!tud_hid_ready())
    return;

  if (ts_act != LV_INDEV_STATE_PRESSED)
    return;
  
  // Remote wakeup
  if (tud_suspended())
    {
      // Wake up host if we are in suspend mode
      // and REMOTE_WAKEUP feature is enabled by host
      tud_remote_wakeup();
      return;
    }

  if (!mouse_p(ts_x, ts_y))
    {
      if (!has_key)
	{
	  key_codes[0] = pos2key(ts_y, 172-ts_x);
	  // send a keyboard report
	  send_hid_report(true);
	  has_key = true;
	}
    }
  else
    {
      int16_t x = (int16_t)ts_y;
      int16_t y = (int16_t)(172-ts_x);
      //ts_act = LV_INDEV_STATE_RELEASED;
 
      if (prev_x != -1 && prev_y != -1)
	{
	  // Calculate relative movement
	  int16_t dx = x - prev_x;
	  int16_t dy = y - prev_y;

	  // Send mouse report if movement occurred
	  // HID relative mouse values are signed 8-bit (-127 to 127)
	  if (dx != 0 || dy != 0)
	    {
               tud_hid_mouse_report(REPORT_ID_MOUSE, 0x00, (int8_t)dx, (int8_t)dy, 0, 0);
            }
        }
      prev_x = x;
      prev_y = y;
    }

  if (to_ms_since_boot(get_absolute_time()) - last_ms > interval_ms)
    {
      last_ms = to_ms_since_boot(get_absolute_time()) ;
      if (ts_timer > 0)
	ts_timer = ts_timer - 1;
    }

  if (ts_timer <= 0)
    {
      ts_act = LV_INDEV_STATE_RELEASED;
      DEV_Digital_Write(12, 0);

      // Reset tracking when finger is lifted
      prev_x = -1;
      prev_y = -1;

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
