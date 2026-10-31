# CoCIS Smart Control - Test Matrix

Status key: PENDING = run it after rebuilding; fill in PASS/FAIL and the date. Cards: Dean `01:02:03:04`, Student `11:22:33:44`, unknown `AA:BB:CC:DD`.

| ID | Category | Setup | Stimulus | Expected | Status |
|---|---|---|---|---|---|
| TC-01 | RFID entry | clock 09:00, building empty | `tap in 01:02:03:04` | GRANTED, entry LED 3 s, occupancy 1 | PENDING |
| TC-02 | Anti-passback | user inside | `tap in 01:02:03:04` | DENIED_IN_BUILDING, occupancy unchanged | PENDING |
| TC-03 | RFID exit | user inside | `tap out 01:02:03:04` | GRANTED, occupancy 0 | PENDING |
| TC-04 | Invalid exit | user not inside | `tap out 11:22:33:44` | DENIED_NOT_INSIDE | PENDING |
| TC-05 | Unauthorized | - | `tap in AA:BB:CC:DD` | DENIED_UNAUTH | PENDING |
| TC-06 | Schedule | `settime 20 0 0` | `tap in 11:22:33:44` | DENIED_HOURS | PENDING |
| TC-07 | Lighting step 1 | Room 1 vacant | `motion 1` | light DIM; terminal prompts for a 2nd motion within 6 s | PENDING |
| TC-08 | Lighting step 2 | after TC-07, < 6 s | `motion 1` again | light BRIGHT; `rooms` shows OCCUPIED | PENDING |
| TC-09 | False trigger | Room 1 vacant | `motion 1`, wait 6 s | terminal prints FALSE TRIGGER; light OFF; FALSE_TRIGGER event logged | PENDING |
| TC-10 | Inactivity | Room 1 BRIGHT | wait 6 s | light OFF; LIGHT_OFF event | PENDING |
| TC-11 | Room independence | Room 1 BRIGHT | `rooms` | Room 2 unaffected (OFF) | PENDING |
| TC-12 | Wokwi PIR | Room 2 vacant | click PIR2 "Simulate motion" twice (after the first pulse ends) | DIM then BRIGHT | PENDING |
| TC-13 | RFID re-tap | entry reader | tap card, wait 2 s, tap again | second tap is read too (antenna stays on) | PENDING |
| TC-14 | SD mount | boot log | read boot log, `storage` | "MicroSD mounted" or a clear error code; header written once | PENDING |
| TC-15 | Persistence | users added, schedule changed, someone inside | reset the ESP32 (not a full Wokwi restart) | users, schedule, occupants restored | PENDING |
| TC-16 | Factory reset | admin | `factoryreset` | data erased, device restarts with defaults | PENDING |
| TC-17 | Emergency | people inside | `emergency` | doors unlock, occupancy 0, event logged | PENDING |
| TC-18 | Admin guard | not logged in | `setschedule 7 0 20 0` | rejected | PENDING |
| TC-19 | Room race | `motion 2` spammed from console while PIR2 also fires | no crash, no lock-up; each state change logged once | PENDING |
