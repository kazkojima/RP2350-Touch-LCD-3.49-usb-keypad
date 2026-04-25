#include "keyboard.h"
#include "lvgl.h"

#if 1
#define KEYSIZEX 56
#define KEYSIZEY 48
#define LALIGN 8
#define TALIGN 8
#define KEYOFSX 8
#define KEYOFSY 6

#define KEYAREA(x,y) (x),(y),((x)+KEYSIZEX),((y)+KEYSIZEY)
#define KEYAREAX(x,y,w,h) (x),(y),((x)+(w)),((y)+(h))

#define PX(r) (LALIGN+(r)*KEYSIZEX+(r)*KEYOFSX)
#define PY(c) (TALIGN+(c)*KEYSIZEY+(c)*KEYOFSY)

struct TouchKey touch_keys[] = { // map touch area to keycode
#if 0
    {KEYAREA(PX(0),PY(0)), HID_KEY_0, "0"},
    {KEYAREA(PX(1),PY(0)), HID_KEY_1, "1"},
    {KEYAREA(PX(2),PY(0)), HID_KEY_2, "2"},
    {KEYAREA(PX(3),PY(0)), HID_KEY_3, "3"},
    {KEYAREA(PX(4),PY(0)), HID_KEY_4, "4"},
    {KEYAREA(PX(5),PY(0)), HID_KEY_5, "5"},
    {KEYAREA(PX(6),PY(0)), HID_KEY_6, "6"},
    {KEYAREA(PX(7),PY(0)), HID_KEY_7, "7"},
    {KEYAREA(PX(0),PY(1)), HID_KEY_8, "8"},
    {KEYAREA(PX(1),PY(1)), HID_KEY_9, "9"},
    {KEYAREA(PX(2),PY(1)), HID_KEY_A, "a"},
    {KEYAREA(PX(3),PY(1)), HID_KEY_B, "b"},
    {KEYAREA(PX(4),PY(1)), HID_KEY_C, "c"},
    {KEYAREA(PX(5),PY(1)), HID_KEY_D, "d"},
    {KEYAREA(PX(6),PY(1)), HID_KEY_E, "e"},
    {KEYAREA(PX(7),PY(1)), HID_KEY_F,  "f"},
#endif
    {KEYAREA(PX(8)+10,PY(0)), HID_KEY_ARROW_UP, LV_SYMBOL_UP},
    {KEYAREA(PX(8)+10,PY(1)), HID_KEY_ARROW_DOWN, LV_SYMBOL_DOWN},
    {KEYAREAX(PX(8),PY(2),2*KEYSIZEX,KEYSIZEY), HID_KEY_ENTER, LV_SYMBOL_NEW_LINE},
    {KEYAREAX(PX(6)+20,PY(2),KEYSIZEX+20,KEYSIZEY), HID_KEY_BACKSPACE, LV_SYMBOL_BACKSPACE},
};
#else
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
#endif

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
