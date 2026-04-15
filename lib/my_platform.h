#ifndef MY_PLATFORM_H
#define MY_PLATFORM_H

#include <stdbool.h>
#include <stdint.h>
#include <uni.h>

// Custom "instance"
typedef struct my_platform_instance_s {
  uni_gamepad_seat_t gamepad_seat;
} my_platform_instance_t;

/**
 * @brief Returns the custom platform implementation.
 *
 * This is the entry point used by the system to retrieve
 * the platform structure with all callbacks configured.
 */
struct uni_platform *get_my_platform(void);

#endif // MY_PLATFORM_H
