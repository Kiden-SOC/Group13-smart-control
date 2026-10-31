#include "sync_manager.h"
#include "app_config.h"
#include "system_events.h"
#include "esp_log.h"

static const char *TAG = "SYNC_MGR";

SemaphoreHandle_t g_occupancy_mutex = NULL;
SemaphoreHandle_t g_schedule_mutex  = NULL;
SemaphoreHandle_t g_room_mutex      = NULL;
QueueHandle_t    g_event_queue     = NULL;

esp_err_t sync_manager_init(void) {
    g_occupancy_mutex = xSemaphoreCreateMutex();
    g_schedule_mutex  = xSemaphoreCreateMutex();
    g_room_mutex      = xSemaphoreCreateMutex();
    g_event_queue     = xQueueCreate(APP_EVENT_QUEUE_LEN, sizeof(system_event_t));

    if (!g_occupancy_mutex || !g_schedule_mutex || !g_room_mutex || !g_event_queue) {
        ESP_LOGE(TAG, "FATAL: Failed to allocate FreeRTOS synchronization primitives.");
        return ESP_ERR_NO_MEM;
    }

    ESP_LOGI(TAG, "Synchronization resources initialized (Mutexes & Event Queue).");
    return ESP_OK;
}