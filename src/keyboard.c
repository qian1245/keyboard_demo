#include "keyboard.h"
#include <stdio.h>
#include <stdlib.h>

#define KEY_HEIGHT 110
#define KEY_GAP 12
#define BASE_U 115.0f

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

// ==================== 软 Q 水滴气泡动画逻辑 ====================

// 1. 上升回调
static void bubble_rise_cb(void * var, int32_t v) { 
    lv_obj_set_y((lv_obj_t *)var, v); 
}

// 2. Q 弹形变回调：让气泡 X 轴拉伸（瘦高 <-> 胖扁）
static void bubble_squish_x_cb(void * var, int32_t v) {
    lv_obj_t * bubble = (lv_obj_t *)var;
    // v 的基准为 256 (100% 原始比例)
    // 改变 X 轴缩放，产生扁平/拉长的弹簧拉伸感
    lv_obj_set_style_transform_scale_x(bubble, v, 0);
}

// 3. Q 弹形变回调：让气泡 Y 轴反向拉伸（互补形变，保持总体体积感）
static void bubble_squish_y_cb(void * var, int32_t v) {
    lv_obj_t * bubble = (lv_obj_t *)var;
    // 当 X 变宽时 Y 变窄，打造果冻水滴质感
    int32_t v_y = 512 - v; // 反向拉伸
    lv_obj_set_style_transform_scale_y(bubble, v_y, 0);
}

// 4. 破裂消失渐变
static void bubble_pop_fade_cb(void * var, int32_t v) {
    lv_obj_t * bubble = (lv_obj_t *)var;
    lv_obj_set_style_opa(bubble, v, 0);
    // 破裂瞬间剧烈膨胀
    int32_t scale = 256 + (255 - v) * 2; 
    lv_obj_set_style_transform_scale(bubble, scale, 0);
}

static void bubble_delete_cb(lv_anim_t * a) { 
    lv_obj_delete((lv_obj_t *)a->var); 
}

// 到达顶端破裂触发
static void bubble_pop_trigger_cb(lv_anim_t * a) {
    lv_obj_t * bubble = (lv_obj_t *)a->var;
    // 停止软 Q 形变动画
    lv_anim_delete(bubble, bubble_squish_x_cb);

    // 播放炸裂淡出动画
    lv_anim_t a_pop;
    lv_anim_init(&a_pop);
    lv_anim_set_var(&a_pop, bubble);
    lv_anim_set_values(&a_pop, 255, 0);
    lv_anim_set_time(&a_pop, 200);
    lv_anim_set_path_cb(&a_pop, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a_pop, bubble_pop_fade_cb);
    lv_anim_set_ready_cb(&a_pop, bubble_delete_cb);
    lv_anim_start(&a_pop);
}

// 按钮按下事件：生成水滴气泡
static void btn_bubble_event_cb(lv_event_t * e) {
    lv_obj_t * btn = lv_event_get_target(e);
    lv_obj_t * label = lv_obj_get_child(btn, 0);
    if(label) {
        printf("[KEYBOARD] 水滴气泡: %s\n", lv_label_get_text(label));
    }

    // 获取按键物理中心
    lv_area_t btn_area;
    lv_obj_get_coords(btn, &btn_area);
    int32_t center_x = btn_area.x1 + lv_area_get_width(&btn_area) / 2;
    int32_t center_y = btn_area.y1 + lv_area_get_height(&btn_area) / 2;

    // 创建气泡本体
    lv_obj_t * bubble = lv_obj_create(lv_screen_active());
    int32_t base_size = 28 + (rand() % 12); // 基础尺寸
    lv_obj_set_size(bubble, base_size, base_size);
    lv_obj_set_pos(bubble, center_x - base_size/2, center_y - base_size/2);
    lv_obj_remove_flag(bubble, LV_OBJ_FLAG_SCROLLABLE);

    // 高透亮水滴渲染样式
    lv_obj_set_style_radius(bubble, LV_RADIUS_CIRCLE, 0);
    lv_obj_set_style_bg_color(bubble, lv_color_hex(0xffffff), 0);
    lv_obj_set_style_bg_opa(bubble, LV_OPA_20, 0);                   // 薄水膜质感
    lv_obj_set_style_border_color(bubble, lv_color_hex(0x66ffff), 0);  // 亮青高光边
    lv_obj_set_style_border_width(bubble, 2, 0);
    lv_obj_set_style_shadow_color(bubble, lv_color_hex(0x00d2ff), 0);  // 水韵蓝发光
    lv_obj_set_style_shadow_width(bubble, 12, 0);

    // --- 动画 1: 从极小瞬间爆发变大，带有弹簧冲过头（OverShoot）的回弹质感 ---
    lv_anim_t a_scale;
    lv_anim_init(&a_scale);
    lv_anim_set_var(&a_scale, bubble);
    // 从 10% 原始大小 (25) 快速弹射到 100% (256)
    lv_anim_set_values(&a_scale, 25, 256);
    lv_anim_set_time(&a_scale, 350);
    // 使用 overshoots 路径：像按压水滴突然释放一样弹开
    lv_anim_set_path_cb(&a_scale, lv_anim_path_overshoot);
    lv_anim_set_exec_cb(&a_scale, (lv_anim_exec_xcb_t)lv_obj_set_style_transform_scale);
    lv_anim_start(&a_scale);

    // --- 动画 2: 软 QQ 呼吸拉伸（胖瘦交替） ---
    lv_anim_t a_squish;
    lv_anim_init(&a_squish);
    lv_anim_set_var(&a_squish, bubble);
    // 在 75% 宽度（瘦高）到 125% 宽度（胖扁）之间震荡
    lv_anim_set_values(&a_squish, 190, 320); 
    int32_t squish_time = 400 + (rand() % 200);
    lv_anim_set_time(&a_squish, squish_time);
    lv_anim_set_playback_time(&a_squish, squish_time);
    lv_anim_set_repeat_count(&a_squish, LV_ANIM_REPEAT_INFINITE);
    lv_anim_set_path_cb(&a_squish, lv_anim_path_ease_in_out);
    lv_anim_set_exec_cb(&a_squish, bubble_squish_x_cb);
    lv_anim_start(&a_squish);

    // Y 轴互补形变
    lv_anim_t a_squish_y = a_squish;
    lv_anim_set_exec_cb(&a_squish_y, bubble_squish_y_cb);
    lv_anim_start(&a_squish_y);

    // --- 动画 3: 水中自然减速上浮 ---
    lv_anim_t a_y;
    lv_anim_init(&a_y);
    lv_anim_set_var(&a_y, bubble);
    lv_anim_set_values(&a_y, center_y - base_size/2, 20); 
    lv_anim_set_time(&a_y, 1600 + (rand() % 400));
    // ease_out: 刚吐出时速度快，越往水面阻力越大越慢
    lv_anim_set_path_cb(&a_y, lv_anim_path_ease_out);
    lv_anim_set_exec_cb(&a_y, bubble_rise_cb);
    lv_anim_set_ready_cb(&a_y, bubble_pop_trigger_cb);
    lv_anim_start(&a_y);
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
            lv_obj_set_style_text_opa(label, LV_OPA_TRANSP, 0);

            lv_obj_add_event_cb(btn, btn_bubble_event_cb, LV_EVENT_PRESSED, NULL);
        }
    }
}