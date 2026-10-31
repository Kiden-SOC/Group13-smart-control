#ifndef ACCESS_CONTROL_H
#define ACCESS_CONTROL_H

#include "esp_err.h"
#include "system_events.h"
#include <stdbool.h>

typedef struct {
    char uid[24];
    char name[32];
    char role[16];
    bool authorized;
} user_record_t;

esp_err_t access_control_init(void);
access_decision_t access_control_evaluate(const char *uid, bool is_entry, char *out_name, size_t name_len, char *out_details, size_t det_len);
void access_control_actuate_door(bool is_entry, bool granted);
void access_control_simulate_scan(const char *uid, bool is_entry);
esp_err_t access_control_add_user(const char *uid, const char *name, const char *role, bool authorized);
esp_err_t access_control_remove_user(const char *uid);
void access_control_list_users(void);
void access_control_trigger_emergency_egress(void);

#endif // ACCESS_CONTROL_H