#include "sdkconfig.h"
#include <btstack_port_esp32.h>
#include <btstack_run_loop.h>
#include <btstack_stdio_esp32.h>
#include <hci_dump.h>
#include <hci_dump_embedded_stdout.h>
#include <stdio.h>
#include <stdlib.h>
#include <uni.h>

#include "../lib/controller.h"
#include "../lib/led.h"
#include "../lib/my_platform.h"
#include "../lib/udp.h"
#include "../lib/wifi.h"
#include "secrets.h"

// Sanity check
#ifndef CONFIG_BLUEPAD32_PLATFORM_CUSTOM
#error "Must use BLUEPAD32_PLATFORM_CUSTOM"
#endif

struct uni_platform *get_my_platform(void);

int app_main(void) {
  init_leds();

  wifi_init();

  esp_err_t ret = wifi_connect(RC_SSID, RC_PASSWORD);
  if (ret != ESP_OK) {
    curr_state = ERROR;
    // need to add logic to wait for conn
    printf("Failed to connect to Wi-Fi network\n");
  }

  curr_state = BT_INIT;

  init_socket();

  // Don't use BTstack buffered UART. It conflicts with the console.
#ifdef CONFIG_ESP_CONSOLE_UART
#ifndef CONFIG_BLUEPAD32_USB_CONSOLE_ENABLE
  btstack_stdio_init();
#endif // CONFIG_BLUEPAD32_USB_CONSOLE_ENABLE
#endif // CONFIG_ESP_CONSOLE_UART

  // Configure BTstack for ESP32 VHCI Controller
  btstack_init();

  // Must be called before uni_init()
  uni_platform_set_custom(get_my_platform());

  // Init Bluepad32.
  uni_init(0, NULL);
  curr_state = BT_WAITING_CONN;

  init_ctrl();

  // Does not return.
  btstack_run_loop_execute();

  return 0;
}
