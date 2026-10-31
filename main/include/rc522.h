#ifndef RC522_H
#define RC522_H

#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "esp_err.h"
#include <stdbool.h>

typedef struct {
    spi_device_handle_t spi;
    gpio_num_t cs_pin;
    gpio_num_t rst_pin;
    bool is_initialized;
} rc522_t;

esp_err_t rc522_init(rc522_t *dev, spi_host_device_t host, gpio_num_t cs_pin, gpio_num_t rst_pin);
bool rc522_is_card_present(rc522_t *dev);
bool rc522_read_uid(rc522_t *dev, char *out_uid_str, size_t max_len);
void rc522_halt(rc522_t *dev);

#endif // RC522_H