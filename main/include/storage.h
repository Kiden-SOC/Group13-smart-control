#ifndef STORAGE_H
#define STORAGE_H

#include <stddef.h>
#include "esp_err.h"
#include "system_events.h"

esp_err_t storage_init(void);
esp_err_t storage_log(const system_event_t *evt);
void storage_print_recent(uint32_t count);
void storage_print_diagnostics(void);

/* Persistent settings in NVS flash (namespace "cocis"): users, schedule, occupancy.
 * load  : ESP_OK only when a blob of exactly expected_len bytes exists.
 *         ESP_ERR_NOT_FOUND = nothing saved yet, ESP_ERR_INVALID_SIZE = stale layout. */
esp_err_t storage_persist_save(const char *key, const void *data, size_t len);
esp_err_t storage_persist_load(const char *key, void *out, size_t expected_len);
esp_err_t storage_persist_erase_all(void);

#endif // STORAGE_H
