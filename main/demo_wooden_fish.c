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

// Wooden fish state
static int s_merit_count = 0;
static int s_hit_count = 0;
static uint32_t s_last_hit_time = 0;

// LVGL objects
static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_label_merit = NULL;
static lv_obj_t *s_label_combo = NULL;

static void refresh_display(void) {
    if (s_label_merit) {
        lv_label_set_text_fmt(s_label_merit, "%d", s_merit_count);
    }
    if (s_label_combo) {
        if (s_hit_count >= 3) {
            lv_label_set_text_fmt(s_label_combo, "x%d", s_hit_count);
        } else {
            lv_label_set_text(s_label_combo, "");
        }
    }
}

// Simple beep sound
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

static void do_hit(void) {
    uint32_t now = esp_timer_get_time() / 1000;
    
    if (now - s_last_hit_time < HIT_COOLDOWN_MS) {
        return;
    }
    s_last_hit_time = now;
    
    int merit_gain = 1;
    s_hit_count++;
    
    if (s_hit_count >= 10) merit_gain = 5;
    else if (s_hit_count >= 5) merit_gain = 3;
    else if (s_hit_count >= 3) merit_gain = 2;
    
    s_merit_count += merit_gain;
    if (s_merit_count > WOODEN_FISH_MAX_MERIT) {
        s_merit_count = WOODEN_FISH_MAX_MERIT;
    }
    
    play_beep(800, 80);
    refresh_display();
    
    ESP_LOGI(TAG, "Hit! Merit: %d, Combo: %d", s_merit_count, s_hit_count);
}

static void do_shake(void) {
    int bonus = (esp_random() % 10) + 5;
    s_merit_count += bonus;
    if (s_merit_count > WOODEN_FISH_MAX_MERIT) {
        s_merit_count = WOODEN_FISH_MAX_MERIT;
    }
    
    play_beep(1000, 120);
    refresh_display();
    
    ESP_LOGI(TAG, "Shake! Bonus: %d, Total: %d", bonus, s_merit_count);
}

static void do_reset(void) {
    s_merit_count = 0;
    s_hit_count = 0;
    refresh_display();
    ESP_LOGI(TAG, "Reset");
}

void demo_wooden_fish_enter(void) {
    ESP_LOGI(TAG, "Enter wooden fish");
    
    s_merit_count = 0;
    s_hit_count = 0;
    s_last_hit_time = 0;
    
    // Create screen with title
    s_screen = ui_pixel_screen_create("FISH");
    
    // Create main panel
    lv_obj_t *panel = ui_pixel_panel_create(s_screen, 12, 28, 104, 100, UI_PAPER);
    lv_obj_set_style_radius(panel, 12, 0);
    
    // Title label
    lv_obj_t *title = ui_pixel_label(panel, "WOODEN FISH", &lv_font_montserrat_14, UI_INK);
    lv_obj_align(title, LV_ALIGN_TOP_MID, 0, 8);
    
    // Merit number display (big and centered)
    s_label_merit = lv_label_create(panel);
    lv_label_set_text_fmt(s_label_merit, "0");
    lv_obj_set_style_text_font(s_label_merit, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_label_merit, lv_color_hex(UI_SKY_DARK), 0);
    lv_obj_align(s_label_merit, LV_ALIGN_CENTER, 0, 5);
    
    // Combo display
    s_label_combo = lv_label_create(panel);
    lv_obj_set_style_text_font(s_label_combo, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_label_combo, lv_color_hex(UI_RED), 0);
    lv_obj_align(s_label_combo, LV_ALIGN_BOTTOM_MID, 0, -8);
    
    // Create mascot (the fish)
    ui_pixel_mascot_create(s_screen, 54, 132);
    
    lv_screen_load(s_screen);
}

void demo_wooden_fish_exit(void) {
    ESP_LOGI(TAG, "Exit wooden fish");
    
    if (s_screen) {
        lv_obj_delete(s_screen);
        s_screen = NULL;
        s_label_merit = NULL;
        s_label_combo = NULL;
    }
}

void demo_wooden_fish_key(bsp_btn_t btn, bsp_btn_ev_t ev) {
    if (ev != BSP_BTN_CLICK) return;
    
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
