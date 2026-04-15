
#ifndef LED_H
#define LED_H

enum State {
  WIFI_INIT,
  BT_INIT,
  BT_WAITING_CONN,
  BT_CONNECTED,
  UDP_CONN,
  WIFI_QUAL,
  ERROR,
  ERROR_1,
  ERROR_2,
  ERROR_3,
  ERROR_4,
};

void init_leds();
extern enum State curr_state;

#endif
