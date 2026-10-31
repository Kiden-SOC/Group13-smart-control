# CoCIS Smart Control and Monitoring System

**Course:** Embedded Systems / BSE III and CS II, 2026/2027
**Platform:** one ESP32 DevKit C, C, ESP-IDF 6.x, FreeRTOS
**Simulation target:** one Wokwi project, one ESP32

The building entrance, Lecture Room 1 and Lecture Room 2 are logical subsystems on the **same ESP32**. Two RC522 RFID readers and a microSD card share one SPI bus; each room has its own PIR sensor and PWM-dimmable light; a serial console is used for management and demonstration.

> **Verification status:** the code compiles in the group's ESP-IDF 6.1 environment (earlier build). The latest changes (two-step lighting, NVS persistence, SD init order, RFID antenna, room mutex) were syntax-checked and the lighting state machine was unit-tested on a host PC, but they still need a rebuild and a run in Wokwi. See `tests/test_scenarios.md`.

## 1. Features

- Entry and exit MFRC522 readers (shared SPI: SCK 18, MISO 19, MOSI 23). Authorized-user table with anti-passback (no double entry, no exit without entry).
- **Two-step room lighting** (per room):
  1. 1st motion -> light **DIM** (PWM) and the terminal prints a prompt to simulate a 2nd motion within 6 s.
  2. 2nd motion within 6 s -> light **BRIGHT**, room OCCUPIED.
  3. No 2nd motion within 6 s -> terminal prints **FALSE TRIGGER**, light OFF.
  4. Bright room with no motion for 6 s -> light OFF.
- Configurable access schedule (default 08:00-18:00, overnight windows supported). Staff role is exempt from the schedule.
- Simulated wall clock (starts 09:00:00, set with `settime`). It is not a real RTC.
- **Persistence (NVS flash):** user table, access schedule and building occupancy survive a reboot.
- **Audit log:** microSD CSV (`/sdcard/cocis_audit.csv`) when the card mounts, plus a 128-slot RAM ring buffer shown by `logs`.
- FreeRTOS tasks, an event queue and mutexes (occupancy, schedule, **room state**).
- Serial management console with admin login. Default admin PIN: **`cocis2026`**.

There is **no physical emergency button** and no flash event history; emergency egress is the serial `emergency` command.

## 2. Project structure

```text
Group13/
  CMakeLists.txt  sdkconfig.defaults  diagram.json  wokwi.toml
  main/
    app_main.c                (creates the shared SPI bus, then starts every module)
    include/  *.h
    modules/
      sync_manager.c          mutexes + event queue
      rc522.c                 MFRC522 SPI driver
      access_control.c        user table, decisions, RFID polling, door/buzzer
      occupancy.c             who is inside (persisted)
      room_control.c          two-step PIR lighting (PWM), room mutex
      schedule.c              access hours, simulated clock, admin PIN (schedule persisted)
      storage.c               SD CSV log, RAM ring buffer, NVS persistence helpers
      communication.c         numbers events and queues them for storage
      serial_menu.c           console
  docs/  tests/
```

## 3. Build and run

```bash
idf.py set-target esp32
idf.py fullclean        # recommended once: sdkconfig and the build cache changed
idf.py build
```

Then start the Wokwi simulation from VS Code (the Wokwi extension reads `wokwi.toml` and `diagram.json`). Serial monitor: **115200 baud**.

Wokwi note: the simulator re-flashes the firmware at every start, so NVS data survives a *reset inside one simulation session* (for example the `factoryreset` command, or the ESP32 reset), but not necessarily a full stop/start of the simulation. On a real board it survives power cycles.

## 4. Console commands

| Command | Notes |
|---|---|
| `help` | show the menu |
| `occupancy` / `logs` / `rooms` / `schedule` / `users` / `storage` | read-only views |
| `tap <in\|out> <UID>` | simulated card tap (same decision path as the RC522 readers) |
| `motion <1\|2>` | simulated PIR motion for that room (call twice for dim -> bright) |
| `settime <HH> <MM> <SS>` | set the simulated clock |
| `login cocis2026` / `logout` | admin session |
| `setschedule <OH> <OM> <CH> <CM>` | admin only |
| `useradd <UID> <Name> <Role>` / `userdel <UID>` | admin only (saved to flash) |
| `factoryreset` | admin only: erase saved users/schedule/occupancy and restart |
| `emergency` | unlock both doors for 4 s and clear occupancy |

Sample cards (also the Wokwi RC522 default `01:02:03:04`): see `users` or `populate_initial_users()` in `access_control.c`.

## 5. Quick demo

```text
login cocis2026
tap in 01:02:03:04        -> GRANTED
tap in 01:02:03:04        -> DENIED_IN_BUILDING
occupancy
motion 2                  -> Room 2 dim + "simulate a 2nd motion" prompt
motion 2                  -> Room 2 bright
(wait 6 s)               -> light OFF
motion 1  (wait 6 s)     -> FALSE TRIGGER, light OFF
tap out 01:02:03:04
```

## 6. Known limitations

- The simulated clock restarts at 09:00:00 after every reboot (it is not persisted).
- Occupancy and the event log are written separately; a crash between them can leave one without the other.
- UID-only RFID is not secure authentication. The admin PIN is hard-coded.
- A PIR detects motion, not presence; a still person is treated as "no motion" after the timeout.
- `access_control_actuate_door()` blocks its calling task for the 3 s unlock, so a second tap during that time waits.
- The SD mount uses `format_if_mount_failed = true` so a blank simulator card works. Do not point it at a real card with data you want to keep.

## 7. Documentation

`docs/architecture.md`, `docs/pin_map.md`, `docs/system_operation_report.md`, `docs/contribution_matrix.md`, `docs/video_demo_guide.md`, `docs/lecturer_questions.md`, `tests/test_scenarios.md`.
