# Likely Lecturer Questions and Honest Answers

**Why one ESP32 instead of several?**
It removes network latency and split-brain problems when deciding occupancy and anti-passback, and avoids multi-board Wokwi instability. The three areas (entrance, Room 1, Room 2) are separate modules and tasks, so they could be split onto separate boards later.

**How do the two RFID readers and the SD card share one SPI bus?**
Same SCK/MISO/MOSI (18/19/23); each device has its own chip select (4, 16, 13). The bus is created once in `app_main()` before any device is attached, then `spi_master` / SDSPI arbitrate transactions.

**How does a card become an access decision?**
The RC522 returns a 4-byte UID; `access_control_evaluate()` looks it up in the user table, checks authorization, schedule (Staff are exempt) and anti-passback, then logs the event and actuates the door LED/buzzer. UID-only access is not secure (cards can be cloned).

**How does the lighting work?**
Two-step confirmation. The first motion dims the light and prints a prompt; a second motion within 6 s makes it bright; if none arrives the terminal prints FALSE TRIGGER and the light goes off. A bright room turns off after 6 s without motion. This filters single stray PIR pulses and saves power. The light is PWM (ESP32 LEDC), not on/off.

**What survives a reboot?**
Users, access schedule and building occupancy are saved in NVS flash. The SD CSV keeps the audit log. The simulated clock and admin login do not survive. In Wokwi, NVS persists across a reset within a session; a full stop/start re-flashes the firmware.

**What if the SD card is missing?**
Mounting fails, the system logs the reason (`storage` command shows it) and keeps logging to a 128-slot RAM ring buffer. The SPI bus is created before the SD mount, which was a bug in an earlier version.

**Why did a second card tap sometimes not read?**
An earlier `rc522_halt()` switched the RFID antenna off and nothing switched it back on. Halt now keeps the antenna on, and the poll functions re-enable it if it is found off.

**Race conditions: what is protected and what was not?**
Occupancy and schedule always had mutexes. The two room structures were originally shared between the room monitor task and the console (`motion`, `rooms`) with no lock, so a read could see half-updated state, or the monitor and console could both treat the same motion as "first". This is fixed with `g_room_mutex` (created in `sync_manager.c`, used in `room_control.c`). Events and printing happen after the lock is released so the lock is held only for short state updates.

**What are the remaining limitations?**
Occupancy and event log are separate writes (not one transaction); the clock is simulated; the PIN is hard-coded; a PIR detects motion, not presence; door actuation blocks its task for 3 s.

**Emergency egress / life safety?**
The coursework restricts egress by schedule, but real buildings must never trap people. `emergency` unlocks both doors for 4 s and clears occupancy regardless of schedule. It is a simulation, not a certified fail-safe lock.
