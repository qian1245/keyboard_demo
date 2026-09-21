#include "keyboard.h"
#include "lvgl/lvgl.h"
#include <stdio.h>
#include <stdlib.h>
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
} BubbleParticle;

typedef struct {
    BubbleParticle particles[5]; // 单次点击爆发 3 ~ 4 个动态气泡
    int count;
} BubbleBurst;

// 动画刷帧回调 (50 FPS)
static void bubble_burst_timer_cb(lv_timer_t * timer) {
    BubbleBurst * burst = (BubbleBurst *)lv_timer_get_user_data(timer);
    if (!burst) return;

    int active_count = 0;

    for (int i = 0; i < burst->count; i++) {
        BubbleParticle * p = &burst->particles[i];
        if (p->life <= 0) continue;

        p->life--;
        active_count++;

        // 生命周期比例 (0.0 -> 1.0)
        float life_ratio = (float)(p->max_life - p->life) / (float)p->max_life;

        // 1. 上浮速度曲线：前 20% 寿命加速，后续匀速
        float accel_phase = 0.20f;
        if (life_ratio < accel_phase) {
            p->current_vy = p->target_vy * (life_ratio / accel_phase);
        } else {
            p->current_vy = p->target_vy;
        }

        // 2. 位置演进：Y 轴上升，X 轴叠加微幅 S 型波浪
        p->phase += p->phase_speed;
        p->y -= p->current_vy;
        p->x += p->vx + sinf(p->phase) * p->wobble_amp;

        // 3. 入场缩放 (0 ~ 12% 阶段弹出)
        if (life_ratio < 0.12f) {
            p->current_scale = p->base_scale * (life_ratio / 0.12f) * 1.1f;
        } else {
            float t = (life_ratio - 0.12f) / 0.88f;
            p->current_scale = p->base_scale * (1.1f - 0.1f * t);
        }

        // 4. 柔和形变 (6% 轻微水滴波动)
        float squish = sinf(p->phase * 1.8f) * 0.06f; 
        float scale_x_factor = p->current_scale * (1.0f + squish);
        float scale_y_factor = p->current_scale * (1.0f - squish);

        // LVGL 9 缩放转换：256 为 100%
        int32_t scale_x = (int32_t)(scale_x_factor * 256.0f);
        int32_t scale_y = (int32_t)(scale_y_factor * 256.0f);

        // 5. 自然渐隐 (末端 25% 生命周期淡出)
        uint8_t opa = 255;
        if (life_ratio > 0.75f) {
            opa = (uint8_t)(255.0f * (1.0f - (life_ratio - 0.75f) / 0.25f));
        }

        // 6. 更新 LVGL 控件属性
        lv_image_set_scale_x(p->img, scale_x);
        lv_image_set_scale_y(p->img, scale_y);
        lv_image_set_rotation(p->img, (int32_t)(sinf(p->phase) * 80.0f));
        lv_obj_set_style_opa(p->img, opa, 0);

        // 保持中心点对齐
        int32_t img_w = bubble_dot.header.w;
        int32_t img_h = bubble_dot.header.h;
        lv_obj_set_pos(p->img, (int32_t)(p->x - img_w / 2), (int32_t)(p->y - img_h / 2));

        // 气泡生命周期结束，回收资源
        if (p->life <= 0) {
            lv_obj_delete(p->img);
            p->img = NULL;
        }
    }

    // 所有气泡消失后，销毁定时器与内存
    if (active_count == 0) {
        free(burst);
        lv_timer_delete(timer);
    }
}

// 点击触发产生气泡群
static void trigger_bubble_burst_effect(lv_obj_t * btn) {
    lv_obj_t * parent = lv_screen_active();

    // 获取点击按键的中心物理坐标
    lv_area_t btn_area;
    lv_obj_get_coords(btn, &btn_area);
    float center_x = (float)(btn_area.x1 + lv_area_get_width(&btn_area) / 2);
    float center_y = (float)(btn_area.y1 + lv_area_get_height(&btn_area) / 2);

    BubbleBurst * burst = (BubbleBurst *)calloc(1, sizeof(BubbleBurst));
    if (!burst) return;

    burst->count = 3 + (rand() % 2);

    int32_t img_w = bubble_dot.header.w;
    int32_t img_h = bubble_dot.header.h;

    for (int i = 0; i < burst->count; i++) {
        BubbleParticle * p = &burst->particles[i];

        p->img = lv_image_create(parent);
        lv_image_set_src(p->img, &bubble_dot);
        lv_image_set_pivot(p->img, img_w / 2, img_h / 2);
        lv_obj_remove_flag(p->img, LV_OBJ_FLAG_CLICKABLE);

        // 初始化物理属性
        p->x = center_x + (float)((rand() % 20) - 10);
        p->y = center_y + (float)((rand() % 10) - 5);
        p->vx = ((rand() % 100) - 50) / 60.0f;               
        p->current_vy = 0.0f;                                

        // 【差异化设置：主大气泡 vs 副小气泡】
        if (i == 0) {
            // 主大气泡：速度稍快，寿命较短 (约 0.6 秒)
            p->base_scale = 0.30f + (float)(rand() % 8) / 100.0f; 
            p->target_vy = 3.5f + (float)(rand() % 15) / 10.0f; 
            p->max_life = 32 + (rand() % 8);                   
        } else {
            // 【关键点】：小气泡大幅延长寿命 (约 1.2 ~ 1.7 秒)，上升速度调小，显得更小巧轻盈
            p->base_scale = 0.13f + (float)(rand() % 10) / 100.0f; 
            p->target_vy = 1.8f + (float)(rand() % 12) / 10.0f; 
            p->max_life = 65 + (rand() % 25);                  
        }

        p->current_scale = 0.0f;
        p->phase = (float)(rand() % 360) * 0.01745f;
        p->phase_speed = 0.06f + (float)(rand() % 8) / 100.0f;
        p->wobble_amp = 0.6f + (float)(rand() % 10) / 10.0f; 
        p->life = p->max_life;

        // 初始第 1 帧定位在按键中心，隐藏尺寸
        lv_image_set_scale(p->img, 0);
        lv_obj_set_pos(p->img, (int32_t)(p->x - img_w / 2), (int32_t)(p->y - img_h / 2));
        lv_obj_move_foreground(p->img);
    }

    // 启动 20ms (50 FPS) 定时器
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