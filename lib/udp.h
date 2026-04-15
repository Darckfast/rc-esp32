#ifndef UDP_H
#define UDP_H

#include <stdint.h>

void init_socket(char *host, uint32_t port);
void send_udp(uint8_t *buf, int len);

#endif
