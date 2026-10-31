#include "access_control.h"
#include "app_config.h"
#include "occupancy.h"
#include "schedule.h"
#include "communication.h"
#include "rc522.h"
#include "storage.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdio.h>

static const char *TAG = "ACCESS_CTRL";

static user_record_t s_user_db[APP_MAX_USERS];
static size_t s_user_count = 0;
static bool s_persist_enabled = false;   /* off while seeding defaults / restoring */

static rc522_t s_entry_reader;
static rc522_t s_exit_reader;

static void persist_users(void) {
    if (!s_persist_enabled) return;
    uint32_t count = (uint32_t)s_user_count;
    storage_persist_save("usr_cnt", &count, sizeof(count));
    storage_persist_save("usr_list", s_user_db, sizeof(s_user_db));
}

/* Returns true if a valid saved user table was restored. */
static bool restore_users(void) {
    uint32_t count = 0;
    if (storage_persist_load("usr_cnt", &count, sizeof(count)) != ESP_OK || count > APP_MAX_USERS) return false;
    if (storage_persist_load("usr_list", s_user_db, sizeof(s_user_db)) != ESP_OK) return false;
    s_user_count = count;
    return true;
}

/* Forward declaration */
static void rfid_poll_task(void *pvParameters);

static void populate_initial_users(void) {
    s_user_count = 0;
    
    // Wokwi Blue Card -> Authorized Faculty (Dean)
    access_control_add_user("01:03:04:00", "Dr. Okello (Dean)", "Faculty", true);
    access_control_add_user("01:02:03:04", "Dr. Okello (Dean)", "Faculty", true);

    // Wokwi Green Card -> Authorized Student
    access_control_add_user("11:33:44:00", "Kato John (CS II)", "Student", true);
    access_control_add_user("11:22:33:44", "Kato John (CS II)", "Student", true);

    // Wokwi Yellow Card -> Authorized Student
    access_control_add_user("55:77:CC:00", "Babirye Sarah (BSE III)", "Student", true);
    access_control_add_user("55:66:77:88", "Babirye Sarah (BSE III)", "Student", true);

    // Wokwi Key Fob (Orange) -> Authorized Staff
    access_control_add_user("C0:EE:48:00", "Security Officer", "Staff", true);
    access_control_add_user("DE:AD:BE:EF", "Security Officer", "Staff", true);

    // Wokwi NFC Tag -> Authorized Guest Researcher
    access_control_add_user("04:22:04:00", "Guest Researcher", "Guest", true);

    // Wokwi Red Card -> Explicitly Unauthorized for testing access denial
    access_control_add_user("AA:CC:00:00", "Unknown Visitor", "Guest", false);
    access_control_add_user("AA:BB:CC:DD", "Unknown Visitor", "Guest", false);
}

access_decision_t access_control_evaluate(const char *uid, bool is_entry, char *out_name, size_t name_len, char *out_details, size_t det_len) {
    const user_record_t *matched = NULL;
    for (size_t i = 0; i < s_user_count; i++) {
        if (strcasecmp(s_user_db[i].uid, uid) == 0) {
            matched = &s_user_db[i];
            break;
        }
    }

    if (!matched || !matched->authorized) {
        if (matched) {
            snprintf(out_name, name_len, "%s", matched->name);
            snprintf(out_details, det_len, "User explicitly revoked");
        } else {
            snprintf(out_name, name_len, "Unregistered Tag");
            snprintf(out_details, det_len, "UID not found in database");
        }
        return DECISION_DENIED_UNAUTHORIZED;
    }

    snprintf(out_name, name_len, "%s", matched->name);

    if (strcmp(matched->role, "Staff") != 0 && !schedule_is_allowed_now()) {
        snprintf(out_details, det_len, "Scan rejected: outside authorized hours");
        return DECISION_DENIED_SCHEDULE;
    }

    bool inside = occupancy_is_inside(uid);

    if (is_entry) {
        if (inside) {
            snprintf(out_details, det_len, "Anti-passback: user already recorded inside");
            return DECISION_DENIED_ALREADY_INSIDE;
        }
    } else {
        if (!inside) {
            snprintf(out_details, det_len, "Invalid egress: user not recorded inside");
            return DECISION_DENIED_NOT_INSIDE;
        }
    }

    snprintf(out_details, det_len, "Access verified for %s (%s)", matched->name, matched->role);
    return DECISION_GRANTED;
}

void access_control_actuate_door(bool is_entry, bool granted) {
    gpio_num_t door_pin = is_entry ? PIN_ENTRY_DOOR_LED : PIN_EXIT_DOOR_LED;
    if (granted) {
        gpio_set_level(door_pin, 1);
        gpio_set_level(PIN_BUZZER, 1);
        vTaskDelay(pdMS_TO_TICKS(150));
        gpio_set_level(PIN_BUZZER, 0);

        vTaskDelay(pdMS_TO_TICKS(APP_DOOR_UNLOCK_MS - 150));
        gpio_set_level(door_pin, 0);
    } else {
        for (int i = 0; i < 3; i++) {
            gpio_set_level(door_pin, 1);
            gpio_set_level(PIN_BUZZER, 1);
            vTaskDelay(pdMS_TO_TICKS(80));
            gpio_set_level(door_pin, 0);
            gpio_set_level(PIN_BUZZER, 0);
            vTaskDelay(pdMS_TO_TICKS(80));
        }
    }
}

void access_control_simulate_scan(const char *uid, bool is_entry) {
    char name[32] = {0};
    char details[64] = {0};
    char time_str[16] = {0};
    schedule_get_clock_str(time_str, sizeof(time_str));

    access_decision_t decision = access_control_evaluate(uid, is_entry, name, sizeof(name), details, sizeof(details));

    if (decision == DECISION_GRANTED) {
        if (is_entry) {
            occupancy_add(uid, name, time_str);
        } else {
            occupancy_remove(uid);
        }
    }

    system_event_t evt = {
        .timestamp_sec = 0,
        .event_type = is_entry ? EVT_ENTRY_SCAN : EVT_EXIT_SCAN,
        .decision = decision,
        .room = ROOM_ENTRANCE,
    };
    snprintf(evt.time_str, sizeof(evt.time_str), "%s", time_str);
    snprintf(evt.uid, sizeof(evt.uid), "%s", uid);
    snprintf(evt.user_name, sizeof(evt.user_name), "%s", name);
    snprintf(evt.details, sizeof(evt.details), "%s", details);

    communication_post_event(&evt);
    access_control_actuate_door(is_entry, (decision == DECISION_GRANTED));
}

static void rfid_poll_task(void *pvParameters) {
    char uid_buf[24] = {0};
    while (1) {
        // Poll Entry Reader
        if (rc522_is_card_present(&s_entry_reader)) {
            if (rc522_read_uid(&s_entry_reader, uid_buf, sizeof(uid_buf))) {
                ESP_LOGI(TAG, "Card detected on ENTRY reader: UID %s", uid_buf);
                access_control_simulate_scan(uid_buf, true);
                rc522_halt(&s_entry_reader);
                vTaskDelay(pdMS_TO_TICKS(1500)); // Debounce
            }
        }

        // Poll Exit Reader
        if (rc522_is_card_present(&s_exit_reader)) {
            if (rc522_read_uid(&s_exit_reader, uid_buf, sizeof(uid_buf))) {
                ESP_LOGI(TAG, "Card detected on EXIT reader: UID %s", uid_buf);
                access_control_simulate_scan(uid_buf, false);
                rc522_halt(&s_exit_reader);
                vTaskDelay(pdMS_TO_TICKS(1500)); // Debounce
            }
        }

        vTaskDelay(pdMS_TO_TICKS(100)); // Poll every 100ms
    }
}

esp_err_t access_control_init(void) {
    s_persist_enabled = false;
    if (restore_users()) {
        ESP_LOGI(TAG, "Restored %u user(s) from NVS", (unsigned)s_user_count);
    } else {
        populate_initial_users();
        ESP_LOGI(TAG, "No saved users: seeded %u default user(s)", (unsigned)s_user_count);
    }
    s_persist_enabled = true;
    persist_users();

    gpio_config_t out_cfg = {
        .pin_bit_mask = (1ULL << PIN_ENTRY_DOOR_LED) | (1ULL << PIN_EXIT_DOOR_LED) | (1ULL << PIN_BUZZER),
        .mode = GPIO_MODE_OUTPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE
    };
    gpio_config(&out_cfg);
    gpio_set_level(PIN_ENTRY_DOOR_LED, 0);
    gpio_set_level(PIN_EXIT_DOOR_LED, 0);
    gpio_set_level(PIN_BUZZER, 0);

    /* The shared SPI3 bus is created once in app_main() before storage/SD and RFID init. */
    if (rc522_init(&s_entry_reader, SPI3_HOST, PIN_ENTRY_RFID_CS, PIN_ENTRY_RFID_RST) != ESP_OK) {
        ESP_LOGE(TAG, "Entry RFID reader init failed");
    }
    if (rc522_init(&s_exit_reader, SPI3_HOST, PIN_EXIT_RFID_CS, PIN_EXIT_RFID_RST) != ESP_OK) {
        ESP_LOGE(TAG, "Exit RFID reader init failed");
    }

    // Create the background RFID hardware polling task
    xTaskCreate(rfid_poll_task, "rfid_poll_task", 5120, NULL, 5, NULL);

    ESP_LOGI(TAG, "Access Control Subsystem initialized.");
    return ESP_OK;
}

esp_err_t access_control_add_user(const char *uid, const char *name, const char *role, bool authorized) {
    for (size_t i = 0; i < s_user_count; i++) {
        if (strcasecmp(s_user_db[i].uid, uid) == 0) {
            snprintf(s_user_db[i].name, sizeof(s_user_db[i].name), "%s", name);
            snprintf(s_user_db[i].role, sizeof(s_user_db[i].role), "%s", role);
            s_user_db[i].authorized = authorized;
            persist_users();
            return ESP_OK;
        }
    }
    if (s_user_count >= APP_MAX_USERS) return ESP_ERR_NO_MEM;

    snprintf(s_user_db[s_user_count].uid, sizeof(s_user_db[s_user_count].uid), "%s", uid);
    snprintf(s_user_db[s_user_count].name, sizeof(s_user_db[s_user_count].name), "%s", name);
    snprintf(s_user_db[s_user_count].role, sizeof(s_user_db[s_user_count].role), "%s", role);
    s_user_db[s_user_count].authorized = authorized;
    s_user_count++;
    persist_users();
    return ESP_OK;
}

esp_err_t access_control_remove_user(const char *uid) {
    for (size_t i = 0; i < s_user_count; i++) {
        if (strcasecmp(s_user_db[i].uid, uid) == 0) {
            for (size_t j = i; j < s_user_count - 1; j++) {
                s_user_db[j] = s_user_db[j + 1];
            }
            s_user_count--;
            memset(&s_user_db[s_user_count], 0, sizeof(s_user_db[s_user_count]));
            persist_users();
            return ESP_OK;
        }
    }
    return ESP_ERR_NOT_FOUND;
}

void access_control_list_users(void) {
    printf("\n=== AUTHORIZED USER REGISTRY ===\n");
    for (size_t i = 0; i < s_user_count; i++) {
        printf("[%d] UID: %-14s | Name: %-24s | Role: %-8s | Status: %s\n",
               (int)(i + 1), s_user_db[i].uid, s_user_db[i].name, s_user_db[i].role,
               s_user_db[i].authorized ? "ACTIVE" : "BLOCKED");
    }
    printf("================================\n\n");
}

void access_control_trigger_emergency_egress(void) {
    ESP_LOGW(TAG, "EMERGENCY EGRESS INITIATED: UNLOCKING DOORS AND EVACUATING.");
    gpio_set_level(PIN_ENTRY_DOOR_LED, 1);
    gpio_set_level(PIN_EXIT_DOOR_LED, 1);
    occupancy_clear_all();

    char time_str[16];
    schedule_get_clock_str(time_str, sizeof(time_str));

    system_event_t evt = {
        .event_type = EVT_EMERGENCY_RELEASE,
        .decision = DECISION_EMERGENCY_OVERRIDE,
        .room = ROOM_ENTRANCE,
    };
    snprintf(evt.time_str, sizeof(evt.time_str), "%s", time_str);
    snprintf(evt.uid, sizeof(evt.uid), "ALL");
    snprintf(evt.user_name, sizeof(evt.user_name), "EMERGENCY SYSTEM");
    snprintf(evt.details, sizeof(evt.details), "Manual building emergency egress triggered");

    communication_post_event(&evt);
    vTaskDelay(pdMS_TO_TICKS(4000));
    gpio_set_level(PIN_ENTRY_DOOR_LED, 0);
    gpio_set_level(PIN_EXIT_DOOR_LED, 0);
}