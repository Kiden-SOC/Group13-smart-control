# Five-Member Contribution Matrix

Fill in each student's name and registration number after the group agrees on the allocation. The repository should remain one integrated project with one authoritative copy of every source file.

| Member | Responsibility | Main files | Required evidence / tests |
|---|---|---|---|
| 1 | RFID access control | `main/modules/rc522.c`, `main/include/rc522.h`, `main/modules/access_control.c`, `main/include/access_control.h` | Show entry and exit readers on the shared SPI bus; demonstrate authorized and unauthorized cards, schedule denial, duplicate entry and emergency release; explain UID reading and decision flow |
| 2 | Lecture Room 1 automation | Room 1 portion of `main/modules/room_control.c`, `main/include/room_control.h`, Room 1 pins in `app_config.h` and `diagram.json` | Demonstrate Room 1 two-step lighting (1st motion dim, 2nd motion within 6 s bright, otherwise FALSE TRIGGER); inactivity timeout; show Room 2 remains independent |
| 3 | Lecture Room 2 automation | Room 2 portion of `main/modules/room_control.c`, `main/include/room_control.h`, Room 2 pins in `app_config.h` and `diagram.json` | Demonstrate Room 2 two-step lighting independently; explain the non-blocking timer and the `g_room_mutex` protecting room state |
| 4 | Storage and occupancy | `main/modules/storage.c`, `main/include/storage.h`, `main/modules/occupancy.c`, `main/include/occupancy.h` | Demonstrate occupant list/count, duplicate-event protection, NVS restore of users/schedule/occupancy after a reset, SD mount (shared SPI bus created first) and RAM fallback, event-log retrieval |
| 5 | Schedule, menu and integration | `main/modules/schedule.c`, `main/include/schedule.h`, `main/modules/serial_menu.c`, `main/include/serial_menu.h`, `main/app_main.c`, `main/modules/sync_manager.c`, `main/modules/communication.c`, build/config and docs | Demonstrate administrator login, clock setup, schedule changes, serial commands, overall integration and full one-board Wokwi project |

## Shared responsibilities

All five members should review the shared interfaces in `main/include/` before editing module code. Every member should run their test scenarios after integration and contribute at least one tested change to the common repository. Member 5 coordinates integration but does not replace the work or explanation of the other four members.

## Student details

| Member | Full name | Registration number | Signature / contribution confirmation |
|---|---|---|---|
| 1 | __________________ | __________________ | __________________ |
| 2 | __________________ | __________________ | __________________ |
| 3 | __________________ | __________________ | __________________ |
| 4 | __________________ | __________________ | __________________ |
| 5 | __________________ | __________________ | __________________ |
