#ifndef COMMUNICATION_H
#define COMMUNICATION_H

#include "esp_err.h"
#include "system_events.h"

esp_err_t communication_init(void);
esp_err_t communication_post_event(const system_event_t *evt);

#endif // COMMUNICATION_H