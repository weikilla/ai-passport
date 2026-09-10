/**
 * @file demo_wooden_fish.c
 * @brief Wooden Fish Demo - AI Passport
 */
#include "demo.h"
#include "ui_pixel.h"
#include "bsp_button.h"
#include "bsp_audio.h"
#include "esp_log.h"
#include "esp_timer.h"
#include "esp_random.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <math.h>

static const char *TAG = "wooden_fish";

#define WOODEN_FISH_MAX_MERIT 99999
#define HIT_COOLDOWN_MS    200
#define COMBO_TIMEOUT_MS   2000

// Wooden fish state
typedef enum {
    WF_STATE_IDLE = 0,
    WF_STATE_HIT,
    WF_STATE_SHAKE
} wf_state_t;

// Wooden fish instance
typedef struct {
    int merit_count;
    wf_state_t state;
    uint32_t last_hit_time;
    uint16_t hit_count;
} wf_t;

static wf_t s_wf = {
    .merit_count = 0,
    .state = WF_STATE_IDLE,
    .last_hit_time = 0,
    .hit_count = 0
};

// LVGL objects
static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_label_merit = NULL;
static lv_obj_t *s_label_combo = NULL;
static lv_obj_t *s_fish_body = NULL;
static lv_obj_t *s_fish_dot = NULL;
static lv_obj_t *s_label_title = NULL;
static lv_obj_t *s_label_tip = NULL;

// Screen size
#define SCR_W 128
#define SCR_H 160

// Colors
#define C_BG    0x1A1A2E
#define C_GOLD  0xFFD700
#define C_RED   0xFF4444
#define C_BROWN 0x8B4513
#define C_LIGHT 0xFFE4B5
#define C_INK   0x00FF88

static void refresh_merit(void) {
    if (s_label_merit && bsp_lvgl_lock(100)) {
        lv_label_set_text_fmt(s_label_merit, "GongDe: %d", s_wf.merit_count);
        bsp_lvgl_unlock();
    }
}

static void refresh_combo(void) {
    if (s_label_combo && bsp_lvgl_lock(100)) {
        if (s_wf.hit_count >= 3) {
            lv_label_set_text_fmt(s_label_combo, "x%d", s_wf.hit_count);
            lv_obj_set_style_text_font(s_label_combo, &lv_font_montserrat_20, 0);
            lv_obj_set_style_text_color(s_label_combo, lv_color_hex(C_RED), 0);
        } else {
            lv_label_set_text(s_label_combo, "");
        }
        bsp_lvgl_unlock();
    }
}

// Simple beep sound generation (sine wave with decay)
static void play_beep(int freq, int duration_ms) {
    const int sample_rate = 8000;
    const int num_samples = sample_rate * duration_ms / 1000;
    static int16_t pcm[8000];
    
    for (int i = 0; i < num_samples && i < 8000; i++) {
        float t = (float)i / sample_rate;
        float decay = expf(-t * 20.0f);
        pcm[i] = (int16_t)(sinf(2 * M_PI * freq * t) * 16000 * decay);
    }
    
    bsp_audio_set_format(sample_rate, 16, 1);
    bsp_audio_write(pcm, num_samples * 2);
}

static void play_hit_effect(void) {
    if (!bsp_lvgl_lock(500)) return;
    
    // Hit animation - scale effect
    if (s_fish_body) {
        lv_obj_set_pos(s_fish_body, 14, 27);
        lv_obj_set_size(s_fish_body, 100, 91);
    }
    
    // Merit popup effect
    lv_obj_t *merit_pop = lv_label_create(s_screen);
    lv_label_set_text(merit_pop, "+1");
    lv_obj_set_pos(merit_pop, 50, 60);
    lv_obj_set_style_text_color(merit_pop, lv_color_hex(C_GOLD), 0);
    lv_obj_set_style_text_font(merit_pop, &lv_font_montserrat_14, 0);
    
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, merit_pop);
    lv_anim_set_duration(&a, 500);
    lv_anim_set_values(&a, 255, 0);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_style_opa);
    lv_anim_set_path_cb(&a, &lv_anim_path_ease_out);
    lv_anim_start(&a);
    
    bsp_lvgl_unlock();
    
    vTaskDelay(pdMS_TO_TICKS(500));
    
    if (bsp_lvgl_lock(500)) {
        lv_obj_delete(merit_pop);
        if (s_fish_body) {
            lv_obj_set_pos(s_fish_body, 10, 25);
            lv_obj_set_size(s_fish_body, 108, 95);
        }
        bsp_lvgl_unlock();
    }
}

static void do_hit(void) {
    uint32_t now = esp_timer_get_time() / 1000;
    
    // Cooldown check
    if (now - s_wf.last_hit_time < HIT_COOLDOWN_MS) {
        return;
    }
    s_wf.last_hit_time = now;
    
    // Calculate merit
    int merit_gain = 1;
    s_wf.hit_count++;
    
    if (s_wf.hit_count >= 10) merit_gain = 5;
    else if (s_wf.hit_count >= 5) merit_gain = 3;
    else if (s_wf.hit_count >= 3) merit_gain = 2;
    
    s_wf.merit_count += merit_gain;
    if (s_wf.merit_count > WOODEN_FISH_MAX_MERIT) {
        s_wf.merit_count = WOODEN_FISH_MAX_MERIT;
    }
    
    s_wf.state = WF_STATE_HIT;
    
    // Play sound
    play_beep(800, 100);
    
    // Update UI
    refresh_merit();
    refresh_combo();
    play_hit_effect();
    
    ESP_LOGI(TAG, "Hit! Merit: %d (+%d), Combo: %d",
             s_wf.merit_count, merit_gain, s_wf.hit_count);
    
    s_wf.state = WF_STATE_IDLE;
}

static void do_shake(void) {
    int bonus = (esp_random() % 10) + 5;
    s_wf.merit_count += bonus;
    if (s_wf.merit_count > WOODEN_FISH_MAX_MERIT) {
        s_wf.merit_count = WOODEN_FISH_MAX_MERIT;
    }
    
    play_beep(1200, 150);
    refresh_merit();
    
    ESP_LOGI(TAG, "Shake! Bonus: %d, Total: %d", bonus, s_wf.merit_count);
}

static void do_reset(void) {
    s_wf.merit_count = 0;
    s_wf.hit_count = 0;
    refresh_merit();
    refresh_combo();
    ESP_LOGI(TAG, "Reset");
}

void demo_wooden_fish_enter(void) {
    ESP_LOGI(TAG, "Enter wooden fish");
    
    // Reset state
    s_wf.state = WF_STATE_IDLE;
    s_wf.hit_count = 0;
    s_wf.last_hit_time = 0;
    
    // Create screen
    s_screen = ui_pixel_screen_create("Fish");
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(C_BG), 0);
    
    // Title
    s_label_title = lv_label_create(s_screen);
    lv_label_set_text(s_label_title, "Wooden Fish");
    lv_obj_set_pos(s_label_title, 30, 3);
    lv_obj_set_style_text_color(s_label_title, lv_color_hex(C_GOLD), 0);
    lv_obj_set_style_text_font(s_label_title, &lv_font_montserrat_14, 0);
    
    // Merit display
    s_label_merit = lv_label_create(s_screen);
    lv_label_set_text_fmt(s_label_merit, "GongDe: 0");
    lv_obj_set_pos(s_label_merit, 5, 17);
    lv_obj_set_style_text_color(s_label_merit, lv_color_hex(C_GOLD), 0);
    lv_obj_set_style_text_font(s_label_merit, &lv_font_montserrat_14, 0);
    
    // Combo display
    s_label_combo = lv_label_create(s_screen);
    lv_label_set_text(s_label_combo, "");
    lv_obj_set_pos(s_label_combo, 88, 125);
    lv_obj_set_style_text_color(s_label_combo, lv_color_hex(C_RED), 0);
    
    // Fish body (oval shape)
    s_fish_body = lv_obj_create(s_screen);
    lv_obj_set_pos(s_fish_body, 10, 25);
    lv_obj_set_size(s_fish_body, 108, 95);
    lv_obj_set_style_bg_color(s_fish_body, lv_color_hex(C_BROWN), 0);
    lv_obj_set_style_radius(s_fish_body, 30, 0);
    lv_obj_set_style_border_width(s_fish_body, 2, 0);
    lv_obj_set_style_border_color(s_fish_body, lv_color_hex(C_LIGHT), 0);
    
    // Fish decoration line
    lv_obj_t *line = lv_line_create(s_fish_body);
    static lv_point_precise_t p[] = {{30, 45}, {78, 45}};
    lv_line_set_points(line, p, 2);
    lv_obj_set_style_line_color(line, lv_color_hex(C_LIGHT), 0);
    lv_obj_set_style_line_width(line, 2, 0);
    
    // Fish center dot
    s_fish_dot = lv_obj_create(s_fish_body);
    lv_obj_set_pos(s_fish_dot, 44, 35);
    lv_obj_set_size(s_fish_dot, 20, 20);
    lv_obj_set_style_bg_color(s_fish_dot, lv_color_hex(C_LIGHT), 0);
    lv_obj_set_style_radius(s_fish_dot, 10, 0);
    
    // Hint text
    s_label_tip = lv_label_create(s_screen);
    lv_label_set_text(s_label_tip, "OK:Hit UP:Shake DOWN:Reset");
    lv_obj_set_pos(s_label_tip, 2, 145);
    lv_obj_set_style_text_color(s_label_tip, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(s_label_tip, &lv_font_montserrat_14, 0);
    
    lv_screen_load(s_screen);
}

void demo_wooden_fish_exit(void) {
    ESP_LOGI(TAG, "Exit wooden fish");
    
    if (bsp_lvgl_lock(1000)) {
        if (s_screen) {
            lv_obj_delete(s_screen);
            s_screen = NULL;
            s_label_merit = NULL;
            s_label_combo = NULL;
            s_fish_body = NULL;
            s_fish_dot = NULL;
            s_label_title = NULL;
            s_label_tip = NULL;
        }
        bsp_lvgl_unlock();
    }
}

void demo_wooden_fish_key(bsp_btn_t btn, bsp_btn_ev_t ev) {
    // Handle CLICK events
    if (ev == BSP_BTN_CLICK) {
        switch (btn) {
            case BSP_BTN_OK:
                do_hit();
                break;
            case BSP_BTN_UP:
                do_shake();
                break;
            case BSP_BTN_DOWN:
                do_reset();
                break;
            default:
                break;
        }
    }
}
