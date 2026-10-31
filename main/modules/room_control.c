#include "room_control.h"
#include "app_config.h"
#include "communication.h"
#include "schedule.h"
#include "sync_manager.h"
#include "driver/gpio.h"
#include "driver/ledc.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <stdio.h>
#include <string.h>

static const char *TAG = "ROOM_CTRL";

/*
 * Two-step lighting (per room):
 *
 *   VACANT ──1st motion──> DIM_PENDING ──2nd motion within CONFIRM window──> BRIGHT
 *                              │                                               │
 *                              └─ no 2nd motion in window: "FALSE TRIGGER"     └─ no motion for
 *                                 light OFF, back to VACANT                       INACTIVITY s: OFF
 *
 * The CONFIRM window only starts counting once the 1st motion has ENDED (PIR output goes
 * low), so the operator always gets the full window to trigger the 2nd motion. Light
 * changes are not instant: set_light_locked() only sets a TARGET duty and room_ramp()
 * fades the real PWM duty towards it a little every 100 ms (smooth dim / brighten / off).
 *
 * All fields of room_state_t are protected by g_room_mutex. The monitor task, the
 * serial console (simulate / status) and any future caller can therefore touch a
 * room at the same time without a data race. Events and console output are emitted
 * AFTER the mutex is released so the lock is never held across a queue send or printf.
 */

typedef enum {
    PHASE_VACANT = 0,
    PHASE_DIM_PENDING,
    PHASE_BRIGHT
} room_phase_t;

typedef enum {
    NOTE_FIRST_MOTION,
    NOTE_WINDOW_OPEN,
    NOTE_CONFIRMED,
    NOTE_FALSE_TRIGGER,
    NOTE_LIGHT_OFF
} note_kind_t;

typedef struct {
    gpio_num_t pir_pin;
    gpio_num_t led_pin;
    ledc_channel_t pwm_channel;
    room_id_t room_id;
    room_phase_t phase;
    uint32_t inactivity_timeout_sec;
    int64_t first_motion_us;     /* when the 1st motion pulse ended = start of confirm window */
    bool    first_pulse_active;  /* 1st motion pulse still in progress (PIR output still high) */
    int64_t last_motion_us;
    uint32_t cur_duty;           /* PWM duty on the pin right now (0..255)            */
    uint32_t target_duty;        /* PWM duty we are fading towards                    */
    int last_pir_level;          /* for rising-edge detection (monitor task only) */
} room_state_t;

static room_state_t s_room1;
static room_state_t s_room2;

static room_state_t *room_from_id(room_id_t room) {
    if (room == ROOM_LECTURE_1) return &s_room1;
    if (room == ROOM_LECTURE_2) return &s_room2;
    return NULL;
}

static int room_number(const room_state_t *r) {
    return (r->room_id == ROOM_LECTURE_1) ? 1 : 2;
}

/* Caller holds g_room_mutex. Sets only the TARGET; room_ramp() does the actual fade. */
static void set_light_locked(room_state_t *r, room_light_t level) {
    r->target_duty = (level == ROOM_LIGHT_BRIGHT) ? APP_LIGHT_BRIGHT_DUTY
                   : (level == ROOM_LIGHT_DIM)    ? APP_LIGHT_DIM_DUTY : 0;
}

/* Called once per 100 ms loop: move the real PWM duty one small step towards the target. */
static void room_ramp(room_state_t *r) {
    xSemaphoreTake(g_room_mutex, portMAX_DELAY);
    uint32_t cur = r->cur_duty, tgt = r->target_duty;
    if (cur < tgt) {
        cur += APP_LIGHT_RAMP_STEP;
        if (cur > tgt) cur = tgt;
    } else if (cur > tgt) {
        cur = (cur > APP_LIGHT_RAMP_STEP + tgt) ? cur - APP_LIGHT_RAMP_STEP : tgt;
    }
    bool changed = (cur != r->cur_duty);
    r->cur_duty = cur;
    if (changed) {
        ledc_set_duty(LEDC_LOW_SPEED_MODE, r->pwm_channel, cur);
        ledc_update_duty(LEDC_LOW_SPEED_MODE, r->pwm_channel);
    }
    xSemaphoreGive(g_room_mutex);
}

static room_light_t light_of_phase(room_phase_t p) {
    return (p == PHASE_BRIGHT) ? ROOM_LIGHT_BRIGHT : (p == PHASE_DIM_PENDING) ? ROOM_LIGHT_DIM : ROOM_LIGHT_OFF;
}

/* Log the event + print the console line. Never called with g_room_mutex held. */
static void emit_note(const room_state_t *r, note_kind_t kind, const char *source) {
    char time_str[16];
    char details[64];
    char console[160];
    event_type_t type = EVT_PIR_MOTION;
    int n = room_number(r);

    switch (kind) {
    case NOTE_FIRST_MOTION:
        type = EVT_PIR_MOTION;
        snprintf(details, sizeof(details), "1st motion; light DIM, awaiting 2nd motion");
        snprintf(console, sizeof(console),
                 "[ROOM %d] 1st motion detected -> light fading to DIM. Waiting for the motion to end...", n);
        break;
    case NOTE_WINDOW_OPEN:
        type = EVT_PIR_MOTION;
        snprintf(details, sizeof(details), "1st motion ended; %ds window open", APP_ROOM_CONFIRM_SEC);
        snprintf(console, sizeof(console),
                 "[ROOM %d] Motion ended -> trigger a 2nd motion within %d s (PIR click or 'motion %d').",
                 n, APP_ROOM_CONFIRM_SEC, n);
        break;
    case NOTE_CONFIRMED:
        type = EVT_PIR_MOTION;
        snprintf(details, sizeof(details), "2nd motion confirmed; light BRIGHT");
        snprintf(console, sizeof(console), "[ROOM %d] 2nd motion confirmed -> light BRIGHT (room occupied).", n);
        break;
    case NOTE_FALSE_TRIGGER:
        type = EVT_FALSE_TRIGGER;
        snprintf(details, sizeof(details), "False trigger: no 2nd motion in %ds; light OFF", APP_ROOM_CONFIRM_SEC);
        snprintf(console, sizeof(console), "[ROOM %d] FALSE TRIGGER - no 2nd motion within %d s -> light OFF.",
                 n, APP_ROOM_CONFIRM_SEC);
        break;
    case NOTE_LIGHT_OFF:
    default:
        type = EVT_LIGHT_OFF;
        snprintf(details, sizeof(details), "Inactivity timeout; light OFF");
        snprintf(console, sizeof(console), "[ROOM %d] No motion for the inactivity timeout -> light OFF (vacant).", n);
        break;
    }

    schedule_get_clock_str(time_str, sizeof(time_str));

    system_event_t evt = {
        .event_type = type,
        .decision = DECISION_NONE,
        .room = r->room_id,
    };
    snprintf(evt.time_str, sizeof(evt.time_str), "%s", time_str);
    bool from_timer = (kind == NOTE_FALSE_TRIGGER || kind == NOTE_LIGHT_OFF);
    snprintf(evt.uid, sizeof(evt.uid), "%s", from_timer ? "TIMER_EXPIRED" : source);
    snprintf(evt.user_name, sizeof(evt.user_name), "%s", room_id_to_str(r->room_id));
    snprintf(evt.details, sizeof(evt.details), "%s", details);
    communication_post_event(&evt);

    printf("\n%s\n", console);
    fflush(stdout);
}

/*
 * One state-machine step for one room.
 *   new_motion : a fresh motion pulse (PIR rising edge or simulated motion)
 *   pir_high   : PIR output currently high (keeps a bright room alive)
 */
static void room_step(room_state_t *r, bool new_motion, bool pir_high, const char *source) {
    note_kind_t notes[3];
    int n_notes = 0;
    int64_t now_us = esp_timer_get_time();

    xSemaphoreTake(g_room_mutex, portMAX_DELAY);

    /* 1) 1st motion pulse just ended: the confirm window starts counting NOW. */
    if (r->phase == PHASE_DIM_PENDING && r->first_pulse_active && !pir_high && !new_motion) {
        r->first_pulse_active = false;
        r->first_motion_us = now_us;
        notes[n_notes++] = NOTE_WINDOW_OPEN;
    }

    /* 2) Timeouts next, so a late 2nd motion is treated as a brand-new 1st motion. */
    if (r->phase == PHASE_DIM_PENDING) {
        if (!r->first_pulse_active &&
            (now_us - r->first_motion_us) / 1000000LL >= APP_ROOM_CONFIRM_SEC) {
            r->phase = PHASE_VACANT;
            set_light_locked(r, ROOM_LIGHT_OFF);
            notes[n_notes++] = NOTE_FALSE_TRIGGER;
        }
    } else if (r->phase == PHASE_BRIGHT && !new_motion && !pir_high) {
        if ((now_us - r->last_motion_us) / 1000000LL >= (int64_t)r->inactivity_timeout_sec) {
            r->phase = PHASE_VACANT;
            set_light_locked(r, ROOM_LIGHT_OFF);
            notes[n_notes++] = NOTE_LIGHT_OFF;
        }
    }

    /* 3) Motion handling. */
    if (new_motion) {
        r->last_motion_us = now_us;
        if (r->phase == PHASE_VACANT) {
            r->phase = PHASE_DIM_PENDING;
            /* Real PIR: pulse is still high, window opens when it drops.
             * Simulated 'motion N' is an instant pulse, so the window opens immediately. */
            r->first_pulse_active = pir_high;
            r->first_motion_us = now_us;
            set_light_locked(r, ROOM_LIGHT_DIM);
            notes[n_notes++] = NOTE_FIRST_MOTION;
            if (!pir_high) notes[n_notes++] = NOTE_WINDOW_OPEN;
        } else if (r->phase == PHASE_DIM_PENDING) {
            r->phase = PHASE_BRIGHT;
            r->first_pulse_active = false;
            set_light_locked(r, ROOM_LIGHT_BRIGHT);
            notes[n_notes++] = NOTE_CONFIRMED;
        }
        /* PHASE_BRIGHT: just refreshes last_motion_us */
    } else if (pir_high && r->phase == PHASE_BRIGHT) {
        r->last_motion_us = now_us;
    }

    xSemaphoreGive(g_room_mutex);

    for (int i = 0; i < n_notes; i++) {
        emit_note(r, notes[i], source);
    }
}

static void poll_room(room_state_t *r) {
    int level = gpio_get_level(r->pir_pin);
    bool rising = (level == 1 && r->last_pir_level == 0);
    r->last_pir_level = level;
    room_step(r, rising, level == 1, "PIR_SENSOR");
    room_ramp(r);
}

static void room_monitor_task(void *pvParameters) {
    while (1) {
        poll_room(&s_room1);
        poll_room(&s_room2);
        vTaskDelay(pdMS_TO_TICKS(100));
    }
}

static void init_room(room_state_t *r, gpio_num_t pir, gpio_num_t led, ledc_channel_t ch, room_id_t id) {
    memset(r, 0, sizeof(*r));
    r->pir_pin = pir;
    r->led_pin = led;
    r->pwm_channel = ch;
    r->room_id = id;
    r->phase = PHASE_VACANT;
    r->inactivity_timeout_sec = APP_ROOM_INACTIVITY_SEC;
}

esp_err_t room_control_init(void) {
    init_room(&s_room1, PIN_ROOM1_PIR, PIN_ROOM1_LED, LEDC_CHANNEL_0, ROOM_LECTURE_1);
    init_room(&s_room2, PIN_ROOM2_PIR, PIN_ROOM2_LED, LEDC_CHANNEL_1, ROOM_LECTURE_2);

    gpio_config_t pir_cfg = {
        .pin_bit_mask = (1ULL << PIN_ROOM1_PIR) | (1ULL << PIN_ROOM2_PIR),
        .mode = GPIO_MODE_INPUT,
        .pull_down_en = GPIO_PULLDOWN_ENABLE,
        .pull_up_en = GPIO_PULLUP_DISABLE
    };
    gpio_config(&pir_cfg);

    /* Room lights are PWM-driven (LEDC) so they can be dim or bright. */
    ledc_timer_config_t timer_cfg = {
        .speed_mode = LEDC_LOW_SPEED_MODE,
        .duty_resolution = LEDC_TIMER_8_BIT,
        .timer_num = LEDC_TIMER_0,
        .freq_hz = APP_LIGHT_PWM_FREQ_HZ,
        .clk_cfg = LEDC_AUTO_CLK,
    };
    esp_err_t err = ledc_timer_config(&timer_cfg);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "LEDC timer config failed: %s", esp_err_to_name(err));
        return err;
    }

    room_state_t *rooms[2] = { &s_room1, &s_room2 };
    for (int i = 0; i < 2; i++) {
        ledc_channel_config_t ch_cfg = {
            .gpio_num = rooms[i]->led_pin,
            .speed_mode = LEDC_LOW_SPEED_MODE,
            .channel = rooms[i]->pwm_channel,
            .intr_type = LEDC_INTR_DISABLE,
            .timer_sel = LEDC_TIMER_0,
            .duty = 0,
            .hpoint = 0,
        };
        err = ledc_channel_config(&ch_cfg);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "LEDC channel config failed: %s", esp_err_to_name(err));
            return err;
        }
    }

    xTaskCreate(room_monitor_task, "room_mon_task", 4096, NULL, 5, NULL);
    ESP_LOGI(TAG, "Room Automation running (two-step lighting). Confirm window: %ds, inactivity timeout: %ds",
             APP_ROOM_CONFIRM_SEC, APP_ROOM_INACTIVITY_SEC);
    return ESP_OK;
}

void room_control_simulate_motion(room_id_t room) {
    room_state_t *target = room_from_id(room);
    if (!target) return;
    room_step(target, true, false, "SIM_MOTION");
}

void room_control_set_timeout(room_id_t room, uint32_t seconds) {
    room_state_t *r = room_from_id(room);
    if (!r) return;
    xSemaphoreTake(g_room_mutex, portMAX_DELAY);
    r->inactivity_timeout_sec = seconds;
    xSemaphoreGive(g_room_mutex);
}

uint32_t room_control_get_timeout(room_id_t room) {
    room_state_t *r = room_from_id(room);
    if (!r) return 0;
    xSemaphoreTake(g_room_mutex, portMAX_DELAY);
    uint32_t v = r->inactivity_timeout_sec;
    xSemaphoreGive(g_room_mutex);
    return v;
}

void room_control_get_status(room_id_t room, bool *out_occupied, room_light_t *out_light, uint32_t *out_remaining_sec) {
    room_state_t *r = room_from_id(room);
    if (!r) return;

    int64_t now_us = esp_timer_get_time();
    xSemaphoreTake(g_room_mutex, portMAX_DELAY);
    room_phase_t phase = r->phase;
    int64_t first_us = r->first_motion_us;
    bool pulse_active = r->first_pulse_active;
    int64_t last_us = r->last_motion_us;
    uint32_t timeout = r->inactivity_timeout_sec;
    xSemaphoreGive(g_room_mutex);

    *out_occupied = (phase == PHASE_BRIGHT);
    *out_light = light_of_phase(phase);

    int64_t window = 0, elapsed = 0;
    if (phase == PHASE_DIM_PENDING) { window = APP_ROOM_CONFIRM_SEC; elapsed = pulse_active ? 0 : (now_us - first_us) / 1000000LL; }
    else if (phase == PHASE_BRIGHT) { window = timeout;              elapsed = (now_us - last_us) / 1000000LL; }
    *out_remaining_sec = (phase != PHASE_VACANT && elapsed < window) ? (uint32_t)(window - elapsed) : 0;
}

void room_control_print_status(void) {
    static const char *const light_names[] = { "OFF", "DIM", "BRIGHT" };
    bool occ[2];
    room_light_t light[2];
    uint32_t rem[2];
    room_control_get_status(ROOM_LECTURE_1, &occ[0], &light[0], &rem[0]);
    room_control_get_status(ROOM_LECTURE_2, &occ[1], &light[1], &rem[1]);

    printf("\n=== LECTURE ROOM AUTOMATION STATUS ===\n");
    for (int i = 0; i < 2; i++) {
        const char *state = occ[i] ? "OCCUPIED" : (light[i] == ROOM_LIGHT_DIM ? "CONFIRMING" : "VACANT");
        const char *timer = (light[i] == ROOM_LIGHT_DIM) ? "2nd-motion window" : "inactivity timeout";
        printf("Lecture Room %d: State: %-10s | Lighting: %-6s | %s: %02us remaining\n",
               i + 1, state, light_names[light[i]], timer, (unsigned)rem[i]);
    }
    printf("======================================\n\n");
}
