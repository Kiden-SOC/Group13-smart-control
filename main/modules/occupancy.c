#include "occupancy.h"
#include "app_config.h"
#include "sync_manager.h"
#include "storage.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "OCCUPANCY";

static occupant_t s_occupants[APP_MAX_OCCUPANTS];
static uint32_t s_count = 0;

/* Caller holds g_occupancy_mutex so the snapshot is consistent. */
static void persist_locked(void) {
    storage_persist_save("occ_cnt", &s_count, sizeof(s_count));
    storage_persist_save("occ_list", s_occupants, sizeof(s_occupants));
}

esp_err_t occupancy_init(void) {
    xSemaphoreTake(g_occupancy_mutex, portMAX_DELAY);
    s_count = 0;
    memset(s_occupants, 0, sizeof(s_occupants));

    uint32_t saved_count = 0;
    if (storage_persist_load("occ_cnt", &saved_count, sizeof(saved_count)) == ESP_OK &&
        saved_count <= APP_MAX_OCCUPANTS &&
        storage_persist_load("occ_list", s_occupants, sizeof(s_occupants)) == ESP_OK) {
        s_count = saved_count;
    } else {
        memset(s_occupants, 0, sizeof(s_occupants));
    }
    xSemaphoreGive(g_occupancy_mutex);
    ESP_LOGI(TAG, "Occupancy tracker initialized. %u occupant(s) restored from NVS.", (unsigned)s_count);
    return ESP_OK;
}

bool occupancy_is_inside(const char *uid) {
    bool found = false;
    xSemaphoreTake(g_occupancy_mutex, portMAX_DELAY);
    for (uint32_t i = 0; i < s_count; i++) {
        if (strcasecmp(s_occupants[i].uid, uid) == 0) {
            found = true;
            break;
        }
    }
    xSemaphoreGive(g_occupancy_mutex);
    return found;
}

esp_err_t occupancy_add(const char *uid, const char *name, const char *time_str) {
    esp_err_t ret = ESP_OK;
    xSemaphoreTake(g_occupancy_mutex, portMAX_DELAY);
    if (s_count >= APP_MAX_OCCUPANTS) {
        ret = ESP_ERR_NO_MEM;
    } else {
        strncpy(s_occupants[s_count].uid, uid, sizeof(s_occupants[s_count].uid) - 1);
        strncpy(s_occupants[s_count].name, name, sizeof(s_occupants[s_count].name) - 1);
        strncpy(s_occupants[s_count].entry_time, time_str, sizeof(s_occupants[s_count].entry_time) - 1);
        s_count++;
        persist_locked();
        ESP_LOGI(TAG, "OCCUPANT ENTERED: %s (%s). Current inside: %u", name, uid, (unsigned)s_count);
    }
    xSemaphoreGive(g_occupancy_mutex);
    return ret;
}

esp_err_t occupancy_remove(const char *uid) {
    esp_err_t ret = ESP_ERR_NOT_FOUND;
    xSemaphoreTake(g_occupancy_mutex, portMAX_DELAY);
    for (uint32_t i = 0; i < s_count; i++) {
        if (strcasecmp(s_occupants[i].uid, uid) == 0) {
            ESP_LOGI(TAG, "OCCUPANT EXITED: %s (%s).", s_occupants[i].name, s_occupants[i].uid);
            for (uint32_t j = i; j < s_count - 1; j++) {
                s_occupants[j] = s_occupants[j + 1];
            }
            s_count--;
            memset(&s_occupants[s_count], 0, sizeof(s_occupants[s_count]));
            persist_locked();
            ret = ESP_OK;
            break;
        }
    }
    xSemaphoreGive(g_occupancy_mutex);
    return ret;
}

uint32_t occupancy_get_count(void) {
    xSemaphoreTake(g_occupancy_mutex, portMAX_DELAY);
    uint32_t c = s_count;
    xSemaphoreGive(g_occupancy_mutex);
    return c;
}

void occupancy_print_list(void) {
    xSemaphoreTake(g_occupancy_mutex, portMAX_DELAY);
    printf("\n=== COCIS CURRENT BUILDING OCCUPANTS (%u) ===\n", (unsigned)s_count);
    if (s_count == 0) {
        printf("Building is currently VACANT.\n");
    } else {
        for (uint32_t i = 0; i < s_count; i++) {
            printf("[%u] UID: %-14s | Name: %-24s | Entered: %s\n",
                   (unsigned)(i + 1), s_occupants[i].uid, s_occupants[i].name, s_occupants[i].entry_time);
        }
    }
    printf("============================================\n\n");
    xSemaphoreGive(g_occupancy_mutex);
}

void occupancy_clear_all(void) {
    xSemaphoreTake(g_occupancy_mutex, portMAX_DELAY);
    s_count = 0;
    memset(s_occupants, 0, sizeof(s_occupants));
    persist_locked();
    xSemaphoreGive(g_occupancy_mutex);
    ESP_LOGW(TAG, "Occupancy registry purged.");
}