#ifndef KEYS_H
#define KEYS_H

#ifndef HID_KEY_NONE
#include "class/hid/hid.h" // HID_KEY_*
#endif
#include <stdint.h>
#include <stddef.h>

struct TouchKey
{
  const uint16_t x0; // key top left x
  const uint16_t y0; // key top left y
  const uint16_t x1; // key bottom right x
  const uint16_t y1; // key bottom right y
  const uint8_t key; // HID_KEY_*
  const char *sym;
};

#define N_KEYS 4

extern struct TouchKey touch_keys[];

uint8_t pos2key(int x, int y);
void keyboard_init(void);


#endif /* KEYS_H */
