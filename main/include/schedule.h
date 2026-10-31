#ifndef SCHEDULE_H
#define SCHEDULE_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

esp_err_t schedule_init(void);
bool schedule_is_allowed_now(void);
esp_err_t schedule_update(uint8_t open_h, uint8_t open_m, uint8_t close_h, uint8_t close_m);
void schedule_get(uint8_t *open_h, uint8_t *open_m, uint8_t *close_h, uint8_t *close_m);
void schedule_set_clock(uint8_t h, uint8_t m, uint8_t s);
void schedule_get_clock_str(char *buf, size_t max_len);
void schedule_get_clock_hms(uint8_t *h, uint8_t *m, uint8_t *s);
bool schedule_admin_login(const char *pin);
void schedule_admin_logout(void);
bool schedule_is_admin(void);

#endif // SCHEDULE_H