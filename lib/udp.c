#include "udp.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "esp_system.h"
#include "led.h"
#include "lwip/inet.h"
#include "lwip/sockets.h"
#include <sys/time.h>

#include "freertos/event_groups.h"
#include "freertos/task.h"

#include "led.h"

static int sock;
struct sockaddr_in dest_addr;

void init_socket(char *host, uint32_t port) {
  int addr_family = 0;
  int ip_protocol = 0;

  dest_addr.sin_addr.s_addr = inet_addr(host);
  dest_addr.sin_family = AF_INET;
  dest_addr.sin_port = htons(port);
  addr_family = AF_INET;
  ip_protocol = IPPROTO_IP;

  sock = socket(addr_family, SOCK_DGRAM, ip_protocol);
  if (sock < 0) {
    curr_state = ERROR_1;
    printf("Unable to create socket: errno %d\n", errno);
    return;
  }

  struct timeval timeout;
  timeout.tv_sec = 0;
  timeout.tv_usec = 61000;
  setsockopt(sock, SOL_SOCKET, SO_RCVTIMEO, &timeout, sizeof timeout);

  printf("Socket created, sending to %s:%d\n", host, port);
}

void send_udp(uint8_t *buf, int len) {
  if (sock < 0) {
    printf("socket not ready\n");
    return;
  }

  int err = sendto(sock, buf, len, 0, (struct sockaddr *)&dest_addr,
                   sizeof(dest_addr));

  if (err < 0) {
    curr_state = ERROR_2;
    printf("Error occurred during sending: %d %d errno %d %d\n",
           (int)esp_get_free_heap_size(), len, errno, err);
    return;
  } else if (curr_state == ERROR_2 || curr_state == BT_CONNECTED) {
    curr_state = UDP_CONN;
  }
}
