#ifndef SYSTEM_EVENTS_H
#define SYSTEM_EVENTS_H

#include <stdint.h>
#include <stdbool.h>

typedef enum {
    EVT_SYSTEM_BOOT = 0,
    EVT_ENTRY_SCAN,
    EVT_EXIT_SCAN,
    EVT_PIR_MOTION,
    EVT_LIGHT_OFF,
    EVT_SCHEDULE_UPDATE,
    EVT_USER_MODIFIED,
    EVT_EMERGENCY_RELEASE,
    EVT_FALSE_TRIGGER
} event_type_t;

typedef enum {
    DECISION_NONE = 0,
    DECISION_GRANTED,
    DECISION_DENIED_UNAUTHORIZED,
    DECISION_DENIED_SCHEDULE,
    DECISION_DENIED_ALREADY_INSIDE,
    DECISION_DENIED_NOT_INSIDE,
    DECISION_EMERGENCY_OVERRIDE
} access_decision_t;

typedef enum {
    ROOM_NONE = 0,
    ROOM_ENTRANCE = 1,
    ROOM_LECTURE_1 = 2,
    ROOM_LECTURE_2 = 3
} room_id_t;

typedef struct {
    uint32_t event_id;
    uint32_t timestamp_sec;
    char time_str[16];
    char uid[24];
    char user_name[32];
    event_type_t event_type;
    access_decision_t decision;
    room_id_t room;
    char details[64];
} system_event_t;

const char* event_type_to_str(event_type_t type);
const char* decision_to_str(access_decision_t decision);
const char* room_id_to_str(room_id_t room);

#endif // SYSTEM_EVENTS_H