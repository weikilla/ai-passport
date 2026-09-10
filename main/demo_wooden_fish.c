/**
 * @file demo_wooden_fish.c
 * @brief 敲木鱼 Demo - AI Passport
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
#define HIT_COOLDOWN_MS    200
#define COMBO_TIMEOUT_MS   2000

// 木鱼状态
typedef enum {
    WF_STATE_IDLE = 0,
    WF_STATE_HIT,
    WF_STATE_SHAKE
} wf_state_t;

// 木鱼实例
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

// LVGL 对象
static lv_obj_t *s_screen = NULL;
static lv_obj_t *s_label_merit = NULL;
static lv_obj_t *s_label_combo = NULL;
static lv_obj_t *s_fish_body = NULL;
static lv_obj_t *s_fish_dot = NULL;
static lv_obj_t *s_label_title = NULL;
static lv_obj_t *s_label_tip = NULL;

// 动画
static bool s_animating = false;
static uint32_t s_anim_time = 0;

// 屏幕尺寸
#define SCR_W 128
#define SCR_H 160

// 颜色
#define C_BG    0x1A1A2E
#define C_GOLD  0xFFD700
#define C_RED   0xFF4444
#define C_BROWN 0x8B4513
#define C_LIGHT 0xFFE4B5
#define C_INK   0x00FF88

static void refresh_merit(void) {
    if (s_label_merit) {
        lv_label_set_text_fmt(s_label_merit, "功德: %d", s_wf.merit_count);
    }
}

static void refresh_combo(void) {
    if (s_label_combo) {
        if (s_wf.hit_count >= 3) {
            lv_label_set_text_fmt(s_label_combo, "x%d", s_wf.hit_count);
            lv_obj_set_style_text_font(s_label_combo, &lv_font_montserrat_20, 0);
            lv_obj_set_style_text_color(s_label_combo, lv_color_hex(C_RED), 0);
        } else {
            lv_label_set_text(s_label_combo, "");
        }
    }
}

// 简单的敲击音效生成 (800Hz 正弦波,衰减)
static void play_beep(int freq, int duration_ms) {
    const int sample_rate = 8000;
    const int num_samples = sample_rate * duration_ms / 1000;
    static int16_t pcm[8000];  // 最多1秒
    
    for (int i = 0; i < num_samples && i < 8000; i++) {
        float t = (float)i / sample_rate;
        float decay = expf(-t * 20.0f);  // 指数衰减
        pcm[i] = (int16_t)(sinf(2 * M_PI * freq * t) * 16000 * decay);
    }
    
    bsp_audio_set_format(sample_rate, 16, 1);
    bsp_audio_write(pcm, num_samples * 2);
}

static void play_hit_effect(void) {
    // 缩放动画
    if (s_fish_body) {
        static int orig_x = 10, orig_y = 25, orig_w = 108, orig_h = 95;
        
        lv_obj_set_pos(s_fish_body, orig_x + 4, orig_y + 2);
        lv_obj_set_size(s_fish_body, orig_w - 8, orig_h - 4);
        
        s_animating = true;
        s_anim_time = esp_timer_get_time() / 1000;
    }
    
    // 功德飞字效果
    static lv_obj_t *merit_pop = NULL;
    if (merit_pop) lv_obj_delete(merit_pop);
    
    merit_pop = lv_label_create(s_screen);
    lv_label_set_text(merit_pop, "+1");
    lv_obj_set_pos(merit_pop, 50, 60);
    lv_obj_set_style_text_color(merit_pop, lv_color_hex(C_GOLD), 0);
    lv_obj_set_style_text_font(merit_pop, &lv_font_montserrat_12, 0);
    
    lv_anim_t a;
    lv_anim_init(&a);
    lv_anim_set_var(&a, merit_pop);
    lv_anim_set_duration(&a, 500);
    lv_anim_set_start_val(&a, 255);
    lv_anim_set_end_val(&a, 0);
    lv_anim_set_exec_cb(&a, (lv_anim_exec_xcb_t)lv_obj_set_opa);
    lv_anim_set_path_cb(&a, &lv_anim_path_ease_out);
    lv_anim_start(&a);
    
    vTaskDelay(pdMS_TO_TICKS(100));
    lv_obj_delete(merit_pop);
}

static void do_hit(void) {
    uint32_t now = esp_timer_get_time() / 1000;
    
    // 冷却检查
    if (now - s_wf.last_hit_time < HIT_COOLDOWN_MS) {
        return;
    }
    s_wf.last_hit_time = now;
    
    // 计算功德
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
    
    // 播放音效
    play_beep(800, 100);
    
    // 更新 UI
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
    
    bsp_audio_play_shake();
    play_beep(1200, 150);
    refresh_merit();
    
    // 特效
    if (s_label_merit) {
        static bool toggle = false;
        toggle = !toggle;
        lv_obj_set_style_text_color(s_label_merit, 
            toggle ? lv_color_hex(C_GOLD) : lv_color_hex(0xFFFFFF), 0);
    }
    
    ESP_LOGI(TAG, "Shake! Bonus: %d, Total: %d", bonus, s_wf.merit_count);
}

static void do_reset(void) {
    s_wf.merit_count = 0;
    s_wf.hit_count = 0;
    refresh_merit();
    refresh_combo();
    ESP_LOGI(TAG, "Reset");
}

static void anim_restore(void *obj, int32_t v) {
    (void)v;
    if (s_fish_body) {
        lv_obj_set_pos(s_fish_body, 10, 25);
        lv_obj_set_size(s_fish_body, 108, 95);
    }
}

void demo_wooden_fish_enter(void) {
    ESP_LOGI(TAG, "Enter wooden fish");
    
    // 重置状态
    s_wf.state = WF_STATE_IDLE;
    s_wf.hit_count = 0;
    s_wf.last_hit_time = 0;
    s_animating = false;
    
    // 创建屏幕
    s_screen = ui_pixel_screen_create("木鱼");
    lv_obj_set_style_bg_color(s_screen, lv_color_hex(C_BG), 0);
    
    // 标题
    s_label_title = lv_label_create(s_screen);
    lv_label_set_text(s_label_title, "🪷 敲木鱼");
    lv_obj_set_pos(s_label_title, 32, 3);
    lv_obj_set_style_text_color(s_label_title, lv_color_hex(C_GOLD), 0);
    lv_obj_set_style_text_font(s_label_title, &lv_font_montserrat_10, 0);
    
    // 功德显示
    s_label_merit = lv_label_create(s_screen);
    lv_label_set_text_fmt(s_label_merit, "功德: 0");
    lv_obj_set_pos(s_label_merit, 5, 17);
    lv_obj_set_style_text_color(s_label_merit, lv_color_hex(C_GOLD), 0);
    lv_obj_set_style_text_font(s_label_merit, &lv_font_montserrat_12, 0);
    
    // 连击显示
    s_label_combo = lv_label_create(s_screen);
    lv_label_set_text(s_label_combo, "");
    lv_obj_set_pos(s_label_combo, 88, 125);
    lv_obj_set_style_text_color(s_label_combo, lv_color_hex(C_RED), 0);
    
    // 木鱼身体 (椭圆形)
    s_fish_body = lv_obj_create(s_screen);
    lv_obj_set_pos(s_fish_body, 10, 25);
    lv_obj_set_size(s_fish_body, 108, 95);
    lv_obj_set_style_bg_color(s_fish_body, lv_color_hex(C_BROWN), 0);
    lv_obj_set_style_radius(s_fish_body, 30, 0);
    lv_obj_set_style_border_width(s_fish_body, 2, 0);
    lv_obj_set_style_border_color(s_fish_body, lv_color_hex(C_LIGHT), 0);
    
    // 木鱼装饰线
    lv_obj_t *line = lv_line_create(s_fish_body);
    static lv_point_t p[] = {{30, 45}, {78, 45}};
    lv_line_set_points(line, p, 2);
    lv_obj_set_style_line_color(line, lv_color_hex(C_LIGHT), 0);
    lv_obj_set_style_line_width(line, 2, 0);
    
    // 木鱼中心圆点
    s_fish_dot = lv_obj_create(s_fish_body);
    lv_obj_set_pos(s_fish_dot, 44, 35);
    lv_obj_set_size(s_fish_dot, 20, 20);
    lv_obj_set_style_bg_color(s_fish_dot, lv_color_hex(C_LIGHT), 0);
    lv_obj_set_style_radius(s_fish_dot, 10, 0);
    
    // 提示
    s_label_tip = lv_label_create(s_screen);
    lv_label_set_text(s_label_tip, "OK敲 UP摇 DOWN重置");
    lv_obj_set_pos(s_label_tip, 8, 145);
    lv_obj_set_style_text_color(s_label_tip, lv_color_hex(0x666666), 0);
    lv_obj_set_style_text_font(s_label_tip, &lv_font_montserrat_8, 0);
    
    lv_screen_load(s_screen);
}

void demo_wooden_fish_exit(void) {
    ESP_LOGI(TAG, "Exit wooden fish");
    
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
