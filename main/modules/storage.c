#include "storage.h"
#include "app_config.h"
#include "sync_manager.h"
#include "esp_log.h"
#include "esp_vfs_fat.h"
#include "sdmmc_cmd.h"
#include "driver/spi_master.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "nvs.h"
#include <sys/stat.h>
#include <string.h>
#include <stdio.h>

static const char *TAG = "STORAGE";

static system_event_t s_log_buffer[APP_LOG_BUFFER_SIZE];
static uint32_t s_head = 0;
static uint32_t s_total_events = 0;
static bool s_sd_mounted = false;
static sdmmc_card_t *s_card = NULL;
static esp_err_t s_sd_err = ESP_OK;

#define PERSIST_NAMESPACE "cocis"
#define SD_AUDIT_PATH     "/sdcard/cocis_audit.csv"

static void storage_worker_task(void *pvParameters) {
    system_event_t evt;
    while (1) {
        if (xQueueReceive(g_event_queue, &evt, portMAX_DELAY) == pdTRUE) {
            s_log_buffer[s_head] = evt;
            s_head = (s_head + 1) % APP_LOG_BUFFER_SIZE;
            s_total_events++;

            ESP_LOGI(TAG, "[EVENT #%lu] %s | Room: %s | UID: %s | User: %s | %s | %s",
                     (unsigned long)evt.event_id, evt.time_str, room_id_to_str(evt.room),
                     evt.uid, evt.user_name, decision_to_str(evt.decision), evt.details);

            if (s_sd_mounted) {
                FILE *f = fopen(SD_AUDIT_PATH, "a");
                if (f) {
                    fprintf(f, "%lu,%s,%s,%s,%s,%s,%s,%s\n",
                            (unsigned long)evt.event_id, evt.time_str, evt.uid, evt.user_name,
                            event_type_to_str(evt.event_type), decision_to_str(evt.decision),
                            room_id_to_str(evt.room), evt.details);
                    fclose(f);
                }
            }
        }
    }
}

esp_err_t storage_init(void) {
    ESP_LOGI(TAG, "Initializing Storage Engine...");

    /* NOTE: the shared SPI3 bus is created in app_main() BEFORE this runs.
     * The SD driver only attaches a device to an existing bus; it never creates one. */
    esp_vfs_fat_sdmmc_mount_config_t mount_config = {
        .format_if_mount_failed = true,   /* simulator card may be blank; the card only holds the audit log */
        .max_files = 3,
        .allocation_unit_size = 16 * 1024
    };

    sdmmc_host_t host = SDSPI_HOST_DEFAULT();
    host.slot = SPI3_HOST;

    sdspi_device_config_t slot_config = SDSPI_DEVICE_CONFIG_DEFAULT();
    slot_config.gpio_cs = PIN_SD_CS;
    slot_config.host_id = SPI3_HOST;

    esp_err_t ret = esp_vfs_fat_sdspi_mount("/sdcard", &host, &slot_config, &mount_config, &s_card);
    s_sd_err = ret;
    if (ret == ESP_OK) {
        s_sd_mounted = true;
        ESP_LOGI(TAG, "MicroSD mounted via SDSPI at /sdcard (CS=GPIO%d)", PIN_SD_CS);

        /* Write the CSV header only when the file does not exist yet (not on every boot). */
        struct stat st;
        if (stat(SD_AUDIT_PATH, &st) != 0) {
            FILE *f = fopen(SD_AUDIT_PATH, "a");
            if (f) {
                fprintf(f, "EventID,Time,UID,Name,Type,Decision,Room,Details\n");
                fclose(f);
            }
        }
    } else {
        s_sd_mounted = false;
        ESP_LOGW(TAG, "SD mount failed (%s). Falling back to circular RAM logging.", esp_err_to_name(ret));
    }

    xTaskCreate(storage_worker_task, "storage_task", 4096, NULL, 4, NULL);
    return ESP_OK;
}

esp_err_t storage_log(const system_event_t *evt) {
    if (xQueueSend(g_event_queue, evt, pdMS_TO_TICKS(100)) != pdTRUE) {
        ESP_LOGE(TAG, "Storage Queue full! Event dropped.");
        return ESP_ERR_NO_MEM;
    }
    return ESP_OK;
}

void storage_print_recent(uint32_t count) {
    printf("\n=== COCIS SYSTEM AUDIT LOGS (Total Recorded: %u) ===\n", (unsigned)s_total_events);
    if (s_total_events == 0) {
        printf("No log events recorded yet.\n");
        printf("====================================================\n\n");
        return;
    }

    uint32_t to_show = (count > s_total_events) ? s_total_events : count;
    if (to_show > APP_LOG_BUFFER_SIZE) to_show = APP_LOG_BUFFER_SIZE;

    uint32_t start_idx = (s_head + APP_LOG_BUFFER_SIZE - to_show) % APP_LOG_BUFFER_SIZE;
    for (uint32_t i = 0; i < to_show; i++) {
        uint32_t idx = (start_idx + i) % APP_LOG_BUFFER_SIZE;
        system_event_t *e = &s_log_buffer[idx];
        printf("[%03lu] %s | %-12s | %-14s | %-20s | %-14s | %s\n",
               (unsigned long)e->event_id, e->time_str, room_id_to_str(e->room), e->uid,
               e->user_name, decision_to_str(e->decision), e->details);
    }
    printf("====================================================\n\n");
}

void storage_print_diagnostics(void) {
    printf("\n=== STORAGE ENGINE DIAGNOSTICS ===\n");
    printf("SD Card Interface    : %s\n", s_sd_mounted ? "MOUNTED (FATFS /sdcard)" : "OFFLINE / UNMOUNTED");
    if (!s_sd_mounted) printf("SD Mount Error       : %s\n", esp_err_to_name(s_sd_err));
    printf("Circular RAM Buffer  : ACTIVE (%d slots)\n", APP_LOG_BUFFER_SIZE);
    printf("Total Events Logged  : %u\n", (unsigned)s_total_events);
    printf("FreeRTOS Event Queue : Depth %d | Free slots: %u\n",
           APP_EVENT_QUEUE_LEN, (unsigned)uxQueueSpacesAvailable(g_event_queue));
    printf("==================================\n\n");
}

/* ---------------- NVS persistence (users, schedule, occupancy) ---------------- */

esp_err_t storage_persist_save(const char *key, const void *data, size_t len) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(PERSIST_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) {
        ESP_LOGW(TAG, "NVS open failed (%s): '%s' not saved", esp_err_to_name(err), key);
        return err;
    }
    err = nvs_set_blob(h, key, data, len);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    if (err != ESP_OK) ESP_LOGW(TAG, "NVS save '%s' failed: %s", key, esp_err_to_name(err));
    return err;
}

esp_err_t storage_persist_load(const char *key, void *out, size_t expected_len) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(PERSIST_NAMESPACE, NVS_READONLY, &h);
    if (err == ESP_ERR_NVS_NOT_FOUND) return ESP_ERR_NOT_FOUND;   /* namespace not created yet */
    if (err != ESP_OK) return err;

    size_t len = 0;
    err = nvs_get_blob(h, key, NULL, &len);
    if (err == ESP_ERR_NVS_NOT_FOUND) { nvs_close(h); return ESP_ERR_NOT_FOUND; }
    if (err != ESP_OK)                { nvs_close(h); return err; }
    if (len != expected_len)          { nvs_close(h); return ESP_ERR_INVALID_SIZE; }

    err = nvs_get_blob(h, key, out, &len);
    nvs_close(h);
    return err;
}

esp_err_t storage_persist_erase_all(void) {
    nvs_handle_t h;
    esp_err_t err = nvs_open(PERSIST_NAMESPACE, NVS_READWRITE, &h);
    if (err != ESP_OK) return err;
    err = nvs_erase_all(h);
    if (err == ESP_OK) err = nvs_commit(h);
    nvs_close(h);
    return err;
}
