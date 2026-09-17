#include <SDL2/SDL.h>
#include "lvgl/lvgl.h"
#include <stdio.h>

#define SCREEN_WIDTH 1920
#define SCREEN_HEIGHT 720

static SDL_Renderer *sdl_renderer = NULL;
static SDL_Texture *sdl_texture = NULL;

// 1. 标准 LVGL 鼠标读取回调（直接从 SDL 获取，100% 准确）
static void mouse_read_cb(lv_indev_t * indev, lv_indev_data_t * data) {
    int x = 0, y = 0;
    uint32_t buttons = SDL_GetMouseState(&x, &y);

    data->point.x = (int32_t)x;
    data->point.y = (int32_t)y;
    data->state = (buttons & SDL_BUTTON(SDL_BUTTON_LEFT)) ? LV_INDEV_STATE_PRESSED : LV_INDEV_STATE_RELEASED;
}

// 按键点击事件回调（用于终端验证）
static void btn_event_cb(lv_event_t * e) {
    lv_obj_t * btn = lv_event_get_target(e);
    lv_obj_t * label = lv_obj_get_child(btn, 0);
    if(label) {
        printf("[KEYBOARD] 点击成功: %s\n", lv_label_get_text(label));
    }
}

// 键盘布局与尺寸定义
#define BASE_U 115.0f
#define KEY_HEIGHT 110
#define KEY_GAP 12

static int32_t get_key_width(float u) {
    return (int32_t)(u * BASE_U + (u - 1.0f) * KEY_GAP);
}

static void disp_flush(lv_display_t * disp, const lv_area_t * area, uint8_t * px_map) {
    SDL_UpdateTexture(sdl_texture, NULL, px_map, SCREEN_WIDTH * 4);
    SDL_RenderClear(sdl_renderer);
    SDL_RenderCopy(sdl_renderer, sdl_texture, NULL, NULL);
    SDL_RenderPresent(sdl_renderer);
    lv_disp_flush_ready(disp);
}

typedef struct { const char *label; float width_u; } KeyDef;

KeyDef row1[] = {{"Esc",1}, {"1",1}, {"2",1}, {"3",1}, {"4",1}, {"5",1}, {"6",1}, {"7",1}, {"8",1}, {"9",1}, {"0",1}, {"-",1}, {"=",1}, {"Backspace", 2.0f}};
KeyDef row2[] = {{"Tab", 1.5f}, {"Q",1}, {"W",1}, {"E",1}, {"R",1}, {"T",1}, {"Y",1}, {"U",1}, {"I",1}, {"O",1}, {"P",1}, {"[",1}, {"]",1}, {"\\", 1.5f}};
KeyDef row3[] = {{"Caps", 1.75f}, {"A",1}, {"S",1}, {"D",1}, {"F",1}, {"G",1}, {"H",1}, {"J",1}, {"K",1}, {"L",1}, {";",1}, {"'",1}, {"Enter", 2.25f}};
KeyDef row4[] = {{"Shift", 2.25f}, {"Z",1}, {"X",1}, {"C",1}, {"V",1}, {"B",1}, {"N",1}, {"M",1}, {",",1}, {".",1}, {"/",1}, {"Shift", 2.75f}};
KeyDef row5[] = {{"Ctrl", 1.25f}, {"Win", 1.25f}, {"Alt", 1.25f}, {"Space", 6.25f}, {"Alt", 1.25f}, {"Fn", 1.25f}, {"Menu", 1.25f}, {"Ctrl", 1.25f}};

KeyDef* layout[] = {row1, row2, row3, row4, row5};
int row_counts[] = {14, 14, 13, 12, 8};

void create_keyboard_ui(void) {
    lv_obj_set_style_bg_color(lv_screen_active(), lv_color_hex(0x0f172a), 0);

    // 基础按键样式
    static lv_style_t style_btn;
    lv_style_init(&style_btn);
    lv_style_set_bg_color(&style_btn, lv_color_hex(0x1e293b));
    lv_style_set_bg_opa(&style_btn, 255);
    lv_style_set_border_width(&style_btn, 2);
    lv_style_set_border_color(&style_btn, lv_color_hex(0x334155));
    lv_style_set_radius(&style_btn, 16);
    lv_style_set_text_color(&style_btn, lv_color_hex(0xf8fafc));

    // 按下时的强烈发光样式
    static lv_style_t style_btn_pr;
    lv_style_init(&style_btn_pr);
    lv_style_set_bg_color(&style_btn_pr, lv_color_hex(0x0284c7)); // 变为明亮天蓝色
    lv_style_set_border_color(&style_btn_pr, lv_color_hex(0x38bdf8));
    lv_style_set_shadow_color(&style_btn_pr, lv_color_hex(0x38bdf8));
    lv_style_set_shadow_width(&style_btn_pr, 40);
    lv_style_set_shadow_opa(&style_btn_pr, 255);

    // 过渡动画
    static const lv_style_prop_t props[] = {LV_STYLE_BG_COLOR, LV_STYLE_SHADOW_WIDTH, 0};
    static lv_style_transition_dsc_t trans;
    lv_style_transition_dsc_init(&trans, props, lv_anim_path_linear, 150, 0, NULL);
    lv_style_set_transition(&style_btn, &trans);

    // 1. 创建主容器
    lv_obj_t * main_cont = lv_obj_create(lv_screen_active());
    lv_obj_set_size(main_cont, 1850, 680);
    lv_obj_center(main_cont);
    lv_obj_set_style_bg_opa(main_cont, 0, 0);
    lv_obj_set_style_border_width(main_cont, 0, 0);
    lv_obj_set_flex_flow(main_cont, LV_FLEX_FLOW_COLUMN);
    lv_obj_set_flex_align(main_cont, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
    lv_obj_set_style_pad_row(main_cont, KEY_GAP, 0);

    // 彻底禁用主容器的滚动行为与滚动条
    lv_obj_remove_flag(main_cont, LV_OBJ_FLAG_SCROLLABLE);
    lv_obj_set_scrollbar_mode(main_cont, LV_SCROLLBAR_MODE_OFF);

    for(int r = 0; r < 5; r++) {
        // 2. 创建行容器
        lv_obj_t * row_cont = lv_obj_create(main_cont);
        lv_obj_set_width(row_cont, LV_PCT(100));
        lv_obj_set_height(row_cont, LV_SIZE_CONTENT);
        lv_obj_set_style_bg_opa(row_cont, 0, 0);
        lv_obj_set_style_border_width(row_cont, 0, 0);
        lv_obj_set_style_pad_all(row_cont, 0, 0);

        // 彻底禁用行容器的滚动行为与滚动条
        lv_obj_remove_flag(row_cont, LV_OBJ_FLAG_SCROLLABLE);
        lv_obj_set_scrollbar_mode(row_cont, LV_SCROLLBAR_MODE_OFF);

        lv_obj_set_flex_flow(row_cont, LV_FLEX_FLOW_ROW);
        lv_obj_set_flex_align(row_cont, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);

        for(int k = 0; k < row_counts[r]; k++) {
            lv_obj_t * btn = lv_button_create(row_cont);
            lv_obj_add_style(btn, &style_btn, 0);
            lv_obj_add_style(btn, &style_btn_pr, LV_STATE_PRESSED);

            int32_t w = get_key_width(layout[r][k].width_u);
            lv_obj_set_size(btn, w, KEY_HEIGHT);

            lv_obj_t * label = lv_label_create(btn);
            lv_label_set_text(label, layout[r][k].label);
            lv_obj_center(label);

            // 绑定事件回调
            lv_obj_add_event_cb(btn, btn_event_cb, LV_EVENT_PRESSED, NULL);
        }
    }
}

int main(int argc, char* argv[]) {
    SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER | SDL_INIT_EVENTS);
    SDL_Window* window = SDL_CreateWindow("61-Key Layout",
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        SCREEN_WIDTH, SCREEN_HEIGHT, SDL_WINDOW_SHOWN);

    sdl_renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED);
    sdl_texture = SDL_CreateTexture(sdl_renderer, SDL_PIXELFORMAT_ARGB8888,
                                    SDL_TEXTUREACCESS_STREAMING, SCREEN_WIDTH, SCREEN_HEIGHT);

    lv_init();
    
    // 1. 创建显示设备
    lv_display_t * disp = lv_display_create(SCREEN_WIDTH, SCREEN_HEIGHT);
    lv_display_set_color_format(disp, LV_COLOR_FORMAT_ARGB8888);
    static uint32_t buf_1[SCREEN_WIDTH * SCREEN_HEIGHT];
    lv_display_set_buffers(disp, buf_1, NULL, sizeof(buf_1), LV_DISPLAY_RENDER_MODE_FULL);
    lv_display_set_flush_cb(disp, disp_flush);

    // 2. 注册输入设备 (显式绑定到当前屏幕)
    lv_indev_t * indev = lv_indev_create();
    lv_indev_set_type(indev, LV_INDEV_TYPE_POINTER);
    lv_indev_set_read_cb(indev, mouse_read_cb);
    lv_indev_set_display(indev, disp); // <-- 修复1: 确保输入设备绑定到正确的屏幕

    create_keyboard_ui();

    int running = 1;
    SDL_Event event;
    
    // 更好的时间管理记录
    uint32_t last_tick = SDL_GetTicks();
    
    while (running) {
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_QUIT) running = 0;
        }
        
        // <-- 修复2: 注入灵魂心跳！计算距离上一次循环过去了多少毫秒
        uint32_t current_tick = SDL_GetTicks();
        lv_tick_inc(current_tick - last_tick);
        last_tick = current_tick;

        lv_timer_handler(); // 处理 LVGL 任务
        SDL_Delay(5);       // 挂起 5ms 释放 CPU
    }

    SDL_DestroyTexture(sdl_texture);
    SDL_DestroyRenderer(sdl_renderer);
    SDL_DestroyWindow(window);
    SDL_Quit();
    return 0;
}