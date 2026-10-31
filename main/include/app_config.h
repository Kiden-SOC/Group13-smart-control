#ifndef APP_CONFIG_H
#define APP_CONFIG_H

#include <stdint.h>
#include "driver/gpio.h"

#define PIN_SPI_SCK             GPIO_NUM_18
#define PIN_SPI_MISO            GPIO_NUM_19
#define PIN_SPI_MOSI            GPIO_NUM_23

#define PIN_ENTRY_RFID_CS       GPIO_NUM_4
#define PIN_ENTRY_RFID_RST      GPIO_NUM_22

#define PIN_EXIT_RFID_CS        GPIO_NUM_16
#define PIN_EXIT_RFID_RST       GPIO_NUM_21

#define PIN_SD_CS               GPIO_NUM_13

#define PIN_ENTRY_DOOR_LED      GPIO_NUM_25
#define PIN_EXIT_DOOR_LED       GPIO_NUM_14
#define PIN_BUZZER              GPIO_NUM_17

#define PIN_ROOM1_PIR           GPIO_NUM_26
#define PIN_ROOM1_LED           GPIO_NUM_32

#define PIN_ROOM2_PIR           GPIO_NUM_27
#define PIN_ROOM2_LED           GPIO_NUM_33

#define APP_DEFAULT_OPEN_HOUR   8
#define APP_DEFAULT_OPEN_MIN    0
#define APP_DEFAULT_CLOSE_HOUR  18
#define APP_DEFAULT_CLOSE_MIN   0

#define APP_ROOM_INACTIVITY_SEC 6    /* bright light -> off after this long with no motion   */
#define APP_ROOM_CONFIRM_SEC    6    /* window for the 2nd motion, counted from when the 1st
                                        motion ENDS (the PIR output drops low)               */

/* Two-step lighting: PWM duty on an 8-bit LEDC channel (0..255) */
#define APP_LIGHT_PWM_FREQ_HZ   5000
#define APP_LIGHT_DIM_DUTY      40
#define APP_LIGHT_BRIGHT_DUTY   255
#define APP_LIGHT_RAMP_STEP     26   /* duty change per 100 ms loop (~10 %): smooth fade     */

/* Shared SPI3 bus (RC522 x2 + microSD). The SD driver needs a large max transfer. */
#define APP_SPI_MAX_TRANSFER_SZ 4000
#define APP_DOOR_UNLOCK_MS      3000
#define APP_MAX_OCCUPANTS       64
#define APP_MAX_USERS           16
#define APP_EVENT_QUEUE_LEN     32
#define APP_LOG_BUFFER_SIZE     128
#define APP_ADMIN_DEFAULT_PIN   "cocis2026"

#endif // APP_CONFIG_H