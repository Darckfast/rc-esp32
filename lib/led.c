#include "led.h"
#include "button_gpio.h"
#include "button_types.h"
#include "esp_wifi.h"
#include "esp_wifi_types_generic.h"
#include "freertos/idf_additions.h"
#include "freertos/task.h"
#include "iot_button.h"
#include "led_strip.h"
#include "led_strip_types.h"
#include <stdio.h>

#define LED_STRIP_GPIO_PIN 0
#define LED_STRIP_LED_COUNT 12
#define INTERVAL 1000
#define COLOR_RED 255, 0, 0
#define COLOR_GREEN 0, 255, 0
#define COLOR_BLUE 0, 0, 255
#define COLOR_YELLOW 255, 255, 0
#define COLOR_PURPLE 128, 0, 128
#define COLOR_CYAN 0, 255, 255
#define COLOR_WHITE 50, 50, 50
#define COLOR_ORANGE 255, 50, 0

enum State curr_state = WIFI_INIT;
enum State prev_state = WIFI_INIT;
led_strip_handle_t l;

static led_strip_handle_t configure_led(void) {
  // LED strip general initialization, according to your led board design
  led_strip_config_t strip_config = {
      .strip_gpio_num = LED_STRIP_GPIO_PIN, // The GPIO that connected to the
                                            // LED strip's data line
      .max_leds = LED_STRIP_LED_COUNT,      // The number of LEDs in the strip,
      .led_model = LED_MODEL_WS2812,        // LED strip model
      // set the color order of the strip: GRB
      .led_pixel_format = LED_PIXEL_FORMAT_GRB,
      .flags = {
          .invert_out = false, // don't invert the output signal
      }};

  // LED strip backend configuration: SPI
  led_strip_spi_config_t spi_config = {
      .clk_src = SPI_CLK_SRC_DEFAULT, // different clock source can lead to
                                      // different power consumption
      .spi_bus = SPI2_HOST,           // SPI bus ID
      .flags = {
          .with_dma = true, // Using DMA can improve performance and help drive
                            // more LEDs
      }};

  // LED Strip object handle
  led_strip_handle_t led_strip;
  led_strip_new_spi_device(&strip_config, &spi_config, &led_strip);
  printf("Created LED strip object with SPI backend\n");

  return led_strip;
}

void pattern_fill(led_strip_handle_t strip, uint8_t r, uint8_t g, uint8_t b) {
  ESP_ERROR_CHECK(led_strip_clear(strip));

  for (int i = 0; i < LED_STRIP_LED_COUNT; i++) {
    ESP_ERROR_CHECK(led_strip_set_pixel(strip, i, r, g, b));
  }

  ESP_ERROR_CHECK(led_strip_refresh(strip));
}

void pattern_blink(led_strip_handle_t strip, uint8_t r, uint8_t g, uint8_t b,
                   int delay_ms) {
  pattern_fill(strip, r, g, b);
  vTaskDelay(pdMS_TO_TICKS(delay_ms));

  ESP_ERROR_CHECK(led_strip_clear(strip));
  ESP_ERROR_CHECK(led_strip_refresh(strip));
  vTaskDelay(pdMS_TO_TICKS(delay_ms));
}

void pattern_chase(led_strip_handle_t strip, uint8_t r, uint8_t g, uint8_t b) {
  ESP_ERROR_CHECK(led_strip_clear(strip));

  for (int i = 0; i < LED_STRIP_LED_COUNT; i++) {
    ESP_ERROR_CHECK(led_strip_clear(strip)); // clear previous
    ESP_ERROR_CHECK(led_strip_set_pixel(strip, i, r, g, b));
    ESP_ERROR_CHECK(led_strip_refresh(strip));
    vTaskDelay(pdMS_TO_TICKS(50));
  }
}

void pattern_pulse(led_strip_handle_t strip, uint8_t r, uint8_t g, uint8_t b) {

  // Fade in
  for (int br = 0; br <= 255; br += 5) {
    for (int i = 0; i < LED_STRIP_LED_COUNT; i++) {
      ESP_ERROR_CHECK(led_strip_set_pixel(strip, i, (r * br) / 255,
                                          (g * br) / 255, (b * br) / 255));
    }
    ESP_ERROR_CHECK(led_strip_refresh(strip));
    vTaskDelay(pdMS_TO_TICKS(10));
  }

  // Fade out
  for (int br = 255; br >= 0; br -= 5) {
    for (int i = 0; i < LED_STRIP_LED_COUNT; i++) {
      ESP_ERROR_CHECK(led_strip_set_pixel(strip, i, (r * br) / 255,
                                          (g * br) / 255, (b * br) / 255));
    }
    ESP_ERROR_CHECK(led_strip_refresh(strip));
    vTaskDelay(pdMS_TO_TICKS(10));
  }
}

static int rssi_to_leds(int rssi) {
  if (rssi <= -100)
    return 0;
  if (rssi >= -30)
    return LED_STRIP_LED_COUNT;

  int range = -30 - (-100); // 70
  int val = rssi - (-100);

  return (val * LED_STRIP_LED_COUNT) / range;
}

void pattern_rssi_gradient(led_strip_handle_t strip, int rssi) {
  ESP_ERROR_CHECK(led_strip_clear(strip));

  int leds_on = rssi_to_leds(rssi);

  for (int i = 0; i < LED_STRIP_LED_COUNT; i++) {
    if (i < leds_on) {
      float t = (float)i / LED_STRIP_LED_COUNT;

      uint8_t r = (1.0f - t) * 255;
      uint8_t g = t * 255;
      uint8_t b = 0;

      ESP_ERROR_CHECK(led_strip_set_pixel(strip, i, r, g, b));
    }
  }

  ESP_ERROR_CHECK(led_strip_refresh(strip));
}

void show_wifi_quality() {
  wifi_ap_record_t ap_info;
  int ret = esp_wifi_sta_get_ap_info(&ap_info);
  if (ret == ESP_ERR_WIFI_CONN) {
    curr_state = ERROR;
  } else if (ret == ESP_ERR_WIFI_NOT_CONNECT) {
    curr_state = ERROR;
  } else {
    int rssi = rssi_to_leds(ap_info.rssi);
    pattern_rssi_gradient(l, rssi);
  }
}

void leds_cb(void *a) {
  const TickType_t period = pdMS_TO_TICKS(15);
  TickType_t last_wake = xTaskGetTickCount();

  while (1) {
    switch (curr_state) {

    case WIFI_INIT:
      pattern_pulse(l, COLOR_BLUE);
      break;

    case BT_INIT:
      pattern_pulse(l, COLOR_PURPLE);
      break;

    case BT_WAITING_CONN:
      pattern_chase(l, COLOR_CYAN);
      break;

    case BT_CONNECTED:
      pattern_fill(l, COLOR_GREEN);
      break;

    case UDP_CONN:
      pattern_chase(l, COLOR_GREEN);
      break;

    case WIFI_QUAL:
      show_wifi_quality();
      break;

    case ERROR:
      pattern_blink(l, COLOR_RED, 500);
      break;

    case ERROR_1:
      pattern_blink(l, COLOR_RED, 100);
      break;

    case ERROR_2:
      pattern_blink(l, COLOR_ORANGE, 200);
      break;

    case ERROR_3:
      pattern_chase(l, COLOR_RED);
      break;

    case ERROR_4:
      pattern_pulse(l, COLOR_RED);
      break;

    default:
      led_strip_clear(l);
      led_strip_refresh(l);
      break;
    }
  }
  vTaskDelayUntil(&last_wake, period);
}

static void btn_p(void *arg, void *usr_data) {
  if (curr_state != WIFI_QUAL) {
    prev_state = curr_state;
    curr_state = WIFI_QUAL;
  } else {
    curr_state = prev_state;
  }
}
static void btn_n(void *arg, void *usr_data) {
  if (curr_state != WIFI_QUAL) {
    prev_state = curr_state;
    curr_state = WIFI_QUAL;
  } else {
    curr_state = prev_state;
  }
}

static void init_btns() {
  // Prev BTN
  const button_config_t p_btn_cfg = {0};
  const button_gpio_config_t p_btn_gpio_cfg = {
      .gpio_num = 12,
      .active_level = 0,
  };
  button_handle_t p_gpio_btn = NULL;
  iot_button_new_gpio_device(&p_btn_cfg, &p_btn_gpio_cfg, &p_gpio_btn);
  if (NULL == p_gpio_btn) {
    printf("Button create failed\n");
  }
  iot_button_register_cb(p_gpio_btn, BUTTON_SINGLE_CLICK, NULL, btn_p, NULL);

  // Next BTN
  const button_config_t n_btn_cfg = {0};
  const button_gpio_config_t n_btn_gpio_cfg = {
      .gpio_num = 13,
      .active_level = 0,
  };
  button_handle_t n_gpio_btn = NULL;
  iot_button_new_gpio_device(&n_btn_cfg, &n_btn_gpio_cfg, &n_gpio_btn);
  if (NULL == n_gpio_btn) {
    printf("Button create failed\n");
  }
  iot_button_register_cb(n_gpio_btn, BUTTON_SINGLE_CLICK, NULL, btn_n, NULL);
}

void init_leds() {
  l = configure_led();

  init_btns();

  xTaskCreate(leds_cb, "cycle_led_status", 1024, NULL, 5, NULL);
}
