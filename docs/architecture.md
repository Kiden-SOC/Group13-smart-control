# Architecture

One ESP32, several FreeRTOS tasks, communicating through an event queue and protected shared state.

```text
 RC522 entry/exit --SPI3--> rfid_poll_task --> access_control --> occupancy (NVS)
 microSD  ---------SPI3---> storage_task  <-- g_event_queue <-- communication_post_event()
 PIR1/PIR2 --> room_monitor_task --> room_control (PWM lights, g_room_mutex)
 UART console --> cli_task --> every module
```

## Start-up order (`app_main.c`)

1. NVS init
2. `sync_manager_init` (mutexes + queue)
3. `communication_init`
4. **shared SPI3 bus** (SCK 18, MISO 19, MOSI 23, 4000-byte transfers)
5. `storage_init` (SD mount on the existing bus; RAM-log fallback)
6. `occupancy_init`, `schedule_init`, `access_control_init`: each restores saved data from NVS if present
7. `room_control_init` (LEDC PWM + monitor task), `serial_menu_init`

## Tasks

| Task | Prio | Stack | Job |
|---|---|---|---|
| `rfid_poll_task` | 5 | 5120 | poll both RC522 every 100 ms, run access decision, actuate door |
| `room_mon_task` | 5 | 4096 | every 100 ms read both PIRs and step each room's state machine |
| `storage_task` | 4 | 4096 | take events from the queue, RAM ring buffer, SD CSV |
| `cli_task` | 3 | 6144 | serial console |

## Two-step lighting (room_control.c)

```text
VACANT --1st motion--> DIM_PENDING --2nd motion <= 6 s--> BRIGHT --6 s no motion--> VACANT
                           |
                           +-- 6 s, no 2nd motion: FALSE TRIGGER --> VACANT (light OFF)
```

- A "motion" is a PIR **rising edge** or a simulated `motion <n>`. A PIR that simply stays high is one motion, not two.
- A 2nd motion that arrives after the window has expired is treated as a new 1st motion.
- Events: `PIR_MOTION` (1st and 2nd), `FALSE_TRIGGER`, `LIGHT_OFF`.

## Synchronisation

| Resource | Protected by |
|---|---|
| occupant list | `g_occupancy_mutex` |
| schedule, clock, admin flag | `g_schedule_mutex` |
| **both room state structs** | `g_room_mutex` (monitor task, console motion/status) |
| event queue | FreeRTOS queue; event IDs use an atomic counter |
| NVS | NVS API is thread-safe |

The room mutex is never held while posting an event or printing: state changes are decided under the lock, then events and console text are emitted after it is released.

## Persistence (NVS namespace `cocis`)

| Key | Content | Saved when |
|---|---|---|
| `usr_cnt`, `usr_list` | user table | add / remove user |
| `sched` | open/close hours | `setschedule` |
| `occ_cnt`, `occ_list` | building occupants | every entry, exit, emergency clear |

Not persisted: the simulated clock, admin login, the RAM log. The SD CSV keeps the audit history. `factoryreset` erases the namespace.

## Access rules

Authorized + (Staff role or inside schedule) + anti-passback (entry only if not inside, exit only if inside). Emergency egress opens both doors for 4 s and clears occupancy regardless of schedule.
