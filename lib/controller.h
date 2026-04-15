#ifndef CONTROLLER_H
#define CONTROLLER_H

#include "controller/uni_gamepad.h"
#include <stdint.h>

extern uni_gamepad_t gamepad;
struct NormalizedGamepad {
  uint16_t Button_mask;
  float Tl;
  float Tr;
  float Lx;
  float Ly;
  float Rx;
  float Ry;
};

void init_ctrl(void);

#endif
