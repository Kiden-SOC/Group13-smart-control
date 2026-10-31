#include "serial_menu.h"
#include "occupancy.h"
#include "access_control.h"
#include "room_control.h"
#include "schedule.h"
#include "storage.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

static void print_banner(void) {
    printf("\n********************************************************\n");
    printf("*   CoCIS SMART CONTROL & MONITORING SYSTEM (ESP32)   *\n");
    printf("*   Makerere University - BSE III / CS II 2026/2027   *\n");
    printf("********************************************************\n");
}

static void print_menu(void) {
    char clock_str[16];
    schedule_get_clock_str(clock_str, sizeof(clock_str));
    uint8_t oh, om, ch, cm;
    schedule_get(&oh, &om, &ch, &cm);

    printf("\n--- MANAGEMENT CONSOLE [%s | Sched: %02d:%02d-%02d:%02d | %s] ---\n",
           clock_str, oh, om, ch, cm, schedule_is_admin() ? "ADMIN AUTHENTICATED" : "READ ONLY");
    printf("1. View Current Building Occupants\n");
    printf("2. View Recent System Audit Logs\n");
    printf("3. View Lecture Rooms Status (PIR & Lighting)\n");
    printf("4. View Access Schedule & Time\n");
    printf("5. Update Access Schedule [Admin: setschedule <OH> <OM> <CH> <CM>]\n");
    printf("6. Set Simulated System Clock [settime <HH> <MM> <SS>]\n");
    printf("7. Simulate RFID Card Tap [tap <in|out> <UID>]\n");
    printf("8. Simulate Room Motion [motion <1|2>] (1st = DIM, 2nd within 6s = BRIGHT)\n");
    printf("9. User Management [Admin: users, useradd <UID> <Name> <Role>, userdel <UID>]\n");
    printf("10. Trigger Emergency Building Evacuation [emergency]\n");
    printf("11. Storage Engine Diagnostics [storage]\n");
    printf("12. Admin Authentication [login <PIN> | logout]\n");
    printf("    Admin: factoryreset  (erase saved users/schedule/occupancy, then restart)\n");
    printf("Enter command number or keyword:\n> ");
    fflush(stdout);
}

static void handle_command(char *line) {
    char cmd[32] = {0};
    char arg1[32] = {0};
    char arg2[32] = {0};
    char arg3[32] = {0};
    char arg4[32] = {0};

    int tokens = sscanf(line, "%31s %31s %31s %31s %31s", cmd, arg1, arg2, arg3, arg4);
    if (tokens <= 0) return;

    if (strcmp(cmd, "1") == 0 || strcasecmp(cmd, "occupancy") == 0) {
        occupancy_print_list();
    } else if (strcmp(cmd, "2") == 0 || strcasecmp(cmd, "logs") == 0) {
        storage_print_recent(20);
    } else if (strcmp(cmd, "3") == 0 || strcasecmp(cmd, "rooms") == 0) {
        room_control_print_status();
    } else if (strcmp(cmd, "4") == 0 || strcasecmp(cmd, "schedule") == 0) {
        uint8_t oh, om, ch, cm;
        schedule_get(&oh, &om, &ch, &cm);
        char clock_str[16];
        schedule_get_clock_str(clock_str, sizeof(clock_str));
        printf("\nConfigured Access Hours : %02d:%02d to %02d:%02d\n", oh, om, ch, cm);
        printf("Current System Time     : %s\n", clock_str);
        printf("Access Currently Status : %s\n\n", schedule_is_allowed_now() ? "AUTHORIZED" : "CLOSED");
    } else if (strcmp(cmd, "5") == 0 || strcasecmp(cmd, "setschedule") == 0) {
        if (!schedule_is_admin()) {
            printf("\n[ERROR] Admin login required to modify schedule. Type: login <PIN>\n");
            return;
        }
        if (tokens < 5) {
            printf("\nUsage: setschedule <OpenH> <OpenM> <CloseH> <CloseM>\nExample: setschedule 8 0 17 30\n");
            return;
        }
        uint8_t oh = atoi(arg1), om = atoi(arg2), ch = atoi(arg3), cm = atoi(arg4);
        if (schedule_update(oh, om, ch, cm) == ESP_OK) {
            printf("\n[SUCCESS] Access schedule updated to %02d:%02d - %02d:%02d\n", oh, om, ch, cm);
        } else {
            printf("\n[ERROR] Invalid schedule parameters provided.\n");
        }
    } else if (strcmp(cmd, "6") == 0 || strcasecmp(cmd, "settime") == 0) {
        if (tokens < 4) {
            printf("\nUsage: settime <HH> <MM> <SS>\nExample: settime 14 30 00\n");
            return;
        }
        schedule_set_clock(atoi(arg1), atoi(arg2), atoi(arg3));
        printf("\n[SUCCESS] System clock set successfully.\n");
    } else if (strcmp(cmd, "7") == 0 || strcasecmp(cmd, "tap") == 0) {
        if (tokens < 3) {
            printf("\nUsage: tap <in|out> <UID>\nExamples:\n  tap in 01:02:03:04\n  tap out 01:02:03:04\n  tap in AA:BB:CC:DD\n");
            return;
        }
        bool is_entry = (strcasecmp(arg1, "in") == 0);
        access_control_simulate_scan(arg2, is_entry);
    } else if (strcmp(cmd, "8") == 0 || strcasecmp(cmd, "motion") == 0) {
        if (tokens < 2) {
            printf("\nUsage: motion <1|2>\n");
            return;
        }
        int rm = atoi(arg1);
        if (rm == 1) room_control_simulate_motion(ROOM_LECTURE_1);
        else if (rm == 2) room_control_simulate_motion(ROOM_LECTURE_2);
        else printf("\n[ERROR] Invalid room number. Choose 1 or 2.\n");
    } else if (strcmp(cmd, "9") == 0 || strcasecmp(cmd, "users") == 0) {
        access_control_list_users();
    } else if (strcasecmp(cmd, "useradd") == 0) {
        if (!schedule_is_admin()) {
            printf("\n[ERROR] Admin login required. Type: login <PIN>\n");
            return;
        }
        if (tokens < 4) {
            printf("\nUsage: useradd <UID> <Name> <Role>\n");
            return;
        }
        access_control_add_user(arg1, arg2, arg3, true);
        printf("\n[SUCCESS] User registered.\n");
    } else if (strcasecmp(cmd, "userdel") == 0) {
        if (!schedule_is_admin()) {
            printf("\n[ERROR] Admin login required.\n");
            return;
        }
        if (tokens < 2) {
            printf("\nUsage: userdel <UID>\n");
            return;
        }
        access_control_remove_user(arg1);
        printf("\n[SUCCESS] User removed.\n");
    } else if (strcmp(cmd, "10") == 0 || strcasecmp(cmd, "emergency") == 0) {
        access_control_trigger_emergency_egress();
    } else if (strcmp(cmd, "11") == 0 || strcasecmp(cmd, "storage") == 0) {
        storage_print_diagnostics();
    } else if (strcmp(cmd, "12") == 0 || strcasecmp(cmd, "login") == 0) {
        if (tokens < 2) {
            printf("\nUsage: login <PIN>\nDefault PIN is: cocis2026\n");
            return;
        }
        if (schedule_admin_login(arg1)) {
            printf("\n[AUTHENTICATED] Admin mode granted.\n");
        } else {
            printf("\n[DENIED] Incorrect administrator PIN.\n");
        }
    } else if (strcasecmp(cmd, "factoryreset") == 0) {
        if (!schedule_is_admin()) {
            printf("\n[ERROR] Admin login required. Type: login <PIN>\n");
            return;
        }
        if (storage_persist_erase_all() == ESP_OK) {
            printf("\n[SUCCESS] Saved users, schedule and occupancy erased. Restarting...\n");
            fflush(stdout);
            vTaskDelay(pdMS_TO_TICKS(300));
            esp_restart();
        } else {
            printf("\n[ERROR] Could not erase saved data.\n");
        }
    } else if (strcasecmp(cmd, "logout") == 0) {
        schedule_admin_logout();
        printf("\n[LOGOUT] Administrator session ended.\n");
    } else if (strcasecmp(cmd, "help") == 0 || strcasecmp(cmd, "menu") == 0) {
        print_menu();
    } else {
        printf("\nCommand not recognized. Type 'help' to review the management menu.\n");
    }
}

static void serial_menu_task(void *pvParameters) {
    char rx_buffer[128];
    print_banner();
    print_menu();

    while (1) {
        if (fgets(rx_buffer, sizeof(rx_buffer), stdin) != NULL) {
            size_t len = strlen(rx_buffer);
            while (len > 0 && (rx_buffer[len - 1] == '\r' || rx_buffer[len - 1] == '\n')) {
                rx_buffer[len - 1] = '\0';
                len--;
            }
            if (len > 0) {
                handle_command(rx_buffer);
                printf("\n> ");
                fflush(stdout);
            }
        }
        vTaskDelay(pdMS_TO_TICKS(50));
    }
}

esp_err_t serial_menu_init(void) {
    xTaskCreate(serial_menu_task, "cli_task", 6144, NULL, 3, NULL);
    return ESP_OK;
}