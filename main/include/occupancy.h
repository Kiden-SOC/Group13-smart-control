#ifndef OCCUPANCY_H
#define OCCUPANCY_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

typedef struct {
    char uid[24];
    char name[32];
    char entry_time[16];
} occupant_t;

esp_err_t occupancy_init(void);
bool occupancy_is_inside(const char *uid);
esp_err_t occupancy_add(const char *uid, const char *name, const char *time_str);
esp_err_t occupancy_remove(const char *uid);
uint32_t occupancy_get_count(void);
void occupancy_print_list(void);
void occupancy_clear_all(void);

#endif // OCCUPANCY_H