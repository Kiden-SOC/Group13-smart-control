#ifndef ROOM_CONTROL_H
#define ROOM_CONTROL_H

#include "esp_err.h"
#include "system_events.h"
#include <stdbool.h>
#include <stdint.h>

typedef enum {
    ROOM_LIGHT_OFF = 0,
    ROOM_LIGHT_DIM,      /* 1st motion seen, waiting for a 2nd motion to confirm */
    ROOM_LIGHT_BRIGHT    /* 2nd motion confirmed: room occupied                   */
} room_light_t;

esp_err_t room_control_init(void);
void room_control_simulate_motion(room_id_t room);
void room_control_set_timeout(room_id_t room, uint32_t seconds);
uint32_t room_control_get_timeout(room_id_t room);

/* occupied  : true only once the 2nd motion confirmed the room
 * light     : OFF / DIM / BRIGHT
 * remaining : seconds left on whichever timer is running (confirm window when DIM,
 *             inactivity timeout when BRIGHT, 0 when OFF)                           */
void room_control_get_status(room_id_t room, bool *out_occupied, room_light_t *out_light, uint32_t *out_remaining_sec);
void room_control_print_status(void);

#endif // ROOM_CONTROL_H
