#include "keyboard.h"
#include "lvgl.h"

#define KEYSIZE0 60
#define KEYPOS(x,y) (x),(y),((x)+KEYSIZE0),((y)+KEYSIZE0)
#define KEYPOSX(x,y,w,h) (x),(y),((x)+(w)),((y)+(h))

struct TouchKey touch_keys[N_KEYS] = { // map touch area to keycode
  {KEYPOS(80,12), HID_KEY_ARROW_UP, LV_SYMBOL_UP},
  {KEYPOS(80,86+12), HID_KEY_ARROW_DOWN, LV_SYMBOL_DOWN},
  {KEYPOS(10,86-30), HID_KEY_ARROW_LEFT, LV_SYMBOL_LEFT},
  {KEYPOS(150,86-30), HID_KEY_ARROW_RIGHT, LV_SYMBOL_RIGHT},
  {KEYPOSX(220,86-30,90,60), HID_KEY_ENTER, LV_SYMBOL_NEW_LINE},
};

uint8_t pos2key(int x, int y)
{
  size_t n = sizeof(touch_keys)/sizeof(struct TouchKey);
  for (size_t i = 0; i < n; i++)
    {
      struct TouchKey k = touch_keys[i];
      if (k.x0 <= x && x < k.x1 && k.y0 <= y && y < k.y1)
	return k.key;
    }

  return HID_KEY_NONE;
}

void keyboard_init()
{
}
