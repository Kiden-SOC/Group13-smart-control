# CoCIS SMART CONTROL AND MONITORING SYSTEM
**Department of Computer Science & Software Engineering**  
**Makerere University, Kampala, Uganda**  
*Academic Year: 2026/2027*

---

### 1. Introduction & Problem Statement
The College of Computing and Information Sciences (CoCIS) at Makerere University experiences high daily foot traffic across lecture halls, laboratory spaces, and administrative offices. Conventional manual management faces two operational issues:
1. Security vulnerabilities and unauthorized entry caused by unmonitored egress and lack of real-time building occupancy records.
2. Electrical energy wastage due to luminaire circuits remaining energized in unoccupied lecture halls.

This project delivers an automated, embedded monitoring and control solution based on the Espressif ESP32 microcontroller and the official ESP-IDF framework, orchestrating access control, automated lighting, event storage, and schedule management.

### 2. Architectural Analysis: Centralized vs. Distributed Controllers
While traditional commercial installations employ distributed terminals (separate microcontrollers per portal networked over CAN, RS-485, or ESP-NOW), this engineering implementation deliberately centralizes all three logical subsystems (Entrance, Lecture Room 1, Lecture Room 2) on **one physical ESP32 SoC**.

**Engineering Rationale:**
- **Deterministic Synchronization:** Eliminates network latency, packet loss, and split-brain states when evaluating building occupancy and anti-passback rules.
- **Resource Efficiency:** The dual-core Xtensa LX6 microcontroller running at 240 MHz possesses computational margin and DMA capabilities to execute multiple concurrent state machines.
- **Simulation Reliability:** Prevents the socket synchronization errors and multi-instance orchestration failures common to simulated multi-board architectures in educational simulators (such as Wokwi).

### 3. Hardware Interfacing & SPI Bus Sharing
Three high-speed SPI slave devices operate over a single VSPI bus (GPIO 18 SCK, GPIO 19 MISO, GPIO 23 MOSI):
- Entry RC522 RFID (Chip Select: GPIO 4)
- Exit RC522 RFID (Chip Select: GPIO 16)
- MicroSD Card Breakout (Chip Select: GPIO 13)

The bus is created once in `app_main()` before the SD card is mounted or the RC522 readers are attached. Bus arbitration is handled transparently by the ESP-IDF `spi_master` driver. Each transaction drives its respective $\overline{\text{CS}}$ pin active-low while maintaining all other chip-select lines high, avoiding bus contention.

### 4. Concurrency Architecture & FreeRTOS Synchronization
To maintain a non-blocking architecture:
1. **`room_mon_task` (Priority 5, 4096 B stack):** Cycles every 100 ms, detects PIR rising edges and steps each room's two-step lighting state machine (dim on 1st motion, bright on 2nd motion within 6 s, FALSE TRIGGER otherwise) using `esp_timer_get_time()`.
2. **`storage_task` (Priority 4, 4096 B stack):** Dequeues audit logs from `g_event_queue` and commits them to memory-buffered storage and the SD filesystem. File I/O delays do not stall access-control tasks.
3. **`cli_task` (Priority 3, 6144 B stack):** Processes operator commands over UART without stalling hardware control tasks.
4. **Shared Resource Protection:**
   - `g_occupancy_mutex`: Guarantees atomic check-and-set updates during occupant entry and exit.
   - `g_schedule_mutex`: Guards wall-clock calibration, schedule windows, and administrative authentication states.
   - `g_room_mutex`: Guards both rooms' state (phase, timers); events and console output are emitted after it is released.

### 5. Life Safety Considerations & Emergency Egress Analysis
The coursework specification instructs that egress be restricted outside scheduled access hours.  
**Critical Safety Distinction:** In real-world physical security engineering, trapping occupants inside a building is a violation of international building and life-safety codes (e.g., NFPA 101 Life Safety Code, BS EN 1125 / EN 179 for panic egress).  
While the firmware demonstrates coursework-compliant schedule-restricted egress, it incorporates a prioritized, unconstrained **Emergency Building Egress** override (`access_control_trigger_emergency_egress()`). When triggered, this override de-energizes fail-safe locks (energizing door output LEDs), purges occupancy counts, and logs an emergency evacuation event.

### 6. Simulation Realities & Hardware Fallbacks
1. **RFID Emulation:** The firmware drives real RC522 hardware over SPI. For testing flexibility in headless or command-line simulation contexts, it also provides an interactive CLI scanning utility (`tap in/out <UID>`).
2. **Storage Subsystem:** The system mounts MicroSD cards via SDSPI at `/sdcard`. If an unformatted or unattached SD card is detected during simulation, it falls back to a 128-record circular RAM buffer to keep audit operations functional without throwing kernel panics.
3. **Persistence:** user table, access schedule and occupancy are stored in NVS flash and restored at boot. The simulated clock is not persisted. In Wokwi, NVS survives a reset within a session but a full simulation restart re-flashes the firmware.
