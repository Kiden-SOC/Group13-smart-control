#include <stdio.h>
#include <string.h>
#include "esp_log.h"
#include "nvs_flash.h"
#include "driver/spi_master.h"
#include "app_config.h"
#include "sync_manager.h"
#include "communication.h"
#include "storage.h"
#include "occupancy.h"
#include "schedule.h"
#include "access_control.h"
#include "room_control.h"
#include "serial_menu.h"
#include "driver/gpio.h"

static const char *TAG = "APP_MAIN";

/*
 * The RC522 readers and the microSD card share one SPI bus (SPI3 / VSPI).
 * The bus MUST exist before anything attaches a device to it, so it is created
 * here, once, before storage_init() (SD mount) and access_control_init() (RC522).
 */
static esp_err_t shared_spi_bus_init(void) {
    spi_bus_config_t buscfg = {
        .miso_io_num = PIN_SPI_MISO,
        .mosi_io_num = PIN_SPI_MOSI,
        .sclk_io_num = PIN_SPI_SCK,
        .quadwp_io_num = -1,
        .quadhd_io_num = -1,
        .max_transfer_sz = APP_SPI_MAX_TRANSFER_SZ,
    };
    esp_err_t ret = spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "Shared SPI bus init failed: %s", esp_err_to_name(ret));
    } else {
        ESP_LOGI(TAG, "Shared SPI3 bus ready (SCK=%d MISO=%d MOSI=%d)", PIN_SPI_SCK, PIN_SPI_MISO, PIN_SPI_MOSI);
    }
    return ret;
}

static void spi_idle_cs_lines(void) {
    const gpio_num_t cs_pins[] = { PIN_ENTRY_RFID_CS, PIN_EXIT_RFID_CS, PIN_SD_CS };
    for (int i = 0; i < 3; i++) {
        gpio_reset_pin(cs_pins[i]);
        gpio_set_direction(cs_pins[i], GPIO_MODE_OUTPUT);
        gpio_set_level(cs_pins[i], 1);   /* deselect: CS is active low */
    }
    gpio_set_pull_mode(PIN_SPI_MISO, GPIO_PULLUP_ONLY);
}

void app_main(void) {
    ESP_LOGI(TAG, "Starting CoCIS Smart Control System (ESP32 Master)...");

    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    ESP_ERROR_CHECK(ret);

    ESP_ERROR_CHECK(sync_manager_init());
    ESP_ERROR_CHECK(communication_init());
    ESP_ERROR_CHECK(shared_spi_bus_init());
    spi_idle_cs_lines();                      /* NEW: deselect RC522s before SD mount */
    ESP_ERROR_CHECK(storage_init());
    ESP_ERROR_CHECK(occupancy_init());
    ESP_ERROR_CHECK(schedule_init());
    ESP_ERROR_CHECK(access_control_init());
    ESP_ERROR_CHECK(room_control_init());
    ESP_ERROR_CHECK(serial_menu_init());

    char time_str[16];
    schedule_get_clock_str(time_str, sizeof(time_str));

    system_event_t boot_evt = {
        .event_type = EVT_SYSTEM_BOOT,
        .decision = DECISION_NONE,
        .room = ROOM_NONE,
    };
    snprintf(boot_evt.time_str, sizeof(boot_evt.time_str), "%s", time_str);
    snprintf(boot_evt.uid, sizeof(boot_evt.uid), "SYSTEM");
    snprintf(boot_evt.user_name, sizeof(boot_evt.user_name), "ESP32_CORE");
    snprintf(boot_evt.details, sizeof(boot_evt.details), "All subsystems initialized on single ESP32 controller");
    communication_post_event(&boot_evt);
    ESP_LOGI(TAG, "Initialization Complete. All subsystems non-blocking and operational.");
}
