#include "controller.h"
#include "controller/uni_gamepad.h"
#include "freertos/idf_additions.h"
#include "freertos/projdefs.h"
#include "math.h"
#include "udp.h"
#include <stdint.h>
#include <string.h>

#define FREQUENCY_IN_HZ 60
#define MAX_THUMB_VAL 512.0
#define MAX_TRIGGER_VAL 1023.0
#define XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE 123
#define XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE 136
#define XINPUT_GAMEPAD_TRIGGER_THRESHOLD 10

static inline void write_double(uint8_t *buf, float f) {
  uint32_t v;
  memcpy(&v, &f, sizeof(v)); // avoid strict aliasing issues

  buf[0] = (uint8_t)(v & 0xFF);
  buf[1] = (uint8_t)((v >> 8) & 0xFF);
  buf[2] = (uint8_t)((v >> 16) & 0xFF);
  buf[3] = (uint8_t)((v >> 24) & 0xFF);
}

uni_gamepad_t gamepad = {};

void serialize_gamepad(uint8_t *buf, struct NormalizedGamepad *g) {

  buf[0] = g->Button_mask & 0xFF;
  buf[1] = (g->Button_mask >> 8) & 0xFF;

  write_double(buf + 2, g->Tl);
  write_double(buf + 6, g->Tr);
  write_double(buf + 10, g->Lx);
  write_double(buf + 14, g->Ly);
  write_double(buf + 18, g->Rx);
  write_double(buf + 22, g->Ry);
}

static struct NormalizedGamepad *ng = NULL;

void calculate_deadzone(uni_gamepad_t g) {
  float mag = sqrt(g.axis_x * g.axis_x + g.axis_y * g.axis_y);
  float nLx = 0;
  float nLy = 0;

  if (mag > XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE) {
    nLx = g.axis_x / MAX_THUMB_VAL;
    nLy = g.axis_y / MAX_THUMB_VAL;
  }

  ng->Lx = nLx;
  ng->Ly = nLy;

  mag = sqrt(g.axis_rx * g.axis_rx + g.axis_ry * g.axis_ry);

  float nRx = 0;
  float nRy = 0;
  if (mag > XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE) {
    nRx = g.axis_rx / MAX_THUMB_VAL;
    nRy = g.axis_ry / MAX_THUMB_VAL;
  }

  ng->Rx = nRx;
  ng->Ry = nRy;

  float thr = 0;

  if (g.throttle > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) {
    thr = g.throttle - XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
    thr = pow(thr / (MAX_TRIGGER_VAL - XINPUT_GAMEPAD_TRIGGER_THRESHOLD), 3);
  }

  ng->Tr = thr;

  float brk = 0;

  if (g.brake > XINPUT_GAMEPAD_TRIGGER_THRESHOLD) {
    brk = g.brake - XINPUT_GAMEPAD_TRIGGER_THRESHOLD;
    brk = pow(brk / (MAX_TRIGGER_VAL - XINPUT_GAMEPAD_TRIGGER_THRESHOLD), 3);
  }

  ng->Tl = brk;
}

static uint8_t buffer[26];

void send_controller_data(void *arg) {
  const TickType_t period = pdMS_TO_TICKS(1000 / FREQUENCY_IN_HZ);
  TickType_t last_wake = xTaskGetTickCount();

  while (1) {
    if (ng == NULL) {
      ng = &(struct NormalizedGamepad){};
    }

    calculate_deadzone(gamepad);
    serialize_gamepad(buffer, ng);
    send_udp(buffer, sizeof(*ng));

    vTaskDelayUntil(&last_wake, period);
  }
}

void init_ctrl() {
  xTaskCreate(send_controller_data, "send_controller_data", 4096, NULL, 5,
              NULL);
}
