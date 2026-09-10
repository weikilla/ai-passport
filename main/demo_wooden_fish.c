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
#include <math.h>

static const char *TAG = "wooden_fish";

#define WOODEN_FISH_MAX_MERIT 99999
#define HIT_COOLDOWN_MS    150

static int s_merit_count = 0;
static int s_hit_count = 0;
static uint32_t s_last_hit_time = 0;

// LVGL objects
static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_label_merit = NULL;
static lv_obj_t *s_label_combo = NULL;
static lv_obj_t *s_fish = NULL;
static lv_obj_t *s_fish_center = NULL;

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
    
    // Hit animation - quick scale
    if (s_fish) {
        lv_obj_set_x(s_fish, 18);
        vTaskDelay(pdMS_TO_TICKS(50));
        lv_obj_set_x(s_fish, 14);
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
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(UI_PAPER), 0);
    
    // Merit number (top area)
    lv_obj_t *top_bar = lv_obj_create(s_screen);
    lv_obj_set_pos(top_bar, 0, 0);
    lv_obj_set_size(top_bar, 128, 30);
    lv_obj_set_style_bg_color(top_bar, lv_color_hex(UI_INK), 0);
    
    s_label_merit = lv_label_create(top_bar);
    lv_label_set_text_fmt(s_label_merit, "0");
    lv_obj_set_style_text_font(s_label_merit, &lv_font_montserrat_20, 0);
    lv_obj_set_style_text_color(s_label_merit, lv_color_hex(UI_YELLOW), 0);
    lv_obj_align(s_label_merit, LV_ALIGN_CENTER, 0, 0);
    
    // Combo display
    s_label_combo = lv_label_create(s_screen);
    lv_obj_set_pos(s_label_combo, 90, 35);
    lv_obj_set_style_text_font(s_label_combo, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(s_label_combo, lv_color_hex(UI_RED), 0);
    
    // === Draw wooden fish ===
    // Fish body - oval shape using rounded rectangle
    s_fish = lv_obj_create(s_screen);
    lv_obj_set_pos(s_fish, 14, 45);
    lv_obj_set_size(s_fish, 100, 80);
    lv_obj_set_style_bg_color(s_fish, lv_color_hex(0x8B4513), 0);  // Brown
    lv_obj_set_style_radius(s_fish, 35, 0);
    lv_obj_set_style_border_width(s_fish, 3, 0);
    lv_obj_set_style_border_color(s_fish, lv_color_hex(0xD2691E), 0);  // Lighter brown
    
    // Fish top decoration line
    lv_obj_t *line1 = lv_obj_create(s_fish);
    lv_obj_set_pos(line1, 20, 20);
    lv_obj_set_size(line1, 60, 2);
    lv_obj_set_style_bg_color(line1, lv_color_hex(0xD2691E), 0);
    lv_obj_set_style_radius(line1, 1, 0);
    
    // Fish center circle
    s_fish_center = lv_obj_create(s_fish);
    lv_obj_set_pos(s_fish_center, 35, 28);
    lv_obj_set_size(s_fish_center, 30, 30);
    lv_obj_set_style_bg_color(s_fish_center, lv_color_hex(0xD2691E), 0);
    lv_obj_set_style_radius(s_fish_center, 15, 0);
    
    // Inner dot
    lv_obj_t *inner = lv_obj_create(s_fish_center);
    lv_obj_set_pos(inner, 10, 10);
    lv_obj_set_size(inner, 10, 10);
    lv_obj_set_style_bg_color(inner, lv_color_hex(0x4A2511), 0);
    lv_obj_set_style_radius(inner, 5, 0);
    
    // === Bottom hint ===
    lv_obj_t *hint = lv_label_create(s_screen);
    lv_label_set_text(hint, "OK:HIT UP:+ DOWN:CLR");
    lv_obj_set_pos(hint, 5, 132);
    lv_obj_set_style_text_font(hint, &lv_font_montserrat_14, 0);
    lv_obj_set_style_text_color(hint, lv_color_hex(UI_MUTED), 0);
    
    lv_screen_load(s_screen);
}

void demo_wooden_fish_exit(void) {
    ESP_LOGI(TAG, "Exit wooden fish");
    
    if (s_screen) {
        lv_obj_delete(s_screen);
        s_screen = NULL;
        s_label_merit = NULL;
        s_label_combo = NULL;
        s_fish = NULL;
        s_fish_center = NULL;
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
