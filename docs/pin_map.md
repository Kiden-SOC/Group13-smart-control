# GPIO Pin Map and Circuit Notes

Source of truth: `main/include/app_config.h` and `diagram.json`. One ESP32 DevKit C.

| Function | GPIO | Notes |
|---|---|---|
| SPI SCK (shared) | 18 | RC522 x2 + microSD |
| SPI MISO (shared) | 19 | |
| SPI MOSI (shared) | 23 | |
| Entry RC522 SDA/CS | 4 | |
| Entry RC522 RST | 22 | |
| Exit RC522 SDA/CS | 16 | |
| Exit RC522 RST | 21 | |
| microSD CS | 13 | SDSPI on SPI3 |
| Entry door LED (green) | 25 | via 220 ohm |
| Exit door LED (blue) | 14 | via 220 ohm |
| Buzzer | 17 | |
| Room 1 PIR | 26 | input, internal pull-down |
| Room 1 light LED | 32 | **LEDC PWM** channel 0 (dim / bright) |
| Room 2 PIR | 27 | input, internal pull-down |
| Room 2 light LED | 33 | **LEDC PWM** channel 1 (dim / bright) |

There is no emergency pushbutton and no red "denied" LED. A denied tap blinks the door LED and buzzer three times.

Room-light PWM: 5 kHz, 8-bit; dim duty 40/255, bright duty 255/255 (`APP_LIGHT_*` in `app_config.h`).

## SPI bus notes

- The shared SPI3 bus is created **once in `app_main()` before** `storage_init()` (SD mount) and `access_control_init()` (RC522). The SD driver and `rc522_init()` only attach devices to it.
- `max_transfer_sz` is 4000 bytes because the SD driver needs large transfers (the old value of 64 was enough only for the RC522).
- RC522 IRQ pins are unused; readers are polled every 100 ms.
- 3.3 V logic everywhere; each LED has its own 220 ohm resistor.
