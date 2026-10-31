#include "rc522.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "RC522_DRV";

#define RC522_REG_COMMAND     0x01
#define RC522_REG_COMIRQ      0x04
#define RC522_REG_ERROR       0x06
#define RC522_REG_FIFODATA    0x09
#define RC522_REG_FIFOLEVEL   0x0A
#define RC522_REG_CONTROL     0x0C
#define RC522_REG_BITFRAMING  0x0D
#define RC522_REG_COLL        0x0E
#define RC522_REG_MODE        0x11
#define RC522_REG_TXCONTROL   0x14
#define RC522_REG_TXASK       0x15
#define RC522_REG_VERSION     0x37
#define RC522_REG_TMODE       0x2A
#define RC522_REG_TPRESCALER  0x2B
#define RC522_REG_TRELOADH    0x2C
#define RC522_REG_TRELOADL    0x2D

#define RC522_CMD_IDLE        0x00
#define RC522_CMD_TRANSCEIVE  0x0C
#define RC522_CMD_SOFT_RESET  0x0F

#define PICC_CMD_REQA         0x26
#define PICC_CMD_ANTICOLL     0x93

static esp_err_t rc522_write_reg(rc522_t *dev, uint8_t reg, uint8_t val) {
    uint8_t tx[2] = { (uint8_t)((reg << 1) & 0x7E), val };
    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = tx,
    };
    return spi_device_polling_transmit(dev->spi, &t);
}

static uint8_t rc522_read_reg(rc522_t *dev, uint8_t reg) {
    uint8_t tx[2] = { (uint8_t)(((reg << 1) & 0x7E) | 0x80), 0 };
    uint8_t rx[2] = { 0 };
    spi_transaction_t t = {
        .length = 16,
        .tx_buffer = tx,
        .rx_buffer = rx,
    };
    spi_device_polling_transmit(dev->spi, &t);
    return rx[1];
}

static void rc522_set_bitmask(rc522_t *dev, uint8_t reg, uint8_t mask) {
    rc522_write_reg(dev, reg, rc522_read_reg(dev, reg) | mask);
}

__attribute__((unused)) static void rc522_clear_bitmask(rc522_t *dev, uint8_t reg, uint8_t mask) {
    rc522_write_reg(dev, reg, rc522_read_reg(dev, reg) & (~mask));
}

/* The antenna (TX1/TX2 drivers) must be ON for the reader to see any card. */
static void rc522_antenna_on(rc522_t *dev) {
    if ((rc522_read_reg(dev, RC522_REG_TXCONTROL) & 0x03) != 0x03) {
        rc522_set_bitmask(dev, RC522_REG_TXCONTROL, 0x03);
    }
}

esp_err_t rc522_init(rc522_t *dev, spi_host_device_t host, gpio_num_t cs_pin, gpio_num_t rst_pin) {
    dev->cs_pin = cs_pin;
    dev->rst_pin = rst_pin;
    dev->is_initialized = false;

    gpio_config_t rst_cfg = {
        .pin_bit_mask = (1ULL << rst_pin),
        .mode = GPIO_MODE_OUTPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE
    };
    gpio_config(&rst_cfg);
    gpio_set_level(rst_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(10));

    spi_device_interface_config_t devcfg = {
        .clock_speed_hz = 4 * 1000 * 1000,
        .mode = 0,
        .spics_io_num = cs_pin,
        .queue_size = 4,
    };

    esp_err_t ret = spi_bus_add_device(host, &devcfg, &dev->spi);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Failed to attach RC522 device to SPI host (CS=%d)", cs_pin);
        return ret;
    }

    rc522_write_reg(dev, RC522_REG_COMMAND, RC522_CMD_SOFT_RESET);
    vTaskDelay(pdMS_TO_TICKS(50));

    rc522_write_reg(dev, RC522_REG_TMODE, 0x8D);
    rc522_write_reg(dev, RC522_REG_TPRESCALER, 0x3E);
    rc522_write_reg(dev, RC522_REG_TRELOADL, 30);
    rc522_write_reg(dev, RC522_REG_TRELOADH, 0);
    rc522_write_reg(dev, RC522_REG_TXASK, 0x40);
    rc522_write_reg(dev, RC522_REG_MODE, 0x3D);

    rc522_set_bitmask(dev, RC522_REG_TXCONTROL, 0x03);

    uint8_t ver = rc522_read_reg(dev, RC522_REG_VERSION);
    ESP_LOGI(TAG, "RC522 attached (CS=%d, RST=%d). Firmware Version: 0x%02X", cs_pin, rst_pin, ver);
    dev->is_initialized = true;
    return ESP_OK;
}

bool rc522_is_card_present(rc522_t *dev) {
    if (!dev->is_initialized) return false;
    rc522_antenna_on(dev);   /* self-heal: never poll with the antenna off */
    rc522_write_reg(dev, RC522_REG_BITFRAMING, 0x07);
    rc522_write_reg(dev, RC522_REG_COMMAND, RC522_CMD_IDLE);
    rc522_write_reg(dev, RC522_REG_COMIRQ, 0x7F);
    rc522_set_bitmask(dev, RC522_REG_FIFOLEVEL, 0x80);
    rc522_write_reg(dev, RC522_REG_FIFODATA, PICC_CMD_REQA);
    rc522_write_reg(dev, RC522_REG_COMMAND, RC522_CMD_TRANSCEIVE);
    rc522_set_bitmask(dev, RC522_REG_BITFRAMING, 0x80);

    for (int i = 0; i < 25; i++) {
        uint8_t n = rc522_read_reg(dev, RC522_REG_COMIRQ);
        if (n & 0x30) return true;
        if (n & 0x01) return false;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    return false;
}

bool rc522_read_uid(rc522_t *dev, char *out_uid_str, size_t max_len) {
    if (!dev->is_initialized) return false;

    rc522_antenna_on(dev);
    rc522_write_reg(dev, RC522_REG_BITFRAMING, 0x00);
    rc522_write_reg(dev, RC522_REG_COMMAND, RC522_CMD_IDLE);
    rc522_write_reg(dev, RC522_REG_COMIRQ, 0x7F);
    rc522_set_bitmask(dev, RC522_REG_FIFOLEVEL, 0x80);

    rc522_write_reg(dev, RC522_REG_FIFODATA, PICC_CMD_ANTICOLL);
    rc522_write_reg(dev, RC522_REG_FIFODATA, 0x20);
    rc522_write_reg(dev, RC522_REG_COMMAND, RC522_CMD_TRANSCEIVE);
    rc522_set_bitmask(dev, RC522_REG_BITFRAMING, 0x80);

    bool success = false;
    for (int i = 0; i < 25; i++) {
        uint8_t n = rc522_read_reg(dev, RC522_REG_COMIRQ);
        if (n & 0x30) { success = true; break; }
        if (n & 0x01) return false;
        vTaskDelay(pdMS_TO_TICKS(1));
    }
    if (!success) return false;

    uint8_t fifo_level = rc522_read_reg(dev, RC522_REG_FIFOLEVEL);
    if (fifo_level < 4) return false;

    uint8_t uid[5] = {0};
    for (int i = 0; i < fifo_level && i < 5; i++) {
        uid[i] = rc522_read_reg(dev, RC522_REG_FIFODATA);
    }

    snprintf(out_uid_str, max_len, "%02X:%02X:%02X:%02X", uid[0], uid[1], uid[2], uid[3]);
    return true;
}

void rc522_halt(rc522_t *dev) {
    if (!dev->is_initialized) return;
    /* Finish the current transaction but KEEP THE ANTENNA ON, otherwise the next tap
     * is never seen. Repeat reads of a card left on the reader are limited by the
     * debounce delay in the polling task. */
    rc522_write_reg(dev, RC522_REG_COMMAND, RC522_CMD_IDLE);
    rc522_set_bitmask(dev, RC522_REG_FIFOLEVEL, 0x80);   /* flush FIFO */
    rc522_antenna_on(dev);
}