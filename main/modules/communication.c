#include "communication.h"
#include "storage.h"
#include "esp_log.h"

static const char *TAG = "COMM_DISP";
static uint32_t s_event_seq = 1;

esp_err_t communication_init(void) {
    ESP_LOGI(TAG, "Internal Event Dispatcher initialized.");
    return ESP_OK;
}

esp_err_t communication_post_event(const system_event_t *evt) {
    system_event_t numbered_evt = *evt;
    numbered_evt.event_id = __atomic_fetch_add(&s_event_seq, 1, __ATOMIC_RELAXED);
    return storage_log(&numbered_evt);
}

const char* event_type_to_str(event_type_t type) {
    switch (type) {
        case EVT_SYSTEM_BOOT:     return "BOOT";
        case EVT_ENTRY_SCAN:      return "ENTRY_SCAN";
        case EVT_EXIT_SCAN:       return "EXIT_SCAN";
        case EVT_PIR_MOTION:      return "PIR_MOTION";
        case EVT_LIGHT_OFF:       return "LIGHT_OFF";
        case EVT_SCHEDULE_UPDATE: return "SCHEDULE_MOD";
        case EVT_USER_MODIFIED:   return "USER_MOD";
        case EVT_EMERGENCY_RELEASE: return "EMERGENCY";
        case EVT_FALSE_TRIGGER:   return "FALSE_TRIGGER";
        default: return "UNKNOWN";
    }
}

const char* decision_to_str(access_decision_t decision) {
    switch (decision) {
        case DECISION_GRANTED:               return "GRANTED";
        case DECISION_DENIED_UNAUTHORIZED:   return "DENIED_UNAUTH";
        case DECISION_DENIED_SCHEDULE:       return "DENIED_HOURS";
        case DECISION_DENIED_ALREADY_INSIDE: return "DENIED_IN_BUILDING";
        case DECISION_DENIED_NOT_INSIDE:     return "DENIED_NOT_INSIDE";
        case DECISION_EMERGENCY_OVERRIDE:    return "EMERGENCY_OPEN";
        case DECISION_NONE:                  return "STATUS_EVENT";
        default: return "UNKNOWN";
    }
}

const char* room_id_to_str(room_id_t room) {
    switch (room) {
        case ROOM_ENTRANCE:  return "ENTRANCE";
        case ROOM_LECTURE_1: return "LECTURE_RM1";
        case ROOM_LECTURE_2: return "LECTURE_RM2";
        default: return "SYSTEM";
    }
}