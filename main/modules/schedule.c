#include "schedule.h"
#include "app_config.h"
#include "sync_manager.h"
#include "storage.h"
#include "esp_timer.h"
#include "esp_log.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "SCHEDULE";

static uint8_t s_open_h = APP_DEFAULT_OPEN_HOUR;
static uint8_t s_open_m = APP_DEFAULT_OPEN_MIN;
static uint8_t s_close_h = APP_DEFAULT_CLOSE_HOUR;
static uint8_t s_close_m = APP_DEFAULT_CLOSE_MIN;

static uint32_t s_clock_seconds = (9 * 3600); // 09:00:00 default
static bool s_admin_logged_in = false;
static int64_t s_last_timer_us = 0;

static void update_simulated_clock(void) {
    int64_t now_us = esp_timer_get_time();
    int64_t diff_sec = (now_us - s_last_timer_us) / 1000000ULL;
    if (diff_sec > 0) {
        s_clock_seconds = (s_clock_seconds + diff_sec) % 86400ULL;
        s_last_timer_us += (diff_sec * 1000000ULL);
    }
}

esp_err_t schedule_init(void) {
    s_last_timer_us = esp_timer_get_time();

    uint8_t saved[4];
    if (storage_persist_load("sched", saved, sizeof(saved)) == ESP_OK &&
        saved[0] <= 23 && saved[2] <= 23 && saved[1] <= 59 && saved[3] <= 59) {
        s_open_h = saved[0]; s_open_m = saved[1]; s_close_h = saved[2]; s_close_m = saved[3];
        ESP_LOGI(TAG, "Restored saved access schedule from NVS");
    }
    ESP_LOGI(TAG, "Access Schedule initialized: %02d:%02d to %02d:%02d. Sim Clock: 09:00:00",
             s_open_h, s_open_m, s_close_h, s_close_m);
    return ESP_OK;
}

bool schedule_is_allowed_now(void) {
    xSemaphoreTake(g_schedule_mutex, portMAX_DELAY);
    update_simulated_clock();

    uint32_t now_min = (s_clock_seconds / 60) % 1440;
    uint32_t open_min = s_open_h * 60 + s_open_m;
    uint32_t close_min = s_close_h * 60 + s_close_m;

    bool allowed = false;
    if (open_min <= close_min) {
        allowed = (now_min >= open_min && now_min < close_min);
    } else {
        // Spans midnight
        allowed = (now_min >= open_min || now_min < close_min);
    }

    xSemaphoreGive(g_schedule_mutex);
    return allowed;
}

esp_err_t schedule_update(uint8_t open_h, uint8_t open_m, uint8_t close_h, uint8_t close_m) {
    if (open_h > 23 || close_h > 23 || open_m > 59 || close_m > 59) return ESP_ERR_INVALID_ARG;
    if (open_h == close_h && open_m == close_m) return ESP_ERR_INVALID_ARG;

    xSemaphoreTake(g_schedule_mutex, portMAX_DELAY);
    s_open_h = open_h;
    s_open_m = open_m;
    s_close_h = close_h;
    s_close_m = close_m;
    ESP_LOGI(TAG, "Schedule changed: %02d:%02d - %02d:%02d", s_open_h, s_open_m, s_close_h, s_close_m);
    xSemaphoreGive(g_schedule_mutex);

    /* Persist outside the mutex (flash write is slow). */
    uint8_t to_save[4] = { open_h, open_m, close_h, close_m };
    storage_persist_save("sched", to_save, sizeof(to_save));
    return ESP_OK;
}

void schedule_get(uint8_t *open_h, uint8_t *open_m, uint8_t *close_h, uint8_t *close_m) {
    xSemaphoreTake(g_schedule_mutex, portMAX_DELAY);
    *open_h = s_open_h;
    *open_m = s_open_m;
    *close_h = s_close_h;
    *close_m = s_close_m;
    xSemaphoreGive(g_schedule_mutex);
}

void schedule_set_clock(uint8_t h, uint8_t m, uint8_t s) {
    if (h > 23 || m > 59 || s > 59) return;
    xSemaphoreTake(g_schedule_mutex, portMAX_DELAY);
    s_clock_seconds = h * 3600 + m * 60 + s;
    s_last_timer_us = esp_timer_get_time();
    xSemaphoreGive(g_schedule_mutex);
    ESP_LOGI(TAG, "Wall-Clock updated to %02d:%02d:%02d", h, m, s);
}

void schedule_get_clock_str(char *buf, size_t max_len) {
    xSemaphoreTake(g_schedule_mutex, portMAX_DELAY);
    update_simulated_clock();
    uint32_t total = s_clock_seconds;
    xSemaphoreGive(g_schedule_mutex);

    uint8_t h = total / 3600;
    uint8_t m = (total % 3600) / 60;
    uint8_t s = total % 60;
    snprintf(buf, max_len, "%02d:%02d:%02d", h, m, s);
}

void schedule_get_clock_hms(uint8_t *h, uint8_t *m, uint8_t *s) {
    xSemaphoreTake(g_schedule_mutex, portMAX_DELAY);
    update_simulated_clock();
    uint32_t total = s_clock_seconds;
    xSemaphoreGive(g_schedule_mutex);

    *h = total / 3600;
    *m = (total % 3600) / 60;
    *s = total % 60;
}

bool schedule_admin_login(const char *pin) {
    xSemaphoreTake(g_schedule_mutex, portMAX_DELAY);
    if (strcmp(pin, APP_ADMIN_DEFAULT_PIN) == 0) {
        s_admin_logged_in = true;
        xSemaphoreGive(g_schedule_mutex);
        return true;
    }
    xSemaphoreGive(g_schedule_mutex);
    return false;
}

void schedule_admin_logout(void) {
    xSemaphoreTake(g_schedule_mutex, portMAX_DELAY);
    s_admin_logged_in = false;
    xSemaphoreGive(g_schedule_mutex);
}

bool schedule_is_admin(void) {
    xSemaphoreTake(g_schedule_mutex, portMAX_DELAY);
    bool logged = s_admin_logged_in;
    xSemaphoreGive(g_schedule_mutex);
    return logged;
}