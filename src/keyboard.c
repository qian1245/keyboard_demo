#include "keyboard.h"
#include "lvgl/lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define KEY_HEIGHT 110
#define KEY_GAP 12
#define BASE_U 115.0f

// 声明 assets 中导入的 C 数组气泡贴图
LV_IMAGE_DECLARE(bubble_dot);

static int32_t get_key_width(float u) {
    return (int32_t)(u * BASE_U + (u - 1.0f) * KEY_GAP);
}

typedef struct { const char *label; float width_u; } KeyDef;
static KeyDef row1[] = {{"Esc",1}, {"1",1}, {"2",1}, {"3",1}, {"4",1}, {"5",1}, {"6",1}, {"7",1}, {"8",1}, {"9",1}, {"0",1}, {"-",1}, {"=",1}, {"Backspace", 2.0f}};
static KeyDef row2[] = {{"Tab", 1.5f}, {"Q",1}, {"W",1}, {"E",1}, {"R",1}, {"T",1}, {"Y",1}, {"U",1}, {"I",1}, {"O",1}, {"P",1}, {"[",1}, {"]",1}, {"\\", 1.5f}};
static KeyDef row3[] = {{"Caps", 1.75f}, {"A",1}, {"S",1}, {"D",1}, {"F",1}, {"G",1}, {"H",1}, {"J",1}, {"K",1}, {"L",1}, {";",1}, {"'",1}, {"Enter", 2.25f}};
static KeyDef row4[] = {{"Shift", 2.25f}, {"Z",1}, {"X",1}, {"C",1}, {"V",1}, {"B",1}, {"N",1}, {"M",1}, {",",1}, {".",1}, {"/",1}, {"Shift", 2.75f}};
static KeyDef row5[] = {{"Ctrl", 1.25f}, {"Win", 1.25f}, {"Alt", 1.25f}, {"Space", 6.25f}, {"Alt", 1.25f}, {"Fn", 1.25f}, {"Menu", 1.25f}, {"Ctrl", 1.25f}};
static KeyDef* layout[] = {row1, row2, row3, row4, row5};
static int row_counts[] = {14, 14, 13, 12, 8};

// ==================== 物理水滴气泡粒子系统 ====================

typedef struct {
    lv_obj_t * img;
    float x, y;          // 实时中心坐标
    float vx;            // X 轴左右散射速度
    float target_vy;     // 目标匀速上浮速度
    float current_vy;    // 实时上浮速度
    float base_scale;    // 目标基础大小
    float current_scale; // 动态缩放系数
    float phase;         // 摇摆与形变相位
    float phase_speed;   // 频率
    float wobble_amp;    // S型波浪飘忽幅度
    int32_t life;        // 剩余寿命 (帧数)
    int32_t max_life;    // 总寿命
    int is_persistent;   // 是否为穿屏不消失的特殊气泡
} BubbleParticle;

typedef struct {
    BubbleParticle particles[35]; // 扩充容量以支持空格键大量小气泡
    int count;
} BubbleBurst;

// 动画刷帧回调 (50 FPS)
static void bubble_burst_timer_cb(lv_timer_t * timer) {
    BubbleBurst * burst = (BubbleBurst *)lv_timer_get_user_data(timer);
    if (!burst) return;

    int active_count = 0;
    int32_t img_h = bubble_dot.header.h;

    for (int i = 0; i < burst->count; i++) {
        BubbleParticle * p = &burst->particles[i];
        if (p->life <= 0) continue;

        // 特殊气泡不递减寿命，由顶部边界控制销毁
        if (!p->is_persistent) {
            p->life--;
        }
        active_count++;

        float life_ratio = 0.0f;
        if (!p->is_persistent) {
            life_ratio = (float)(p->max_life - p->life) / (float)p->max_life;
        }

        // 1. 上浮速度曲线
        if (!p->is_persistent) {
            float accel_phase = 0.20f;
            if (life_ratio < accel_phase) {
                p->current_vy = p->target_vy * (life_ratio / accel_phase);
            } else {
                p->current_vy = p->target_vy;
            }
        } else {
            p->current_vy = p->target_vy; // 持续匀速向上飘
        }

        // 2. 位置演进：Y 轴上升，X 轴叠加微幅 S 型波浪
        p->phase += p->phase_speed;
        p->y -= p->current_vy;
        p->x += p->vx + sinf(p->phase) * p->wobble_amp;

        int32_t scale_x = 256, scale_y = 256;
        uint8_t opa = 255;

        if (p->is_persistent) {
            // 穿屏气泡：保持固定微小尺寸，不渐隐，直到完全飞出屏幕顶部
            p->current_scale = p->base_scale;
            scale_x = (int32_t)(p->current_scale * 256.0f);
            scale_y = (int32_t)(p->current_scale * 256.0f);
            opa = 255;

            // 当气泡完全超出屏幕顶部时销毁
            if (p->y + img_h < 0) {
                p->life = 0;
            }
        } else {
            // 3. 普通气泡：入场缩放，保持稳定大小
            if (life_ratio < 0.12f) {
                p->current_scale = p->base_scale * (life_ratio / 0.12f) * 1.1f;
            } else {
                p->current_scale = p->base_scale; 
            }

            // 4. 柔和形变
            float squish = sinf(p->phase * 1.8f) * 0.06f; 
            float scale_x_factor = p->current_scale * (1.0f + squish);
            float scale_y_factor = p->current_scale * (1.0f - squish);

            scale_x = (int32_t)(scale_x_factor * 256.0f);
            scale_y = (int32_t)(scale_y_factor * 256.0f);

            // 5. 单纯的透明度渐隐（后 40% 生命周期开始淡出）
            if (life_ratio > 0.6f) {
                float fade_progress = (life_ratio - 0.6f) / 0.4f; 
                opa = (uint8_t)(255.0f * (1.0f - fade_progress));
            }
        }

        // 6. 更新 LVGL 控件属性
        lv_image_set_scale_x(p->img, scale_x);
        lv_image_set_scale_y(p->img, scale_y);
        lv_image_set_rotation(p->img, (int32_t)(sinf(p->phase) * 60.0f));
        lv_obj_set_style_opa(p->img, opa, 0);

        // 保持中心点对齐
        int32_t img_w = bubble_dot.header.w;
        lv_obj_set_pos(p->img, (int32_t)(p->x - img_w / 2), (int32_t)(p->y - img_h / 2));

        if (p->life <= 0) {
            lv_obj_delete(p->img);
            p->img = NULL;
        }
    }

    if (active_count == 0) {
        free(burst);
        lv_timer_delete(timer);
    }
}

// 点击触发产生气泡群（小气泡占比多、纵坐标与速度错落）
static void trigger_bubble_burst_effect(lv_obj_t * btn) {
    lv_obj_t * parent = lv_screen_active();

    lv_area_t btn_area;
    lv_obj_get_coords(btn, &btn_area);

    // 检查当前点击的是不是空格键
    bool is_space = false;
    lv_obj_t * label_child = lv_obj_get_child(btn, 0);
    if (label_child) {
        const char * txt = lv_label_get_text(label_child);
        if (txt && strcmp(txt, "Space") == 0) {
            is_space = true;
        }
    }

    BubbleBurst * burst = (BubbleBurst *)calloc(1, sizeof(BubbleBurst));
    if (!burst) return;

    if (is_space) {
        burst->count = 16 + (rand() % 8); // 空格键产生 16 ~ 23 个小气泡
    } else {
        burst->count = 4 + (rand() % 4);  // 普通按键产生 4 ~ 7 个气泡
    }

    int32_t img_w = bubble_dot.header.w;
    int32_t img_h = bubble_dot.header.h;

    for (int i = 0; i < burst->count; i++) {
        BubbleParticle * p = &burst->particles[i];

        p->img = lv_image_create(parent);
        lv_image_set_src(p->img, &bubble_dot);
        lv_image_set_pivot(p->img, img_w / 2, img_h / 2);
        lv_obj_remove_flag(p->img, LV_OBJ_FLAG_CLICKABLE);

        if (is_space) {
            // 【空格键逻辑】：全是小气泡，横向铺满，纵向范围错落
            int btn_w = lv_area_get_width(&btn_area);
            int btn_h = lv_area_get_height(&btn_area);
            p->x = (float)btn_area.x1 + (float)(rand() % (btn_w > 0 ? btn_w : 1));
            p->y = (float)btn_area.y1 + (float)(rand() % (btn_h > 0 ? btn_h : 1)); // 纵坐标在按键内部高低错落
            p->vx = ((rand() % 50) - 25) / 100.0f;               
            p->current_vy = 0.0f;                                
            p->is_persistent = 0;

            // 细小气泡，但大小和速度各有不同，避免死板
            p->base_scale = 0.08f + (float)(rand() % 10) / 100.0f; // 0.08 ~ 0.17
            p->target_vy = 0.8f + (float)(rand() % 15) / 10.0f;   // 0.8 ~ 2.3 速度错开
            p->max_life = 55 + (rand() % 35);                    
        } else {
            // 【普通按键逻辑】：纵坐标大幅错开，小气泡占绝大多数
            float center_x = (float)(btn_area.x1 + lv_area_get_width(&btn_area) / 2);
            float center_y = (float)(btn_area.y1 + lv_area_get_height(&btn_area) / 2);

            // 纵坐标和横坐标上下左右错开，不再在同一水平线上产生
            p->x = center_x + (float)((rand() % 30) - 15);
            p->y = center_y + (float)((rand() % 26) - 13); // 纵坐标上下分散
            p->vx = ((rand() % 60) - 30) / 100.0f;               
            p->current_vy = 0.0f;                                

            int type_rand = rand() % 100;
            if (i == 0 && type_rand < 15) {
                // 极少概率出现的大气泡（慢速）
                p->base_scale = 0.24f + (float)(rand() % 6) / 100.0f; 
                p->target_vy = 0.7f + (float)(rand() % 4) / 10.0f;  
                p->max_life = 90 + (rand() % 20);                  
                p->is_persistent = 0;
            } else if (type_rand >= 85) {
                // 极少概率出现的穿屏气泡
                p->base_scale = 0.08f + (float)(rand() % 4) / 100.0f; 
                p->target_vy = 0.6f + (float)(rand() % 4) / 10.0f;  
                p->max_life = 999999;                                
                p->is_persistent = 1;                                
            } else {
                // 大多数都是小气泡，但彼此的体积和速度各不相同
                p->base_scale = 0.09f + (float)(rand() % 10) / 100.0f; // 0.09 ~ 0.18
                p->target_vy = 1.0f + (float)(rand() % 12) / 10.0f;  // 1.0 ~ 2.2 速度差异明显
                p->max_life = 55 + (rand() % 35);                  
                p->is_persistent = 0;
            }
        }

        p->current_scale = 0.0f;
        p->phase = (float)(rand() % 360) * 0.01745f;
        p->phase_speed = 0.03f + (float)(rand() % 5) / 100.0f; 
        p->wobble_amp = 0.15f + (float)(rand() % 10) / 30.0f;    
        p->life = p->max_life;

        lv_image_set_scale(p->img, 0);
        lv_obj_set_pos(p->img, (int32_t)(p->x - img_w / 2), (int32_t)(p->y - img_h / 2));
        lv_obj_move_foreground(p->img);
    }

    lv_timer_create(bubble_burst_timer_cb, 20, burst);
}

static void btn_bubble_event_cb(lv_event_t * e) {
    lv_event_code_t code = lv_event_get_code(e);
    lv_obj_t * btn = lv_event_get_target(e);

    if (code == LV_EVENT_PRESSED) {
        trigger_bubble_burst_effect(btn);
    }
}

// 搭建界面布局
void create_keyboard_ui(void) {
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x050505), 0);

    static lv_style_t style_btn;
    lv_style_init(&style_btn);
    lv_style_set_bg_opa(&style_btn, LV_OPA_TRANSP);
    lv_style_set_border_width(&style_btn, 1);
    lv_style_set_border_color(&style_btn, lv_color_hex(0x333333));
    lv_style_set_shadow_width(&style_btn, 0);
    lv_style_set_radius(&style_btn, 8);

    static lv_style_t style_btn_pr;
    lv_style_init(&style_btn_pr);
    lv_style_set_bg_opa(&style_btn_pr, LV_OPA_TRANSP);
    lv_style_set_border_color(&style_btn_pr, lv_color_hex(0x333333));

    lv_obj_t * main_cont = lv_obj_create(lv_screen_active());
    lv_obj_set_size(main_cont, 1850, 680);
    lv_obj_center(main_cont);
    lv_obj_set_style_bg_opa(main_cont, 0, 0);
    lv_obj_set_style_border_width(main_cont, 0, 0);
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

    lv_obj_set_style_pad_row(main_cont, KEY_GAP, 0);

    lv_obj_remove_flag(main_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(main_cont, LV_SCROLLBAR_MODE_OFF);

    for(int r = 0; r < 5; r++) {
        lv_obj_t * row_cont = lv_obj_create(main_cont);
        lv_obj_set_width(row_cont, LV_PCT(100));
        lv_obj_set_height(row_cont, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(row_cont, 0, 0);
        lv_obj_set_style_border_width(row_cont, 0, 0);
        lv_obj_set_style_pad_all(row_cont, 0, 0);

        lv_obj_remove_flag(row_cont, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(row_cont, LV_SCROLLBAR_MODE_OFF);
        lv_obj_set_flex_flow(row_cont, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        for(int k = 0; k < row_counts[r]; k++) {
            lv_obj_t * btn = lv_button_create(row_cont);
            lv_obj_add_style(btn, &style_btn, LV_STATE_DEFAULT);
            lv_obj_add_style(btn, &style_btn_pr, LV_STATE_PRESSED);

            int32_t w = get_key_width(layout[r][k].width_u);
            lv_obj_set_size(btn, w, KEY_HEIGHT);

            lv_obj_t * label = lv_label_create(btn);
            lv_label_set_text(label, layout[r][k].label);
            lv_obj_center(label);

            lv_obj_add_event_cb(btn, btn_bubble_event_cb, LV_EVENT_PRESSED, NULL);
        }
    }
}