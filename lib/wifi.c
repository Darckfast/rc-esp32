#include "wifi.h"

#include <inttypes.h>
#include <stdio.h>
#include <string.h>

#include "esp_err.h"
#include "esp_event.h"
#include "esp_netif.h"
#include "esp_wifi.h"
#include "nvs_flash.h"

#include "freertos/event_groups.h"

#define WIFI_AUTHMODE WIFI_AUTH_WPA2_PSK

#define WIFI_CONNECTED_BIT BIT0
#define WIFI_FAIL_BIT BIT1

static const int WIFI_RETRY_ATTEMPT = 3;
static int wifi_retry_count = 0;
static esp_netif_t *netif_sta = NULL;
static esp_event_handler_instance_t ip_event_handler;
static esp_event_handler_instance_t wifi_event_handler;
static EventGroupHandle_t wifi_event_group = NULL;

static void handle_ip_event(void *arg, esp_event_base_t event_base,
                            int32_t event_id, void *event_data) {
  printf("[IP] Event received: 0x%" PRIx32 "\n", event_id);

  switch (event_id) {
  case IP_EVENT_STA_GOT_IP: {
    ip_event_got_ip_t *event = (ip_event_got_ip_t *)event_data;
    printf("[IP] Got IPv4: " IPSTR "\n", IP2STR(&event->ip_info.ip));

    wifi_retry_count = 0;
    xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    break;
  }

  case IP_EVENT_STA_LOST_IP:
    printf("[IP] Lost IP address\n");
    break;

  case IP_EVENT_GOT_IP6: {
    ip_event_got_ip6_t *event = (ip_event_got_ip6_t *)event_data;
    printf("[IP] Got IPv6: " IPV6STR "\n", IPV62STR(event->ip6_info.ip));

    wifi_retry_count = 0;
    xEventGroupSetBits(wifi_event_group, WIFI_CONNECTED_BIT);
    break;
  }

  default:
    printf("[IP] Unhandled event\n");
    break;
  }
}

static void handle_wifi_event(void *arg, esp_event_base_t event_base,
                              int32_t event_id, void *event_data) {
  printf("[WIFI] Event received: 0x%" PRIx32 "\n", event_id);

  switch (event_id) {
  case WIFI_EVENT_STA_START:
    printf("[WIFI] Started. Connecting...\n");
    esp_wifi_connect();
    break;

  case WIFI_EVENT_STA_CONNECTED:
    printf("[WIFI] Connected to AP\n");
    break;

  case WIFI_EVENT_STA_DISCONNECTED:
    printf("[WIFI] Disconnected\n");

    if (wifi_retry_count < WIFI_RETRY_ATTEMPT) {
      printf("[WIFI] Retrying... (%d/%d)\n", wifi_retry_count + 1,
             WIFI_RETRY_ATTEMPT);

      esp_wifi_connect();
      wifi_retry_count++;
    } else {
      printf("[WIFI] Failed to connect\n");
      xEventGroupSetBits(wifi_event_group, WIFI_FAIL_BIT);
    }
    break;

  case WIFI_EVENT_WIFI_READY:
    printf("[WIFI] Ready\n");
    break;

  case WIFI_EVENT_SCAN_DONE:
    printf("[WIFI] Scan done\n");
    break;

  case WIFI_EVENT_STA_STOP:
    printf("[WIFI] Stopped\n");
    break;

  default:
    printf("[WIFI] Unhandled event\n");
    break;
  }
}

esp_err_t wifi_init(void) {
  esp_err_t ret;

  /* NVS init */
  ret = nvs_flash_init();
  if (ret == ESP_ERR_NVS_NO_FREE_PAGES ||
      ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
    printf("[INIT] Erasing NVS...\n");
    ESP_ERROR_CHECK(nvs_flash_erase());
    ret = nvs_flash_init();
  }

  if (ret != ESP_OK) {
    printf("[INIT] NVS init failed\n");
    return ret;
  }

  /* Event group */
  wifi_event_group = xEventGroupCreate();
  if (!wifi_event_group) {
    printf("[INIT] Failed to create event group\n");
    return ESP_FAIL;
  }

  /* Network stack */
  ret = esp_netif_init();
  if (ret != ESP_OK) {
    printf("[INIT] esp_netif_init failed\n");
    return ret;
  }

  /* Event loop */
  ret = esp_event_loop_create_default();
  if (ret != ESP_OK) {
    printf("[INIT] Event loop creation failed\n");
    return ret;
  }

  /* Default WiFi handlers */
  ret = esp_wifi_set_default_wifi_sta_handlers();
  if (ret != ESP_OK) {
    printf("[INIT] Failed to set default WiFi handlers\n");
    return ret;
  }

  /* Create STA interface */
  netif_sta = esp_netif_create_default_wifi_sta();
  if (!netif_sta) {
    printf("[INIT] Failed to create STA interface\n");
    return ESP_FAIL;
  }

  /* WiFi init */
  wifi_init_config_t cfg = WIFI_INIT_CONFIG_DEFAULT();
  ESP_ERROR_CHECK(esp_wifi_init(&cfg));

  /* Register events */
  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      WIFI_EVENT, ESP_EVENT_ANY_ID, &handle_wifi_event, NULL,
      &wifi_event_handler));

  ESP_ERROR_CHECK(esp_event_handler_instance_register(
      IP_EVENT, ESP_EVENT_ANY_ID, &handle_ip_event, NULL, &ip_event_handler));

  printf("[INIT] WiFi initialized\n");
  return ESP_OK;
}

esp_err_t wifi_connect(char *ssid, char *password) {
  wifi_config_t config = {
      .sta =
          {
              .threshold.authmode = WIFI_AUTHMODE,
          },
  };

  strncpy((char *)config.sta.ssid, ssid, sizeof(config.sta.ssid));
  strncpy((char *)config.sta.password, password, sizeof(config.sta.password));

  config.sta.ssid[sizeof(config.sta.ssid) - 1] = '\0';
  config.sta.password[sizeof(config.sta.password) - 1] = '\0';

  ESP_ERROR_CHECK(esp_wifi_set_ps(WIFI_PS_NONE));
  ESP_ERROR_CHECK(esp_wifi_set_storage(WIFI_STORAGE_RAM));

  ESP_ERROR_CHECK(esp_wifi_set_mode(WIFI_MODE_STA));
  ESP_ERROR_CHECK(esp_wifi_set_config(WIFI_IF_STA, &config));

  printf("[CONNECT] Connecting to: %s\n", config.sta.ssid);

  ESP_ERROR_CHECK(esp_wifi_start());

  EventBits_t bits =
      xEventGroupWaitBits(wifi_event_group, WIFI_CONNECTED_BIT | WIFI_FAIL_BIT,
                          pdFALSE, pdFALSE, portMAX_DELAY);

  if (bits & WIFI_CONNECTED_BIT) {
    printf("[CONNECT] Connected successfully\n");
    return ESP_OK;
  }

  if (bits & WIFI_FAIL_BIT) {
    printf("[CONNECT] Connection failed\n");
    return ESP_FAIL;
  }

  printf("[CONNECT] Unknown error\n");
  return ESP_FAIL;
}

esp_err_t wifi_disconnect(void) {
  if (wifi_event_group) {
    vEventGroupDelete(wifi_event_group);
    wifi_event_group = NULL;
  }

  printf("[DISCONNECT] Disconnecting...\n");
  return esp_wifi_disconnect();
}

esp_err_t mwifi_deinit(void) {
  esp_err_t ret = esp_wifi_stop();

  if (ret == ESP_ERR_WIFI_NOT_INIT) {
    printf("[DEINIT] WiFi not initialized\n");
    return ret;
  }

  ESP_ERROR_CHECK(esp_wifi_deinit());

  ESP_ERROR_CHECK(esp_wifi_clear_default_wifi_driver_and_handlers(netif_sta));

  if (netif_sta) {
    esp_netif_destroy(netif_sta);
    netif_sta = NULL;
  }

  ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
      IP_EVENT, ESP_EVENT_ANY_ID, ip_event_handler));

  ESP_ERROR_CHECK(esp_event_handler_instance_unregister(
      WIFI_EVENT, ESP_EVENT_ANY_ID, wifi_event_handler));

  printf("[DEINIT] WiFi deinitialized\n");
  return ESP_OK;
}
