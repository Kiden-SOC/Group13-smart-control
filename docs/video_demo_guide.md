# Individual Video Demonstration Guide

Record separate videos or clearly separated segments for the five members. Each student should show the relevant live component, the code they own, the output/result and a brief explanation in their own words. Do not record a demonstration as passed unless the feature has actually been tested.

## Member 1 — RFID access control

**Files:** `rc522.c`, `rc522.h`, `access_control.c`, `access_control.h`, `app_config.h`.

Suggested sequence:

1. Introduce the two RFID readers and point out that they share SPI CLK, MISO and MOSI on the one ESP32.
2. Show the separate chip-select and reset connections for entry and exit.
3. Explain how the reader obtains a four-byte UID and how the UID is looked up in the authorized-user table.
4. Set the demonstration clock with `settime`.
5. Present one authorized card at entry and show the entry indicator/occupancy update.
6. Present an unknown card, then repeat an entry for a user already inside.
7. Show a valid exit and an invalid exit.
8. Tap twice in a row on the same reader (with a gap) to show the antenna stays on, then run the `emergency` command to show it bypasses the schedule.

If the RFID simulator cannot be verified, show the serial `tap in|out <UID>` command as a **mock input** and explicitly say that it tests the access decision path, not the physical RFID reader.

## Member 2 — Lecture Room 1 automation

**Files:** Room 1 portion of `room_control.c`, `room_control.h`, `app_config.h`, `diagram.json`.

Suggested sequence:

1. Identify the Room 1 PIR sensor connected to GPIO26 and light indicator connected to GPIO32.
2. Explain the room state variables: occupied, light state and last-motion tick.
3. Trigger Room 1 motion once: light goes DIM and the terminal asks for a 2nd motion.
4. Trigger a 2nd motion within 6 s: light goes BRIGHT. Then trigger motion once and wait: FALSE TRIGGER and light OFF. Show the bright light turning OFF after 6 s of no motion.
5. Trigger Room 2 separately and show that Room 1 is not accidentally controlled by Room 2 activity.
6. Explain that PIR detects motion rather than guaranteed continuous human presence.

## Member 3 — Lecture Room 2 automation

**Files:** Room 2 portion of `room_control.c`, `room_control.h`, `app_config.h`, `diagram.json`.

Suggested sequence:

1. Identify the Room 2 PIR on GPIO27 and light on GPIO33.
2. Trigger Room 2 motion twice within 6 s (dim then bright).
3. Explain the non-blocking timing and why room state is protected by `g_room_mutex`.
4. Show Room 1 and Room 2 operating independently.
5. Use the `rooms` serial command to report each room's state.

## Member 4 — Storage and occupancy management

**Files:** `storage.c`, `storage.h`, `occupancy.c`, `occupancy.h`.

Suggested sequence:

1. Explain the event record fields, the SD CSV log, the RAM ring buffer and the NVS-persisted data.
2. Show one successful entry and the corresponding occupant list.
3. Attempt duplicate entry and invalid exit; show the count does not become inconsistent.
4. Show a valid exit and updated list.
5. Use `logs` and `storage` to display stored records, SD status and the mount error if any. Check the boot log for the SD mount line.
6. Reset the ESP32 and show users, schedule and occupancy are restored.
7. Explain the limitation that occupancy persistence and event logging are separate commits.

## Member 5 — Schedule, serial menu and integration

**Files:** `schedule.c`, `schedule.h`, `serial_menu.c`, `serial_menu.h`, `sync_manager.c`, `communication.c`, `app_main.c`, CMake and documentation files.

Suggested sequence:

1. Show the single ESP32 Wokwi diagram and explain the lecturer's one-board requirement.
2. Explain the four FreeRTOS tasks and the queues/mutexes.
3. Show `help`, `schedule`, `rooms`, `occupancy`, `logs` and `storage` commands.
4. Show that an unauthenticated user cannot change the schedule.
5. `login cocis2026` and change the schedule with `setschedule`.
6. Set the temporary clock with `settime` and demonstrate access permitted/denied at appropriate times.
7. Explain that the temporary clock is not a real RTC or network clock.
8. Show the successful build log and genuine Wokwi share URL after the group has generated them.

## Suggested group integration video

At the end, all five members can appear briefly and explain how the modules work together on a single ESP32. Include the live circuit, the serial console, two independent two-step room lights, one entry/exit sequence, an event-log view and the emergency release. Add only results that were actually observed.
