#ifndef SYNC_MANAGER_H
#define SYNC_MANAGER_H

#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "freertos/queue.h"
#include "esp_err.h"

extern SemaphoreHandle_t g_occupancy_mutex;
extern SemaphoreHandle_t g_schedule_mutex;
extern SemaphoreHandle_t g_room_mutex;
extern QueueHandle_t    g_event_queue;

esp_err_t sync_manager_init(void);

#endif // SYNC_MANAGER_H